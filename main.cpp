#include "CLI/CLIArgs.hpp"
#include "CLI/CLIPrinter.hpp"
#include "fragments/fo2/FO2Generator.hpp"
#include "fragments/fluted/FlutedGenerator.hpp"
#include "vampire/VampireRunner.hpp"
#include <iostream>

template <typename Gen>
static int runGenerator(Gen & gen, const GenConfig & cfg, int count,
    bool verify, const VampireRunner * runner, int timeout)
{
    int failures = 0;
    int generatedCount = 0;
    const int MAX_ATTEMPTS = 50; // Soglia di retry per formula

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

                // Controllo Semantico Centralizzato con Vampire
                auto res = runner->run(formula, timeout);
                if (res.runError) {
                    std::cerr << "  [ERRORE VAMPIRE] " << res.rawOutput << "\n";
                    continue;
                }

                // Logica di validazione basata sulla modalità richiesta
                if (cfg.mode == GenMode::SAT) {
                    if (res.status == "Satisfiable" || res.status == "CounterSatisfiable") {
                        formulaAccepted = true;
                    }
                    // Se UNSAT o Timeout, il ciclo continua e rigenera
                }
                else if (cfg.mode == GenMode::UNSAT) {
                    if (res.status == "Unsatisfiable") {
                        formulaAccepted = true;
                    }
                }
                else {
                    // Modalità FREE: accettiamo qualsiasi risultato valido da Vampire
                    formulaAccepted = true;
                }

            }
            catch (const std::exception& e) {
                // Fallimento del budget o errore interno di buildFL/buildFO2
                continue;
            }
        }

        if (formulaAccepted) {
            ++generatedCount;
            printFormula(generatedCount, cfg, formula, verify, runner, timeout);
        }
        else {
            std::cerr << "[-] Impossibile generare una formula valida per l'indice "
                << (generatedCount + 1) << " dopo " << MAX_ATTEMPTS << " tentativi.\n";
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