#include "writeback_stage.hpp"

retired_Res WritebackStage :: evaluate(arch_state& reg_file, const reg_MEM_WB& mem_wb_reg){

    retired_Res reg;

    // This is for reporting to core
    reg.pc        = mem_wb_reg.pc;

    reg.writes_rd = mem_wb_reg.writes_rd;
    reg.rd        = mem_wb_reg.rd;
    reg.rd_data   = mem_wb_reg.rd_data;

    reg.exc       = mem_wb_reg.exc;
    reg.exc_val   = mem_wb_reg.exc_val;
    reg.valid     = mem_wb_reg.valid;
    reg.why       = mem_wb_reg.why;
    
    if (!mem_wb_reg.valid){
        return reg;
    }

    if (mem_wb_reg.exc != Exception::NONE){
        reg.trap = true;
        return reg;
    }
    
    // The write has happened on the negative edge (write-first writeback stage)
    // The decode can safely read in the same cycle
    if(reg.writes_rd){
        reg_file.write(reg.rd, reg.rd_data);
    }

    reg.retired = true;
    return reg;
}
