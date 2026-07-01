#include "CLI/CLIArgs.hpp"
#include "CLI/CLIPrinter.hpp"
#include "fragments/fo2/FO2Generator.hpp"
#include <fragments/fo2/sat/FO2SATGenerator.hpp>
#include "fragments/fluted/FlutedGenerator.hpp"
#include "fragments/guarded/GuardedGenerator.hpp"
#include "fragments/unarynegation/UnaryNegGenerator.hpp"
#include "fragments/modal/ModalGenerator.hpp"
#include "vampire/VampireRunner.hpp"
#include "tests/benchmark.hpp" 
#include <iostream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

template <typename Gen>
static int runGenerator(Gen& gen, const GenConfig& cfg, int count, bool verify, const VampireRunner* runner, int timeout) {
    int failures = 0;
    int generatedCount = 0;
    const int MAX_ATTEMPTS = 50;

    //output directory
    std::string outputDir = "generated_output";
    fs::create_directories(outputDir);

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
                    std::cerr << "VAMPIRE ERROR" << res.rawOutput << "\n";
                    continue;
                }

                if (cfg.mode == GenMode::SAT || cfg.mode == GenMode::SATBUILD) {
                    if (res.status == "Satisfiable" || res.status == "CounterSatisfiable") {
                        formulaAccepted = true;

                        if (attempts > 1) {
                            std::cout << "  [INFO] SAT formula found after " << attempts << " attempts (Discarded: " << (attempts - 1) << ").\n";
                        }
                    }
                }
                else if (cfg.mode == GenMode::UNSAT) {
                    if (res.status == "Unsatisfiable" || res.status == "Theorem" || res.status == "Contradiction") {
                        formulaAccepted = true;

                        if (attempts > 1) {
                            std::cout << "  [INFO] UNSAT formula found after " << attempts << " attempts (Discarded: " << (attempts - 1) << ").\n";
                        }
                    }
                }
                else {
                    formulaAccepted = true;
                }
            }
            catch (const std::exception& e) {
                continue;
            }
        }

        if (formulaAccepted) {
            ++generatedCount;

            std::string tptpHeader = "% ATTEMPTS: " + std::to_string(attempts) + "\n";
            formula = tptpHeader + formula;
            printFormula(generatedCount, cfg, formula, verify, runner, timeout);

            std::string extension = (cfg.output == OutputFormat::TPTP) ? ".p" : ".txt";
            std::string fileName = outputDir + "/formula_" + std::to_string(generatedCount) + extension;

            std::ofstream outFile(fileName);
            if (outFile) {
                outFile << formula;
                outFile.close();
            }
        }
        else {
            std::cerr << "Unable to generate a valid formula after " << MAX_ATTEMPTS << " attempts. Skipping.\n";

            ++failures;
            ++generatedCount;
        }
    }

    return failures;
}

int main(int argc, char* argv[]) {
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

    VampireRunner vampireRunner(args.vampirePath);

    if (args.verify && !vampireRunner.isAvailable()) {
        std::cerr << "WARNING Vampire not found at path: '" << args.vampirePath << "'.\n" << "Verification will return 'Error'.\n\n";
    }

    if (args.runBenchmarks) {
        runDatasetBenchmarks(args, vampireRunner);
        return 0;
    }

    const VampireRunner* runnerPtr = args.verify ? &vampireRunner : nullptr;
    int failures = 0;

    if (args.fragment == "fo2") {
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
    else if (args.fragment == "modal") {
        printHeader("Modal", args.cfg, args.count, args.seed, args.cfg.vocab, args.verify, args.vampirePath, args.vampireTimeout);
        ModalGenerator gen(args.cfg.vocab, args.seed);
        failures = runGenerator(gen, args.cfg, args.count, args.verify, runnerPtr, args.vampireTimeout);
    }

    if (failures > 0)
        std::cerr << "\n WARNING: " << failures << "/" << args.count << " formulae not generated.\n"
        << "Suggestion: increase --depth or reduce constraints.\n\n";

    return failures > 0 ? 1 : 0;
}