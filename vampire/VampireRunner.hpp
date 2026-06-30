#pragma once
#include <string>

class VampireRunner {
public:
    struct Result {
        std::string status;
        std::string rawOutput;
        double elapsedTime = 0.0;
        bool timedOut = false;
        bool runError = false;
        long long generatedClauses = 0;
    };

    explicit VampireRunner(std::string vampirePath = "");

   
    Result run(const std::string& tptpFormula, int timeLimitSec = 10, const std::string& extraFlags = "") const;

    bool isAvailable() const;

private:
    std::string vampirePath_;
    static std::string extractSZSStatus(const std::string& output);
    static double extractElapsedTime(const std::string& output);
    static long long extractGeneratedClauses(const std::string& output);
};