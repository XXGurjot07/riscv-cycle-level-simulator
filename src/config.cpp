#include "config.hpp"
#include <cstring>
#include <cstdlib>

SimConfig parse_args(int argc, char** argv){

    SimConfig config_;
    for (int i = 1; i < argc; i++){
        
        const char* arg = argv[i];

        if(std::strcmp(arg, "-quiet") == 0){
            config_.log_on = false;
        }
        else if (std::strcmp(arg, "-forwarding") == 0){
            config_.forwarding = true;
        }

        else if (std::strncmp(arg, "-mul-lat=", 9) == 0){
            config_.mul_latency = std::atoi(arg + 9);
        }
        else if (std::strcmp(arg, "-mul-pipe") == 0){
            config_.mul_pipelined = true;
        }
        
        else if (std::strncmp(arg, "-div-lat=", 9) == 0){
            config_.div_latency = std::atoi(arg + 9);
        }
        else if (std::strcmp(arg, "-div-pipe") == 0){
            config_.div_pipelined = true;
        }

        else if (std::strncmp(arg, "-memsize=", 9) == 0){
            config_.MEM_SIZE = std::atoi(arg + 9);
        }
        else if (std::strncmp(arg, "-maxcycl=", 9) == 0){
            config_.MAX_CYCLES = std::atoi(arg + 9);
        }

        else if (arg[0] == '-'){ 
            std::fprintf(stderr, "unknown option %s\n", arg); 
            std::exit(2); 
        }
        else{
            config_.benchmark_elf = arg;
        }

    }

    return config_;
}

const char* why_name(Bubble_Why w){
    switch(w){
        case FILL              : return "Fill";
        case RAW               : return "RAW Stall";
        case LOAD_USE          : return "Load-Use";
        case BRANCH_MISPREDICT : return "Branch/Jump Flush";
        case FENCE_I           : return "Fence.i Flush";
        case EX_BUSY           : return "EX Busy";
        default                : return "?";
    }
}

uint64_t Core_HPM_Stats::accounted() const {
    uint64_t sum = retired + trap_cycles;
    for (int i = 0; i < NWHY; i++)
        sum += lost[i];
    return sum;
}

// one row: bucket name, cycles, CPI contribution, share of all cycles
static void stack_row(const char* name, uint64_t c, uint64_t retired, uint64_t cycles) {
    double cpi   = retired ? double(c) / double(retired) : 0.0;
    double share = cycles  ? 100.0 * double(c) / double(cycles) : 0.0;
    std::printf("  %-18s %12llu %8.3f %7.1f%%\n", name, (unsigned long long)c, cpi, share);
}

void Core_HPM_Stats::print_cpi_stack() const {
    std::printf("  %-18s %12s %8s %8s\n", "bucket", "cycles", "CPI", "share");
    stack_row("base (retired)", retired, retired, cycles);
    for (int i = 0; i < NWHY; i++)
        stack_row(why_name(Bubble_Why(i)), lost[i], retired, cycles);
    stack_row("trap", trap_cycles, retired, cycles);

    uint64_t sum = accounted();
    std::printf("  %-18s %12llu %8.3f   %s\n", "total", (unsigned long long)sum,
                retired ? double(sum) / double(retired) : 0.0,
                sum == cycles ? "(== cycles)" : "MISMATCH with cycles!");
}

void Core_HPM_Stats::print(const SimConfig& cfg) const {
    std::printf("\n== %s ==\n", cfg.benchmark_elf);
    std::printf("cycles %llu, retired %llu, CPI %.3f\n",
                (unsigned long long)cycles, (unsigned long long)retired, cpi());
    print_cpi_stack();
}

void printSimConfig(const SimConfig& c){
    std::printf("config: elf %s | forwarding %s | mul %d cyc %s | div %d cyc %s | mem %llu KB | max %llu cycles\n",
                c.benchmark_elf, c.forwarding ? "on" : "off",
                c.mul_latency, c.mul_pipelined ? "pipelined" : "blocking",
                c.div_latency, c.div_pipelined ? "pipelined" : "blocking",
                (unsigned long long)(c.MEM_SIZE >> 10), (unsigned long long)c.MAX_CYCLES);
}