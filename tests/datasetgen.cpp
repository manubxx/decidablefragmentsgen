#include "datasetgen.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <fragments/fo2/sat/FO2SATGenerator.hpp>
#include <map>

namespace fs = std::filesystem;

void saveDatasetFile(const std::string& path, const std::string& formula) {
    std::ofstream out(path);
    if (out) out << formula;
    else std::cerr << "Error during saving " << path << "\n";
}

std::vector<PredInfo> buildVocab(const std::string& s) {
    std::vector<PredInfo> vocab;
    std::map<int, int> mapPreds;
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

    std::vector<GenMode> modes = { GenMode::UNSAT, GenMode::SAT };
    auto modeStr = [](GenMode m) { return m == GenMode::UNSAT ? "unsat" : (m == GenMode::SAT ? "sat" : "free"); };

    auto safeGenerateAndSave = [&](const std::string& path, GenConfig& cfg, FlutedGenerator& generator) {
        bool success = false;
        int max_retries = 1000;
        int attempts = 0;
        while (!success && max_retries > 0) {
            attempts++;
            try {
                std::string formula = generator.generateFormatted(cfg);
                std::string finalOutput = "% ATTEMPTS: " + std::to_string(attempts) + "\n" + formula;
                saveDatasetFile(path, finalOutput);
                success = true;
            }
            catch (...) {
                max_retries--;
            }
        }
        if (!success) std::cout << "Skipped formula (too hard to fit the budget)\n";
        };

    // 1. DEPTH SCALING
    std::vector<int> depths = { 6, 10, 14, 18, 22 };
    for (auto mode : modes) {
        for (int d : depths) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = d;
                cfg.vocab = buildVocab("10/1,5/2,2/3");

                std::string path = baseDir + "/depthscaling/" + modeStr(mode) + "_d" + std::to_string(d) + "_" + std::to_string(i) + ".p";
                safeGenerateAndSave(path, cfg, gen);
            }
        }
    }

    // 2. QUANTIFIER SCALING
    std::vector<int> quantBudgets = { 4, 8, 12, 16 };
    for (auto mode : modes) {
        for (int qb : quantBudgets) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = 18;
                cfg.vocab = buildVocab("10/2,5/3,3/4");

                cfg.budget.forall_count = { qb, qb };
                cfg.budget.exists_count = { qb, qb };

                std::string path = baseDir + "/quantifierscaling/" + modeStr(mode) + "_q" + std::to_string(qb) + "_" + std::to_string(i) + ".p";
                safeGenerateAndSave(path, cfg, gen);
            }
        }
    }

    // 3. BRANCHING SCALING 
    std::vector<int> branchBudgets = { 6, 10, 14, 18 };
    for (auto mode : modes) {
        for (int bb : branchBudgets) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = 14; 
                cfg.vocab = buildVocab("20/1,10/2");

                cfg.budget.and_count = { std::max(2, bb - 2), bb };
                cfg.budget.or_count = { std::max(2, bb - 1), bb + 1 };

                std::string path = baseDir + "/branchingscaling/" + modeStr(mode) + "_branch" + std::to_string(bb) + "_" + std::to_string(i) + ".p";
                safeGenerateAndSave(path, cfg, gen);
            }
        }
    }

    // ARITY & IFF CROSS-SCALING 
    std::vector<std::string> predsList = { "6/3", "3/5" };
    std::vector<int> iffBudgets = { 4, 6 }; 

    for (auto mode : modes) {
        for (const auto& preds : predsList) {
            for (int iff_val : iffBudgets) {
                for (int i = 1; i <= count; ++i) {
                    GenConfig cfg;
                    cfg.mode = mode;
                    cfg.output = OutputFormat::TPTP;
                    cfg.depth = 22;
                    cfg.vocab = buildVocab(preds);

                    cfg.budget.iff_count = { iff_val, iff_val };

                    std::string safePreds = preds;
                    std::replace(safePreds.begin(), safePreds.end(), '/', '_');
                    std::string path = baseDir + "/arityscaling/" + modeStr(mode) + "_p" + safePreds + "_iff" + std::to_string(iff_val) + "_" + std::to_string(i) + ".p";
                    safeGenerateAndSave(path, cfg, gen);
                }
            }
        }
    }
}

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
    fs::create_directories(baseDir + "/denseequality");

    std::vector<GenMode> modes = { GenMode::UNSAT, GenMode::SAT };
    auto modeStr = [](GenMode m) { return m == GenMode::UNSAT ? "unsat" : "sat"; };

    auto safeGenerateAndSave = [&](const std::string& path, GenConfig& cfg, FO2Generator& generator) {
        bool success = false;
        int max_retries = 1000;
        int attempts = 0;
        while (!success && max_retries > 0) {
            attempts++;
            try {
                std::string formula = generator.generateFormatted(cfg);
                std::string finalOutput = "% ATTEMPTS: " + std::to_string(attempts) + "\n" + formula;
                saveDatasetFile(path, finalOutput);
                success = true;
            }
            catch (...) {
                max_retries--;
            }
        }
        if (!success) std::cout << "Skipped formula (too hard to fit the budget)\n";
        };

    // 1. VOCAB SCALING
    std::vector<std::string> predsList = { "5/1,2/2", "10/1,5/2", "15/1,8/2", "20/1,10/2" };
    for (auto mode : modes) {
        for (const auto& preds : predsList) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = 12;
                cfg.vocab = buildVocab(preds);

                cfg.budget.exists_count = { 4, 6 };
                cfg.budget.forall_count = { 4, 6 };

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

                cfg.budget.exists_count = { 0, 4 };
                cfg.budget.forall_count = { 0, 4 };

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
                cfg.depth = qb * 2 + 10;
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
            cfg.depth = 12;
            cfg.vocab = buildVocab("2/1,1/2");
            cfg.domainSize = ds;

            cfg.budget.exists_count = { 1, 1 };
            cfg.budget.forall_count = { 1, 1 };

            unsigned dynamicSeed = 12345 + (ds * 100) + i;
            FO2SATGenerator satGen(cfg.vocab, dynamicSeed);

            std::string path = baseDir + "/satbuildscaling/satbuild_dom" + std::to_string(ds) + "_" + std::to_string(i) + ".p";

            bool success = false;
            int max_retries = 1000;
            int attempts = 0;
            while (!success && max_retries > 0) {
                attempts++;
                try {
                    std::string formula = satGen.generateFormatted(cfg);
                    std::string finalOutput = "% ATTEMPTS: " + std::to_string(attempts) + "\n" + formula;
                    saveDatasetFile(path, finalOutput);
                    success = true;
                }
                catch (...) { max_retries--; }
            }
            if (success) {
                std::cout << "SATBUILD: Created domain: " << ds << " (Iteration " << i << ")...\n";
            }
        }
    }

    // 6. DENSE EQUALITY & IFF SCALING
    std::vector<int> denseEqBudgets = { 10, 16, 20, 26, 30 };
    for (auto mode : modes) {
        for (int eq : denseEqBudgets) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = 30;
                cfg.vocab = buildVocab("2/1,2/2");

                cfg.budget.eq_count = { eq, eq };
                cfg.budget.iff_count = { eq / 3, eq / 2 };
                cfg.budget.exists_count = { 4, 8 };
                cfg.budget.forall_count = { 4, 8 };
                cfg.budget.or_count = { eq / 2, eq };

                std::string path = baseDir + "/denseequality/" + modeStr(mode) + "_eq" + std::to_string(eq) + "_" + std::to_string(i) + ".p";
                safeGenerateAndSave(path, cfg, gen);
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
        int attempts = 0;
        while (!success && max_retries > 0) {
            attempts++;
            try {
                std::string formula = generator.generateFormatted(cfg);
                std::string finalOutput = "% ATTEMPTS: " + std::to_string(attempts) + "\n" + formula;
                saveDatasetFile(path, finalOutput);
                success = true;
            }
            catch (...) {
                max_retries--;
            }
        }
        if (!success) std::cout << "Skipped formula (too hard to fit the budget) for: " << path << "\n";
        };

    
    std::string richVocab = "20/1,10/2,5/3";

    // 1. DEPTH SCALING (
    std::vector<int> depths = { 10, 18, 26, 34 };
    for (auto mode : modes) {
        for (int d : depths) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = d;
                cfg.vocab = buildVocab(richVocab);

                
                cfg.budget.exists_count = { d / 4, d / 2 };
                cfg.budget.forall_count = { d / 4, d / 2 };
                cfg.budget.and_count = { d / 2, d };
                cfg.budget.or_count = { d / 2, d };

                std::string path = baseDir + "/depthscaling/" + modeStr(mode) + "_d" + std::to_string(d) + "_" + std::to_string(i) + ".p";
                safeGenerateAndSave(path, cfg, gen);
            }
        }
    }

    // 2. QUANTIFIER ALTERNATION SCALING 
    std::vector<int> altBudgets = { 4, 8, 12, 16 }; 
    for (auto mode : modes) {
        for (int qb : altBudgets) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = 22; 
                cfg.vocab = buildVocab(richVocab);

                cfg.budget.exists_count = { qb, qb };
                cfg.budget.forall_count = { qb, qb };

                std::string path = baseDir + "/alternationscaling/" + modeStr(mode) + "_alt" + std::to_string(qb) + "_" + std::to_string(i) + ".p";
                safeGenerateAndSave(path, cfg, gen);
            }
        }
    }

    // 3. BRANCHING SCALING 
    std::vector<int> branchBudgets = { 20, 35, 50, 65 };
    for (auto mode : modes) {
        for (int bb : branchBudgets) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = 30; 
                cfg.vocab = buildVocab("40/1,20/2,10/3"); 

                cfg.budget.and_count = { bb / 2, bb };
                cfg.budget.or_count = { bb / 2, bb };
                cfg.budget.implies_count = { bb / 3, bb };
                cfg.budget.iff_count = { bb / 3, bb };

                cfg.budget.exists_count = { 6, 12 };
                cfg.budget.forall_count = { 6, 12 };

                std::string path = baseDir + "/branchingscaling/" + modeStr(mode) + "_branch" + std::to_string(bb) + "_" + std::to_string(i) + ".p";
                safeGenerateAndSave(path, cfg, gen);
            }
        }
    }

    // 4. EQUALITY SCALING 
    std::vector<int> eqBudgets = { 4, 8, 12, 16 };
    for (auto mode : modes) {
        for (int eq : eqBudgets) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = 18; 
                cfg.vocab = buildVocab(richVocab);

                cfg.budget.eq_count = { eq / 2, eq };
                cfg.budget.exists_count = { 2, 6 };
                cfg.budget.forall_count = { 2, 6 };

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
        int attempts = 0;
        while (!success && max_retries > 0) {
            attempts++;
            try {
                std::string formula = generator.generateFormatted(cfg);
                std::string finalOutput = "% ATTEMPTS: " + std::to_string(attempts) + "\n" + formula;
                saveDatasetFile(path, finalOutput);
                success = true;
            }
            catch (...) { max_retries--; }
        }
        if (!success) std::cout << "Skipped formula (too hard to fit the budget) for: " << path << "\n";
        };

    std::string standardVocab = "15/1,10/2,5/3";

    // 1. DEPTH SCALING
    std::vector<int> depths = { 6, 11, 16, 21 };
    for (auto mode : modes) {
        for (int d : depths) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = d;
                cfg.vocab = buildVocab(standardVocab);

                cfg.budget.not_count = { 1, 3 };
                cfg.budget.exists_count = { 1, 3 };
                cfg.budget.forall_count = { 1, 3 };

                std::string path = baseDir + "/depthscaling/" + modeStr(mode) + "_d" + std::to_string(d) + "_" + std::to_string(i) + ".p";
                safeGenerateAndSave(path, cfg, gen);
            }
        }
    }

    // 2. QUANTIFIER ALTERNATION SCALING
    std::vector<int> quantBudgets = { 2, 5, 8, 11 };
    for (auto mode : modes) {
        for (int qb : quantBudgets) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = 18;
                cfg.vocab = buildVocab(standardVocab);

                cfg.budget.exists_count = { qb, qb + 2 };
                cfg.budget.forall_count = { qb, qb + 2 };
                cfg.budget.not_count = { 2, 4 };

                std::string path = baseDir + "/quantifierscaling/" + modeStr(mode) + "_q" + std::to_string(qb) + "_" + std::to_string(i) + ".p";
                safeGenerateAndSave(path, cfg, gen);
            }
        }
    }

    // 3. NEGATION SCALING
    std::vector<int> negBudgets = { 6, 10, 14, 18 };
    for (auto mode : modes) {
        for (int nb : negBudgets) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = 20;
                cfg.vocab = buildVocab("10/1,5/2");

                cfg.budget.not_count = { std::max(2, nb - 2), nb };
                cfg.budget.exists_count = { nb / 2, nb };
                cfg.budget.forall_count = { nb / 2, nb };

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
                cfg.depth = 16;
                cfg.vocab = buildVocab(standardVocab);

                cfg.budget.eq_count = { eq / 2, eq };
                cfg.budget.not_count = { 2, 4 };
                cfg.budget.exists_count = { 2, 4 };
                cfg.budget.forall_count = { 2, 4 };

                std::string path = baseDir + "/equalityscaling/" + modeStr(mode) + "_eq" + std::to_string(eq) + "_" + std::to_string(i) + ".p";
                safeGenerateAndSave(path, cfg, gen);
            }
        }
    }
}
// ---------------------------------------------------------
// MODAL FRAGMENT
template <>
void generateDatasetsNative<ModalGenerator>(ModalGenerator& gen, const std::string& /*fragment*/, int count) {
    std::string baseDir = "./modal_datasets";
    std::cout << "\nGENERATING MODAL DATASETS  | COUNT=" << count << "\n";

    fs::remove_all(baseDir);
    fs::create_directories(baseDir + "/modaldepthscaling");
    fs::create_directories(baseDir + "/alternationscaling");
    fs::create_directories(baseDir + "/branchingscaling");
    fs::create_directories(baseDir + "/vocabscaling");

    std::vector<GenMode> modes = { GenMode::UNSAT, GenMode::SAT };
    auto modeStr = [](GenMode m) { return m == GenMode::UNSAT ? "unsat" : "sat"; };

    auto safeGenerateAndSave = [&](const std::string& path, GenConfig& cfg, ModalGenerator& generator) {
        bool success = false;
        int max_retries = 1000;
        int attempts = 0;
        while (!success && max_retries > 0) {
            attempts++;
            try {
                std::string formula = generator.generateFormatted(cfg);
                std::string finalOutput = "% ATTEMPTS: " + std::to_string(attempts) + "\n" + formula;
                saveDatasetFile(path, finalOutput);
                success = true;
            }
            catch (...) { max_retries--; }
        }
        if (!success) std::cout << "Skipped formula (too hard to fit the budget) for: " << path << "\n";
        };

    // 1. MODAL DEPTH SCALING 
    std::vector<int> depths = { 6, 12, 18, 24 };
    for (auto mode : modes) {
        for (int d : depths) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = d;
                cfg.vocab = buildVocab("25/1");

                cfg.budget.exists_count = { d / 3, d / 2 };
                cfg.budget.forall_count = { d / 3, d / 2 };
                cfg.budget.and_count = { d / 2, d };
                cfg.budget.or_count = { d / 2, d };

                std::string path = baseDir + "/modaldepthscaling/" + modeStr(mode) + "_d" + std::to_string(d) + "_" + std::to_string(i) + ".p";
                safeGenerateAndSave(path, cfg, gen);
            }
        }
    }

    // 2. BOX-DIAMOND ALTERNATION SCALING 
    std::vector<int> altBudgets = { 4, 8, 12, 16 };
    for (auto mode : modes) {
        for (int qb : altBudgets) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = 18; 
                cfg.vocab = buildVocab("20/1");

                cfg.budget.exists_count = { qb, qb };
                cfg.budget.forall_count = { qb, qb };
                cfg.budget.and_count = { 4, 8 };
                cfg.budget.or_count = { 4, 8 };
                cfg.budget.iff_count = { 2, 4 };

                std::string path = baseDir + "/alternationscaling/" + modeStr(mode) + "_alt" + std::to_string(qb) + "_" + std::to_string(i) + ".p";
                safeGenerateAndSave(path, cfg, gen);
            }
        }
    }

    // 3. PROPOSITIONAL BRANCHING SCALING
    std::vector<int> branchBudgets = { 8, 16, 24, 32 };
    for (auto mode : modes) {
        for (int bb : branchBudgets) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = 14;
                cfg.vocab = buildVocab("30/1");

                cfg.budget.and_count = { bb / 2, bb };
                cfg.budget.or_count = { bb / 2, bb };
                cfg.budget.implies_count = { 2, bb / 2 };
                cfg.budget.exists_count = { 3, 6 };
                cfg.budget.forall_count = { 3, 6 };

                std::string path = baseDir + "/branchingscaling/" + modeStr(mode) + "_branch" + std::to_string(bb) + "_" + std::to_string(i) + ".p";
                safeGenerateAndSave(path, cfg, gen);
            }
        }
    }

    // 4. VOCABULARY SCALING 
    std::vector<std::string> vocabSizes = { "10/1", "25/1", "40/1", "60/1" };
    for (auto mode : modes) {
        for (const auto& v : vocabSizes) {
            for (int i = 1; i <= count; ++i) {
                GenConfig cfg;
                cfg.mode = mode;
                cfg.output = OutputFormat::TPTP;
                cfg.depth = 12;
                cfg.vocab = buildVocab(v);

                cfg.budget.exists_count = { 4, 8 };
                cfg.budget.forall_count = { 4, 8 };
                cfg.budget.and_count = { 6, 12 };
                cfg.budget.or_count = { 6, 12 };

                std::string safeV = v;
                std::replace(safeV.begin(), safeV.end(), '/', '_');
                std::string path = baseDir + "/vocabscaling/" + modeStr(mode) + "_v" + safeV + "_" + std::to_string(i) + ".p";
                safeGenerateAndSave(path, cfg, gen);
            }
        }
    }
}