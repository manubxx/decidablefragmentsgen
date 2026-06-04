#pragma once
#include "../../formulae/FormulaBuilder.hpp"
#include <vector>
#include <string>
#include <memory>

// Guarded Generator
class GuardedGenerator : public FormulaBuilder {
public:
    using BudgetState = FormulaBuilder::BudgetState;

    explicit GuardedGenerator(std::vector<PredInfo> vocab, unsigned seed);
    ~GuardedGenerator() override = default;

 
    //  FormulaBuilder Interface
   
    [[nodiscard]] std::string fragmentName() const override { return "GF"; }

    [[nodiscard]] std::string startVar() const override { return "x1"; }
    [[nodiscard]] std::string nextVar(const std::string& current) const override;

    [[nodiscard]] std::string generateFormatted(const GenConfig& cfg) override;

    [[nodiscard]] std::unique_ptr<AtomicNode> buildAtomic(const std::string& currentVar) override;

    [[nodiscard]] std::unique_ptr<ASTNode> generateSAT(int depth, int domainSize, BudgetState& budget) override; 

private: 
   
    std::vector<PredInfo> vocab_;
    std::vector<PredInfo> activeVocab_;
    std::vector<std::string> currScopeVars; // Free Vars during the local recursive scope 

    std::unique_ptr<ASTNode> buildGF(int depth, BudgetState& budget);

    std::unique_ptr<AtomicNode> buildAtomicLeaf(const std::vector<std::string>& vars);
   
    std::unique_ptr<AtomicNode> buildGuard(const std::vector<std::string>& scopeVars, const std::vector<std::string>& boundVars);  // alpha(x-bar, y-bar): arity == scopeVars.size() + boundVars.size()

    static std::unique_ptr<ASTNode> wrapQuantifiers(Symbol quantSym, const std::vector<std::string>& boundVars, std::unique_ptr<ASTNode> body);  // (EXISTS, {y1,y2}, body) => EXISTS x1 (EXISTS x2 (body))


    //  Utility
    static std::string varName(int n);
    [[nodiscard]] std::vector<std::string> nextVarNames(int k) const;   //nextFreeIdx of last var in currScopeVars

    [[nodiscard]] std::vector<int> admissibleGuards(int totalVars) const; //arity == totalVars (buildGuard)
    [[nodiscard]] std::vector<int> admissibleAtoms(int maxArity) const;  //arity <= maxArity   (buildAtomicLeaf)

};