

#include "CliPrinter.hpp"

#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>


// ─────────────────────────────────────────────────────────────────────────────
//  Helpers interni
// ─────────────────────────────────────────────────────────────────────────────
namespace {

std::string modeStr(GenMode m)
{
    switch (m) {
    case GenMode::FREE:  return "FREE";
    case GenMode::SAT:   return "SAT";
    case GenMode::UNSAT: return "UNSAT";
    }
    return "?";
}

// Formatta lo status SZS di Vampire con colore ANSI e annotazione [OK]/[INATTESO].
std::string formatVampireStatus(const std::string& status, GenMode mode)
{
    const char* GREEN  = "\033[32m";
    const char* RED    = "\033[31m";
    const char* YELLOW = "\033[33m";
    const char* RESET  = "\033[0m";

    bool ok = false;
    if (mode == GenMode::SAT)
        ok = (status == "CounterSatisfiable" || status == "Satisfiable");
    else if (mode == GenMode::UNSAT)
        ok = (status == "Unsatisfiable");

    std::string color;
    if (status == "Timeout" || status == "Error" || status == "Unknown")
        color = YELLOW;
    else if (mode == GenMode::FREE)
        color = "";
    else
        color = ok ? GREEN : RED;

    std::string label = status;
    if (mode != GenMode::FREE
        && status != "Timeout"
        && status != "Error"
        && status != "Unknown")
        label += ok ? " [OK]" : " [INATTESO]";

    if (!color.empty()) return color + label + RESET;
    return label;
}

} 

// ─────────────────────────────────────────────────────────────────────────────
//  printHeader
// ─────────────────────────────────────────────────────────────────────────────

void printHeader(const std::string&           fragment,
                 const GenConfig&             cfg,
                 int                          count,
                 unsigned                     seed,
                 const std::vector<PredInfo>& vocab,
                 bool                         verify,
                 const std::string&           vampirePath,
                 int                          vampireTimeout)
{
    std::cout << "\n=== Generatore " << fragment << " ===\n"
              << "  Modalita'     : " << modeStr(cfg.mode) << "\n"
              << "  Profondita'   : " << cfg.depth << "\n"
              << "  Formule       : " << count << "\n"
              << "  Seed          : " << seed << "\n"
              << "  Trasformazione: "
              << (cfg.transform == TransformMode::NNF ? "NNF" : "none") << "\n"
              << "  Output        : "
              << (cfg.output == OutputFormat::TPTP ? "TPTP" : "default") << "\n"
              << "  Arita' filtro : "
              << (cfg.arityFilter == 0 ? "mixed" : std::to_string(cfg.arityFilter)) << "\n";

    if (cfg.domainSize > 0) {
        std::cout << "  Domain size   : " << cfg.domainSize << "\n";
        if (cfg.mode == GenMode::SAT) {
            long long exponent = 0;
            for (const auto& p : vocab) {
                long long ne = 1;
                for (int k = 0; k < p.arity; ++k) ne *= cfg.domainSize;
                exponent += ne;
            }
            std::cout << "  Modelli (~2^" << exponent << ")\n";
        }
    } else {
        std::cout << "  Domain size   : non applicabile (FREE/UNSAT)\n";
    }

    // Budget — stampato solo se almeno un vincolo è attivo
    const auto& b = cfg.budget;
    if (b.hasAnyConstraint()) {
        auto printRange = [](const char* name, const BudgetRange& r) {
            if (!r.isConstrained()) return;
            std::cout << name << "=";
            if (r.min == r.max)  std::cout << r.min;
            else if (r.max < 0)  std::cout << r.min << "+";
            else if (r.min <= 0) std::cout << "<=" << r.max;
            else                 std::cout << r.min << ":" << r.max;
            std::cout << " ";
        };
        std::cout << "  Budget        : ";
        printRange("AND",     b.and_count);
        printRange("OR",      b.or_count);
        printRange("NOT",     b.not_count);
        printRange("EXISTS",  b.exists_count);
        printRange("FORALL",  b.forall_count);
        printRange("IMPLIES", b.implies_count);
        printRange("EQ",      b.eq_count);
        std::cout << "\n";
    }

    // Vocabolario raggruppato per arità
    std::cout << "  Vocabolario   : ";
    std::map<int, std::vector<std::string>> byArity;
    for (const auto& p : vocab)
        byArity[p.arity].push_back(p.name + "/" + std::to_string(p.arity));
    bool first = true;
    for (const auto& [arity, names] : byArity) {
        if (!first) std::cout << "  |  ";
        for (std::size_t i = 0; i < names.size(); ++i) {
            if (i) std::cout << ", ";
            std::cout << names[i];
        }
        first = false;
    }
    std::cout << "\n";

    if (verify)
        std::cout << "  Verifica      : Vampire  (path=" << vampirePath
                  << ", timeout=" << vampireTimeout << "s)\n";

    std::cout << "\n";
}


// ─────────────────────────────────────────────────────────────────────────────
//  printFormula
// ─────────────────────────────────────────────────────────────────────────────

void printFormula(int                  idx,
                  const GenConfig&     cfg,
                  const std::string&   formulaStr,
                  bool                 verify,
                  const VampireRunner* runner,
                  int                  vampireTimeout)
{
    // Tag modalità 
    std::string tag;
    switch (cfg.mode) {
    case GenMode::SAT:   tag = "[SAT]  "; break;
    case GenMode::UNSAT: tag = "[UNSAT]"; break;
    case GenMode::FREE:  tag = "[FREE] "; break;
    }

    if (cfg.output == OutputFormat::TPTP) {
        std::cout << "% Formula " << idx << "\n" << formulaStr << "\n";
    } else {
        std::cout << "  " << std::setw(3) << idx << ".  "
                  << tag << "  " << formulaStr << "\n";
    }

    if (verify && runner) {
        if (cfg.output != OutputFormat::TPTP) {
            std::cout << "  [Vampire] SKIP — output non TPTP\n";
        } else {
            auto res = runner->run(formulaStr, vampireTimeout);
            std::string label = formatVampireStatus(res.status, cfg.mode);
            std::cout << "% Vampire: " << label << "\n";
            if (res.runError)
                std::cerr << "  [Vampire] Errore: " << res.rawOutput << "\n";
        }
    }

    std::cout << "\n";
}