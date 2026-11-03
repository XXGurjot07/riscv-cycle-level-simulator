#!/usr/bin/env python3
"""Lockstep check: run one ELF on Spike and on our sim, compare their commit logs line by line.

usage: python3 tools/cosim.py programs/prog.elf [--sim ./sim] [--spike spike] [--sim-arg=-forwarding]
PASS = identical commit logs up to (and including) the tohost store, and both exit codes 0.
"""
import argparse
import re
import struct
import subprocess
import sys
from pathlib import Path

MEM_BASE = 0x80000000
COMMIT   = re.compile(r"^core\s+0: 3 ")      # Spike commit lines (priv 3 = M-mode); our sim prints the same
CONTEXT  = 5


def elf_symbol(path, name):
    """Address of `name` in the ELF symbol table, or None."""
    d = Path(path).read_bytes()
    shoff, = struct.unpack_from("<Q", d, 0x28)
    shentsize, shnum = struct.unpack_from("<HH", d, 0x3A)
    sections = [struct.unpack_from("<IIQQQQIIQQ", d, shoff + i * shentsize) for i in range(shnum)]
    for sh in sections:
        if sh[1] != 2:                                   # SHT_SYMTAB
            continue
        str_off = sections[sh[6]][4]                     # linked string table's file offset
        for at in range(sh[4], sh[4] + sh[5], 24):
            st_name, = struct.unpack_from("<I", d, at)
            st_value, = struct.unpack_from("<Q", d, at + 8)
            end = d.index(b"\0", str_off + st_name)
            if d[str_off + st_name:end].decode() == name:
                return st_value
    return None


def parse(line):
    """Split a commit line into named fields, so a mismatch can say which field differs."""
    t = line.split()
    f = {"pc": t[3], "inst": t[4]}
    rest = t[5:]
    if rest and rest[0].startswith("x"):
        f["rd"], f["rd_value"] = rest[0], rest[1]
        rest = rest[2:]
    if rest and rest[0] == "mem":
        f["mem_addr"] = rest[1]
        if len(rest) > 2:
            f["store_data"] = rest[2]
    return f


def is_store_to(line, addr):
    f = parse(line)
    return "store_data" in f and int(f["mem_addr"], 16) == addr


def commits(text, tohost):
    """Commit lines in program memory, cut after the tohost store (Spike keeps spinning after it).
    Also returns how many commits came after that store: must be 0 for our sim (it must halt there)."""
    out, after = [], 0
    for line in text.splitlines():
        if not COMMIT.match(line):
            continue
        line = " ".join(line.split())                    # spacing is not significant
        if int(line.split()[3], 16) < MEM_BASE:          # Spike's boot ROM at 0x1000
            continue
        if out and tohost is not None and is_store_to(out[-1], tohost):
            after += 1
            continue
        out.append(line)
    return out, after


def run(cmd, timeout):
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        sys.exit(f"TIMEOUT: {' '.join(cmd)} (did the program never write tohost?)")
    except FileNotFoundError:
        sys.exit(f"not found: {cmd[0]}")
    return p.returncode, p.stdout, p.stderr


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("elf")
    ap.add_argument("--sim", default="./sim")
    ap.add_argument("--spike", default="spike")
    ap.add_argument("--sim-arg", action="append", default=[], help="extra option for our sim (repeatable)")
    ap.add_argument("--timeout", type=int, default=60)
    ap.add_argument("--out", default="cosim_out", help="directory for the two filtered logs")
    a = ap.parse_args()

    if not Path(a.elf).is_file():
        sys.exit(f"ELF not found: {a.elf} (run 'make programs' first)")
    tohost = elf_symbol(a.elf, "tohost")
    if tohost is None:
        print("warning: no 'tohost' symbol; comparing full logs")

    spike_rc, _, spike_err = run([a.spike, "--isa=rv64i", "-l", "--log-commits", a.elf], a.timeout)
    sim_rc, sim_out, _     = run([a.sim, a.elf, "-cosim", *a.sim_arg], a.timeout)

    ref, _         = commits(spike_err, tohost)          # Spike writes its log to stderr
    dut, sim_after = commits(sim_out, tohost)

    out = Path(a.out)
    out.mkdir(exist_ok=True)
    (out / "spike.log").write_text("\n".join(ref) + "\n")
    (out / "sim.log").write_text("\n".join(dut) + "\n")

    print(f"tohost = {hex(tohost) if tohost is not None else 'none'}")
    print(f"spike : {len(ref):7d} commits, exit code {spike_rc}")
    print(f"sim   : {len(dut):7d} commits, exit code {sim_rc}")

    for i, (r, d) in enumerate(zip(ref, dut)):
        if r != d:
            fr, fd = parse(r), parse(d)
            fields = [k for k in dict.fromkeys(list(fr) + list(fd)) if fr.get(k) != fd.get(k)]
            print(f"\nMISMATCH at commit #{i}  (differs in: {', '.join(fields)})")
            for j in range(max(0, i - CONTEXT), i):
                print(f"   ok    {ref[j]}")
            print(f"   spike {r}")
            print(f"   sim   {d}")
            print(f"\nfull logs: {out}/spike.log  {out}/sim.log")
            return 1

    if len(ref) != len(dut):
        short, n = ("sim", len(dut)) if len(dut) < len(ref) else ("spike", len(ref))
        longer = ref if short == "sim" else dut
        print(f"\nMISMATCH: {short} stopped after {n} commits; next expected line:")
        print(f"   {longer[n]}")
        return 1

    if sim_after:
        print(f"\nFAIL: sim kept running after the tohost store ({sim_after} more commits): it must halt there")
        return 1

    if spike_rc != 0 or sim_rc != 0:
        print(f"\nlogs match, but exit codes are spike={spike_rc} sim={sim_rc} (0 = program passed)")
        return 1

    print("\nPASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
