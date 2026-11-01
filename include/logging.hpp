#pragma once
#include <cstdio>
#include "config.hpp"
#include "definitions_isa.hpp"
#include "definitions_archstate.hpp"
#include "definitions_pipeline.hpp"

// Logging helpers
void log_header();
void log_commit(uint64_t cycle, const retired_Res& wb);
void log_trap(const retired_Res& wb);