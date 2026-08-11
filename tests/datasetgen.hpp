#pragma once
#include "../FragmentTypes.hpp"
#include <fragments/fluted/FlutedGenerator.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

inline void saveDatasetFile(const std::string& path, const std::string& formula) {
    std::ofstream out(path);
    if (out) out << formula;
    else std::cerr << "Error during saving " << path << "\n";
}

inline std::vector<PredInfo> buildVocab(const std::string& s) {
    std::vector<PredInfo> vocab;
    size_t slash = s.find('/');
    if (slash != std::string::npos) {
        int count = std::stoi(s.substr(0, slash));
        int arity = std::stoi(s.substr(slash + 1));
        for (int i = 0; i < count; ++i) {
            vocab.push_back({ "P" + std::to_string(arity) + "_" + std::to_string(i + 1), arity });
        }
    }
    return vocab;
}

// BASE TEMPLATE
template <typename Gen>
void generateDatasetsNative(Gen& gen, const std::string& fragment, int count) {
    std::cout << "No benchmark defined for this fragment: " << fragment << "\n";
}

// FLUTED BENCHMARK
template <>
inline void generateDatasetsNative<FlutedGenerator>(FlutedGenerator& gen, const std::string& /*fragment*/, int count) {
    std::string baseDir = "./fluted_datasets";
    std::cout << "\nGENERATING FLUTED DATASETS | COUNT=" << count << "\n";

    fs::remove_all(baseDir);
    fs::create_directories(baseDir + "/arityscaling");
    fs::create_directories(baseDir + "/depthscaling");
    fs::create_directories(baseDir + "/quantifierscaling");
    fs::create_directories(baseDir + "/branchingscaling");

    std::vector<GenMode> modes = { GenMode::UNSAT, GenMode::SAT, GenMode::FREE };
    auto modeStr = [](GenMode m) { return m == GenMode::UNSAT ? "unsat" : (m == GenMode::SAT ? "sat" : "free"); };

    // ARITY SCALING
    std::vector<std::string> predsList = { "10/2", "6/3", "4/4", "3/5", "2/6" };
    for (auto mode : modes) {
        for (const auto& preds : predsList) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = 10;
                cfg.vocab = buildVocab(preds);

                std::string safePreds = preds;
                std::replace(safePreds.begin(), safePreds.end(), '/', '_');
                std::string path = baseDir + "/arityscaling/" + modeStr(mode) + "_p" + safePreds + "_" + std::to_string(i) + ".p";
                saveDatasetFile(path, gen.generateFormatted(cfg));
            }
        }
    }

    // DEPTH SCALING
    std::vector<int> depths = { 5, 8, 12, 16, 20 };
    for (auto mode : modes) {
        for (int d : depths) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = d;
                cfg.vocab = buildVocab("5/3");

                std::string path = baseDir + "/depthscaling/" + modeStr(mode) + "_d" + std::to_string(d) + "_" + std::to_string(i) + ".p";
                saveDatasetFile(path, gen.generateFormatted(cfg));
            }
        }
    }

    // QUANTIFIER SCALING
    std::vector<int> quantBudgets = { 4, 8, 12, 16 };
    for (auto mode : modes) {
        for (int qb : quantBudgets) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = 12;
                cfg.vocab = buildVocab("4/4");

                cfg.budget.forall_count = { qb, qb };
                cfg.budget.exists_count = { qb, qb };

                std::string path = baseDir + "/quantifierscaling/" + modeStr(mode) + "_q" + std::to_string(qb) + "_" + std::to_string(i) + ".p";
                saveDatasetFile(path, gen.generateFormatted(cfg));
            }
        }
    }

    // BRANCHING SCALING
    std::vector<int> branchBudgets = { 5, 9, 13, 17 };
    for (auto mode : modes) {
        for (int bb : branchBudgets) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = 12;
                cfg.vocab = buildVocab("5/3");

              
                // AND ODD
                cfg.budget.and_count = { bb, bb };
                // OR EVEN 
                cfg.budget.or_count = { bb + 1, bb + 1 };

                std::string path = baseDir + "/branchingscaling/" + modeStr(mode) + "_branch" + std::to_string(bb) + "_" + std::to_string(i) + ".p";
                saveDatasetFile(path, gen.generateFormatted(cfg));
            }
        }
    }
}