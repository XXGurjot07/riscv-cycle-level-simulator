#pragma once
#include "config.hpp"
#include "definitions_isa.hpp"
#include "definitions_pipeline.hpp"

class MemStage{
    private :
        uint64_t mem_size_;

    public : 
        MemStage(uint64_t mem_size);
        reg_MEM_WB evaluate(uint8_t* mem, const reg_EX_MEM& ex_mem_reg);            
};