#include "CLI/CLIArgs.hpp"
#include "CLI/CLIPrinter.hpp"
#include "fragments/fo2/FO2Generator.hpp"
#include <fragments/fo2/FO2SATGenerator.hpp>
#include "fragments/fluted/FlutedGenerator.hpp"
#include "fragments/guarded/GuardedGenerator.hpp"
#include "fragments/unarynegation/UnaryNegGenerator.hpp"
#include "vampire/VampireRunner.hpp"
#include <iostream>

template <typename Gen>
static int runGenerator(Gen& gen, const GenConfig& cfg, int count, bool verify, const VampireRunner* runner, int timeout)
{
    int failures = 0;
    int generatedCount = 0;
    const int MAX_ATTEMPTS = 50; // limite di tentativi per singola formula

    while (generatedCount < count) {
        int attempts = 0;
        bool formulaAccepted = false;
        std::string formula;

        while (!formulaAccepted && attempts < MAX_ATTEMPTS) {
            ++attempts;
            try {
                formula = gen.generateFormatted(cfg);

                if (!verify || !runner || cfg.output != OutputFormat::TPTP) {
                    formulaAccepted = true;
                    break;
                }

                auto res = runner->run(formula, timeout);
                if (res.runError) {
                    std::cerr << "  [VAMPIRE ERROR] " << res.rawOutput << "\n";
                    continue;
                }

                
                if (cfg.mode == GenMode::SAT || cfg.mode == GenMode::SATBUILD) {
                    if (res.status == "Satisfiable" || res.status == "CounterSatisfiable")
                        formulaAccepted = true;
                }
                else if (cfg.mode == GenMode::UNSAT) {
                    if (res.status == "Unsatisfiable")
                        formulaAccepted = true;
                }
                else {
                    formulaAccepted = true; // FREE
                }
            }
            catch (const std::exception&) {
                continue;
            }
        }

        if (formulaAccepted) {
            ++generatedCount;
            printFormula(generatedCount, cfg, formula, verify, runner, timeout);
        }
        else {
            std::cerr << "Unable to generate a valid formula after " << MAX_ATTEMPTS << " attempts\n";
            ++failures;
            ++generatedCount;
        }
    }
    return failures;
}

int main(int argc, char* argv[])
{
    AppArgs args;
    try {
        args = parseArgs(argc, argv);
    }
    catch (const HelpRequest&) {
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    args.cfg.mode = GenMode::SATBUILD;

    VampireRunner vampireRunner(args.vampirePath);

    if (args.verify && !vampireRunner.isAvailable()) {
        std::cerr << "WARNING: Vampire not found at path: '" << args.vampirePath << "'.\n"
            << "Verification will return 'Error'.\n\n";
    }

    const VampireRunner* runnerPtr = args.verify ? &vampireRunner : nullptr;
    int failures = 0;

    if (args.fragment == "fo2") {
        // Allineamento di sicurezza se il parser fa un fallback silenzioso dell'enum
        if (args.cfg.mode == GenMode::FREE && argc > 1) {
            for (int i = 1; i < argc; ++i) {
                if (std::string(argv[i]) == "satbuild") {
                    args.cfg.mode = GenMode::SATBUILD;
                }
            }
        }

        printHeader("FO2", args.cfg, args.count, args.seed, args.cfg.vocab, args.verify, args.vampirePath, args.vampireTimeout);

        if (args.cfg.mode == GenMode::SATBUILD) {
            FO2SATGenerator gen(args.cfg.vocab, args.seed);
            failures = runGenerator(gen, args.cfg, args.count, args.verify, runnerPtr, args.vampireTimeout);
        }
        else {
            FO2Generator gen(args.cfg.vocab, args.seed);
            failures = runGenerator(gen, args.cfg, args.count, args.verify, runnerPtr, args.vampireTimeout);
        }
    }
    else if (args.fragment == "fluted") {
        printHeader("Fluted", args.cfg, args.count, args.seed, args.cfg.vocab, args.verify, args.vampirePath, args.vampireTimeout);
        FlutedGenerator gen(args.cfg.vocab, args.seed);
        failures = runGenerator(gen, args.cfg, args.count, args.verify, runnerPtr, args.vampireTimeout);
    }
    else if (args.fragment == "guarded") {
        printHeader("Guarded", args.cfg, args.count, args.seed, args.cfg.vocab, args.verify, args.vampirePath, args.vampireTimeout);
        GuardedGenerator gen(args.cfg.vocab, args.seed);
        failures = runGenerator(gen, args.cfg, args.count, args.verify, runnerPtr, args.vampireTimeout);
    }
    else if (args.fragment == "unaryneg") {
        printHeader("UnaryNegation", args.cfg, args.count, args.seed, args.cfg.vocab, args.verify, args.vampirePath, args.vampireTimeout);
        UnaryNegGenerator gen(args.cfg.vocab, args.seed);
        failures = runGenerator(gen, args.cfg, args.count, args.verify, runnerPtr, args.vampireTimeout);
    }

    if (failures > 0)
        std::cerr << "\n WARNING: " << failures << "/" << args.count << " formulae not generated.\n"
        << "Suggestion: increase --depth or reduce constraints.\n\n";

    return 0;
}