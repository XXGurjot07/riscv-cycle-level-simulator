#pragma once
#include "config.hpp"
#include "elf64_loader.hpp"

#include "definitions_isa.hpp"
#include "definitions_archstate.hpp"
#include "definitions_pipeline.hpp"

#include "fetch_stage.hpp"
#include "decode_stage.hpp"
#include "execute_stage.hpp"
#include "memory_stage.hpp"
#include "writeback_stage.hpp"

class Core{
    private :
        SimConfig  cfg_;                  // A core config

        uint8_t*   mem_;                  // NOT owned: main owns it (multi-core shares it)

        arch_state regs_;                 // architectural state (register file + retired pc)
        uint64_t   pc_;                   // fetch pc (speculative)
        uint64_t   to_host_;              // address of tohost symbol
        int        exit_code_ = 0;        // exit code to return to main

        Core_HPM_Stats stats_;

        reg_IF_ID   if_id_;                        
        reg_ID_EX   id_ex_;
        reg_EX_MEM  ex_mem_;                      
        reg_MEM_WB  mem_wb_;

        FetchStage     fetch_;                 
        DecodeStage    decode_;
        ExecuteStage   execute_;
        MemStage       memstage_;
        WritebackStage writeback_;
        
        bool halted_ = false;

    public :
        Core(uint8_t* mem, uint64_t entry_pc, const SimConfig& cfg, uint64_t to_host);  // constructor
        bool tick();        // one clock cycle; returns false once halted
        void run();         // tick until halted or the cycle limit

        int  exit_code() const { return exit_code_; }
        bool halted()  const { return halted_; }
        const Core_HPM_Stats& stats() const { return stats_; }
        const arch_state& regs() const { return regs_;   }   

};