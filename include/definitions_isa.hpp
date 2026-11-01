#pragma once
#include<cstdint>

// An instruction is completely defined by major opcode (6:0) and funct3 (14:12) and funct7/6 (31:25/26).

enum class FUType : uint8_t{NONE, ALU, MUL, DIV, BRANCH, JUMP, LOAD, STORE, SYSTEM};

enum class Format : uint8_t{None, R, I, S, B, U, J};

enum class Op : uint8_t {
    // Loads
    LD, LW, LWU, LH, LHU, LB, LBU,

    // Stores
    SD, SW, SH, SB,

    // I-Type (dont need to be drive, R type and mux select is I type)
    ADDI, ADDIW, SLTI, SLTIU, ANDI, ORI, XORI, SLLI, SLLIW, SRLI, SRLIW, SRAI, SRAIW,

    // U-Type
    LUI, AUIPC,

    // R-Type
    ADD, ADDW, SLT, SLTU, AND, OR, XOR, SLL, SLLW, SRL, SRLW, SUB, SUBW, SRA, SRAW,

    // Multiply / Divide
    MUL, MULW, MULH, MULHU, MULHSU, DIV, DIVW, REM, REMW,

    // Control Flow
    JAL, JALR, BEQ, BNE, BLT, BLTU, BGE, BGEU,

    // NOP & Misc Markers
    NOP, COMPRESSED,

    // CSRs
    CSRRW, CSRRS, CSRRC, CSRRWI, CSRRSI, CSRRCI,

    // Atomics (AMO, LR-SC)
    AMO_LR_D, AMO_LR_W, AMO_SC_D, AMO_SC_W, AMO_SWAP_D, AMO_SWAP_W, AMO_ADD_D, AMO_ADD_W, 
    AMO_AND_D, AMO_AND_W, AMO_OR_D, AMO_OR_W, AMO_XOR_D, AMO_XOR_W, AMO_MIN_D, AMO_MIN_W, 
    AMO_MIN_WU, AMO_MAX_D, AMO_MAX_W, AMO_MAX_WU,

    // System & Miscellaneous
    FENCE, FENCE_I, ECALL, EBREAK, ILLEGAL
};

enum class Exception : uint8_t {
    NONE               = 255,
    INST_MISALIGNED    = 0, 
    INST_ACCESS_FAULT  = 1, 
    ILLEGAL_INST       = 2, 
    BREAKPOINT         = 3,
    LOAD_MISALIGNED    = 4, 
    LOAD_ACCESS_FAULT  = 5, 
    STORE_MISALIGNED   = 6, 
    STORE_ACCESS_FAULT = 7,
    ECALL_U            = 8, 
    ECALL_S            = 9, 
    ECALL_M            = 11
};

struct Decoded_Inst{
    FUType fu_type = FUType::NONE;
    Format format  = Format::None;
    Op     op      = Op::ILLEGAL;

    uint8_t  rd  = 0;
    uint8_t  rs1 = 0;
    uint8_t  rs2 = 0;

    // For renaming
    bool reads_rs1 = false;
    bool reads_rs2 = false;
    bool writes_rd = false;

    // For Load/Store instructions, mem_bytes = 0/1/2/4/8 for RV64
    uint8_t  mem_bytes  = 0;  
    bool     mem_signed = true; // For Load instructions, true if signed load

    int64_t imm = 0;

    bool is_call       = false;
    bool is_ret        = false;
    bool is_compressed = false;
};

struct FU_Input{
    uint64_t pc = 0;
    Decoded_Inst deco_inst;

    uint64_t a_data = 0;        // rs1 data or pc  or forwarded
    uint64_t b_data = 0;        // rs2 data or imm or forwareded
    uint64_t predicted_pc = 0;  // to compare with output pc if branch
};

struct FU_Output{
    uint64_t rd_data = 0;           // rd_value
    uint64_t addr = 0;              // mem_addr
 
    uint64_t store_data = 0;        // mem_data
 
    bool     redirect = false;
    uint64_t redirect_target = 0;

    Exception exc = Exception::NONE;
    uint64_t  exc_val = 0;
};