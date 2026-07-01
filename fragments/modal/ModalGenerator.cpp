#include "ModalGenerator.hpp"


ModalGenerator::ModalGenerator(std::vector<PredInfo> propositions, unsigned seed) : FormulaBuilder(seed), props(std::move(propositions)) {}

std::unique_ptr<AtomicNode> ModalGenerator::buildAtomic(const std::string& currentVar) {
    std::vector<PredInfo> propos;
    for (const auto& p : props) {
        if (p.arity == 1) propos.push_back(p);
    }

    if (propos.empty()) {
        throw std::runtime_error("Vocabolario modale vuoto o senza proposizioni!");
    }

    int idx = randInt(0, static_cast<int>(propos.size()) - 1);
    return std::make_unique<AtomicNode>(Symbol::pred(propos[idx].name, 1) , std::vector<Symbol>{Symbol::var(currentVar)} );
}

std::unique_ptr<ASTNode> ModalGenerator::build(int depth, const std::string& currentVar, BudgetState& budget) {

    if (depth <= 0) return buildAtomic(currentVar);

    auto candidates = candidateTypes(depth, budget);
    SymbolType chosen = pickType(depth, budget, candidates);

    // Standard Traslation
    if (chosen == SymbolType::EXISTS || chosen == SymbolType::FORALL) {
        std::string next = nextVar(currentVar);

        auto r_atom = std::make_unique<AtomicNode>(Symbol::pred("R", 2) , std::vector<Symbol>{Symbol::var(currentVar), Symbol::var(next)}
        );

        auto phi = build(depth - 1, next, budget);

   
        if (chosen == SymbolType::EXISTS) { 
            auto and_node = std::make_unique<BinaryConnNode>(Symbol::and_(), std::move(r_atom), std::move(phi));
            return std::make_unique<QuantifierNode>(Symbol::exists(), Symbol::var(next), std::move(and_node));
        }
        else { 
            auto imp_node = std::make_unique<BinaryConnNode>(Symbol::implies(), std::move(r_atom), std::move(phi));
            return std::make_unique<QuantifierNode>(Symbol::forall(), Symbol::var(next), std::move(imp_node));
        }
    }
    return FormulaBuilder::build(depth, currentVar, budget);
}

std::unique_ptr<ASTNode> ModalGenerator::generateSAT(int depth, int domainSize, BudgetState& budget) {
    return build(depth, startVar(), budget);
}

std::unique_ptr<ASTNode> ModalGenerator::buildComponentUNSAT(int depth, BudgetState& budget) {
   
    std::string x = startVar();
    std::string y = nextVar(x);

    auto r_atom = std::make_unique<AtomicNode>(Symbol::pred("R", 2), std::vector<Symbol>{Symbol::var(x), Symbol::var(y)});
    auto p_y = std::make_unique<AtomicNode>(Symbol::pred("P1", 1), std::vector<Symbol>{Symbol::var(y)});
    
    // R(x,y) => P(y)
    auto impl = std::make_unique<BinaryConnNode>(Symbol::implies(), std::move(r_atom), std::move(p_y));
    return std::make_unique<QuantifierNode>(Symbol::forall(), Symbol::var(y), std::move(impl));
}
