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
RV_ARCH  = -march=rv64i -mabi=lp64 -O1 -ffreestanding -fno-exceptions -fno-rtti -nostdlib
RV_FLAGS = $(RV_ARCH) -T programs/link.ld

programs: programs/prog.elf

programs/prog.elf: programs/start.S programs/prog.cpp programs/link.ld
	$(RV_CC) $(RV_FLAGS) programs/start.S programs/prog.cpp -o $@

# Same program, linked for Spike: RAM at 0x80000000, exit through tohost (exit code = main's return)
SPIKE ?= $(HOME)/riscv_sim/riscv-isa-sim/build/spike

programs/spike/prog.elf: programs/spike/start.S programs/prog.cpp programs/spike/link.ld
	$(RV_CC) $(RV_ARCH) -T programs/spike/link.ld programs/spike/start.S programs/prog.cpp -o $@

spike: programs/spike/prog.elf
	$(SPIKE) --isa=rv64i -l --log-commits $< 2> programs/spike/prog.log; echo "spike exit code (= a0): $$?"
	grep -E '^core +0: 3 0x00000000800' programs/spike/prog.log | uniq | head -40

clean:
	rm -f sim sim.exe programs/*.elf programs/spike/*.elf programs/spike/*.log

.PHONY: programs spike clean
