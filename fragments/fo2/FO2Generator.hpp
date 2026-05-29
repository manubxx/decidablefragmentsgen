#pragma once
#include "../../formulae/FormulaBuilder.hpp"

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

    std::unique_ptr<ASTNode>      build(int depth, const std::string& currentVar, BudgetState& budget);
    std::unique_ptr<EqualityNode> buildEqualityAtom(const std::string& currentVar);

public:

    [[nodiscard]] std::string generateFormatted(const GenConfig& cfg) override;
};