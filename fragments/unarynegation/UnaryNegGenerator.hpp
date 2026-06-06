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

    [[nodiscard]] std::string fragmentName() const override;
    [[nodiscard]] std::string generateFormatted(const GenConfig& cfg) override;

protected:

    // FormulaBuilder pure-virtual interface

    [[nodiscard]] std::unique_ptr<AtomicNode> buildAtomic(const std::string& currentVar) override;
    [[nodiscard]] std::unique_ptr<ASTNode>    generateSAT(int depth, int domainSize, BudgetState& budget) override;

    [[nodiscard]] std::string startVar() const override;
    [[nodiscard]] std::string nextVar(const std::string& current) const override;

private:

    // Core recursive builder 

    // freeVars : variables that are free in the current formula context
    // Negation is legal only when freeVars.size() <= 1.
    [[nodiscard]] std::unique_ptr<ASTNode> buildUN(int depth, const std::vector<std::string>& freeVars, BudgetState& budget);

    // Builds a single atomic leaf using variables drawn from `scope`.
    [[nodiscard]] std::unique_ptr<AtomicNode> buildAtomicUN(const std::vector<std::string>& scope);

    // Builds a negated sub-formula.
    // Precondition: freeVars.size() <= 1  (enforced by the caller).
    [[nodiscard]] std::unique_ptr<ASTNode> buildNegatedBody(int depth, const std::vector<std::string>& freeVars, BudgetState& budget);

    // candidateTypes restricted to what is legal given freeVars.
    [[nodiscard]] std::vector<SymbolType> candidateTypesUN(int depth, const std::vector<std::string>& freeVars, const BudgetState& bs) const;

  
    // Utility

    static std::string varName(int n);

    [[nodiscard]] std::vector<int> admissiblePreds(const std::vector<std::string>& scope) const;

    [[nodiscard]] std::vector<Symbol> predArgNames(const std::vector<std::string>& scope, int arity);

    // Data
    std::vector<PredInfo> vocab_;
    std::vector<PredInfo> activeVocab_;   
};