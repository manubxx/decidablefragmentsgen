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
#include <unistd.h>
#include <regex>


static std::string sanitizeTPTP(const std::string& raw) {
    std::stringstream ss(raw);
    std::stringstream clean;
    std::string line;
    while (std::getline(ss, line)) {
      
        std::string trimmed = line;
        trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));


        if (trimmed.empty() ||
            trimmed.rfind("%", 0) == 0 ||
            trimmed.rfind("fof", 0) == 0 ||
            trimmed.rfind("cnf", 0) == 0 ||
            trimmed.rfind("include", 0) == 0) {
            clean << line << "\n";
        }
    }
    return clean.str();
}

// Constructor
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
bool VampireRunner::isAvailable() const { return std::filesystem::exists(vampirePath_); }

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

    // Scriviamo la versione pulita
    out << cleanFormula;
    out.close();

    int hardTimeout = timeLimitSec + 2;
    int memoryLimit = 4096;

    // Comando per Vampire
    std::string cmd = "timeout " + std::to_string(hardTimeout) + " " + vampirePath_ + " -t " + std::to_string(timeLimitSec) + " -m " + std::to_string(memoryLimit) + " " + extraFlags + " " + inputFile + " 2>&1";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::filesystem::remove(inputFile);
        result.runError = true;
        result.status = "Error";
        result.rawOutput = "popen() failed.";
        return result;
    }

    std::array<char, 256> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result.rawOutput += buffer.data();
    }
    pclose(pipe);

    std::filesystem::remove(inputFile);

    result.status = extractSZSStatus(result.rawOutput);
    result.elapsedTime = extractElapsedTime(result.rawOutput);

    if (result.status == "Timeout") {
        result.timedOut = true;
    }

    if (result.rawOutput.empty()) {
        result.runError = true;
        result.status = "Error";
        result.rawOutput = "No output received from Vampire.";
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