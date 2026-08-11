#pragma once
#include <string>
#include "../vampire/VampireRunner.hpp"
#include "../CLI/CLIArgs.hpp" 

void runDatasetBenchmarks(const AppArgs& args, const VampireRunner& runner);
void runTimeoutAnalysisNative(const AppArgs& baseArgs, const VampireRunner& runner);