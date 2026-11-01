#include "fetch_stage.hpp"

FetchStage :: FetchStage (uint64_t mem_size) : mem_size_(mem_size) {}

reg_IF_ID FetchStage::evaluate(const uint8_t* mem, uint64_t pc){
    reg_IF_ID reg;
    bool mem_in_bounds;
    
    reg.pc = pc;

    pc = pc - MEM_BASE;

    mem_in_bounds = (pc <= mem_size_-4);
    
    if(!mem_in_bounds){
        reg.instruction = 0x00000013; // NOP
        reg.exc = Exception::INST_ACCESS_FAULT;
        reg.exc_val = reg.pc;
    }
    else{
        reg.instruction = (uint32_t)mem[pc] | ((uint32_t)mem[pc+1] << 8) 
                    | ((uint32_t)mem[pc+2] << 16) | ((uint32_t)mem[pc+3] << 24);
    }

    reg.valid = true;
    return reg;
}
