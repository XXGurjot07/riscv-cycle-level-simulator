#pragma once
#include <deque>
#include "definitions_isa.hpp"
#include "definitions_pipeline.hpp"

// ALU, BRU, AGU, MUL/DIV
FU_Output do_alu(const FU_Input& fu_in);
FU_Output do_bru(const FU_Input& fu_in);
FU_Output do_agu(const FU_Input& fu_in);
FU_Output do_mul_div(const FU_Input& fu_in);

// FUnit : houses a EX Type and Occupancy Rules (Latency & Pipelined)
template <typename payload>
class FunctionalUnit{
    private :
        struct Slot{
            payload p;
            int left;
        };                // one pipeline register + its countdown

        int latency_;     // latency of this EX unit
        bool pipelined_;  // is it pipelined or not, can accept a new input every cycle or not?

        std::deque<Slot> inflight_; // stateful inflight instructions in EX stage
    
    public :

        FunctionalUnit(int latency, bool pipelined) : latency_(latency), pipelined_(pipelined) {}

        // for the input side : this is busy check
        bool idle() const {
            return inflight_.empty();
        }

        bool can_accept() const {
            return pipelined_ || inflight_.empty();    
        }

        // compute at issue, but deliver result after latency_ cycles
        void issue(const payload& p){
            inflight_.push_back({p, latency_});
        }

        // advance all instructions by 1 per tick
        void tick(){
            for (size_t i = 0; i < inflight_.size(); i++){
                if(inflight_[i].left > 0){
                    inflight_[i].left--;
                }
            }
        }

        // this is valid for in-order execute.
        bool has_result() const {
            return !inflight_.empty() && inflight_.front().left == 0;
        }

        // if computed and latency is exhausted, pop entry
        payload pop(){
            payload p = inflight_.front().p;
            inflight_.pop_front();
            return p;
        }

        void flush(){
            inflight_.clear();
        }

};

using ComputeFn = FU_Output (*)(const FU_Input&);

struct FU_Lane{
    FunctionalUnit<reg_EX_MEM> unit;   // timing, your class
    ComputeFn compute;                 // which combinational block this lane uses   
};

class ExecuteStage{
    private :
        FU_Lane alu_{{1, true}, do_alu};
        FU_Lane bru_{{1, true}, do_bru};
        FU_Lane agu_{{1, true}, do_agu};
    
        bool       all_idle() const;
        FU_Lane&   unit_for(const reg_ID_EX& id_ex_reg);
        reg_EX_MEM build   (const reg_ID_EX& id_ex_reg, ComputeFn compute) const;

    public : 
        reg_EX_Ports evaluate(const reg_ID_EX& id_ex_reg);    // this is called once per cycle
        void flush();          
};