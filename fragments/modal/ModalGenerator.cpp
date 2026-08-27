#include "ModalGenerator.hpp"
#include <algorithm>

ModalGenerator::ModalGenerator(std::vector<PredInfo> propositions, unsigned seed)
    : FormulaBuilder(seed), props(std::move(propositions)) {
}

std::unique_ptr<AtomicNode> ModalGenerator::buildAtomic(const std::string& currentVar) {
    std::vector<PredInfo> propos;
    for (const auto& p : props) {
        if (p.arity == 1) propos.push_back(p);
    }
    if (propos.empty()) throw std::runtime_error("Empty modal vocab");
    int idx = randInt(0, static_cast<int>(propos.size()) - 1);
    return std::make_unique<AtomicNode>(Symbol::pred(propos[idx].name, 1), std::vector<Symbol>{Symbol::var(currentVar)});
}

std::vector<SymbolType> ModalGenerator::candidateTypes(int depth, const BudgetState& budget) const {
    if (depth == 0) return {};
    static const SymbolType pool[] = {
        SymbolType::AND, SymbolType::OR, SymbolType::NEG,
        SymbolType::IMPLIES, SymbolType::EXISTS, SymbolType::FORALL
    };
    std::vector<SymbolType> candidates;
    for (auto t : pool) {
        if (budget.canUse(t)) candidates.push_back(t);
    }
    return candidates;
}

std::unique_ptr<ASTNode> ModalGenerator::build(int depth, const std::string& currentVar, BudgetState& budget) {
    if (depth <= 0) return buildAtomic(currentVar);
    auto candidates = candidateTypes(depth, budget);
    if (candidates.empty()) return buildAtomic(currentVar);

    SymbolType chosen = pickType(depth, budget, candidates);

    switch (chosen) {
    case SymbolType::NEG:
        return std::make_unique<NegNode>(build(depth - 1, currentVar, budget));
    case SymbolType::AND:
        return std::make_unique<BinaryConnNode>(Symbol::and_(), build(depth - 1, currentVar, budget), build(depth - 1, currentVar, budget));
    case SymbolType::OR:
        return std::make_unique<BinaryConnNode>(Symbol::or_(), build(depth - 1, currentVar, budget), build(depth - 1, currentVar, budget));
    case SymbolType::IMPLIES:
        return std::make_unique<BinaryConnNode>(Symbol::implies(), build(depth - 1, currentVar, budget), build(depth - 1, currentVar, budget));
    case SymbolType::EXISTS: {
        std::string next = nextVar(currentVar);
        auto r_atom = std::make_unique<AtomicNode>(Symbol::pred("r", 2), std::vector<Symbol>{Symbol::var(currentVar), Symbol::var(next)});
        auto phi = build(depth - 1, next, budget);
        auto and_node = std::make_unique<BinaryConnNode>(Symbol::and_(), std::move(r_atom), std::move(phi));
        return std::make_unique<QuantifierNode>(Symbol::exists(), Symbol::var(next), std::move(and_node));
    }
    case SymbolType::FORALL: {
        std::string next = nextVar(currentVar);
        auto r_atom = std::make_unique<AtomicNode>(Symbol::pred("r", 2), std::vector<Symbol>{Symbol::var(currentVar), Symbol::var(next)});
        auto phi = build(depth - 1, next, budget);
        auto imp_node = std::make_unique<BinaryConnNode>(Symbol::implies(), std::move(r_atom), std::move(phi));
        return std::make_unique<QuantifierNode>(Symbol::forall(), Symbol::var(next), std::move(imp_node));
    }
    default:
        return buildAtomic(currentVar);
    }
}

std::unique_ptr<ASTNode> ModalGenerator::generateSAT(int depth, int domainSize, BudgetState& budget) {
    auto body = build(depth, startVar(), budget);
    return std::make_unique<QuantifierNode>(Symbol::exists(), Symbol::var(startVar()), std::move(body));
}

std::unique_ptr<ASTNode> ModalGenerator::buildComponentUNSAT(int depth, BudgetState& budget) {
    return build(depth, startVar(), budget);
}

// Different UNSAT logic
std::unique_ptr<ASTNode> ModalGenerator::generateUNSAT(int depth, BudgetState& budget) {

    budget.consume(SymbolType::AND); budget.consume(SymbolType::AND); budget.consume(SymbolType::NEG);
    budget.consume(SymbolType::EXISTS); budget.consume(SymbolType::FORALL); budget.consume(SymbolType::IMPLIES);

    int childDepth = (depth > 0) ? depth - 1 : 0;
    std::string current = startVar();
    std::string next = nextVar(current);

    BudgetState originalBudget = budget;
    BudgetState phiBudget = budget;
    phiBudget.and_left = (budget.and_left > 0) ? budget.and_left / 2 : budget.and_left;
    phiBudget.or_left = (budget.or_left > 0) ? budget.or_left / 2 : budget.or_left;
    phiBudget.not_left = (budget.not_left > 0) ? budget.not_left / 2 : budget.not_left;
    phiBudget.exists_left = (budget.exists_left > 0) ? budget.exists_left / 2 : budget.exists_left;
    phiBudget.forall_left = (budget.forall_left > 0) ? budget.forall_left / 2 : budget.forall_left;
    phiBudget.implies_left = (budget.implies_left > 0) ? budget.implies_left / 2 : budget.implies_left;

    auto phi = build(childDepth, next, phiBudget);
    auto phi_copy = phi->clone();

    
    auto applyDoubleDelta = [](int& realB, int B, int finalPhiB) {
        if (B <= 0) return;
        int alloc = B / 2;
        int consumedByPhi = alloc - finalPhiB;
        realB = std::max(0, B - (consumedByPhi * 2) - (B - alloc * 2));
        };
    applyDoubleDelta(budget.and_left, originalBudget.and_left, phiBudget.and_left);
    applyDoubleDelta(budget.or_left, originalBudget.or_left, phiBudget.or_left);
    applyDoubleDelta(budget.not_left, originalBudget.not_left, phiBudget.not_left);
    applyDoubleDelta(budget.exists_left, originalBudget.exists_left, phiBudget.exists_left);
    applyDoubleDelta(budget.forall_left, originalBudget.forall_left, phiBudget.forall_left);
    applyDoubleDelta(budget.implies_left, originalBudget.implies_left, phiBudget.implies_left);

    // Box: ! [next] : (r(current, next) => phi)
    auto r_box = std::make_unique<AtomicNode>(Symbol::pred("r", 2), std::vector<Symbol>{Symbol::var(current), Symbol::var(next)});
    auto imp_node = std::make_unique<BinaryConnNode>(Symbol::implies(), std::move(r_box), std::move(phi));
    auto box_node = std::make_unique<QuantifierNode>(Symbol::forall(), Symbol::var(next), std::move(imp_node));

    // Diamond: ? [next] : (r(current, next) & ~phi_copy)
    auto r_dia = std::make_unique<AtomicNode>(Symbol::pred("r", 2), std::vector<Symbol>{Symbol::var(current), Symbol::var(next)});
    auto neg_phi = std::make_unique<NegNode>(std::move(phi_copy));
    auto and_dia = std::make_unique<BinaryConnNode>(Symbol::and_(), std::move(r_dia), std::move(neg_phi));
    auto dia_node = std::make_unique<QuantifierNode>(Symbol::exists(), Symbol::var(next), std::move(and_dia));

    auto rootAnd = std::make_unique<BinaryConnNode>(Symbol::and_(), std::move(box_node), std::move(dia_node));
    return std::make_unique<QuantifierNode>(Symbol::forall(), Symbol::var(current), std::move(rootAnd));

}