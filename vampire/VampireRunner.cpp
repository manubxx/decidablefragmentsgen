#include "VampireRunner.hpp"
#include <filesystem>
#include <cstdio>
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <array>
#include <iostream>

#ifdef _WIN32
#  include <windows.h>
#  define POPEN  _popen
#  define PCLOSE _pclose
#else
#  include <cstring>
#  define POPEN  popen
#  define PCLOSE pclose
#endif


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
#ifdef _WIN32
    std::string checkCmd = "wsl " + vampirePath_ + " --version 2>&1";
    FILE* pipe = _popen(checkCmd.c_str(), "r");
    if (!pipe) return false;
    char buf[64];
    bool hasOutput = std::fgets(buf, sizeof(buf), pipe) != nullptr;
    _pclose(pipe);
    return hasOutput;
#else
    return std::filesystem::exists(vampirePath_);
#endif
}


// run
VampireRunner::Result VampireRunner::run(const std::string& tptpFormula,
    int timeLimitSec) const
{
    Result result;

    // Build a unique temporary file name 
#ifdef _WIN32
    char tmpPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tmpPath);
    for (char* p = tmpPath; *p; ++p) if (*p == '\\') *p = '/';
    std::string inputFile = std::string(tmpPath) + "vamp_input_"
        + std::to_string(GetCurrentProcessId()) + ".p";
#else
    std::string inputFile = "/tmp/vamp_input_"
        + std::to_string(getpid()) + ".p";
#endif

    {
        std::ofstream ofs(inputFile);
        if (!ofs.is_open()) {
            result.runError = true;
            result.status = "Error";
            result.rawOutput = "Could not create temporary input file: " + inputFile;
            std::remove(inputFile.c_str());
            return result;
        }
        ofs << tptpFormula << "\n";
    }

    std::ostringstream cmdStream;

#ifdef _WIN32
    // Convert a Windows absolute path (e.g. C:\...) to its WSL mount point
    std::string wslVampire = vampirePath_;
    if (wslVampire.size() >= 2 && wslVampire[1] == ':') {
        char d = std::tolower(wslVampire[0]);
        wslVampire = "/mnt/" + std::string(1, d) + wslVampire.substr(2);
    }
    std::replace(wslVampire.begin(), wslVampire.end(), '\\', '/');

    std::string wslInput = inputFile;
    if (wslInput.size() >= 2 && wslInput[1] == ':') {
        char d = std::tolower(wslInput[0]);
        wslInput = "/mnt/" + std::string(1, d) + wslInput.substr(2);
    }
    std::replace(wslInput.begin(), wslInput.end(), '\\', '/');

    cmdStream << "wsl " << wslVampire
        << " --time_limit " << timeLimitSec
        << " --memory_limit 2048"
        << " " << wslInput
        << " 2>&1";
#else
    // Native Linux: invoke Vampire directly with a fixed memory
    cmdStream << vampirePath_
        << " --time_limit " << timeLimitSec
        << " --memory_limit 2048"
        << " " << inputFile
        << " 2>&1";
#endif

    std::string cmd = cmdStream.str();

    FILE* pipe = POPEN(cmd.c_str(), "r");
    if (!pipe) {
        result.runError = true;
        result.status = "Error";
        result.rawOutput = "Failed to launch Vampire. Check that '" + vampirePath_ + "' is on the PATH.";
        std::remove(inputFile.c_str());
        return result;
    }

    // Vampire's stdout/stderr into a single string.
    std::ostringstream outputStream;
    std::array<char, 256> buf;
    while (std::fgets(buf.data(), static_cast<int>(buf.size()), pipe))
        outputStream << buf.data();

    PCLOSE(pipe);
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
        // No SZS line found; check for an explicit time-limit message as fallback.
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
    // Vampire prints "% Time elapsed: X.XXX s" at the end of every run.
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
        return 0.0; // Parsing failed
    }
}