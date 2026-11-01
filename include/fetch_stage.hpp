#pragma once
#include "config.hpp"
#include "definitions_isa.hpp"
#include "definitions_archstate.hpp"
#include "definitions_pipeline.hpp"

class FetchStage{
    private :
        uint64_t mem_size_;

    public :
        FetchStage (uint64_t mem_size);
        reg_IF_ID evaluate(const uint8_t* mem, uint64_t pc);            
};