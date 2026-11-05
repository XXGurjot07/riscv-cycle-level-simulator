#include "decode_stage.hpp"

// Refer  : The RISC-V Instruction Set Manual, Volume I
// Page   : 584
// RV64GC : IMAFDZicsr_Zifencei + C (eventual goal)

// Immediate extraction functions
static inline int64_t imm_i(uint32_t instruction){
    int32_t imm = (int32_t)instruction >> 20; 
    return (int64_t)imm;
}

static inline int64_t imm_s(uint32_t instruction){
    int32_t imm_u = (int32_t)instruction >> 20 & 0xffffffe0;
    int32_t imm_l = instruction >> 7 & 0x1f;
    return (int64_t)(imm_u | imm_l);
}

static inline int64_t imm_b(uint32_t instruction){
    int32_t imm_12    = ((int32_t)instruction >> 19) & 0xfffff000; 
    int32_t imm_10_5  = instruction >> 20 & 0x7e0;  
    int32_t imm_4_1   = instruction >> 7 & 0x1e;             
    int32_t imm_11    = instruction << 4 & 0x800;            
    return (int64_t)(imm_12 | imm_11 | imm_10_5 | imm_4_1);
}

static inline int64_t imm_u(uint32_t instruction){
    int32_t imm = (int32_t)(instruction & 0xfffff000); 
    return (int64_t)imm;
}

static inline int64_t imm_j(uint32_t instruction) {
    int32_t imm_20    = ((int32_t)instruction >> 11) & 0xfff00000;
    int32_t imm_10_1  = instruction >> 20 & 0x7fe;  
    int32_t imm_19_12 = instruction & 0x000ff000;             
    int32_t imm_11    = (instruction >> 9) & 0x00000800;
    return (int64_t)(imm_20 | imm_19_12 | imm_11 | imm_10_1);
}

