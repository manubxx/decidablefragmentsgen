#include "CliArgs.hpp"
#include "fragments/fo2/FO2Vocab.hpp"       
#include "fragments/fluted/FlutedVocab.hpp"  
#include <algorithm>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#include <fstream>


namespace {

    // Naming automatico dei predicati
    std::string predName(int arity, int index)
    {
        if (arity >= 1 && arity <= 25) {
            char letter = static_cast<char>('A' + arity);
            return std::string(1, letter) + std::to_string(index);
        }
        return "P" + std::to_string(arity) + "_" + std::to_string(index);
    }

    //  Parsing mode 
    GenMode parseMode(const std::string& s)
    {
        if (s == "sat")   return GenMode::SAT;
        if (s == "unsat") return GenMode::UNSAT;
        if (s == "free")  return GenMode::FREE;
        throw std::invalid_argument(
            "Modalita' non riconosciuta: '" + s + "' (valori: free, sat, unsat)");
    }

    // Parsing transform
    TransformMode parseTransform(const std::string& s)
    {
        if (s == "none") return TransformMode::NONE;
        if (s == "nnf")  return TransformMode::NNF;
        throw std::invalid_argument(
            "Transform non riconosciuto: '" + s + "' (valori: none, nnf)");
    }

    // Parsing output format 
    OutputFormat parseOutput(const std::string& s)
    {
        if (s == "default") return OutputFormat::DEFAULT;
        if (s == "tptp")    return OutputFormat::TPTP;
        throw std::invalid_argument(
            "Formato output non riconosciuto: '" + s + "' (valori: default, tptp)");
    }

    //Parsing budget range 
     BudgetRange parseRange(const std::string& s)
    {
        auto colon = s.find(':');
        if (colon == std::string::npos) {
            int v = std::stoi(s);
            if (v < 0)
                throw std::invalid_argument("Valore budget negativo: " + s);
            return { v, v };
        }
        int lo = std::stoi(s.substr(0, colon));
        int hi = std::stoi(s.substr(colon + 1));
        BudgetRange r{ lo, hi };
        r.validate("range");
        return r;
    }

    //  Parsing --preds 
    //  Formato: <count>/<arity>
    std::vector<PredInfo> parsePreds(const std::string& s, const std::string& fragment)
    {
        std::vector<PredInfo> vocab;

        // Gestione virgola
        std::vector<std::string> specs;
        {
            std::string cur;
            for (char c : s) {
                if (c == ',') {
                    if (!cur.empty()) { specs.push_back(cur); cur.clear(); }
                }
                else {
                    cur += c;
                }
            }
            if (!cur.empty()) specs.push_back(cur);
        }

        if (specs.empty())
            throw std::invalid_argument("--preds: specificazione vuota");

        for (const auto& spec : specs) {
            auto slash = spec.find('/');
            if (slash == std::string::npos)
                throw std::invalid_argument(
                    "--preds: formato non valido '" + spec +
                    "' (atteso: <count>/<arita'>, es. 3/2)");

            int count, arity;
            try {
                count = std::stoi(spec.substr(0, slash));
                arity = std::stoi(spec.substr(slash + 1));
            }
            catch (...) {
                throw std::invalid_argument(
                    "--preds: valori non interi in '" + spec + "'");
            }

            if (count < 1)
                throw std::invalid_argument(
                    "--preds: il numero di predicati deve essere >= 1 (in '" + spec + "')");
            if (arity < 1)
                throw std::invalid_argument(
                    "--preds: l'arita' deve essere >= 1 (in '" + spec + "')");
            if (fragment == "fo2" && arity > 2)
                throw std::invalid_argument(
                    "--preds: FO2 ammette solo predicati di arita' 1 o 2. "
                    "Arita' " + std::to_string(arity) + " non ammessa.\n"
                    "  Usa --fragment fluted per arita' arbitraria.");

            // Indice di partenza per il naming
            int startIdx = 1;
            for (const auto& p : vocab)
                if (p.arity == arity) ++startIdx;

            for (int i = 0; i < count; ++i)
                vocab.push_back({ predName(arity, startIdx + i), arity });
        }

        return vocab;
    }

