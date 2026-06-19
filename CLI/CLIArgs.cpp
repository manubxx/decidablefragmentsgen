#include "CLIArgs.hpp"
#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#include <fstream>
#include <map>
#include <fragments/fo2/FO2vocab.hpp>
#include <fragments/fluted/FLvocab.hpp>
#include <fragments/guarded/GFvocab.hpp>
#include <fragments/unarynegation/UNvocab.hpp>

namespace {

    // generates a predicate name based on its arity
    std::string predName(int arity, int index)
    {
        if (arity >= 1 && arity <= 25) {
            char letter = static_cast<char>('A' + (arity - 1));
            return std::string(1, letter) + std::to_string(index);
        }
        return "P" + std::to_string(arity) + "_" + std::to_string(index);
    }

    // command --mode
    GenMode parseMode(const std::string& s)
    {
        if (s == "sat")   return GenMode::SAT;
        if (s == "unsat") return GenMode::UNSAT;
        if (s == "free")  return GenMode::FREE;
        if (s == "satbuild")  return GenMode::SATBUILD;
        throw std::invalid_argument("Unknown mode: '" + s + "' (allowed: free, sat, unsat, satbuild(ONLY FO2)");
    }

    // command --transform
    TransformMode parseTransform(const std::string& s)
    {
        if (s == "nnf") return TransformMode::NNF;
        throw std::invalid_argument("Unknown transform. (allowed: nnf)");
    }

    // command --output
    OutputFormat parseOutput(const std::string& s)
    {
        if (s == "default") return OutputFormat::DEFAULT;
        if (s == "tptp")    return OutputFormat::TPTP;
        throw std::invalid_argument("Unknown output format: '" + s + "' (allowed: default, tptp)");
    }

    // command --arity
    int parseArityFilter(const std::string& val)
    {
        if (val == "mixed") return 0; // mixed mode

        int filter = std::stoi(val);
        if (filter < 1) { throw std::invalid_argument("--arity must be >= 1"); }
        return filter;
    }

    // Vocab based on --arity filter
    std::vector<PredInfo> filterVocab(const std::vector<PredInfo>& base, int arityFilter)
    {
        if (arityFilter == 0) return base; // mixed mode

        std::vector<PredInfo> result;
        for (const auto& p : base) {
            if (p.arity == arityFilter) {
                result.push_back(p);
            }
        }
        return result;
    }

    // command --preds <count/arity>
    std::vector<PredInfo> parsePreds(const std::string& s, const std::string& fragment)
    {
        if (s.empty()) throw std::invalid_argument("--preds: empty specification");

        std::vector<PredInfo> vocab;
        std::map<int, int> mapPreds; // key: arity, value: count

        size_t iterator = 0;
        while (iterator < s.size()) {
            size_t separator = s.find(',', iterator);
            std::string token = s.substr(iterator, separator - iterator);
            iterator = (separator == std::string::npos) ? s.size() : separator + 1;

            if (token.empty()) continue;

            size_t slash = token.find('/');
            if (slash == std::string::npos) {
                throw std::invalid_argument("--preds: invalid format. Expected <count>/<arity>");
            }

            int count = std::stoi(token.substr(0, slash));
            int arity = std::stoi(token.substr(slash + 1));

            if (count < 1 || arity < 1) {
                throw std::invalid_argument("--preds: count and arity must be >= 1");
            }
            if (fragment == "fo2" && arity > 2) {
                throw std::invalid_argument("--preds: FO2 only allows predicates of arity 1 or 2.");
            }

            // automatic mapping
            int startIdx = mapPreds[arity] + 1;
            mapPreds[arity] += count;
            for (int i = 0; i < count; ++i) {
                vocab.push_back({ predName(arity, startIdx + i), arity });
            }
        }

        if (vocab.empty()) throw std::invalid_argument("--preds: empty specification");
        return vocab;
    }

    // command --help
    void printUsage() {
        std::ifstream file("usage.txt");
        if (!file.is_open()) {
            std::cerr << "Error: Cannot find 'usage.txt'.\n";
            return;
        }
        std::cerr << file.rdbuf();
    }

    // parses the exact value or min:max interval of connectives constraints (--and, --or,...)
    BudgetRange parseRange(const std::string& s)
    {
        auto separator = s.find(':');
        if (separator == std::string::npos) {
            int exactValue = std::stoi(s);
            if (exactValue < 0) throw std::invalid_argument("Negative budget value: " + s);
            return { exactValue, exactValue };
        }
        int lo = std::stoi(s.substr(0, separator));
        int hi = std::stoi(s.substr(separator + 1));
        BudgetRange rangeValue{ lo, hi };
        rangeValue.validate("range");
        return rangeValue;
    }

} // namespace closed