Decoded_Inst do_decode(uint32_t instruction){
    Decoded_Inst deco_inst;
    
    deco_inst.is_compressed = (instruction & 3) != 0x3;  
    
    if(deco_inst.is_compressed){
        // placeholder for converting compressed instruction to 32-bit instruction
        deco_inst.op = Op::ILLEGAL; 
        return deco_inst;
    }
    
    deco_inst.rd  = (instruction >> 7)  & 31;
    deco_inst.rs1 = (instruction >> 15) & 31;
    deco_inst.rs2 = (instruction >> 20) & 31;

    uint8_t opcode = (instruction >> 2)  & 31;
    uint8_t funct3 = (instruction >> 12) & 7;
    uint8_t funct6 = (instruction >> 26) & 63;
    uint8_t funct7 = (instruction >> 25) & 127;

    switch(opcode){

        // --------------------------------------------//
        // RV64I Basic Decoding //
        // --------------------------------------------//

        default:{
            deco_inst.op = Op::ILLEGAL;   
            break;
        }
        case 0x00: // Load
        {
            static const Op t[8] = {Op::LB, Op::LH, Op::LW, Op::LD, Op::LBU, Op::LHU, Op::LWU, Op::ILLEGAL};
            deco_inst.op         = t[funct3]; 
            deco_inst.format     = Format::I; 
            deco_inst.fu_type    = FUType::LOAD;
            deco_inst.mem_bytes  = 1 << (funct3 & 3); 
            deco_inst.mem_signed = !(funct3 & 4);

            deco_inst.reads_rs1 = true;
            deco_inst.reads_rs2 = false;
            deco_inst.writes_rd = deco_inst.rd != 0;
            
            deco_inst.imm = imm_i(instruction);
            break;
        }
        case 0x08: // Store
        {
            static const Op t[8] = {Op::SB, Op::SH, Op::SW, Op::SD, Op::ILLEGAL, Op::ILLEGAL, Op::ILLEGAL, Op::ILLEGAL};
            deco_inst.op         = t[funct3]; 
            deco_inst.format     = Format::S; 
            deco_inst.fu_type    = FUType::STORE;
            deco_inst.mem_bytes  = 1 << (funct3 & 3); 

            deco_inst.reads_rs1 = true;
            deco_inst.reads_rs2 = true;
            deco_inst.writes_rd = false;
            
            deco_inst.imm = imm_s(instruction);
            break;
        }
        case 0x18: // Branch
        {
            static const Op t[8] = {Op::BEQ, Op::BNE, Op::ILLEGAL, Op::ILLEGAL, Op::BLT, Op::BGE, Op::BLTU, Op::BGEU};
            deco_inst.op         = t[funct3]; 
            deco_inst.format     = Format::B; 
            deco_inst.fu_type    = FUType::BRANCH;

            deco_inst.reads_rs1 = true;
            deco_inst.reads_rs2 = true;
            deco_inst.writes_rd = false;
            
            deco_inst.imm = imm_b(instruction);
            break;
        }
        case 0x1b: // JAL
        {
            deco_inst.op         = Op::JAL; 
            deco_inst.format     = Format::J; 
            deco_inst.fu_type    = FUType::JUMP;

            deco_inst.reads_rs1 = false;
            deco_inst.reads_rs2 = false;
            deco_inst.writes_rd = deco_inst.rd != 0;
            
            deco_inst.is_call   = (deco_inst.rd == 1 || deco_inst.rd == 5);
            deco_inst.imm       = imm_j(instruction);
            break;
        }
        case 0x19: // JALR
        {
            deco_inst.op         = Op::JALR; 
            deco_inst.format     = Format::I; 
            deco_inst.fu_type    = FUType::JUMP;

            deco_inst.reads_rs1 = true;
            deco_inst.reads_rs2 = false;
            deco_inst.writes_rd = deco_inst.rd != 0;
            
            deco_inst.is_call  = (deco_inst.rd == 1 || deco_inst.rd == 5);
            deco_inst.is_ret   = (deco_inst.rs1 == 1 || deco_inst.rs1 == 5) && !deco_inst.is_call;
            deco_inst.imm      = imm_i(instruction);
            break;
        }
        case 0x05: // AUIPC
        {
            deco_inst.op         = Op::AUIPC; 
            deco_inst.format     = Format::U; 
            deco_inst.fu_type    = FUType::ALU;

            deco_inst.reads_rs1 = false;
            deco_inst.reads_rs2 = false;
            deco_inst.writes_rd = deco_inst.rd != 0;
            
            deco_inst.imm      = imm_u(instruction);
            break;
        }
        case 0x0d: // LUI
        {
            deco_inst.op         = Op::LUI; 
            deco_inst.format     = Format::U; 
            deco_inst.fu_type    = FUType::ALU;

            deco_inst.reads_rs1 = false;
            deco_inst.reads_rs2 = false;
            deco_inst.writes_rd = deco_inst.rd != 0;
            
            deco_inst.imm      = imm_u(instruction);
            break;
        }
        case 0x0c: // R-type
        {
            static const Op t[8] = {Op::ADD, Op::SLL, Op::SLT, Op::SLTU, Op::XOR, Op::SRL, Op::OR, Op::AND};
            deco_inst.op         = t[funct3];
            
            if(deco_inst.op == Op::ADD){
                if      (funct7 == 32) { deco_inst.op = Op::SUB; }    
                else if (funct7 != 0)  { deco_inst.op = Op::ILLEGAL; }      
            }

            else if(deco_inst.op == Op::SRL){
                if      (funct7 == 32) { deco_inst.op = Op::SRA; }    
                else if (funct7 != 0)  { deco_inst.op = Op::ILLEGAL; }     
            }

            else if(funct7 != 0){ deco_inst.op = Op::ILLEGAL; }

            // Adding M extension here as a override for R-type, as they have same opcode
            if(funct7 == 1){
                static const Op t_m[8] = {Op::MUL, Op::MULH, Op::MULHSU, Op::MULHU, Op::DIV, Op::DIVU, Op::REM, Op::REMU};
                deco_inst.op = t_m[funct3];

                if(funct3 < 4) deco_inst.fu_type = FUType::MUL;
                else           deco_inst.fu_type = FUType::DIV;

            }
            else{
                deco_inst.fu_type = FUType::ALU;
            }

            deco_inst.format = Format::R;

            deco_inst.reads_rs1 = true;
            deco_inst.reads_rs2 = true;
            deco_inst.writes_rd = deco_inst.rd != 0;

            break;
        }
        case 0x0e: // R-type Word
        {
            static const Op t[8] = {Op::ADDW, Op::SLLW, Op::ILLEGAL, Op::ILLEGAL, Op::ILLEGAL, Op::SRLW, Op::ILLEGAL, Op::ILLEGAL};
            deco_inst.op         = t[funct3];
            
            if(deco_inst.op == Op::ADDW){
                if      (funct7 == 32) { deco_inst.op = Op::SUBW; }    
                else if (funct7 != 0)  { deco_inst.op = Op::ILLEGAL; }      
            }

            else if(deco_inst.op == Op::SRLW){
                if      (funct7 == 32) { deco_inst.op = Op::SRAW; }    
                else if (funct7 != 0)  { deco_inst.op = Op::ILLEGAL; }     
            }

            else if(funct7 != 0){ deco_inst.op = Op::ILLEGAL; }

            // Adding M word extension here as a override for R-type word, as they have same opcode
            if(funct7 == 1){
                static const Op t_m[8] = {Op::MULW, Op::ILLEGAL, Op::ILLEGAL, Op::ILLEGAL, Op::DIVW, Op::DIVUW, Op::REMW, Op::REMUW};
                deco_inst.op = t_m[funct3];

                if(funct3 == 0) deco_inst.fu_type = FUType::MUL;
                else if(funct3 >= 4 && funct3 <= 7) deco_inst.fu_type = FUType::DIV;
                else deco_inst.fu_type = FUType::ALU;

            }
            else{
                deco_inst.fu_type = FUType::ALU;
            }

            deco_inst.format     = Format::R; 

            deco_inst.reads_rs1 = true;
            deco_inst.reads_rs2 = true;
            deco_inst.writes_rd = deco_inst.rd != 0;

            break; 
        }
        case 0x04: // I-type
        {
            // removed I suffix, as in execute stage after op_b mux select, r and  i are similar operations
            static const Op t[8] = {Op::ADD, Op::SLL, Op::SLT, Op::SLTU, Op::XOR, Op::SRL, Op::OR, Op::AND};
            deco_inst.op         = t[funct3]; 

            if(deco_inst.op == Op::SLL){
                deco_inst.op = (funct6 == 0) ? deco_inst.op : Op::ILLEGAL;    
            }
            
            if(deco_inst.op == Op::SRL){
                if      (funct6 == 16) { deco_inst.op = Op::SRA; }    
                else if (funct6 != 0)  { deco_inst.op = Op::ILLEGAL; }     
            }

            deco_inst.format     = Format::I; 
            deco_inst.fu_type    = FUType::ALU;

            deco_inst.reads_rs1 = true;
            deco_inst.reads_rs2 = false;
            deco_inst.writes_rd = deco_inst.rd != 0;
            
            deco_inst.imm = imm_i(instruction);
            break;
        }
        case 0x06: // I-type Word
        {
            static const Op t[8] = {Op::ADDIW, Op::SLLIW, Op::ILLEGAL, Op::ILLEGAL, Op::ILLEGAL, Op::SRLIW, Op::ILLEGAL, Op::ILLEGAL};
            deco_inst.op         = t[funct3]; 
            
            if(deco_inst.op == Op::SLLIW){
                deco_inst.op = (funct7 == 0) ? deco_inst.op : Op::ILLEGAL;    
            }
            
            if(deco_inst.op == Op::SRLIW){
                if      (funct7 == 32) { deco_inst.op = Op::SRAIW; }    
                else if (funct7 != 0)  { deco_inst.op = Op::ILLEGAL; }     
            }

            deco_inst.format     = Format::I; 
            deco_inst.fu_type    = FUType::ALU;

            deco_inst.reads_rs1 = true;
            deco_inst.reads_rs2 = false;
            deco_inst.writes_rd = deco_inst.rd != 0;
            
            deco_inst.imm      = imm_i(instruction);
            break;
        }

        // --------------------------------------------//
        // Now, any special decodings  //
        // --------------------------------------------//

        case 0x1c: // ECALL, EBREAK & CSR
        {
            if(funct3 == 0 && deco_inst.rd == 0){
                deco_inst.format  = Format::I;
                deco_inst.fu_type = FUType::SYSTEM;

                deco_inst.reads_rs1 = false;
                deco_inst.reads_rs2 = false;
                deco_inst.writes_rd = false;

                deco_inst.imm = imm_i(instruction);

                if     (deco_inst.imm == 0) {deco_inst.op = Op::ECALL;  }    
                else if(deco_inst.imm == 1) {deco_inst.op = Op::EBREAK; }
                
                else                        {deco_inst.op = Op::ILLEGAL;}   
            }

            // CSR will go here later

            break;
        }
        case 0x03: // FENCE, FENCE.TSO, PAUSE, FENCE.I
        {
            // just NOPS here
            if(funct3 == 0){
                deco_inst.op = Op::FENCE;    
            }
            else if (funct3 == 1){
                deco_inst.op = Op::FENCE_I;  
            }
            
            deco_inst.format  = Format::I;
            deco_inst.fu_type = FUType::SYSTEM;
               
            break;
        }
        case 0x0b: // Atomic
        {
            deco_inst.op = Op::ILLEGAL;   
            break;
        }
        
    }

    return deco_inst;
}

// Decode Evaluate
reg_ID_EX DecodeStage::evaluate(const arch_state& regfile, const reg_IF_ID& if_id_reg){
    reg_ID_EX reg;

    reg.pc  = if_id_reg.pc;
    reg.why = if_id_reg.why;
    reg.instruction = if_id_reg.instruction;

    if(!if_id_reg.valid){
        reg.valid = false;
        return reg;    
    }

    if (if_id_reg.exc != Exception::NONE){
        reg.exc     = if_id_reg.exc;
        reg.exc_val = if_id_reg.exc_val;
        reg.valid   = true;
        return reg;         
    }

    reg.deco_inst = do_decode(if_id_reg.instruction);
    reg.rs1_data  = regfile.read(reg.deco_inst.rs1);
    reg.rs2_data  = regfile.read(reg.deco_inst.rs2);

    reg.exc     = (reg.deco_inst.op == Op::ILLEGAL) ? Exception::ILLEGAL_INST : Exception::NONE;
    reg.exc_val = (reg.deco_inst.op == Op::ILLEGAL) ? if_id_reg.instruction : 0;

    reg.valid   = true;
    
    return reg;
}