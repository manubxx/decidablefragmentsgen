#pragma once

#include "../../formulae/FormulaBuilder.hpp"
#include <string>
#include <vector>

// UnaryNegationGenerator — UNFO fragment
//  Ten Cate & Segoufin, 2011/2013.
// 
// Core syntactic rule:
//   ¬φ  is allowed  iff  φ has at most ONE free variable.
// 
//   `currFreeVars` is the set of variables currently free in the sub-context..


class UnaryNegGenerator : public FormulaBuilder {
public:
    explicit UnaryNegGenerator(std::vector<PredInfo> vocab, unsigned seed);
     
    // Formula
    [[nodiscard]] std::string fragmentName() const override;
    [[nodiscard]] std::string generateFormatted(const GenConfig& cfg) override;

protected:

    // FormulaBuilder pure-virtual interface

    [[nodiscard]] std::unique_ptr<AtomicNode> buildAtomic(const std::string& currentVar) override;
    [[nodiscard]] std::unique_ptr<ASTNode>    generateSAT(int depth, int domainSize, BudgetState& budget) override;
    [[nodiscard]] std::unique_ptr<ASTNode> buildComponentUNSAT(int depth, BudgetState& budget) override;

    [[nodiscard]] std::string startVar() const override;
    [[nodiscard]] std::string nextVar(const std::string& current) const override;

private:

    std::vector<PredInfo> vocab_;
    std::vector<PredInfo> activeVocab_;

    // Core recursive builder 
    [[nodiscard]] std::unique_ptr<ASTNode> buildUN(int depth, const std::vector<std::string>& currFreeVars, BudgetState& budget);

  
    // Builds a negated sub-formula.
    [[nodiscard]] std::unique_ptr<ASTNode> buildNegatedBody(int depth, const std::vector<std::string>& currFreeVars, BudgetState& budget);

    // FormulaBuilder::buildAtomic specification
    [[nodiscard]] std::unique_ptr<AtomicNode> buildAtomicUN(const std::vector<std::string>& scope);

    // FormulaBuilder::candidateTypes specification
    [[nodiscard]] std::vector<SymbolType> candidateTypesUN(int depth, const std::vector<std::string>& currFreeVars, const BudgetState& bs) const;

  
    // Utility

    static std::string varName(int n);

    [[nodiscard]] std::vector<int> admissiblePreds(const std::vector<std::string>& scope) const;

    [[nodiscard]] std::vector<Symbol> predArgNames(const std::vector<std::string>& scope, int arity);

    
     
};