#include "benchmark.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <regex>

namespace fs = std::filesystem;

void runDatasetBenchmarks(const AppArgs& args, const VampireRunner& runner) {
    if (!fs::exists(args.benchmarkPath) || !fs::is_directory(args.benchmarkPath)) {
        std::cerr << "Error: The specified path is not a valid directory: " << args.benchmarkPath << "\n";
        return;
    }

   
    std::string reportDir = "benchmarkreport";
    fs::create_directories(reportDir);
    std::string reportPath = reportDir + "/report_benchmark_casc.csv";

    std::string discardedDir = "tests/discardedformulas";
    fs::create_directories(discardedDir);

    std::cout << "STARTING BENCHMARK CAMPAIGN (CASC MODE)\n";
    std::ofstream csv(reportPath);

    csv << "Dataset,FileName,SZS_Status,Time_Elapsed(s),Generated_Clauses,Generation_Attempts\n";

    for (const auto& entry : fs::recursive_directory_iterator(args.benchmarkPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".p") {

            std::ifstream file(entry.path());
            std::string formulaStr((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();

            int attempts = 1; 
            std::smatch match;
            std::regex attemptsRegex(R"(% ATTEMPTS:\s*(\d+))");
            if (std::regex_search(formulaStr, match, attemptsRegex)) {
                attempts = std::stoi(match[1].str());
            }

            std::string datasetName = entry.path().parent_path().filename().string();
            std::string fileName = entry.path().filename().string();

            std::cout << "Analyzing: [" << datasetName << "] " << fileName << " -> ";

            auto res = runner.run(formulaStr, args.vampireTimeout, "--mode casc");

            if (res.timedOut || res.runError) {
                fs::path p(entry.path());
                fs::copy_file(p, fs::path(discardedDir) / p.filename(), fs::copy_options::overwrite_existing);
            }

            std::cout << "[" << res.status << "] Time: " << res.elapsedTime
                << "s | Clauses: " << res.generatedClauses
                << " | Attempts: " << attempts << "\n";

     
            csv << datasetName << ","
                << fileName << ","
                << res.status << ","
                << res.elapsedTime << ","
                << res.generatedClauses << ","
                << attempts << "\n";
        }
    }
    csv.close();
    std::cout << "BENCHMARK DONE. Report saved in " << reportPath << "\n";
}