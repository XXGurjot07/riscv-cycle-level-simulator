#include "core.hpp"
#include "hazard_unit.hpp"
#include "logging.hpp"

// core initializer list
Core::Core(uint8_t* mem, uint64_t entry_pc, const SimConfig& cfg, uint64_t to_host) : cfg_(cfg), mem_(mem), 
    pc_(entry_pc), to_host_(to_host), fetch_(cfg.MEM_SIZE), memstage_(cfg.MEM_SIZE) {}    

bool Core::tick() {
    
    // check halt
    if (halted_) 
        return false;

    // commit
    retired_Res ret_result = writeback_.evaluate(regs_, mem_wb_);
    if (ret_result.retired){
        stats_.retired++;
        regs_.pc = ret_result.pc;
        if(cfg_.cosim)
            log_commit_spike(ret_result);
        else if(cfg_.log_on) 
            log_commit(stats_.cycles, ret_result);
    }

    if(ret_result.retired && ret_result.is_store && ret_result.mem_addr == to_host_){
        halted_ = true;
        exit_code_ = int(ret_result.mem_data >> 1);
        stats_.cycles++;
        return false;
    }
    
    if(ret_result.trap){
        log_trap(ret_result);
        halted_ = true;
        stats_.trap_cycles++; 
        stats_.cycles++;
        return false;
    }

    else if (!ret_result.valid){
        stats_.lost[ret_result.why]++;
    }

    // evaluate using double buffering
    reg_MEM_WB   n_mem_wb = memstage_.evaluate(mem_, ex_mem_);
    reg_EX_Ports ex       = execute_.evaluate(id_ex_);
    reg_ID_EX    n_id_ex  = decode_.evaluate(regs_, if_id_);
    reg_IF_ID    n_if_id  = fetch_.evaluate(mem_, pc_);

    // control: redirect, ex_stall, hazard detect
    bool redirect = ex.reg.valid && ex.reg.redirect;
    bool ex_stall = !ex.accepted;
        
    bool haz_raw_d1   = hazard_raw_d1(n_id_ex, id_ex_);
    bool haz_raw_d2   = hazard_raw_d2(n_id_ex, ex_mem_);
    bool haz_load_use = hazard_load_use(n_id_ex, id_ex_);

    // backend clock edge these stages always advance
    mem_wb_  = n_mem_wb;
    ex_mem_  = ex.reg;

    // frontend may need to flush or stall
    if(redirect){
        // flush inflight and redirect pc
        id_ex_ = {};
        if_id_ = {};

        pc_ = ex.reg.redirect_target;

        // bubble cause - jump or branch
        Bubble_Why why = (ex.reg.fu_type == FUType::SYSTEM) ? FENCE_I : BRANCH_MISPREDICT;
        id_ex_.why = why;
        if_id_.why = why;
    }
    else if(ex_stall){
        // dont advance pc

        // bubble cause - nop from ex
    }
    else if(haz_raw_d1 || haz_raw_d2 || haz_load_use){
        if(cfg_.forwarding){
        
            // forwarding logic 
            // load use stall is 1 bubble and done advance pc
            if(haz_load_use){
                id_ex_ = {};

                // bubble cause - load use
                id_ex_.why = Bubble_Why :: LOAD_USE;
            }
            else{

                id_ex_ = n_id_ex;
                if_id_ = n_if_id;
                pc_ += 4;

                // forward path for rs1
                if(haz_raw_d1 && id_ex_.deco_inst.rs1 == ex_mem_.rd){
                    id_ex_.rs1_data = ex_mem_.rd_data;
                }
                else if(haz_raw_d2 && id_ex_.deco_inst.rs1 == mem_wb_.rd){
                    id_ex_.rs1_data = mem_wb_.rd_data;
                }

                // forward path for rs2
                if(haz_raw_d1 && id_ex_.deco_inst.rs2 == ex_mem_.rd){
                    id_ex_.rs2_data = ex_mem_.rd_data;
                }
                else if(haz_raw_d2 && id_ex_.deco_inst.rs2 == mem_wb_.rd){
                    id_ex_.rs2_data = mem_wb_.rd_data;
                }
            }
        }
        else{
            id_ex_ = {};

            // bubble cause - nop into ex
            id_ex_.why = Bubble_Why :: RAW;
        }
    }
    else{
        // if no hazards
        id_ex_ = n_id_ex;
        if_id_ = n_if_id;
        pc_ += 4;    
    }

    stats_.cycles++;
    return true;
}    

void Core::run() {
    while (stats_.cycles < cfg_.MAX_CYCLES && tick()){
    }
}