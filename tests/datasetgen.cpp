#include "datasetgen.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <fragments/fo2/sat/FO2SATGenerator.hpp>
#pragma once
namespace fs = std::filesystem;

void saveDatasetFile(const std::string& path, const std::string& formula) {
    std::ofstream out(path);
    if (out) out << formula;
    else std::cerr << "Error during saving " << path << "\n";
}

#include <map>

std::vector<PredInfo> buildVocab(const std::string& s) {
    std::vector<PredInfo> vocab;
    std::map<int, int> mapPreds; // Traccia l'indice di partenza per ogni arietà
    size_t iterator = 0;

    while (iterator < s.size()) {
        size_t separator = s.find(',', iterator);
        std::string token = s.substr(iterator, separator == std::string::npos ? s.size() - iterator : separator - iterator);
        iterator = (separator == std::string::npos) ? s.size() : separator + 1;

        if (token.empty()) continue;

        size_t slash = token.find('/');
        if (slash != std::string::npos) {
            int count = std::stoi(token.substr(0, slash));
            int arity = std::stoi(token.substr(slash + 1));

            int startIdx = mapPreds[arity] + 1;
            mapPreds[arity] += count;

            for (int i = 0; i < count; ++i) {
                vocab.push_back({ "p" + std::to_string(arity) + "_" + std::to_string(startIdx + i), arity });
            }
        }
    }
    return vocab;
}

// BASE TEMPLATE
template <typename Gen>
void generateDatasetsNative(Gen& gen, const std::string& fragment, int count) {
    std::cout << "No benchmark defined for this fragment: " << fragment << "\n";
}

// ---------------------------------------------------------
// FLUTED FRAGMENT

template <>
void generateDatasetsNative<FlutedGenerator>(FlutedGenerator& gen, const std::string& /*fragment*/, int count) {
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
                cfg.vocab = buildVocab("10/1,5/2,2/3");

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
                cfg.vocab = buildVocab("5/2,3/3,2/4");

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
                cfg.vocab = buildVocab("20/1,10/2");


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


// ---------------------------------------------------------
// FO2 FRAGMENT
template <>
void generateDatasetsNative<FO2Generator>(FO2Generator& gen, const std::string& /*fragment*/, int count) {
    std::string baseDir = "./fo2_datasets";
    std::cout << "\nGENERATING FO2 DATASETS | COUNT=" << count << "\n";

    fs::remove_all(baseDir);
    fs::create_directories(baseDir + "/depthscaling");
    fs::create_directories(baseDir + "/vocabscaling");
    fs::create_directories(baseDir + "/quantifierscaling");
    fs::create_directories(baseDir + "/equalityscaling");
    fs::create_directories(baseDir + "/satbuildscaling");

    // Modalità FREE rimossa per evitare gli Unknown da "mancanza di congettura" in CASC
    std::vector<GenMode> modes = { GenMode::UNSAT, GenMode::SAT };
    auto modeStr = [](GenMode m) { return m == GenMode::UNSAT ? "unsat" : "sat"; };

    auto safeGenerateAndSave = [&](const std::string& path, GenConfig& cfg, FO2Generator& generator) {
        bool success = false;
        int max_retries = 1000;
        while (!success && max_retries > 0) {
            try {
                saveDatasetFile(path, generator.generateFormatted(cfg));
                success = true;
            }
            catch (...) {
                max_retries--;
            }
        }
        if (!success) std::cout << "Skipped formula (too hard to fit the budget)\n";
        };

    // 1. VOCAB SCALING
    std::vector<std::string> predsList = { "10/1", "5/2", "10/2", "15/2" };
    for (auto mode : modes) {
        for (const auto& preds : predsList) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = 12;
                cfg.vocab = buildVocab(preds);

                cfg.budget.exists_count = { 2, 2 };
                cfg.budget.forall_count = { 2, 2 };

                std::string safePreds = preds;
                std::replace(safePreds.begin(), safePreds.end(), '/', '_');
                std::string path = baseDir + "/vocabscaling/" + modeStr(mode) + "_p" + safePreds + "_" + std::to_string(i) + ".p";
                safeGenerateAndSave(path, cfg, gen);
            }
        }
    }

    // 2. DEPTH SCALING
    std::vector<int> depths = { 5, 8, 12, 16 };
    for (auto mode : modes) {
        for (int d : depths) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = d;
                cfg.vocab = buildVocab("15/1,10/2");

                cfg.budget.exists_count = { 2, 2 };
                cfg.budget.forall_count = { 2, 2 };

                std::string path = baseDir + "/depthscaling/" + modeStr(mode) + "_d" + std::to_string(d) + "_" + std::to_string(i) + ".p";
                safeGenerateAndSave(path, cfg, gen);
            }
        }
    }

    // 3. QUANTIFIER SCALING
    std::vector<int> quantBudgets = { 2, 4, 6, 8 };
    for (auto mode : modes) {
        for (int qb : quantBudgets) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = 18;
                cfg.vocab = buildVocab("20/1,5/2");

                cfg.budget.forall_count = { qb, qb };
                cfg.budget.exists_count = { qb, qb };

                std::string path = baseDir + "/quantifierscaling/" + modeStr(mode) + "_q" + std::to_string(qb) + "_" + std::to_string(i) + ".p";
                safeGenerateAndSave(path, cfg, gen);
            }
        }
    }

    // 4. EQUALITY SCALING
    std::vector<int> eqBudgets = { 2, 4, 6, 8 };
    for (auto mode : modes) {
        for (int eq : eqBudgets) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = 18;
                cfg.vocab = buildVocab("5/1,15/2");

                cfg.budget.eq_count = { eq, eq };
                cfg.budget.exists_count = { 2, 2 };
                cfg.budget.forall_count = { 2, 2 };

                std::string path = baseDir + "/equalityscaling/" + modeStr(mode) + "_eq" + std::to_string(eq) + "_" + std::to_string(i) + ".p";
                safeGenerateAndSave(path, cfg, gen);
            }
        }
    }

    // 5. SATBUILD DOMAIN SCALING
    std::vector<int> satbuildDomains = { 1, 2, 3 };
    for (int ds : satbuildDomains) {
        for (int i = 1; i <= count; ++i) {
            GenConfig cfg;
            cfg.mode = GenMode::SATBUILD;
            cfg.output = OutputFormat::TPTP;
            cfg.depth = 8;
            cfg.vocab = buildVocab("2/1,1/2");
            cfg.domainSize = ds;

            cfg.budget.exists_count = { 2, 2 };
            cfg.budget.forall_count = { 2, 2 };

            unsigned dynamicSeed = 12345 + (ds * 100) + i;
            FO2SATGenerator satGen(cfg.vocab, dynamicSeed);
            std::string path = baseDir + "/satbuildscaling/satbuild_dom" + std::to_string(ds) + "_" + std::to_string(i) + ".p";

            bool success = false;
            int max_retries = 1000;
            while (!success && max_retries > 0) {
                try {
                    saveDatasetFile(path, satGen.generateFormatted(cfg));
                    success = true;
                }
                catch (...) { max_retries--; }
            }
            if (success) {
                std::cout << "SATBUILD: Created domain: " << ds << " (Iteration " << i << ")...\n";
            }
        }
    }


}


