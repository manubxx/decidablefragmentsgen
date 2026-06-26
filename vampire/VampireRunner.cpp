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
    // Fall back to expecting "vampire" on the system PATH.
    vampirePath_ = "vampire";
#endif
}

// isAvailable
bool VampireRunner::isAvailable() const
{
    // Ora controlla nativamente il filesystem POSIX
    return std::filesystem::exists(vampirePath_);
}

// run
VampireRunner::Result VampireRunner::run(const std::string& tptpFormula, int timeLimitSec, int memoryLimit) const
{
    Result result;

    static std::atomic<int> fileCounter{ 0 };
    std::string inputFile = "temp_vampire_" + std::to_string(getpid()) + "_" + std::to_string(++fileCounter) + ".p";

    std::ofstream out(inputFile);
    if (!out) {
        result.runError = true;
        result.status = "Error";
        result.rawOutput = "Cannot create temporary file.";
        return result;
    }
    out << tptpFormula;
    out.close();

    // Setup del comando nativo Linux con il tool `timeout`
    int hardTimeout = timeLimitSec + 5;
    std::string cmd = "timeout " + std::to_string(hardTimeout) + " " + vampirePath_ +
        " -t " + std::to_string(timeLimitSec) +
        " -m " + std::to_string(memoryLimit) + " " + inputFile + " 2>&1";

    
    FILE* pipe = popen(cmd.c_str(), "r");

    if (!pipe) {
        result.runError = true;
        result.status = "Error";
        result.rawOutput = "Failed to launch Vampire. Check that '" + vampirePath_ + "' is on the PATH.";
        std::remove(inputFile.c_str());
        return result;
    }

    // Cattura stdout e stderr
    std::ostringstream outputStream;
    std::array<char, 256> buf;
    while (std::fgets(buf.data(), static_cast<int>(buf.size()), pipe))
        outputStream << buf.data();

    pclose(pipe);
    std::remove(inputFile.c_str());

    result.rawOutput = outputStream.str();
    result.status = extractSZSStatus(result.rawOutput);
    result.elapsedTime = extractElapsedTime(result.rawOutput);

    if (result.status == "Timeout")
        result.timedOut = true;

    if (result.rawOutput.empty()) {
        result.runError = true;
        result.status = "Error";
        result.rawOutput = "No output received from Vampire. "
            "Check that '" + vampirePath_ + "' is on the PATH.";
    }

    return result;
}

// extractSZSStatus
std::string VampireRunner::extractSZSStatus(const std::string& output)
{
    const std::string marker = "SZS status ";
    auto pos = output.find(marker);
    if (pos == std::string::npos) {
        // Fallback per limiti di tempo espliciti
        if (output.find("Time limit") != std::string::npos ||
            output.find("time limit") != std::string::npos)
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