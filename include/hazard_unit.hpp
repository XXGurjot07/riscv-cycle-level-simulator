#pragma once
#include "definitions_isa.hpp"
#include "definitions_pipeline.hpp"

// Hazards Data
bool hazard_raw_d1(const reg_ID_EX& new_id_ex, const reg_ID_EX& id_ex);
bool hazard_raw_d2(const reg_ID_EX& new_id_ex, const reg_EX_MEM& ex_mem);
bool hazard_load_use(const reg_ID_EX& new_id_ex, const reg_ID_EX& id_ex);

void forward_operands(reg_ID_EX& id_ex_, const reg_EX_MEM& ex_mem_, const reg_MEM_WB& mem_wb_);

// Hazards Control
// Handled implicitly inside BRU FU in EXE Stage

// Hazards Structural
// Handled implicitly inside FU Router