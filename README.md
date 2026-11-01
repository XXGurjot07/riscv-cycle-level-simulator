# rv64-cycle-level-performance-simulator

Gurjot Singh

A cycle-level C++ model of a 5-stage in-order RV64I pipeline, written from scratch.
It is the first step of a full roadmap: forwarding and MFAD extensions, then caches,
then multi-core with coherence, then out-of-order cores.

## Status (`v1.0-stall-only`)
- IF / ID / EX / MEM / WB with double-buffered latches; precise traps taken at writeback.
- RAW hazards are resolved by stalling: no forwarding, branch prediction or caches yet.
- Branches, jumps and `fence.i` resolve in EX and flush the two younger stages.
- EX is built from functional-unit lanes (ALU, BRU, AGU) with configurable latency.
- A CPI stack built from tagged bubbles that are counted at writeback, so every cycle is accounted for.
- Memory map matches Spike Simulator: 64 KB of RAM at `0x80000000`.

## Build
```bash
make            # simulator     (g++, C++17)
make programs   # test programs (riscv64-unknown-elf-gcc on PATH)
make spike      # same program on Spike, with its commit log
```
On Windows without `make`:
```bash
g++ -std=c++17 -O2 -Wall -Wextra -Wshadow -Iinclude src/*.cpp -o sim
```

## Run (from the repo root)
```bash
./sim                              # runs programs/prog.elf
./sim path/to/test.elf -quiet      # no commit log, only the summary and CPI stack
```

| option | meaning |
|---|---|
| `-quiet` | turn off the per-instruction commit log |
| `-maxcycl=N` | cycle limit (default 1M) |
| `-memsize=N` | simulated RAM in bytes (default 64 KB) |
| `-forwarding`, `-mul-lat=N`, `-mul-pipe`, `-div-lat=N`, `-div-pipe` | reserved for the next version |

Exit code: 0 = halted on a trap (`ebreak` ends a program), 1 = cycle limit reached, 2 = ELF failed to load.

## Output
The commit log has one line per retired instruction: `cycle  pc  rd  value`.
The run ends with a CPI stack. CPI is cycles per retired instruction (lower is better).
Each row is the number of cycles that went to one cause:

## How it works
- **One `tick()` = one clock edge.** WB runs first (write-first register file),
  then MEM, EX, ID and IF compute their next latch values from the current ones.
  The core then commits them under one priority chain: redirect > EX busy > RAW stall > advance.
- **Bubbles carry a cause.** Every pipeline latch has a `why` tag next to `valid`. A bubble is
  tagged where it is created (a RAW stall, a flush, EX busy) and copied by every stage.
  Only writeback counts it, so flushed and wrong-path work is never counted, and
  `retired + trap + bubbles == cycles` always holds.

## Layout
| path | contents |
|---|---|
| `include/definitions_*.hpp` | ISA enums, architectural state, pipeline latches and bubble tags |
| `include/config.hpp`, `src/config.cpp` | `SimConfig` (run-time options), argument parsing, stats printing |
| `src/core.cpp` | `Core`: the cycle loop (commit, evaluate, control, clock edge) |
| `src/{fetch,decode,execute,memory,writeback}_stage.cpp` | one class per stage |
| `include/execute_stage.hpp` | `FunctionalUnit<T>` latency/occupancy model and the EX lanes |
| `src/hazard_unit.cpp` | RAW hazard detection |
| `src/logging.cpp` | commit log, trap and bubble-cause names |
| `include/elf64_loader.hpp` | loads ELF64 RISC-V programs into simulated memory |
| `programs/` | bare-metal test program (`start.S`, `link.ld`, `prog.cpp`) |
| `programs/spike/` | the same program linked for Spike (exits through `tohost`) |
