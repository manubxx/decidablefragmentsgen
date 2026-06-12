#include "CliPrinter.hpp"
#include <iostream>
#include <string>
#include <vector>

// printHeader 
void printHeader(
    const std::string& fragment,
    const GenConfig& cfg,
    int                          count,
    unsigned                     seed,
    const std::vector<PredInfo>& vocab,
    bool                         verify,
    const std::string& vampirePath,
    int                          vampireTimeout) {

    // CORREZIONE: Gestione della stringa SATBUILD
    std::string modeStr = "FREE";
    if (cfg.mode == GenMode::SAT) modeStr = "SAT";
    else if (cfg.mode == GenMode::UNSAT) modeStr = "UNSAT";
    else if (cfg.mode == GenMode::SATBUILD) modeStr = "SATBUILD";

    std::cout << "--- GENERATION CONFIGURATION ---\n"
        << "Fragment: " << fragment << "\n"
        << "Mode: " << modeStr << "\n" // <-- Stampa la stringa corretta
        << "Depth: " << cfg.depth << " | Count: " << count << " | Seed: " << seed << "\n"
        << "Domain Size: " << cfg.domainSize << "\n"
        << "Vocabulary Size: " << vocab.size() << " predicates.\n";

    if (verify) {
        std::cout << "Verification: Vampire Prover active (" << vampireTimeout << "s timeout)\n";
    }
    std::cout << "--------------------------------\n\n";
}

// printFormula
void printFormula(
    int                  idx,
    const GenConfig& cfg,
    const std::string& formulaStr,
    bool                 verify,
    const VampireRunner* runner,
    int                  vampireTimeout) {

    // TPTP output
    if (cfg.output == OutputFormat::TPTP) {
        std::cout << "% Formula " << idx << "\n" << formulaStr << "\n";

        if (verify && runner) {
            auto result = runner->run(formulaStr, vampireTimeout);
            std::cout << "% Vampire Verification Status: " << result.status << "\n";
        }
    }
    else {
        // Standard output
     
        std::string modeT = "[FREE]";
        if (cfg.mode == GenMode::SAT) modeT = "[SAT]";
        else if (cfg.mode == GenMode::UNSAT) modeT = "[UNSAT]";
        else if (cfg.mode == GenMode::SATBUILD) modeT = "[SATBUILD]";

        std::cout << modeT << " (" << idx << "): " << formulaStr << "\n";
    }
    std::cout << "\n";
}