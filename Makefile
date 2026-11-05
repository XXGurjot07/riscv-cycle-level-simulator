# make sim        -> build the simulator (host g++)
# make programs   -> build the RISC-V test programs (needs riscv64-unknown-elf-gcc on PATH)
# make check      -> run the cosim on both spike isa simulator & riscv simulator
# make run ARGS   -> run only the simulator
# make clean      -> delete the sim and elf files   

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wshadow -Iinclude

SRCS = $(wildcard src/*.cpp)
HDRS = $(wildcard include/*.hpp)

sim: $(SRCS) $(HDRS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $@

# RV64I only for now: programs must not use * / % until the M extension exists
RV_CC    = riscv64-unknown-elf-gcc
RV_ARCH  = -march=rv64i 
RV_FLAGS = -mabi=lp64 -O1 -ffreestanding -fno-exceptions -fno-rtti -nostdlib -mcmodel=medany

programs: programs/benchmark.elf

programs/benchmark.elf: programs/start.S programs/benchmark.cpp programs/link.ld
	$(RV_CC) $(RV_ARCH) $(RV_FLAGS) -T programs/link.ld programs/start.S programs/benchmark.cpp -o $@

# Same program, linked for Spike: RAM at 0x80000000, exit through tohost (exit code = main's return)
SPIKE ?= spike

spike: programs/benchmark.elf
	$(SPIKE) -$(RV_ARCH) -l --log-commits $< 2> programs/prog.log; echo "spike exit code (= a0): $$?"
	grep -E '^core +0: 3 0x00000000800' programs/prog.log | uniq | head -40

# Lockstep check: same ELF on Spike and on our sim, commit logs must match up to the tohost store
check: sim programs/benchmark.elf
	python3 tools/cosim.py programs/benchmark.elf --sim ./sim --spike $(SPIKE)
	python3 tools/cosim.py programs/benchmark.elf --sim ./sim --spike $(SPIKE) --sim-arg=-forwarding


# Run benchmark and report CPI Stack
ARGS ?= 

run: sim programs/benchmark.elf
	./sim programs/benchmark.elf $(ARGS) 

clean:
	rm -f sim sim.exe programs/*.elf programs/*.log
	rm -rf cosim_out

.PHONY: programs spike check clean
