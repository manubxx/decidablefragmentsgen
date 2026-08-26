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
#include "tests/datasetgen.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

template <typename Gen>
static int runGenerator(Gen& gen, const GenConfig& cfg, int count, bool verify, const VampireRunner* runner, int timeout) {
    int failures = 0;
    int generatedCount = 0;
    const int MAX_ATTEMPTS = 200;

    std::string outputDir = "generated_output";
    fs::create_directories(outputDir);

    while (generatedCount < count) {
        int attempts = 0;
        bool formulaAccepted = false;
        std::string formula;
        std::string lastError = "Unknown error";

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
                    lastError = "Vampire Error: " + res.rawOutput;
                    continue;
                }

                if ((cfg.mode == GenMode::SAT && (res.status == "Satisfiable" || res.status == "CounterSatisfiable")) ||
                    (cfg.mode == GenMode::UNSAT && (res.status == "Unsatisfiable" || res.status == "Theorem" || res.status == "Contradiction")) ||
                    (cfg.mode == GenMode::FREE)) {
                    formulaAccepted = true;
                }
            }
            catch (const std::exception& e) {
                lastError = e.what();
                continue;
            }
        }

        if (formulaAccepted) {
            ++generatedCount;

          
            std::cout << "\n% Formula " << generatedCount << " \n";
            std::cout << "% ATTEMPTS: " << attempts << "\n";
            std::cout << formula << "\n";

           
            std::string extension = (cfg.output == OutputFormat::TPTP) ? ".p" : ".txt";
            std::ofstream outFile(outputDir + "/formula_" + std::to_string(generatedCount) + extension);
            if (outFile) {
                outFile << formula;
                outFile.close();
            }
        }
        else {
            std::string discardedDir = "tests/discardedformulas";
            fs::create_directories(discardedDir);
            std::ofstream errOut(discardedDir + "/failed_gen_" + std::to_string(generatedCount) + ".p");

            errOut << "% GENERATION FAILED\n";
            errOut << "% Reason: " << lastError << "\n";
            errOut << "% Attempts: " << attempts << "\n";
            errOut << "% Configuration: Depth=" << cfg.depth << ", ArityFilter=" << cfg.arityFilter << "\n";
            errOut.close();

            std::cerr << "Failed to generate formula " << generatedCount << ". Reason: " << lastError << "\n";
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

    if (args.runTimeoutAnalysis) {
        runTimeoutAnalysisNative(args, vampireRunner);
        return 0;
    }

    if (args.generateDatasets) {
        std::cout << "\n[INFO] Using PRNG Seed: " << args.seed << "\n";

        if (args.fragment == "fluted") {
            FlutedGenerator gen(args.cfg.vocab, args.seed);
            generateDatasetsNative(gen, args.fragment, args.count);
        }
        else if (args.fragment == "fo2") {
           
            FO2Generator gen(args.cfg.vocab, args.seed);

            generateDatasetsNative<FO2Generator>(gen, args.fragment, args.count);
        }

        else if (args.fragment == "guarded") {
            GuardedGenerator gen(args.cfg.vocab, args.seed);
            generateDatasetsNative<GuardedGenerator>(gen, args.fragment, args.count);
        }
        else {
            std::cout << "Generation not defined for the fragment:" << args.fragment << "\n";
        }
        return 0;
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