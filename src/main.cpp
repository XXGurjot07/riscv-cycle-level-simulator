#include "core.hpp"
#include "config.hpp"
#include "logging.hpp"

int main(int argc, char** argv){

    SimConfig cfg = parse_args(argc, argv);
    printSimConfig(cfg);

    // allocated on heap with an initial size
    std::vector<uint8_t> memory(cfg.MEM_SIZE);

    // load elf
    uint64_t entry;
    uint64_t to_host;
    try { 
        entry = load_elf64(cfg.benchmark_elf, memory.data(), memory.size(), MEM_BASE); 
        to_host = elf64_symbol(cfg.benchmark_elf, "tohost");
    }
    catch (const std::exception& e) { 
        std::fprintf(stderr, "load failed: %s\n", e.what()); 
        return 2; 
    }

    // core 0
    Core core(memory.data(), entry, cfg, to_host);

    if (cfg.log_on && !cfg.cosim){
        log_header();
    }
    // core 0 run
    core.run(); 
    core.stats().print(cfg);

    return core.exit_code();
}