    // Parsing --arity
    int parseArityFilter(const std::string& s, const std::string& fragment)
    {
        if (s == "mixed") return 0;

        int k;
        try { k = std::stoi(s); }
        catch (...) {
            throw std::invalid_argument(
                "--arity: valore non riconosciuto '" + s +
                "' (valori: mixed, o un intero >= 1)");
        }

        if (k < 1)
            throw std::invalid_argument(
                "--arity: l'arita' deve essere >= 1 (ricevuto: " + s + ")");
        if (fragment == "fo2" && k > 2)
            throw std::invalid_argument(
                "--arity: FO2 ammette solo arita' 1 o 2 (ricevuto: " + s + ")");

        return k;
    }

    // Validazione semantica post-parsing 
    void validateArgs(const AppArgs& a)
    {
        if (a.cfg.depth < 0)
            throw std::invalid_argument("--depth deve essere >= 0");
        if (a.count <= 0)
            throw std::invalid_argument("--count deve essere >= 1");
        if (a.cfg.domainSize < 0)
            throw std::invalid_argument("--domain-size deve essere >= 0");
        if (a.vampireTimeout <= 0)
            throw std::invalid_argument("--vampire-timeout deve essere > 0");

        if (a.cfg.vocab.empty()) {
            std::string msg = "Il vocabolario risultante e' vuoto";
            if (a.cfg.arityFilter != 0)
                msg += " (--arity " + std::to_string(a.cfg.arityFilter) +
                " non corrisponde a nessun predicato nel vocabolario)";
            throw std::invalid_argument(msg + ".");
        }

        // Budget
        a.cfg.budget.validate();   // lancia std::invalid_argument se min > max
    }

} // Chiusura del namespace


// Forward declarations esterne
extern const std::vector<PredInfo> kVocabFO2;
extern const std::vector<PredInfo> kVocabFluted;



namespace {

    std::vector<PredInfo> buildVocab(const std::string& fragment,
        bool                          hasExplicit,
        const std::vector<PredInfo>& explicitVocab,
        int                           arityFilter)
    {
        const std::vector<PredInfo>& base =
            hasExplicit ? explicitVocab
            : (fragment == "fo2" ? kVocabFO2 : kVocabFluted);

        std::vector<PredInfo> result;
        result.reserve(base.size());
        for (const auto& p : base)
            if (arityFilter == 0 || p.arity == arityFilter)
                result.push_back(p);

        return result;
    }




    static void printUsage(const char* prog)
    {
        // Apre il file di testo
        std::ifstream file("usage.txt");

        // Se il file non viene trovato nella cartella corrente, mostra un avviso
        if (!file.is_open()) {
            std::cerr << "Errore: Impossibile trovare il file 'usage.txt'.\n"
                << "Assicurati che sia nella stessa cartella di " << prog << "\n";
            return;
        }

        std::string line;
        // Legge il file riga per riga tramite il getline
        while (std::getline(file, line)) {

            // Cerca se nella riga corrente esiste il tag "{prog}"
            size_t pos = line.find("{prog}");
            while (pos != std::string::npos) {
                // Sostituisce "{prog}" (6 caratteri) con il nome reale (es. "DecidableFragmentsGen")
                line.replace(pos, 6, prog);
                // Cerca se ci sono altri "{prog}" nella stessa riga (es. nella sezione ESEMPI)
                pos = line.find("{prog}", pos + strlen(prog));
            }

            // Stampa la riga corretta sul terminale
            std::cerr << line << "\n";
        }
    }

} // namespace



