#include "execute_stage.hpp"

// do exe functions are pure combinational functions
// To model the pipelining of ALU, model it outside the function (Timing Wrapper) called as FunctionalUnit
// To model the whole execute stage, we will use the (Dispatch Wrapper)

// Timeline :
// 1. Check if all units are idle, if not idle, decode inserts a bubble through the backward flag !accepted
// 2. If accepted, check which FULane it requires, and "build" the output packet. (We build at issue only, but it becomes valid to EX_MEM latch only after latency cycles)
// 3. Tick each FULane's clock
// 4. Check if anything has been completed, if yes, pop and return a valid packet, if not return a bubble
// X. Can flush all inflight, if core detects a misspeculation

static inline uint64_t sext32(uint64_t x) { return (uint64_t)(int64_t)(int32_t)(uint32_t)x; }

FU_Output do_alu(const FU_Input& fu_in){
    FU_Output result;

    Op op = fu_in.deco_inst.op;
    
    int64_t  a_signed   = int64_t(fu_in.a_data);
    uint64_t a_unsigned = fu_in.a_data;

    int64_t  b_signed   = int64_t(fu_in.b_data);
    uint64_t b_unsigned = fu_in.b_data;
    
    uint64_t rd_data = 0;

    uint8_t shftamt   = fu_in.b_data & 0x3f;
    uint8_t shftamt_w = fu_in.b_data & 0x1f;
    
    switch (op){
        default       :{ rd_data = 0; break;}

        case Op::AUIPC:{ rd_data = a_unsigned + b_unsigned; break;}
        case Op::LUI  :{ rd_data = b_signed;                break;}

        case Op::ADD  :{ rd_data = a_unsigned + b_unsigned; break;}
        case Op::SUB  :{ rd_data = a_unsigned - b_unsigned; break;}
        case Op::SLL  :{ rd_data = a_unsigned << shftamt;   break;}

        case Op::SLT  :{ rd_data = (a_signed   < b_signed);   break;}
        case Op::SLTU :{ rd_data = (a_unsigned < b_unsigned); break;}
        
        case Op::XOR  :{ rd_data = a_unsigned ^ b_unsigned; break;}
        case Op::SRL  :{ rd_data = a_unsigned >> shftamt;   break;}
        case Op::SRA  :{ rd_data = a_signed   >> shftamt;   break;}
        case Op::OR   :{ rd_data = a_unsigned | b_unsigned; break;}
        case Op::AND  :{ rd_data = a_unsigned & b_unsigned; break;}

        case Op::ADDW:
        case Op::ADDIW:{ rd_data = sext32(a_unsigned + b_unsigned); break;}
        case Op::SUBW :{ rd_data = sext32(a_unsigned - b_unsigned); break;}

        case Op::SLLW :
        case Op::SLLIW:{ rd_data = sext32((uint32_t)a_unsigned << shftamt_w); break;}

        case Op::SRLW :
        case Op::SRLIW:{ rd_data = sext32((uint32_t)a_unsigned >> shftamt_w); break;}

        case Op::SRAW :
        case Op::SRAIW:{ rd_data = sext32((uint32_t)((int32_t)a_unsigned >> shftamt_w)); break;}

        // Special Cases
        case Op::ECALL:{
            result.exc     = Exception::ECALL_M;
            result.exc_val = 0; 
            break;   
        }
        case Op::EBREAK:{
            result.exc     = Exception::BREAKPOINT;
            result.exc_val = fu_in.pc; 
            break;     
        }

        // NOP for FENCE, Redirect for FENCE_I
        case Op::FENCE: {
            break; 
        }
        case Op::FENCE_I: {
            result.redirect = true;
            result.redirect_target = fu_in.pc + 4;
            break;
        }

    }

    result.rd_data = rd_data;
    return result;
}

FU_Output do_bru(const FU_Input& fu_in){
    FU_Output result;

    Op op = fu_in.deco_inst.op;
    
    int64_t  a_signed   = int64_t(fu_in.a_data);
    uint64_t a_unsigned = fu_in.a_data;

    int64_t  b_signed   = int64_t(fu_in.b_data);
    uint64_t b_unsigned = fu_in.b_data;

    uint64_t actual_target = 0;
    uint64_t redirect_target = 0;

    bool taken      = false;
    bool mispredict = false;
    bool misaligned = false;

    redirect_target = fu_in.pc + fu_in.deco_inst.imm;

    switch (op){
        default      :{ taken = false; break;}
        case Op::BEQ :{ taken = (a_unsigned == b_unsigned); break;}
        case Op::BNE :{ taken = (a_unsigned != b_unsigned); break;}
        case Op::BLT :{ taken = (a_signed   <  b_signed);   break;}
        case Op::BLTU:{ taken = (a_unsigned <  b_unsigned); break;}
        case Op::BGE :{ taken = (a_signed   >= b_signed);   break;}
        case Op::BGEU:{ taken = (a_unsigned >= b_unsigned); break;}

        case Op::JAL :{ taken = true; break;}
        case Op::JALR:{ taken = true; break;}

    }

    if(op == Op::JALR){ actual_target = (a_unsigned + fu_in.deco_inst.imm) & ~1;}
    else              { actual_target = taken ? redirect_target : fu_in.pc + 4;}

    mispredict = (actual_target != fu_in.predicted_pc);
    misaligned = (actual_target & 3) != 0; // computed_target[1:0] must be 00

    result.exc     = misaligned ? Exception::INST_MISALIGNED : Exception::NONE;
    result.exc_val = result.exc != Exception::NONE ? actual_target : result.exc_val;

    result.rd_data = fu_in.pc + 4;
    result.redirect_target = actual_target;
    result.redirect = !misaligned && mispredict;
    
    return result;
}

