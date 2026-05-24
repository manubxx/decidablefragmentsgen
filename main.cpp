#include "cli/CliArgs.hpp"
#include "cli/CliPrinter.hpp"
#include "fragments/fo2/FO2Generator.hpp"
#include "fragments/fluted/FlutedGenerator.hpp"
#include "vampire/VampireRunner.hpp"
#include <iostream>

template <typename Gen>
static int runGenerator(Gen& gen, const GenConfig& cfg, int count,
                        bool verify, const VampireRunner* runner, int timeout)
{
    int failures = 0;
    for (int i = 1; i <= count; ++i) {
        try {
            std::string formula = gen.generateFormatted(cfg);
            printFormula(i, cfg, formula, verify, runner, timeout);
        } catch (const std::exception& e) {
            std::cerr << "  Errore generazione formula " << i
                      << ": " << e.what() << "\n\n";
            ++failures;
        }
    }
    return failures;
}

int main(int argc, char* argv[])
{
    AppArgs args;
    try {
        args = parseArgs(argc, argv);
    } catch (const HelpRequest&) {
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Errore: " << e.what() << "\n";
        return 1;
    }

    VampireRunner vampireRunner(args.vampirePath);
    if (args.verify && !vampireRunner.isAvailable()) {
        std::cerr << "ATTENZIONE Vampire non trovato al percorso '"
                  << args.vampirePath << "'.\n"
                  << "  La generazione procedera' ma la verifica restituira' 'Error'.\n\n";
    }

    const VampireRunner* runnerPtr = args.verify ? &vampireRunner : nullptr;
    int failures = 0;

    if (args.fragment == "fo2") {
        printHeader("FO2", args.cfg, args.count, args.seed,
                    args.cfg.vocab, args.verify, args.vampirePath, args.vampireTimeout);
        FO2Generator gen(args.cfg.vocab, args.seed);
        failures = runGenerator(gen, args.cfg, args.count,
                                args.verify, runnerPtr, args.vampireTimeout);
    }
    else if (args.fragment == "fluted") {
        printHeader("Fluted", args.cfg, args.count, args.seed,
                    args.cfg.vocab, args.verify, args.vampirePath, args.vampireTimeout);
        FlutedGenerator gen(args.cfg.vocab, args.seed);
        failures = runGenerator(gen, args.cfg, args.count,
                                args.verify, runnerPtr, args.vampireTimeout);
    }

    if (failures > 0)
        std::cerr << "\n ATTENZIONE" << failures << "/" << args.count
                  << " formule non generate.\n"
                  << "Suggerimento: aumenta --depth o riduci i vincoli.\n\n";

    return 0;
}