#include "BenchmarkSuite.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

void runDatasetBenchmarks(const AppArgs& args, const VampireRunner& runner) {
    if (!fs::exists(args.benchmarkPath) || !fs::is_directory(args.benchmarkPath)) {
        std::cerr << "Error: The specified path is not a valid directory: " << args.benchmarkPath << "\n";
        return;
    }

    std::cout << "=== STARTING BENCHMARK CAMPAIGN ON DATASET (CASC MODE) ===\n";
    std::ofstream csv("report_benchmark_casc.csv");
    csv << "FileName,SZS_Status,Time_Elapsed(s)\n";

    for (const auto& entry : fs::recursive_directory_iterator(args.benchmarkPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".p") {
            std::ifstream file(entry.path());
            std::string formulaStr((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();

            std::cout << "Analyzing file: " << entry.path().filename().string() << " -> ";

            auto res = runner.run(formulaStr, args.vampireTimeout, "--mode casc");

            std::cout << "[" << res.status << "] in " << res.elapsedTime << "s\n";
            csv << entry.path().filename().string() << "," << res.status << "," << res.elapsedTime << "\n";
        }
    }
    csv.close();
    std::cout << "=== BENCHMARK FINISHED. Report saved in report_benchmark_casc.csv ===\n";
}