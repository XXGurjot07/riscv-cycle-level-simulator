#pragma once
#include "config.hpp"
#include "definitions_archstate.hpp"
#include "definitions_isa.hpp"
#include "definitions_pipeline.hpp"

// Version 0.4 - Added Support for M Extension Integer Multiply/Divide

// Function Declarations
Decoded_Inst do_decode(uint32_t instruction);

class DecodeStage{
    public :
        reg_ID_EX evaluate(const arch_state& reg_file, const reg_IF_ID& if_id_reg);            
};