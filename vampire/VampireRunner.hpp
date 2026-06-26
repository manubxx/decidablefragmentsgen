#pragma once
#include <string>

/**
 * SZS ontology statuses returned by Vampire:
 *   Theorem            — the formula is logically valid (the negation is UNSAT)
 *   CounterSatisfiable — a counter-model exists (negation is SAT)
 *   Satisfiable        — the formula is satisfiable
 *   Unsatisfiable      — the formula is unsatisfiable
 *   Timeout            — Vampire exceeded the time limit
 *   Unknown            — Vampire terminated without a definitive answer
 *   Error              — the runner itself failed
 */
class VampireRunner {
public:
   
    struct Result {          
        
        std::string status;      
        std::string rawOutput;  
        double elapsedTime = 0.0; 

        bool timedOut = false;
        bool runError = false;
    };

    
 
    explicit VampireRunner(std::string vampirePath = "");


    /**
     * Runs Vampire on the TPTP formula and returns the result.
     * The formula is written to a uniquely named temporary file, 
     * removed before returning. Vampire runs with its default saturation algorithm (LRS) 
                
     */
    Result run(const std::string& tptpFormula, int timeLimitSec = 10, int memoryLimit = 4096) const;

    bool isAvailable() const;

private:

    std::string vampirePath_;

    static std::string extractSZSStatus(const std::string& output);
    static double extractElapsedTime(const std::string& output);
};