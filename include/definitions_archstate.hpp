#pragma once
#include<cstdint>

// Architectural Registers : x0 - x31 and PC
struct arch_state{
    private:
        uint64_t x[32] = {0};
    public:
        uint64_t pc = 0;

        uint64_t read(uint8_t reg_idx) const {
            if(reg_idx == 0) 
                return 0;
            return x[reg_idx];
        }

        void write(uint8_t reg_idx, uint64_t value){
            if(reg_idx != 0) 
                x[reg_idx] = value;        
            return;
        }
};