// Main parsing function
AppArgs parseArgs(int argc, char* argv[])
{
    AppArgs args;

    // Default benchmark session values.
    args.fragment = "fo2";
    args.count = 1;
    args.seed = 0;
    args.verify = false;
    args.vampirePath = "vampire";
    args.vampireTimeout = 10;

    std::vector<PredInfo> newVocab;
    bool hasExplicitVocab = false;
    int arityFilter = 0;
    bool hasSeed = false;


    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help") {
            printUsage();
            throw HelpRequest{};
        }
        if (arg == "--verify") {
            args.verify = true;
            continue;
        }

        // Safety: next commands require an argument
        if (i + 1 >= argc) {
            throw std::invalid_argument("Flag '" + arg + "' requires an argument");
        }

        std::string val = argv[++i];

        if (arg == "--fragment") {
            if (val != "fo2" && val != "fluted" && val != "guarded" && val!="unaryneg")
                throw std::invalid_argument("Unsupported fragment: '" + val + "' (allowed: fo2, fluted, guarded, unaryneg)");
            args.fragment = val; 
        }

        else if (arg == "--mode")            args.cfg.mode = parseMode(val);
        else if (arg == "--depth")           args.cfg.depth = std::stoi(val);
        else if (arg == "--count")           args.count = std::stoi(val);
        else if (arg == "--domain-size")     args.cfg.domainSize = std::stoi(val);
        else if (arg == "--transform")       args.cfg.transform = parseTransform(val);
        else if (arg == "--output")          args.cfg.output = parseOutput(val);
        else if (arg == "--vampire-path")    args.vampirePath = val;
        else if (arg == "--vampire-timeout") args.vampireTimeout = std::stoi(val);
        else if (arg == "--seed")          { args.seed = static_cast<unsigned>(std::stoul(val)); hasSeed = true; }
        else if (arg == "--arity")           arityFilter = parseArityFilter(val);
        else if (arg == "--preds")         { newVocab = parsePreds(val, args.fragment); hasExplicitVocab = true; }
        else if (arg == "--and")             args.cfg.budget.and_count = parseRange(val);
        else if (arg == "--or")              args.cfg.budget.or_count = parseRange(val);
        else if (arg == "--not")             args.cfg.budget.not_count = parseRange(val);
        else if (arg == "--exists")          args.cfg.budget.exists_count = parseRange(val);
        else if (arg == "--forall")          args.cfg.budget.forall_count = parseRange(val);
        else if (arg == "--implies")         args.cfg.budget.implies_count = parseRange(val);
        else if (arg == "--eq")              args.cfg.budget.eq_count = parseRange(val);
        else if (arg == "--test") {          args.testMode = val;}

        else {
            throw std::invalid_argument("Unknown option: '" + arg + "'. Use --help.");
        }
    }

    if (!hasExplicitVocab) {
        if (args.fragment == "fo2") { newVocab = kVocabFO2; }
        else if (args.fragment == "fluted") { newVocab = kVocabFL; }
        else if (args.fragment == "guarded") { newVocab = kVocabGF; }
        else if (args.fragment == "unaryneg") { newVocab = kVocabUN; }
    }

    // filter vocabulary
    args.cfg.vocab = filterVocab(newVocab, arityFilter);
    args.cfg.arityFilter = arityFilter;

    // random seed generation if omitted
    if (!hasSeed) {
        std::random_device rd;
        args.seed = rd();
    }

    // consistency check satbuild(only FO2)
    if (args.cfg.mode == GenMode::SATBUILD && args.fragment != "fo2")
        throw std::invalid_argument("--mode satbuild is only available for --fragment fo2");

    // consistency check vampire output
    if (args.verify && args.cfg.output != OutputFormat::TPTP) {
        std::cerr << "[INFO] --verify requires TPTP output format. Forcing output to 'tptp'.\n";
        args.cfg.output = OutputFormat::TPTP;
    }


    // Semantic validation
    if (args.cfg.depth < 0)       throw std::invalid_argument("--depth must be >= 0");
    if (args.count <= 0)          throw std::invalid_argument("--count must be >= 1");
    if (args.cfg.domainSize < 0)  throw std::invalid_argument("--domain-size must be >= 0");
    if (args.vampireTimeout <= 0) throw std::invalid_argument("--vampire-timeout must be > 0");
    if (args.cfg.vocab.empty())   throw std::invalid_argument("The resulting vocabulary is empty.");
    args.cfg.budget.validate();

    return args;
}