FU_Output do_agu(const FU_Input& fu_in){
    FU_Output result;

    uint64_t a_unsigned = fu_in.a_data;
    uint64_t b_unsigned = fu_in.b_data;
    uint64_t imm        = fu_in.deco_inst.imm;
    uint8_t  mem_byte   = fu_in.deco_inst.mem_bytes;
    FUType   fu_type    = fu_in.deco_inst.fu_type;
    uint64_t addr = 0;

    bool misaligned = false;

    addr = a_unsigned + imm;
    misaligned = (addr & (mem_byte - 1)) != 0;

    if(misaligned){
        if      (fu_type == FUType::LOAD)  { result.exc = Exception::LOAD_MISALIGNED; }
        else if (fu_type == FUType::STORE) { result.exc = Exception::STORE_MISALIGNED;}      
    }

    result.addr       = addr;
    result.store_data = b_unsigned;
    result.exc_val    = result.exc != Exception::NONE ? addr : result.exc_val;

    return result;
} 


bool ExecuteStage :: all_idle() const {
    bool alu = alu_.unit.idle();
    bool agu = agu_.unit.idle();
    bool bru = bru_.unit.idle();

    return (alu && agu && bru);
}

FU_Lane& ExecuteStage :: unit_for(const reg_ID_EX& id_ex_reg){
    const Decoded_Inst& deco = id_ex_reg.deco_inst;

    if(id_ex_reg.exc != Exception::NONE){
        return alu_;   
    }

    switch(deco.fu_type){
        case FUType::NONE  :
        case FUType::SYSTEM: 
        case FUType::ALU   : return alu_;
        case FUType::LOAD  : 
        case FUType::STORE : return agu_;
        case FUType::BRANCH:
        case FUType::JUMP  : return bru_;
        default            : return alu_;
    }
}

reg_EX_MEM ExecuteStage :: build(const reg_ID_EX& id_ex_reg, ComputeFn compute) const {
    reg_EX_MEM reg;
    reg.pc = id_ex_reg.pc;
    reg.instruction = id_ex_reg.instruction;

    if(id_ex_reg.exc != Exception::NONE){
        reg.exc     = id_ex_reg.exc;
        reg.exc_val = id_ex_reg.exc_val;
        reg.valid   = id_ex_reg.valid;
        return reg;
    }
    
    // build FU_Input for EX Units
    const Decoded_Inst& deco = id_ex_reg.deco_inst;
    FU_Input fu;

    // always not-taken prediction
    fu.pc           = id_ex_reg.pc;
    fu.predicted_pc = id_ex_reg.pc + 4;
    fu.deco_inst    = id_ex_reg.deco_inst;

    switch(deco.format){
        case Format::B:
        case Format::R:
        case Format::S:
        {
            fu.a_data = id_ex_reg.rs1_data; 
            fu.b_data = id_ex_reg.rs2_data; 
            break;
        }
        case Format::I:
        case Format::J:
        {
            fu.a_data = id_ex_reg.rs1_data; 
            fu.b_data = deco.imm; 
            break;
        } 
        case Format::U:
        {
            if(deco.op == Op :: LUI) fu.a_data = 0;
            else                     fu.a_data = id_ex_reg.pc; 
            fu.b_data = deco.imm; 
            break;
        } 
        default:
        {
            fu.a_data = id_ex_reg.rs1_data; 
            fu.b_data = id_ex_reg.rs2_data; 
            break;
        }
    }

    // FU_Input is is ready to be sent to EX Units
    FU_Output fu_output = compute(fu);

    // repack to reg_EX_MEM
    reg.fu_type = deco.fu_type;

    reg.mem_bytes  = deco.mem_bytes;  
    reg.mem_signed = deco.mem_signed; 

    reg.mem_addr   = fu_output.addr; 
    reg.mem_data   = fu_output.store_data; 

    reg.rd        = deco.rd;
    reg.rd_data   = fu_output.rd_data;
    reg.writes_rd = deco.writes_rd;

    reg.redirect        = fu_output.redirect;
    reg.redirect_target = fu_output.redirect_target;

    reg.exc     = fu_output.exc;
    reg.exc_val = fu_output.exc_val;
    reg.valid   = true;

    return reg;
}

void ExecuteStage :: flush(){
    alu_.unit.flush();
    agu_.unit.flush();
    bru_.unit.flush();
}

reg_EX_Ports ExecuteStage :: evaluate(const reg_ID_EX& id_ex_reg){
    reg_EX_Ports reg;
    reg_EX_MEM   completed_reg;
    reg.accepted = false;

    // default output if nothing pops: blame EX if it held a valid inst, else pass the upstream tag
    completed_reg.why = id_ex_reg.valid ? Bubble_Why::EX_BUSY : id_ex_reg.why;

    // bubble in, yes accepted and bubble out
    if(!id_ex_reg.valid){
        reg.accepted = true;
    }

    // a valid in, can be accepted or not?
    else if(all_idle()){
        reg.accepted = true;

        FU_Lane&   lane  = unit_for(id_ex_reg);
        reg_EX_MEM built = build(id_ex_reg, lane.compute);
        
        lane.unit.issue(built);
    }

    // tick now, so this cycles result can turn complete
    alu_.unit.tick();
    agu_.unit.tick();
    bru_.unit.tick();

    // check for any completed instructions
    bool alu_r = alu_.unit.has_result();
    bool agu_r = agu_.unit.has_result();
    bool bru_r = bru_.unit.has_result();

    // only 1 can complete in this inorder ver
    if     (alu_r) completed_reg = alu_.unit.pop();
    else if(agu_r) completed_reg = agu_.unit.pop();
    else if(bru_r) completed_reg = bru_.unit.pop();

    reg.reg = completed_reg;

    return reg;
}

