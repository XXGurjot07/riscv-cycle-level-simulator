# make            -> build the simulator (host g++)
# make programs   -> build the RISC-V test programs (needs riscv64-unknown-elf-gcc on PATH)
# make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wshadow -Iinclude

SRCS = $(wildcard src/*.cpp)
HDRS = $(wildcard include/*.hpp)

sim: $(SRCS) $(HDRS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $@

# RV64I only for now: programs must not use * / % until the M extension exists
RV_CC    = riscv64-unknown-elf-gcc
RV_ARCH  = -march=rv64i -mabi=lp64 -O1 -ffreestanding -fno-exceptions -fno-rtti -nostdlib -mcmodel=medany
RV_FLAGS = $(RV_ARCH) -T programs/link.ld

programs: programs/prog.elf

programs/prog.elf: programs/start.S programs/prog.cpp programs/link.ld
	$(RV_CC) $(RV_FLAGS) programs/start.S programs/prog.cpp -o $@

# Same program, linked for Spike: RAM at 0x80000000, exit through tohost (exit code = main's return)
SPIKE ?= spike

programs/prog.elf: programs/start.S programs/prog.cpp programs/link.ld
	$(RV_CC) $(RV_ARCH) -T programs/link.ld programs/start.S programs/prog.cpp -o $@

spike: programs/prog.elf
	$(SPIKE) --isa=rv64i -l --log-commits $< 2> programs/prog.log; echo "spike exit code (= a0): $$?"
	grep -E '^core +0: 3 0x00000000800' programs/prog.log | uniq | head -40

# Lockstep check: same ELF on Spike and on our sim, commit logs must match up to the tohost store
check: sim programs/prog.elf
	python3 tools/cosim.py programs/prog.elf --sim ./sim --spike $(SPIKE)
	python3 tools/cosim.py programs/prog.elf --sim ./sim --spike $(SPIKE) --sim-arg=-forwarding

clean:
	rm -f sim sim.exe programs/*.elf programs/*.log
	rm -rf cosim_out

.PHONY: programs spike check clean
