#pragma once
#include "fragments/fo2/FO2Generator.hpp"
#include "models/FiniteModel.hpp"
#include "FO2Evaluator.hpp"
#include <vector>
#include <memory>
#include <string>

class FO2SATGenerator : public FO2Generator {
public:
    using FO2Generator::FO2Generator;

    std::unique_ptr<ASTNode> generateSAT(int depth, int domainSize, BudgetState& budget) override;

private:
    std::unique_ptr<ASTNode> buildSAT(int depth, const Targets& targets, const FiniteModel& model, Variable currentVar, BudgetState& budget);

    // Target propagation 
    std::unique_ptr<ASTNode> buildAtomicSAT(const Targets& targets, const FiniteModel& model, Variable currentVar);
    std::unique_ptr<ASTNode> buildEqualitySAT(const Targets& targets, const FiniteModel& model, Variable currentVar);
    std::unique_ptr<ASTNode> buildNegSAT(int depth, const Targets& targets, const FiniteModel& model, Variable currentVar, BudgetState& budget);
    std::unique_ptr<ASTNode> buildAndSAT(int depth, const Targets& targets, const FiniteModel& model, Variable currentVar, BudgetState& budget);
    std::unique_ptr<ASTNode> buildOrSAT(int depth, const Targets& targets, const FiniteModel& model, Variable currentVar, BudgetState& budget);
    std::unique_ptr<ASTNode> buildImpliesSAT(int depth, const Targets& targets, const FiniteModel& model, Variable currentVar, BudgetState& budget);
    std::unique_ptr<ASTNode> buildExistsSAT(int depth, const Targets& targets, const FiniteModel& model, Variable currentVar, BudgetState& budget);
    std::unique_ptr<ASTNode> buildForallSAT(int depth, const Targets& targets, const FiniteModel& model, Variable currentVar, BudgetState& budget);

    
    std::vector<Symbol> generateFO2Args(int arity, Variable currentVar, Variable other);
    bool evaluateASTNode(const ASTNode& node, const Assignment& assign, const FiniteModel& model);

    Targets complementTargets(int domainSize, const Targets& targets, Variable currentVar);

    
    static Variable varFromName(const std::string& name) {
        return (name == "V1" || name == "v1" || name == "0") ? 0 : 1;
    }
    static std::string nameFromVar(Variable v) {
        return v == 0 ? "V1" : "V2";
    }
};