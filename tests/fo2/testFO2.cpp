#include "testFO2.hpp"
#include "../../fragments/fo2/FO2Generator.hpp"
#include "../../fragments/fo2/sat/FO2SATGenerator.hpp"
#include "FragmentTypes.hpp"
#include <iostream>
#include <string>
#include <vector>

namespace tests {

    static void printTest(const std::string& name, bool passed) {
        std::cout << "[TEST] " << name << " : " << (passed ? "Correct" : "Failed") << "\n";
    }

    // TEST 1: depth limits
    static void test1_depthlimits() {

        std::vector<PredInfo> vocab = { {"P", 1}, {"Q", 2} };
        FO2Generator gen(vocab, 42);

        GenConfig cfg;
        cfg.mode = GenMode::FREE;

        // 1. atomic limit
        {
            cfg.depth = 0;
            std::string generatedFormula = gen.generateFormatted(cfg);
            bool isAtomic = (generatedFormula.find('&') == std::string::npos &&
                generatedFormula.find('|') == std::string::npos &&
                generatedFormula.find('~') == std::string::npos &&
                generatedFormula.find('!') == std::string::npos &&
                generatedFormula.find('?') == std::string::npos);

            printTest("FO2Test1 Depth 0", isAtomic && !generatedFormula.empty());
        }

        // 2. nominal allocation
        {
            cfg.depth = 5;
            std::string generatedFormula;
            bool Success = true;
            try { generatedFormula = gen.generateFormatted(cfg); }
            catch (...) { Success = false; }

            printTest("FO2Test2 Depth 5", Success && !generatedFormula.empty());
        }

        // 3. stress allocation
        {
            cfg.depth = 12;
            std::string generatedFormula;
            bool Success = true;
            try { generatedFormula = gen.generateFormatted(cfg); }
            catch (...) { Success = false; }

            printTest("FO2Test3 Depth 12", Success && !generatedFormula.empty());
        }
    }

    // TEST 2: budget limits
    static void test2_budgetlimits() {
        std::vector<PredInfo> vocab = { {"A", 1}, {"B", 2} };
        FO2Generator gen(vocab, 42);

        GenConfig cfg;
        cfg.mode = GenMode::FREE;
        cfg.depth = 3;

        // In the AST with d depth, the max number of nodes is (2^d) - 1. 
        // If depth = 3, limit is (2^3) - 1 = 7.
        int maxNodes = (1 << cfg.depth) - 1;
        int boundaryLimit = maxNodes + 1; // 8 nodes

        cfg.budget.and_count = { boundaryLimit, boundaryLimit };

        bool expectedError = false;
        try {
            gen.generateFormatted(cfg);
        }
        catch (const std::invalid_argument& e) {
            std::string msg = e.what();
            if (msg.find("Cannot satisfy the requested budget") != std::string::npos) {
                expectedError = true;
            }
        }
        catch (...) {
            expectedError = false;
        }

        printTest("FO2Test4 Budget Constraint", expectedError);
    }

    // TEST 3: arity filter limits
    static void test3_arityfilterlimits() {

        // predicates with arity > 2 are forbidden in FO2
        bool arityError = false;
        try {
            std::vector<PredInfo> badVocab = { {"P", 3} }; // arity 3
            FO2Generator gen(badVocab, 42);
        }
        catch (const std::invalid_argument& e) {
            std::string msg = e.what();
            if (msg.find("arity <1 or >2") != std::string::npos) arityError = true;
        }
        catch (...) {}
        printTest("FO2Test5 Arity", arityError);
 
        // the arity filter must match the active vocabulary
        std::vector<PredInfo> vocab = { {"R", 2} }; // arity 2
        FO2Generator gen(vocab, 42);

        GenConfig cfg;
        cfg.mode = GenMode::FREE;
        cfg.arityFilter = 1; // forced arity 1

        bool filterError = false;
        try {
            gen.generateFormatted(cfg);
        }
        catch (const std::invalid_argument& e) {
            std::string msg = e.what();
            if (msg.find("no predicates with arity") != std::string::npos) filterError= true;
        }
        catch (...) {}
        printTest("FO2Test6 Filter", filterError);
    }

    // TEST 4: SATBUILD logic
    static void test4_satbuildlogic() {

        std::vector<PredInfo> vocab = { {"P", 1}, {"R", 2} };
        FO2SATGenerator gen(vocab, 42);

        GenConfig cfg;
        cfg.mode = GenMode::SATBUILD;

        // basic allocation
        {
            cfg.depth = 2;
            cfg.domainSize = 1;
            std::string generatedFormula;
            bool Success = true;
            try { generatedFormula = gen.generateFormatted(cfg); }
            catch (...) { Success = false; }
            printTest("FO2SATTest7 SATBUILD", Success && !generatedFormula.empty());
        }

        // fallback allocation
        {
            cfg.depth = 2;
            cfg.domainSize = 0;
            std::string generatedFormula;
            bool Success = true;
            try { generatedFormula = gen.generateFormatted(cfg); }
            catch (...) { Success = false; }
            printTest("FO2SATTest8 SATBUILD Fallback ", Success && !generatedFormula.empty());
        }

        // stress limit allocation
        {
            cfg.depth = 4;
            cfg.domainSize = 10;
            std::string generatedFormula;
            bool Success = true;
            try { generatedFormula = gen.generateFormatted(cfg); }
            catch (...) { Success = false; }
            printTest("FO2SATTest9 SATBUILD Max Limit", Success && !generatedFormula.empty());
        }
    }

    void runFO2Tests() {
        std::cout << "\n=== STARTING TEST SUITE: FO2 ===\n\n";

        test1_depthlimits();
        test2_budgetlimits();
        test3_arityfilterlimits();
        test4_satbuildlogic();

        std::cout << "\n=== END TEST SUITE: FO2 ===\n\n";
    }
}