// ---------------------------------------------------------
// GUARDED FRAGMENT
template <>
void generateDatasetsNative<GuardedGenerator>(GuardedGenerator& gen, const std::string& /*fragment*/, int count) {
    std::string baseDir = "./guarded_datasets";
    std::cout << "\nGENERATING GUARDED DATASETS | COUNT=" << count << "\n";

    fs::remove_all(baseDir);
    fs::create_directories(baseDir + "/depthscaling");
    fs::create_directories(baseDir + "/alternationscaling");
    fs::create_directories(baseDir + "/branchingscaling");
    fs::create_directories(baseDir + "/equalityscaling");

    std::vector<GenMode> modes = { GenMode::UNSAT, GenMode::SAT };
    auto modeStr = [](GenMode m) { return m == GenMode::UNSAT ? "unsat" : "sat"; };

    auto safeGenerateAndSave = [&](const std::string& path, GenConfig& cfg, GuardedGenerator& generator) {
        bool success = false;
        int max_retries = 1000;
        while (!success && max_retries > 0) {
            try {
                saveDatasetFile(path, generator.generateFormatted(cfg));
                success = true;
            }
            catch (...) {
                max_retries--;
            }
        }
        if (!success) std::cout << "Skipped formula (too hard to fit the budget) for: " << path << "\n";
        };

    std::string defaultVocab = "20/1";

    // 1. DEPTH SCALING 
    std::vector<int> depths = { 5, 10, 15, 20 };
    for (auto mode : modes) {
        for (int d : depths) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = d;
                cfg.vocab = buildVocab(defaultVocab);

                std::string path = baseDir + "/depthscaling/" + modeStr(mode) + "_d" + std::to_string(d) + "_" + std::to_string(i) + ".p";
                safeGenerateAndSave(path, cfg, gen);
            }
        }
    }

    // 2. QUANTIFIER ALTERNATION SCALING 
    std::vector<int> altBudgets = { 2, 4, 6, 8 };
    for (auto mode : modes) {
        for (int qb : altBudgets) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = 15;
                cfg.vocab = buildVocab(defaultVocab);

                cfg.budget.exists_count = { qb, qb };
                cfg.budget.forall_count = { qb, qb };

                std::string path = baseDir + "/alternationscaling/" + modeStr(mode) + "_alt" + std::to_string(qb) + "_" + std::to_string(i) + ".p";
                safeGenerateAndSave(path, cfg, gen);
            }
        }
    }

    // 3. BRANCHING SCALING (Propositional Blowup) 
    std::vector<int> branchBudgets = { 10, 20, 30, 40 };
    for (auto mode : modes) {
        for (int bb : branchBudgets) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = 15;
                cfg.vocab = buildVocab("30/1,10/2");

                cfg.budget.and_count = { bb / 2, bb };
                cfg.budget.or_count = { bb / 2, bb };
                cfg.budget.implies_count = { 0, bb / 2 };

                cfg.budget.exists_count = { 2, 6 };
                cfg.budget.forall_count = { 2, 6 };

                std::string path = baseDir + "/branchingscaling/" + modeStr(mode) + "_branch" + std::to_string(bb) + "_" + std::to_string(i) + ".p";
                safeGenerateAndSave(path, cfg, gen);
            }
        }
    }

    // 4. EQUALITY SCALING
    std::vector<int> eqBudgets = { 2, 4, 6, 8 };
    for (auto mode : modes) {
        for (int eq : eqBudgets) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = 12;
                cfg.vocab = buildVocab(defaultVocab);

                cfg.budget.eq_count = { eq / 2, eq };
                cfg.budget.exists_count = { 1, 4 };
                cfg.budget.forall_count = { 1, 4 };

                std::string path = baseDir + "/equalityscaling/" + modeStr(mode) + "_eq" + std::to_string(eq) + "_" + std::to_string(i) + ".p";
                safeGenerateAndSave(path, cfg, gen);
            }
        }
    }
}



