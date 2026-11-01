#include "memory_stage.hpp"

// As this is cacheless, all Mem Op are single cycle
// Also, the memory is byte addressable and we assume multiple read/write are possible

static uint64_t do_load(uint8_t* mem, uint8_t mem_bytes, bool mem_signed, uint64_t addr){
        
    uint64_t rd_data = 0;
    bool     sign    = false;
        
    switch (mem_bytes)
    {
        case 1: {
            rd_data = (uint64_t)mem[addr];  
            
            sign = (rd_data & 0x80);

            if(sign && mem_signed){
                rd_data |= 0xFFFFFFFFFFFFFF00ULL;
            }

            break;
        }
        case 2: {
            rd_data = (uint64_t)mem[addr]          | ((uint64_t)mem[addr+1] << 8);
            
            sign = (rd_data & 0x8000);

            if(sign && mem_signed){
                rd_data |= 0xFFFFFFFFFFFF0000ULL;
            }
            
            break;
        }
        case 4: {
            rd_data =   (uint64_t)mem[addr]          | ((uint64_t)mem[addr+1] << 8) 
                    |  ((uint64_t)mem[addr+2] << 16) | ((uint64_t)mem[addr+3] << 24);

            sign = rd_data & 0x80000000;   
            
            if(sign && mem_signed){
                rd_data |= 0xFFFFFFFF00000000ULL;
            }

            break;
        }
        case 8: {
            rd_data =   (uint64_t)mem[addr]          | ((uint64_t)mem[addr+1] << 8) 
                    |  ((uint64_t)mem[addr+2] << 16) | ((uint64_t)mem[addr+3] << 24) 
                    |  ((uint64_t)mem[addr+4] << 32) | ((uint64_t)mem[addr+5] << 40)
                    |  ((uint64_t)mem[addr+6] << 48) | ((uint64_t)mem[addr+7] << 56);
            break;
        }
    }
    return rd_data;
}

static void do_store(uint8_t* mem, uint8_t mem_bytes, uint64_t addr, uint64_t data){

    switch (mem_bytes) {
        case 1: { // SB (Store Byte)
            mem[addr] = static_cast<uint8_t>(data & 0xFF);
            break;
        }
        case 2: { // SH (Store Halfword)
            mem[addr]     = static_cast<uint8_t>(data & 0xFF);
            mem[addr + 1] = static_cast<uint8_t>((data >> 8) & 0xFF);
            break;
        }
        case 4: { // SW (Store Word)
            mem[addr]     = static_cast<uint8_t>(data & 0xFF);
            mem[addr + 1] = static_cast<uint8_t>((data >> 8) & 0xFF);
            mem[addr + 2] = static_cast<uint8_t>((data >> 16) & 0xFF);
            mem[addr + 3] = static_cast<uint8_t>((data >> 24) & 0xFF);
            break;
        }
        case 8: { // SD (Store Doubleword)
            mem[addr]     = static_cast<uint8_t>(data & 0xFF);
            mem[addr + 1] = static_cast<uint8_t>((data >> 8) & 0xFF);
            mem[addr + 2] = static_cast<uint8_t>((data >> 16) & 0xFF);
            mem[addr + 3] = static_cast<uint8_t>((data >> 24) & 0xFF);
            mem[addr + 4] = static_cast<uint8_t>((data >> 32) & 0xFF);
            mem[addr + 5] = static_cast<uint8_t>((data >> 40) & 0xFF);
            mem[addr + 6] = static_cast<uint8_t>((data >> 48) & 0xFF);
            mem[addr + 7] = static_cast<uint8_t>((data >> 56) & 0xFF);
            break;
        }
    }
    return;
}

MemStage :: MemStage(uint64_t mem_size) : mem_size_(mem_size) {}

reg_MEM_WB MemStage :: evaluate (uint8_t* mem, const reg_EX_MEM& ex_mem_reg){

    reg_MEM_WB reg;
    FUType     futype = ex_mem_reg.fu_type;
    bool       is_mem = (futype == FUType::LOAD || futype == FUType::STORE);

    reg.pc        = ex_mem_reg.pc;
    reg.rd        = ex_mem_reg.rd;
    reg.rd_data   = ex_mem_reg.rd_data;
    reg.writes_rd = ex_mem_reg.writes_rd;
    reg.exc       = ex_mem_reg.exc;
    reg.exc_val   = ex_mem_reg.exc_val;
    reg.valid     = ex_mem_reg.valid;
    reg.why       = ex_mem_reg.why;

    if (!ex_mem_reg.valid || ex_mem_reg.exc != Exception::NONE || !is_mem) {
        return reg;
    }

    uint64_t addr       = ex_mem_reg.mem_addr;
    uint64_t data       = ex_mem_reg.mem_data;
    bool     mem_signed = ex_mem_reg.mem_signed;
    uint8_t  mem_bytes  = ex_mem_reg.mem_bytes;
    
    // Addressing Offset
    addr = addr - MEM_BASE;

    // mem out of bound access check
    bool mem_in_bounds = (addr <= mem_size_-mem_bytes);

    if(!mem_in_bounds){
        if(futype == FUType::LOAD)  { reg.exc = Exception::LOAD_ACCESS_FAULT;}
        if(futype == FUType::STORE) { reg.exc = Exception::STORE_ACCESS_FAULT;}
        
        reg.exc_val = ex_mem_reg.mem_addr;
        return reg;
    }

    if (futype == FUType::LOAD){
        reg.rd_data =  do_load(mem, mem_bytes, mem_signed, addr);        
    }
    else if (futype == FUType::STORE){
        do_store(mem, mem_bytes, addr, data);     
    }
    
    return reg;
}