// function parseArgs
AppArgs parseArgs(int argc, char* argv[])
{
    // Raccolta dei parametri
    struct RawParam {
        std::string flag;
        std::string value;   // flag booleani (--verify, --help)
    };
    std::vector<RawParam> parameters;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        // Flag booleani 
        if (arg == "--help" || arg == "--verify") {
            parameters.push_back({ arg, "" });
            continue;
        }

        // Tutti gli altri flag richiedono un valore
        if (i + 1 >= argc)
            throw std::invalid_argument(
                "Flag '" + arg + "' richiede un argomento");

        parameters.push_back({ arg, argv[++i] });
    }

    // Check --help 
    for (const auto& t : parameters) {
        if (t.flag == "--help") {
            printUsage(argv[0]);
            throw HelpRequest{};
        }
    }

    // Determina il fragment prima degli altri flag
    std::string fragment = "fo2";
    for (const auto& t : parameters) {
        if (t.flag == "--fragment") {
            fragment = t.value;
            break;
        }
    }
    if (fragment != "fo2" && fragment != "fluted" && fragment != "fl")
        throw std::invalid_argument(
            "Frammento non supportato: '" + fragment + "'\n"
            "Frammenti disponibili: fo2, fluted");

    if (fragment == "fl") fragment = "fluted";

    // Parsing dei flag
    AppArgs args;
    args.fragment = fragment;

    bool                  hasExplicitVocab = false;
    std::vector<PredInfo> explicitVocab;
    std::string           arityStr;
    bool                  hasSeed = false;

    for (const auto& t : parameters) {
        const std::string& f = t.flag;
        const std::string& v = t.value;

        if (f == "--fragment") { /* già gestito */ }
        else if (f == "--mode")            args.cfg.mode = parseMode(v);
        else if (f == "--depth")           args.cfg.depth = std::stoi(v);
        else if (f == "--count")           args.count = std::stoi(v);
        else if (f == "--seed") { args.seed = static_cast<unsigned>(std::stoul(v)); hasSeed = true; }
        else if (f == "--transform")       args.cfg.transform = parseTransform(v);
        else if (f == "--output")          args.cfg.output = parseOutput(v);
        else if (f == "--arity")           arityStr = v;
        else if (f == "--domain-size")     args.cfg.domainSize = std::stoi(v);
        else if (f == "--preds") { explicitVocab = parsePreds(v, fragment); hasExplicitVocab = true; }
        // Budget
        else if (f == "--and")             args.cfg.budget.and_count = parseRange(v);
        else if (f == "--or")              args.cfg.budget.or_count = parseRange(v);
        else if (f == "--not")             args.cfg.budget.not_count = parseRange(v);
        else if (f == "--exists")          args.cfg.budget.exists_count = parseRange(v);
        else if (f == "--forall")          args.cfg.budget.forall_count = parseRange(v);
        else if (f == "--implies")         args.cfg.budget.implies_count = parseRange(v);
        else if (f == "--eq")              args.cfg.budget.eq_count = parseRange(v);
        // Vampire
        else if (f == "--verify")          args.verify = true;
        else if (f == "--vampire-path")    args.vampirePath = v;
        else if (f == "--vampire-timeout") args.vampireTimeout = std::stoi(v);
        else {
            throw std::invalid_argument(
                "Opzione non riconosciuta: '" + f + "'\n"
                "Usa --help per la lista delle opzioni disponibili.");
        }
    }

    // Risoluzione dell'arity
    if (!arityStr.empty())
        args.cfg.arityFilter = parseArityFilter(arityStr, fragment);

    // Costruzione del vocabolario finale 
    args.cfg.vocab = buildVocab(fragment, hasExplicitVocab,
        explicitVocab, args.cfg.arityFilter);

    // Seed
    if (!hasSeed) {
        std::random_device rd;
        args.seed = rd();
    }

    // Controllo --verify e --output
    if (args.verify && args.cfg.output != OutputFormat::TPTP) {
        std::cerr << "[INFO] --verify richiede output TPTP: "
            "--output forzato a 'tptp'.\n";
        args.cfg.output = OutputFormat::TPTP;
    }

    // Validazione semantica
    validateArgs(args);

    return args;
}