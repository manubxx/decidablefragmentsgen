#pragma once
#include "FO2Generator.hpp"
#include "models/FiniteModel.hpp"
#include <map>


using Assignment = std::map<std::string, std::string>;
using Targets = std::vector<Assignment>;

class FO2SATGenerator : public FO2Generator {
public:
    using FO2Generator::FO2Generator;

    std::unique_ptr<ASTNode> generateSAT(int depth, int domainSize, BudgetState& budget) override;

private:
    std::unique_ptr<ASTNode> buildSAT(int depth, const Targets& targets, const FiniteModel& model, const std::string& currentVar, BudgetState& budget);

    // Target propagation methods of AST nodes 
    std::unique_ptr<ASTNode> buildAtomicSAT(const Targets& targets, const FiniteModel& model, const std::string& currentVar);
    std::unique_ptr<ASTNode> buildEqualitySAT(const Targets& targets, const FiniteModel& model, const std::string& currentVar);
    std::unique_ptr<ASTNode> buildNegSAT(int depth, const Targets& targets, const FiniteModel& model, const std::string& currentVar, BudgetState& budget);
    std::unique_ptr<ASTNode> buildAndSAT(int depth, const Targets& targets, const FiniteModel& model, const std::string& currentVar, BudgetState& budget);
    std::unique_ptr<ASTNode> buildOrSAT(int depth, const Targets& targets, const FiniteModel& model, const std::string& currentVar, BudgetState& budget);
    std::unique_ptr<ASTNode> buildImpliesSAT(int depth, const Targets& targets, const FiniteModel& model, const std::string& currentVar, BudgetState& budget);
    std::unique_ptr<ASTNode> buildExistsSAT(int depth, const Targets& targets, const FiniteModel& model, const std::string& currentVar, BudgetState& budget);
    std::unique_ptr<ASTNode> buildForallSAT(int depth, const Targets& targets, const FiniteModel& model, const std::string& currentVar, BudgetState& budget);

    //Utility
    std::vector<Symbol> generateFO2Args(int arity, const std::string& currentVar, const std::string& other);
    Targets allAssignments(const std::vector<std::string>& domain);
    Targets complementTargets(const Targets& allAssign, const Targets& targets);
    bool evaluateASTNode(const ASTNode& node, const Assignment& assign, const FiniteModel& model);
};