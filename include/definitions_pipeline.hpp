#pragma once
#include "definitions_isa.hpp"

// Recommended Pattern: Input by const&, Output by Value
// Pass the incoming register struct by const& (read-only, zero copy)
// Return the outgoing register struct by value (optimized directly into CPU registers by the compiler).

// Every registor/pipeline stage should be of form {valid, pc, exc, tval + whatever data that stage passes on}
enum Bubble_Why : int { 
    FILL,               // 4 - CPI Penalty  
    RAW,                // 2 or 1 
    LOAD_USE,           // 1
    BRANCH_MISPREDICT,  // 2
    FENCE_I,            // 2
    EX_BUSY,            // X (variable)
    RAW_MULDIV,         // variable
    NWHY                // 0
};

// Pipeline Registors after each stage :  IF | ID | EX | MEM | WB
struct reg_IF_ID{
    uint64_t   pc          = 0;
    uint32_t   instruction = 0;
    Exception  exc         = Exception::NONE;
    uint64_t   exc_val     = 0;
    Bubble_Why why         = FILL;
    bool       valid       = false;
};

struct reg_ID_EX{
    uint64_t     pc = 0;
    Decoded_Inst deco_inst;
    uint64_t     rs1_data = 0;
    uint64_t     rs2_data = 0;

    Exception exc = Exception::NONE;
    uint64_t  exc_val = 0;

    Bubble_Why why = FILL;
    bool valid = false;

    uint32_t instruction = 0; // for Spike logging
};

struct reg_EX_MEM{
    uint64_t pc = 0;

    FUType   fu_type = FUType::NONE;

    uint8_t  mem_bytes  = 0;  
    bool     mem_signed = true; // For Load instructions, true if signed load
    uint64_t mem_addr   = 0; 
    uint64_t mem_data   = 0; 

    uint8_t  rd = 0;
    uint64_t rd_data = 0;
    bool     writes_rd = false;

    bool     redirect = false;
    uint64_t redirect_target = 0;

    Exception exc = Exception::NONE;
    uint64_t  exc_val = 0;

    Bubble_Why why = FILL;
    bool valid = false;

    uint32_t instruction = 0; // for Spike logging
};

struct reg_EX_Ports{
    reg_EX_MEM reg;             // facing mem logic
    bool accepted = false;      // facing decode logic
};

struct reg_MEM_WB{
    uint64_t pc = 0;

    uint8_t  rd        = 0;
    uint64_t rd_data   = 0;
    bool     writes_rd = false;

    Exception exc     = Exception::NONE;
    uint64_t  exc_val = 0;

    Bubble_Why why = FILL;
    bool valid = false;

    uint32_t instruction = 0;     // for Spike logging
    bool     is_load     = false; // for Spike logging
    bool     is_store    = false; // for Spike logging
    uint64_t mem_addr    = 0;     // for Spike logging
    uint64_t mem_data    = 0;     // for Spike logging
    uint8_t  mem_bytes   = 0;     // for Spike logging
};

struct retired_Res{
    uint64_t pc = 0;

    Exception exc     = Exception::NONE;
    uint64_t  exc_val = 0;

    uint8_t  rd        = 0;
    uint64_t rd_data   = 0;
    bool     writes_rd = false;

    bool retired = false;
    bool trap    = false;

    Bubble_Why why = FILL;
    bool valid = false;

    uint32_t instruction = 0;     // for Spike logging
    bool     is_load     = false; // for Spike logging
    bool     is_store    = false; // for Spike logging
    uint64_t mem_addr    = 0;     // for Spike logging
    uint64_t mem_data    = 0;     // for Spike logging
    uint8_t  mem_bytes   = 0;     // for Spike logging
};