#include "logging.hpp"

const char* exc_name(Exception e){
    switch(e){
        case Exception::INST_MISALIGNED    : return "instruction misaligned";
        case Exception::INST_ACCESS_FAULT  : return "instruction access fault";
        case Exception::ILLEGAL_INST       : return "illegal instruction";
        case Exception::BREAKPOINT         : return "breakpoint";
        case Exception::LOAD_MISALIGNED    : return "load misaligned";
        case Exception::LOAD_ACCESS_FAULT  : return "load access fault";
        case Exception::STORE_MISALIGNED   : return "store misaligned";
        case Exception::STORE_ACCESS_FAULT : return "store access fault";
        case Exception::ECALL_U            : return "ecall from U";
        case Exception::ECALL_S            : return "ecall from S";
        case Exception::ECALL_M            : return "ecall from M";
        default                            : return "none";
    }
}

void log_commit(uint64_t cycle, const retired_Res& wb){
    if (wb.writes_rd && wb.rd != 0)
        std::printf("%8llu  0x%08llx  x%-2u  0x%016llx\n", (unsigned long long)cycle,
                    (unsigned long long)wb.pc, (unsigned)wb.rd, (unsigned long long)wb.rd_data);
    else
        std::printf("%8llu  0x%08llx\n", (unsigned long long)cycle, (unsigned long long)wb.pc);
}

// Same format as `spike -l --log-commits`, so tools/cosim.py can diff the two line by line
void log_commit_spike(const retired_Res& wb){
    std::printf("core   0: 3 0x%016llx (0x%08x)", (unsigned long long)wb.pc, (unsigned)wb.instruction);

    if (wb.writes_rd && wb.rd != 0)                       // Spike never prints writes to x0
        std::printf(" x%-2u 0x%016llx", (unsigned)wb.rd, (unsigned long long)wb.rd_data);

    if (wb.is_load)
        std::printf(" mem 0x%016llx", (unsigned long long)wb.mem_addr);

    if (wb.is_store) {
        // only the stored bytes, printed at the store's width (sb = 2 hex digits ... sd = 16)
        uint64_t mask = (wb.mem_bytes >= 8) ? ~0ull : ((1ull << (8 * wb.mem_bytes)) - 1);
        std::printf(" mem 0x%016llx 0x%0*llx", (unsigned long long)wb.mem_addr,
                    2 * wb.mem_bytes, (unsigned long long)(wb.mem_data & mask));
    }

    std::printf("\n");
}

void log_header(){
    std::printf("   cycle  pc          rd   value\n");
}

void log_trap(const retired_Res& wb){
    std::printf("trap: %s (cause %d) at pc 0x%08llx, tval 0x%llx\n", exc_name(wb.exc), (int)wb.exc,
                (unsigned long long)wb.pc, (unsigned long long)wb.exc_val);
}
