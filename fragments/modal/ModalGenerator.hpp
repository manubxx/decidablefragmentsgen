#pragma once
#include "../../formulae/FormulaBuilder.hpp"

class ModalGenerator : public FormulaBuilder {
public:
    explicit ModalGenerator(std::vector<PredInfo> propositions, unsigned seed);

    [[nodiscard]] std::string fragmentName() const override { return "MODAL"; }

protected:
    std::vector<PredInfo> props;

    [[nodiscard]] std::string startVar() const override { return "w1"; }
    [[nodiscard]] std::string nextVar(const std::string& v) const override { return (v == "w1") ? "w2" : "w1"; }

    [[nodiscard]] std::unique_ptr<AtomicNode> buildAtomic(const std::string& currentVar) override;

    [[nodiscard]] std::unique_ptr<ASTNode> build(int depth, const std::string& currentVar, BudgetState& budget) override;

    
    [[nodiscard]] std::unique_ptr<ASTNode> generateSAT(int depth, int domainSize, BudgetState& budget) override;
    [[nodiscard]] std::unique_ptr<ASTNode> buildComponentUNSAT(int depth, BudgetState& budget) override;
};