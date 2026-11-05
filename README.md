# rv64-cycle-level-performance-simulator

A cycle-level C++ model of a 5-stage in-order RV64IM pipeline, written from scratch.

Gurjot Singh

## Features
- IF / ID / EX / MEM / WB with double-buffered latches; one `tick()` = one clock edge.
- **RV64IM**: integer base plus multiply/divide, including all the M-extension edge cases.
- **Forwarding** (EX/MEM and MEM/WB bypass, 1-cycle load-use stall) or stall-only mode.
- **Multi-cycle EX**: functional-unit lanes (ALU, BRU, AGU, MUL, DIV) with configurable
  latency and pipelining, a **scoreboard** for MUL/DIV results, and a **result shift register**
  so results still complete in order while independent instructions overlap.
- **CPI stack**: every bubble carries its cause and is counted at writeback, so
  `retired + trap + bubbles == cycles` always holds.
- **Lockstep cosim** with Spike's commit log (`tools/cosim.py`).

## Requirements
- `g++` with C++17
- `riscv64-unknown-elf-gcc` (with an `rv64im` / `lp64` multilib)
- `spike` and `python3` for `make check`

## Build and run
```bash
make                 # build the simulator  -> ./sim
make programs        # build programs/benchmark.elf
make check           # cosim against Spike, stall-only and forwarding
make run ARGS="-quiet -forwarding"
```

```bash
./sim                              # runs programs/benchmark.elf
./sim path/to/prog.elf -quiet      # summary and CPI stack only
```

| option | meaning | default |
|---|---|---|
| `-quiet` | no per-instruction commit log | off |
| `-forwarding` | enable the bypass network | off (stall-only) |
| `-mul-lat=N`, `-div-lat=N` | MUL / DIV latency in cycles | 3 / 10 |
| `-mul-pipe`, `-div-pipe` | unit accepts a new op every cycle | MUL yes / DIV no |
| `-maxcycl=N` | cycle limit | 1M |
| `-memsize=N` | RAM in bytes | 64 KB |
| `-cosim` | print Spike-format commit lines (used by `cosim.py`) | off |

**Exit code:** the value `main` returns (it is passed out through `tohost`), or 2 if the ELF fails to load.
A trap or the cycle limit currently also exits with 0, so rely on `make check` for pass or fail.

## Benchmark
`programs/benchmark.cpp` is self-checking: `main` returns 0, or N if kernel N gave the wrong checksum.
Each kernel targets one CPI-stack bucket:

| # | kernel | stresses |
|---|---|---|
| 1 | pointer_chase | load-use |
| 2–3 | insertion_sort, binary_search | branch flushes |
| 4 | crc32 | ALU dependency chains (forwarding) |
| 5 | fib | calls, returns, stack traffic |
| 6 | mem | every load/store width |
| 7 | mul_chain | dependent MUL chain |
| 8 | matmul | back-to-back independent MULs (pipelining) |
| 9 | mul_high | `mulh` / `mulhu` / `mulhsu` / `mulw` |
| 10 | div | dependent DIV/REM on the non-pipelined divider |
| 11 | muldiv_edges | divide by zero, INT_MIN / −1, sign mixes |

Kernels 7–11 are built only with `ISA = rv64im` (set at the top of the `Makefile`).
To regenerate the expected checksums: `g++ -DHOST -O1 programs/benchmark.cpp -o ref && ./ref`.

| mode | cycles | CPI |
|---|---|---|
| stall-only | 685,042 | 2.561 |
| forwarding | 449,254 | 1.679 |

## Layout
| path | contents |
|---|---|
| `src/core.cpp` | the cycle loop: commit, evaluate, hazards and scoreboard, clock edge |
| `src/{fetch,decode,execute,memory,writeback}_stage.cpp` | one class per stage |
| `include/execute_stage.hpp` | `FunctionalUnit<T>` latency model and the EX lanes |
| `src/hazard_unit.cpp` | RAW / load-use detection and the forwarding muxes |
| `include/definitions_*.hpp` | ISA enums, architectural state, pipeline latches, bubble tags |
| `src/config.cpp` | command-line options and the CPI stack printout |
| `programs/` | `start.S`, `link.ld`, `benchmark.cpp` (Spike memory map: RAM at `0x80000000`) |
| `tools/cosim.py` | lockstep commit-log comparison against Spike |

## Roadmap
- [x] 5-stage in-order RV64I, stall-only
- [x] Forwarding
- [x] M extension, scoreboard, in-order completion
- [ ] Branch prediction
- [ ] CSRs and traps
- [ ] Caches
- [ ] Multi-core with coherence
- [ ] Out-of-order core
