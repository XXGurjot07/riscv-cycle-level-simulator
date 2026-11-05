#include "hazard_unit.hpp"

// hazards structural

// current version, handles by !accepted from ex to dec

// hazards data

// "does this older instruction write rs?"
static bool writes(uint8_t rs, bool valid, bool writes_rd, uint8_t rd){
    if(!valid)     return false;
    if(!writes_rd) return false;
    if(rd == 0)    return false;
    if(rd == rs)   return true;
    
    return false;    
}   

bool hazard_raw_d1 (const reg_ID_EX& new_id_ex, const reg_ID_EX& id_ex){

    bool needs = false;
    if(new_id_ex.deco_inst.reads_rs1){
        needs |= writes(new_id_ex.deco_inst.rs1, id_ex.valid, id_ex.deco_inst.writes_rd, id_ex.deco_inst.rd);
    }
    if(new_id_ex.deco_inst.reads_rs2){
        needs |= writes(new_id_ex.deco_inst.rs2, id_ex.valid, id_ex.deco_inst.writes_rd, id_ex.deco_inst.rd);
    }
    return needs;
}

bool hazard_raw_d2 (const reg_ID_EX& new_id_ex, const reg_EX_MEM& ex_mem){

    bool needs = false;
    if(new_id_ex.deco_inst.reads_rs1){
        needs |= writes(new_id_ex.deco_inst.rs1, ex_mem.valid, ex_mem.writes_rd, ex_mem.rd);
    }
    if(new_id_ex.deco_inst.reads_rs2){
        needs |= writes(new_id_ex.deco_inst.rs2, ex_mem.valid, ex_mem.writes_rd, ex_mem.rd);
    }
    return needs;
}

bool hazard_load_use(const reg_ID_EX& new_id_ex, const reg_ID_EX& id_ex){
    
    bool valid      = id_ex.valid;
    bool load_valid = id_ex.deco_inst.fu_type == FUType::LOAD;
    bool valid_a    = valid && load_valid;

    bool needs = false;
    if(new_id_ex.deco_inst.reads_rs1){
        needs |= writes(new_id_ex.deco_inst.rs1, valid_a, id_ex.deco_inst.writes_rd, id_ex.deco_inst.rd);
    }
    if(new_id_ex.deco_inst.reads_rs2){
        needs |= writes(new_id_ex.deco_inst.rs2, valid_a, id_ex.deco_inst.writes_rd, id_ex.deco_inst.rd);
    }
    return needs;    
}

// forward operands

void forward_operands(reg_ID_EX& id_ex_, const reg_EX_MEM& ex_mem_, const reg_MEM_WB& mem_wb_){

    // forward path for rs1
    if(ex_mem_.valid && ex_mem_.writes_rd && (ex_mem_.rd != 0) && id_ex_.deco_inst.reads_rs1 && (id_ex_.deco_inst.rs1 == ex_mem_.rd)){
        id_ex_.rs1_data = ex_mem_.rd_data;
    }
    else if(mem_wb_.valid && mem_wb_.writes_rd && (mem_wb_.rd != 0) && id_ex_.deco_inst.reads_rs1 && (id_ex_.deco_inst.rs1 == mem_wb_.rd)){
        id_ex_.rs1_data = mem_wb_.rd_data;
    }

    // forward path for rs2
    if(ex_mem_.valid && ex_mem_.writes_rd && (ex_mem_.rd != 0) && id_ex_.deco_inst.reads_rs2 && (id_ex_.deco_inst.rs2 == ex_mem_.rd)){
        id_ex_.rs2_data = ex_mem_.rd_data;
    }
    else if(mem_wb_.valid && mem_wb_.writes_rd && (mem_wb_.rd != 0) && id_ex_.deco_inst.reads_rs2 && (id_ex_.deco_inst.rs2 == mem_wb_.rd)){
        id_ex_.rs2_data = mem_wb_.rd_data;
    }
}

// hazards control flow