// ---------------------------------------------------------
// UNARY NEGATION FRAGMENT (UNFO)
template <>
void generateDatasetsNative<UnaryNegGenerator>(UnaryNegGenerator& gen, const std::string& /*fragment*/, int count) {
    std::string baseDir = "./unaryneg_datasets";
    std::cout << "\nGENERATING UNFO DATASETS | COUNT=" << count << "\n";

    fs::remove_all(baseDir);
    fs::create_directories(baseDir + "/depthscaling");
    fs::create_directories(baseDir + "/quantifierscaling");
    fs::create_directories(baseDir + "/negationscaling"); 
    fs::create_directories(baseDir + "/equalityscaling");

    std::vector<GenMode> modes = { GenMode::UNSAT, GenMode::SAT };
    auto modeStr = [](GenMode m) { return m == GenMode::UNSAT ? "unsat" : "sat"; };

    auto safeGenerateAndSave = [&](const std::string& path, GenConfig& cfg, UnaryNegGenerator& generator) {
        bool success = false;
        int max_retries = 1000;
        while (!success && max_retries > 0) {
            try {
                saveDatasetFile(path, generator.generateFormatted(cfg));
                success = true;
            }
            catch (...) { max_retries--; }
        }
        if (!success) std::cout << "Skipped formula (too hard to fit the budget) for: " << path << "\n";
        };

   
    std::string defaultVocab = "15/1,10/2,5/3";

    // 1. DEPTH SCALING
    std::vector<int> depths = { 5, 10, 15, 20 };
    for (auto mode : modes) {
        for (int d : depths) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = d;
                cfg.vocab = buildVocab(defaultVocab);

         
                cfg.budget.not_count = { 1, 4 };
                cfg.budget.exists_count = { 1, 4 };
                cfg.budget.forall_count = { 1, 4 };

                std::string path = baseDir + "/depthscaling/" + modeStr(mode) + "_d" + std::to_string(d) + "_" + std::to_string(i) + ".p";
                safeGenerateAndSave(path, cfg, gen);
            }
        }
    }

    // 2. QUANTIFIER ALTERNATION SCALING
    std::vector<int> quantBudgets = { 2, 4, 6, 8 };
    for (auto mode : modes) {
        for (int qb : quantBudgets) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = 15;
                cfg.vocab = buildVocab(defaultVocab);

                cfg.budget.exists_count = { qb, qb + 2 };
                cfg.budget.forall_count = { qb, qb + 2 };
                cfg.budget.not_count = { 2, 4 };

                std::string path = baseDir + "/quantifierscaling/" + modeStr(mode) + "_q" + std::to_string(qb) + "_" + std::to_string(i) + ".p";
                safeGenerateAndSave(path, cfg, gen);
            }
        }
    }

    // 3. NEGATION SCALING
 
    std::vector<int> negBudgets = { 4, 8, 12, 16 };
    for (auto mode : modes) {
        for (int nb : negBudgets) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = 15;
               
                cfg.vocab = buildVocab("30/1,5/2");

                cfg.budget.not_count = { nb / 2, nb };
              
                cfg.budget.exists_count = { 3, 6 };
                cfg.budget.forall_count = { 3, 6 };

                std::string path = baseDir + "/negationscaling/" + modeStr(mode) + "_neg" + std::to_string(nb) + "_" + std::to_string(i) + ".p";
                safeGenerateAndSave(path, cfg, gen);
            }
        }
    }

    // 4. EQUALITY SCALING
    std::vector<int> eqBudgets = { 2, 4, 6, 8 };
    for (auto mode : modes) {
        for (int eq : eqBudgets) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = 15;
                cfg.vocab = buildVocab(defaultVocab);

                cfg.budget.eq_count = { eq / 2, eq };
                cfg.budget.not_count = { 2, 5 };
                cfg.budget.exists_count = { 2, 5 };
                cfg.budget.forall_count = { 2, 5 };

                std::string path = baseDir + "/equalityscaling/" + modeStr(mode) + "_eq" + std::to_string(eq) + "_" + std::to_string(i) + ".p";
                safeGenerateAndSave(path, cfg, gen);
            }
        }
    }
}