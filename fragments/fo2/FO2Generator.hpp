#pragma once
#include "../../formulae/FormulaBuilder.hpp"
#include "../../models/FiniteModel.hpp"
#include <map>
#include <numeric>

// Assignment / Targets 
using Assignment = std::map<std::string, std::string>;
using Targets = std::vector<Assignment>;

//  FO2Generator
class FO2Generator : public FormulaBuilder {
public:
    using BudgetState = FormulaBuilder::BudgetState;
    explicit FO2Generator(std::vector<PredInfo> vocab, unsigned seed);

    [[nodiscard]] std::string fragmentName() const override { return "FO2"; }

protected:
    
// FormulaBuilder Interface

    [[nodiscard]] std::string startVar() const override { return "v1"; }

    [[nodiscard]] std::string nextVar(const std::string& v) const override {
        return (v == "v1") ? "v2" : "v1";
    }

    [[nodiscard]] std::unique_ptr<AtomicNode>
        buildAtomic(const std::string& currentVar) override;

    [[nodiscard]] std::unique_ptr<ASTNode>
        generateSAT(int depth, int domainSize, BudgetState& budget) override;

private:

    std::vector<PredInfo> vocab_;
    std::vector<PredInfo> activeVocab_;





   
  
    std::unique_ptr<ASTNode> build(int depth, const std::string& currentVar, BudgetState& budget);
    // Target propagation SAT 

    std::unique_ptr<ASTNode> buildSAT(int depth, const Targets&, const FiniteModel&,
        const std::string& currentVar, FormulaBuilder::BudgetState&);

    std::unique_ptr<ASTNode> buildAtomicSAT(const Targets&, const FiniteModel&,
        const std::string& currentVar);
    std::unique_ptr<ASTNode> buildNegSAT(int depth, const Targets&, const FiniteModel&,
        const std::string& currentVar, FormulaBuilder::BudgetState&);
    std::unique_ptr<ASTNode> buildAndSAT(int depth, const Targets&, const FiniteModel&,
        const std::string& currentVar, FormulaBuilder::BudgetState&);
    std::unique_ptr<ASTNode> buildOrSAT(int depth, const Targets&, const FiniteModel&,
        const std::string& currentVar, FormulaBuilder::BudgetState&);
    std::unique_ptr<ASTNode> buildImpliesSAT(int depth, const Targets&, const FiniteModel&,
        const std::string& currentVar, FormulaBuilder::BudgetState&);
    std::unique_ptr<ASTNode> buildExistsSAT(int depth, const Targets&, const FiniteModel&,
        const std::string& currentVar, FormulaBuilder::BudgetState&);
    std::unique_ptr<ASTNode> buildForallSAT(int depth, const Targets&, const FiniteModel&,
        const std::string& currentVar, FormulaBuilder::BudgetState&);
    std::unique_ptr<ASTNode> buildEqualitySAT(const Targets&, const FiniteModel&,
        const std::string& currentVar);
     std::unique_ptr<EqualityNode> buildEqualityAtom(const std::string& currentVar);

public:
    
    [[nodiscard]] std::string generateFormatted(const GenConfig& cfg) override;
};