#pragma once
#include <string>

/**
 *
 * VampireRunner serialises a first-order formula (already encoded in TPTP
 * syntax) to a temporary file, invokes Vampire as a child process, and
 * parses the SZS status line from its standard output.  The class is
 * intentionally stateless between calls so that multiple formulas can be
 * verified independently during benchmark runs.
 *
 * SZS ontology statuses returned by Vampire:
 *   Theorem            — the formula is logically valid (the negation is UNSAT)
 *   CounterSatisfiable — a counter-model exists (negation is SAT)
 *   Satisfiable        — the formula is satisfiable
 *   Unsatisfiable      — the formula is unsatisfiable
 *   Timeout            — Vampire exceeded the time limit
 *   Unknown            — Vampire terminated without a definitive answer
 *   Error              — the runner itself failed
 *
 *  For reproducible benchmark timings, native Linux execution is strongly recommended.
 */
class VampireRunner {
public:
    //outcome of a single Vampire invocation.
    struct Result {          
        
        std::string status;       //SZS status 

        std::string rawOutput;    // Full standard output of the Vampire process

        double elapsedTime = 0.0; // Time reported by Vampire itself 

        bool timedOut = false;
        bool runError = false;
    };

    
    //Constructs a VampireRunner targeting the given executable path.
    explicit VampireRunner(std::string vampirePath = "");



    /**
     * Runs Vampire on the TPTP formula and returns the result.
     * The formula is written to a uniquely named temporary file, 
     * removed before returning. Vampire runs with its default saturation algorithm (LRS) 
     
     * @param tptpFormula  a complete, well-formed TPTP problem string.
     * @param timeLimitSec time limit forwarded to Vampire (seconds).
     * @return             a Result struct containing the SZS status, elapsed time, and the raw output.                 
     */
    Result run(const std::string& tptpFormula, int timeLimitSec = 10) const;


    /**
     * Checks whether the Vampire binary is reachable.
     * @return True if the executable exists (POSIX) 
     */
    bool isAvailable() const;

private:

    std::string vampirePath_;

    /**
     * Extracts the SZS status token from Vampire's raw output.
     * @param output Raw stdout captured from the Vampire process.
     * @return       The SZS status string.
     */
    static std::string extractSZSStatus(const std::string& output);

    /**
     * @param output Raw stdout captured from the Vampire process.
     * @return       Elapsed time in seconds.
     */
    static double extractElapsedTime(const std::string& output);
};