#include "core.hpp"
#include "hazard_unit.hpp"
#include "logging.hpp"

// core initializer list
Core::Core(uint8_t* mem, uint64_t entry_pc, const SimConfig& cfg, uint64_t to_host) : cfg_(cfg), mem_(mem), 
    pc_(entry_pc), to_host_(to_host), fetch_(cfg.MEM_SIZE), execute_(cfg.mul_latency, cfg.mul_pipelined, cfg.div_latency, cfg.div_pipelined), memstage_(cfg.MEM_SIZE) {}    

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

    // backend clock edge these stages always advance
    mem_wb_  = n_mem_wb;
    ex_mem_  = ex.reg;

    // control: redirect, ex_stall, hazard detect
    bool redirect = ex_mem_.valid && ex_mem_.redirect;
    bool ex_stall = !ex.accepted;
    bool scoreboard_stall = false;
        
    bool haz_raw_d1   = hazard_raw_d1(n_id_ex, id_ex_);
    bool haz_raw_d2   = hazard_raw_d2(n_id_ex, ex_mem_);
    bool haz_load_use = hazard_load_use(n_id_ex, id_ex_);

    // advanced decode - issue logic
    // we can't forward if the register is being written to by an inflight instruction
    scoreboard_update(id_ex_, ex, ret_result);

    scoreboard_stall = n_id_ex.valid &&
        ((scoreboard_op_in_flight_[n_id_ex.deco_inst.rs1] && n_id_ex.deco_inst.reads_rs1) ||
         (scoreboard_op_in_flight_[n_id_ex.deco_inst.rs2] && n_id_ex.deco_inst.reads_rs2) ||
         (scoreboard_op_in_flight_[n_id_ex.deco_inst.rd]  && n_id_ex.deco_inst.writes_rd));

    // frontend may need to flush or stall
    if(redirect){
        // flush inflight and redirect pc
        id_ex_ = {};
        if_id_ = {};

        pc_ = ex_mem_.redirect_target;

        // bubble cause - jump or branch
        Bubble_Why why = (ex_mem_.fu_type == FUType::SYSTEM) ? FENCE_I : BRANCH_MISPREDICT;
        id_ex_.why = why;
        if_id_.why = why;
    }
    else if(ex_stall){
        // dont advance pc

        // bubble cause - nop from ex
    }
    else if(scoreboard_stall){
        // dont advance pc, but put bubble into the current cycle decoded register
        id_ex_ = {};
        
        // bubble cause - nop from ex
        id_ex_.why = RAW_MULDIV;
    }
    else if(haz_raw_d1 || haz_raw_d2 || haz_load_use){
        if(cfg_.forwarding){
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

    if(cfg_.forwarding){
        forward_operands(id_ex_, ex_mem_, mem_wb_);
    }

    stats_.cycles++;
    return true;
}    

void Core::run() {
    while (stats_.cycles < cfg_.MAX_CYCLES && tick()){
    }
}

void Core::scoreboard_update(const reg_EX_Ports& ex, const retired_Res& ret_result){
    // if the instruction is accepted into EX, we can set the scoreboard for the destination register
    uint8_t set_rd = 0;
    uint8_t clr_rd = 0;

    bool is_mul_div = id_ex_.deco_inst.fu_type == FUType::MUL || id_ex_.deco_inst.fu_type == FUType::DIV;

    set_rd = (is_mul_div && ex.accepted && id_ex_.valid && id_ex_.deco_inst.writes_rd && id_ex_.exc == Exception::NONE) ? id_ex_.deco_inst.rd : 0;

    if(cfg_.forwarding){
        clr_rd = (ex_mem_.valid && ex_mem_.writes_rd) ? ex_mem_.rd : 0;
    }
    else{
        clr_rd = (ret_result.retired && ret_result.writes_rd) ? ret_result.rd : 0;
    }

    if(set_rd != 0){
        scoreboard_op_in_flight_[set_rd] = true;
    }

    // if the instruction can be forwarded, we can clear the scoreboard
    if(clr_rd != 0){
        scoreboard_op_in_flight_[clr_rd] = false;
    }
}
