#include "benchmark.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <regex>
#include <iomanip>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#include <regex>
#pragma GCC diagnostic pop

namespace fs = std::filesystem;

void runDatasetBenchmarks(const AppArgs& args, const VampireRunner& runner) {
    if (!fs::exists(args.benchmarkPath) || !fs::is_directory(args.benchmarkPath)) {
        std::cerr << "Error: The specified path is not a valid directory: " << args.benchmarkPath << "\n";
        return;
    }

    std::string reportDir = "benchmarkreport";
    fs::create_directories(reportDir);

    
    std::string reportPath = reportDir + "/report_benchmark_" + std::to_string(args.vampireTimeout) + "s.csv";

    fs::path rootPath(args.benchmarkPath);

    std::string discardedDir = args.benchmarkPath + "/timeout_cand_" + std::to_string(args.vampireTimeout);
    std::string unknownDir = args.benchmarkPath + "/unknown_cand_" + std::to_string(args.vampireTimeout);

    fs::create_directories(discardedDir);
    fs::create_directories(unknownDir);

    std::cout << "STARTING BENCHMARK CAMPAIGN (SATURATION MODE - Timeout: " << args.vampireTimeout << "s)\n";
    std::ofstream csv(reportPath);

    csv << "Dataset,FileName,SZS_Status,Time_Elapsed(s),Generated_Clauses,Generation_Attempts,Formula_Length\n";

    for (const auto& entry : fs::recursive_directory_iterator(args.benchmarkPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".p") {

            std::string parentPath = entry.path().parent_path().string();
            if (parentPath.find("timeout_cand_") != std::string::npos && parentPath != args.benchmarkPath) {
                continue;
            }
            if (parentPath.find("unknown_cand_") != std::string::npos && parentPath != args.benchmarkPath) {
                continue;
            }

            std::ifstream file(entry.path());
            std::string formulaStr((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();

            size_t formulaLength = formulaStr.length();
            int attempts = 1;
            std::smatch match;
            std::regex attemptsRegex(R"(% ATTEMPTS:\s*(\d+))");
            if (std::regex_search(formulaStr, match, attemptsRegex)) {
                attempts = std::stoi(match[1].str());
            }

            std::string datasetName = entry.path().parent_path().filename().string();
            std::string fileName = entry.path().filename().string();

            std::cout << "Analyzing: [" << datasetName << "] " << fileName << " -> ";

            auto res = runner.run(formulaStr, args.vampireTimeout, " ");

            fs::path p(entry.path());
            std::string safeDiscardName = datasetName + "_" + fileName;

            if (res.timedOut || res.runError || res.status == "Timeout" || res.status == "Error") {
                fs::copy_file(p, fs::path(discardedDir) / safeDiscardName, fs::copy_options::overwrite_existing);
            }
            else if (res.status == "Unknown") {
                fs::copy_file(p, fs::path(unknownDir) / safeDiscardName, fs::copy_options::overwrite_existing);
            }

            std::cout << "[" << res.status << "] Time: " << std::fixed << std::setprecision(4) << res.elapsedTime
                << "s | Length: " << formulaLength
                << " | Attempts: " << attempts << "\n";

            csv << datasetName << "," << fileName << "," << res.status << ","
                << res.elapsedTime << "," << res.generatedClauses << ","
                << attempts << "," << formulaLength << "\n";
        }
    }

    csv.close();
    std::cout << "\nBENCHMARK DONE. Report saved in " << reportPath << "\n";
    std::cout << "Discarded formulas (Timeout/Errors) saved in " << discardedDir << "\n";
}

void runTimeoutAnalysisNative(const AppArgs& baseArgs, const VampireRunner& runner) {

 
    std::string currentTargetDir = baseArgs.benchmarkPath.empty() ?
        "./" + baseArgs.fragment + "_datasets/timeout_cand_30/timeout_cand_60" :
        baseArgs.benchmarkPath + "/timeout_cand_30/timeout_cand_60";

    
    std::vector<int> timeouts = { 120 };

    for (int t : timeouts) {
        if (!fs::exists(currentTargetDir) || fs::is_empty(currentTargetDir)) {
            std::cout << "\nNo formula found in " << currentTargetDir << ". Done.\n";
            break;
        }

        std::cout << " Timeout Analysis (Calibration): " << t << "s \n";
        std::cout << " Reading from: " << currentTargetDir << "\n";

        AppArgs currentArgs = baseArgs;
        currentArgs.benchmarkPath = currentTargetDir;
        currentArgs.vampireTimeout = t;

        runDatasetBenchmarks(currentArgs, runner);

     
        currentTargetDir = currentTargetDir + "/timeout_cand_" + std::to_string(t);
    }
}