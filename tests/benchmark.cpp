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
    std::string reportPath = reportDir + "/report_benchmark_casc.csv";

    fs::path rootPath(args.benchmarkPath);

    std::string discardedDir;
    std::string unknownDir;

 
    if (rootPath.filename() == "timeout_cand") {
        discardedDir = args.benchmarkPath;
    }
    else {
        discardedDir = args.benchmarkPath + "/timeout_cand";
        unknownDir = args.benchmarkPath + "/unknown_cand";
        fs::create_directories(unknownDir);
    }
    fs::create_directories(discardedDir);

    std::cout << "STARTING BENCHMARK CAMPAIGN (CASC MODE)\n";
    std::ofstream csv(reportPath);

    csv << "Dataset,FileName,SZS_Status,Time_Elapsed(s),Generated_Clauses,Generation_Attempts,Formula_Length\n";

    for (const auto& entry : fs::recursive_directory_iterator(args.benchmarkPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".p") {

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

           
            if ((datasetName == "timeout_cand" || datasetName == "unknown_cand") && rootPath.filename() != datasetName) {
                continue;
            }
            std::string fileName = entry.path().filename().string();

            std::cout << "Analyzing: [" << datasetName << "] " << fileName << " -> ";

            auto res = runner.run(formulaStr, args.vampireTimeout, "--mode casc");

            fs::path p(entry.path());
            std::string safeDiscardName = datasetName + "_" + fileName;

           
            if (res.timedOut || res.runError || res.status == "Timeout" || res.status == "Error") {
                fs::copy_file(p, fs::path(discardedDir) / safeDiscardName, fs::copy_options::overwrite_existing);
            }
            else if (res.status == "Unknown" && !unknownDir.empty()) {
                fs::copy_file(p, fs::path(unknownDir) / safeDiscardName, fs::copy_options::overwrite_existing);
            }

            std::cout << "[" << res.status << "] Time: " << std::fixed << std::setprecision(4) << res.elapsedTime
                << "s | Length: " << formulaLength
                << " | Attempts: " << attempts << "\n";

            csv << datasetName << ","
                << fileName << ","
                << res.status << ","
                << res.elapsedTime << ","
                << res.generatedClauses << ","
                << attempts << ","
                << formulaLength << "\n";
        }
    }

    csv.close();
    std::cout << "\nBENCHMARK DONE. Report saved in " << reportPath << "\n";
    std::cout << "Discarded formulas (Timeout/Errors) saved in " << discardedDir << "\n";
    if (!unknownDir.empty()) {
        std::cout << "Unknown formulas saved in " << unknownDir << "\n";
    }
}

void runTimeoutAnalysisNative(const AppArgs& baseArgs, const VampireRunner& runner) {
    std::string targetDir = baseArgs.benchmarkPath.empty() ?
        "./" + baseArgs.fragment + "_datasets/timeout_cand" :
        baseArgs.benchmarkPath;

    std::string reportDir = "benchmarkreport";

    if (!fs::exists(targetDir) || fs::is_empty(targetDir)) {
        std::cout << "No formula found in " << targetDir << "\n";
        return;
    }

    std::vector<int> timeouts = { 30, 60, 120 };

    for (int t : timeouts) {
        std::cout << "\n Timeout Analysis (Calibration): " << t << "s ===\n";

        AppArgs currentArgs = baseArgs;
        currentArgs.benchmarkPath = targetDir;
        currentArgs.vampireTimeout = t;

        runDatasetBenchmarks(currentArgs, runner);

        std::string defaultReport = reportDir + "/report_benchmark_casc.csv";

        std::string targetReport = reportDir + "/report_" + baseArgs.fragment + "_timeout_" + std::to_string(t) + "s.csv";

        if (fs::exists(defaultReport)) {
            fs::rename(defaultReport, targetReport);
            std::cout << "DONE. Saved in: " << targetReport << "\n";
        }
    }
}