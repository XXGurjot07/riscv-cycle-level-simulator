#pragma once
#include <cstdint>
#include <cstdio>
#include <cstddef>
#include "definitions_pipeline.hpp"

constexpr size_t   XLEN = 64;               // 64-bit RV Machine 
constexpr uint64_t MEM_BASE = 0x80000000;   // Spike ISA Mem Base

// default core configs
struct SimConfig {
    const char* benchmark_elf = "programs/benchmark.elf";

    bool log_on     = true;
    bool cosim      = false;
    bool forwarding = false;

    int  mul_latency   = 3;
    bool mul_pipelined = true;
    int  div_latency   = 10;
    bool div_pipelined = false;

    uint64_t MEM_SIZE   = (1 << 16);  // 64 KB
    uint64_t MAX_CYCLES = (1 << 20);  // 1M  
};

struct Core_HPM_Stats {
    // cycle accounting (CPI stack): every cycle lands in exactly one bucket
    uint64_t cycles = 0;

    uint64_t retired = 0;
    uint64_t trap_cycles = 0;
    uint64_t lost[NWHY] = {};

    // events
    double cpi() const { 
        return retired ? double(cycles) / double(retired) : 0.0; 
    }
    
    uint64_t accounted()                 const;   // retired + trap + all lost; must equal cycles
    void     print(const SimConfig& cfg) const;   // summary + CPI stack
    void     print_cpi_stack()           const;
};

// to pass from main
SimConfig parse_args(int argc, char** argv);

// to print current config
void printSimConfig(const SimConfig& config);