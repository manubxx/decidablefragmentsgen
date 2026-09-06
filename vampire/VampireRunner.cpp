#include "VampireRunner.hpp"
#include <filesystem>
#include <cstdio>
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <array>
#include <iostream>
#include <atomic>
#include <cstring>
#include <regex>
#include <chrono>


#if defined(_MSC_VER) || defined(_WIN32)
#include <process.h>
#define popen _popen
#define pclose _pclose
#define getpid _getpid
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

static std::string sanitizeTPTP(const std::string& raw) {
    return raw;
}

// Costruttore
VampireRunner::VampireRunner(std::string vampirePath)
{
    if (!vampirePath.empty()) {
        vampirePath_ = std::move(vampirePath);
        return;
    }

#ifdef VAMPIRE_DEFAULT_PATH
    vampirePath_ = VAMPIRE_DEFAULT_PATH;
#else
    vampirePath_ = "vampire";
#endif
}

// isAvailable
bool VampireRunner::isAvailable() const {
    if (vampirePath_ == "vampire") {
        return true;
    }
    return fs::exists(vampirePath_);
}

// run 
VampireRunner::Result VampireRunner::run(const std::string& tptpFormula, int timeLimitSec, const std::string& extraFlags) const {
    Result result;

    std::string cleanFormula = sanitizeTPTP(tptpFormula);

    static std::atomic<int> fileCounter{ 0 };
    std::string inputFile = "temp_vampire_" + std::to_string(getpid()) + "_" + std::to_string(++fileCounter) + ".p";

    std::ofstream out(inputFile);
    if (!out) {
        result.runError = true;
        result.status = "Error";
        result.rawOutput = "Failed to create temporary file for Vampire.";
        return result;
    }

    out << cleanFormula;
    out.close();

  
    std::string flags = extraFlags.empty() ? "--mode casc" : extraFlags;
    std::string cmd = vampirePath_ + " " + flags + " --time_limit " + std::to_string(timeLimitSec) + " " + inputFile + " 2>&1";

    
    auto startWallTime = std::chrono::high_resolution_clock::now();

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        fs::remove(inputFile);
        result.runError = true;
        result.status = "Error";
        result.rawOutput = "popen() failed.";
        return result;
    }

    std::array<char, 256> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result.rawOutput += buffer.data();
    }
    int exitCode = pclose(pipe);

    auto endWallTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = endWallTime - startWallTime;
    result.elapsedTime = elapsed.count(); 

    fs::remove(inputFile);

    if (result.rawOutput.empty()) {
        result.runError = true;
        result.status = "Error";
        result.rawOutput = "No output received from Vampire.";
        return result;
    }

    // Estrazione dello stato SZS
    result.status = extractSZSStatus(result.rawOutput);

   
    if (result.status == "Timeout" || result.rawOutput.find("Time limit") != std::string::npos || result.rawOutput.find("time limit") != std::string::npos) {
        result.timedOut = true;
        result.status = "Timeout";
    }
    else if (result.status == "Error" || result.rawOutput.find("Error") != std::string::npos || result.rawOutput.find("syntax error") != std::string::npos || exitCode != 0) {
        result.runError = true;
        result.status = "Error";
    }

    result.generatedClauses = extractGeneratedClauses(result.rawOutput);
    return result;
}

// extractSZSStatus
std::string VampireRunner::extractSZSStatus(const std::string& output)
{
    const std::string marker = "SZS status ";
    auto pos = output.find(marker);
    if (pos == std::string::npos) {
        if (output.find("Time limit") != std::string::npos || output.find("time limit") != std::string::npos)
            return "Timeout";
        if (output.find("Error") != std::string::npos)
            return "Error";
        return "Unknown";
    }

    pos += marker.size();
    auto end = output.find_first_of(" \t\r\n", pos);
    if (end == std::string::npos)
        return output.substr(pos);

    return output.substr(pos, end - pos);
}

// extractElapsedTime 
double VampireRunner::extractElapsedTime(const std::string& output)
{
    const std::string marker = "Time elapsed: ";
    auto pos = output.find(marker);
    if (pos == std::string::npos) return 0.0;

    pos += marker.size();
    auto end = output.find(" s", pos);
    if (end == std::string::npos) return 0.0;

    try {
        return std::stod(output.substr(pos, end - pos));
    }
    catch (...) {
        return 0.0;
    }
}

// extractGeneratedClauses
long long VampireRunner::extractGeneratedClauses(const std::string& output) {
    std::smatch match;
    std::regex reg(R"((\d+)\s+generated clauses)");

    if (std::regex_search(output, match, reg)) {
        try {
            return std::stoll(match[1].str());
        }
        catch (...) {
            return 0;
        }
    }
    return 0;
}