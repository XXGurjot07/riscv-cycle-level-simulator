#pragma once
#include "definitions_isa.hpp"
#include "definitions_archstate.hpp"
#include "definitions_pipeline.hpp"

class WritebackStage{
    public:
        retired_Res evaluate(arch_state& reg_file, const reg_MEM_WB& mem_wb_reg);
};