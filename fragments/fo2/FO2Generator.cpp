#include "FO2Generator.hpp"
#include <stdexcept>
#include <algorithm>

//Constructor
FO2Generator::FO2Generator(std::vector<PredInfo> vocab, unsigned seed) : FormulaBuilder(seed), vocab_(std::move(vocab))
{
    if (vocab_.empty())
        throw std::invalid_argument("FO2: empty vocabulary");
    for (const auto& p : vocab_)
        if (p.arity < 1 || p.arity > 2)
            throw std::invalid_argument("FO2: predicate '" + p.name + "' has arity <1 or >2 (not allowed in FO2)");
}


// generateFormatted 
std::string FO2Generator::generateFormatted(const GenConfig& cfg)
{
    activeVocab_.clear();
    for (const auto& p : vocab_)
        if (cfg.arityFilter == 0 || p.arity == cfg.arityFilter)
            activeVocab_.push_back(p);

    if (activeVocab_.empty())
        throw std::invalid_argument("FO2: no predicates with arity " + std::to_string(cfg.arityFilter));

    return FormulaBuilder::generateFormatted(cfg);
}


// generateSAT 
// SAT is verified externally by Vampire using --verify.
std::unique_ptr<ASTNode> FO2Generator::generateSAT(int depth, int /*domainSize*/, BudgetState & budget)
{
    return build(depth, startVar(), budget);  
}



// buildAtomic 
std::unique_ptr<AtomicNode> FO2Generator::buildAtomic(const std::string& currentVar)
{
    const std::string other = nextVar(currentVar);
    const auto& p = activeVocab_[randInt(0, static_cast<int>(activeVocab_.size()) - 1)];

    std::vector<Symbol> args;
    if (p.arity == 1) {
        std::string chosenVar = (randInt(0, 1) == 0) ? currentVar : other;
        args.push_back(Symbol::var(chosenVar));
    }
    else {
        if (randInt(0, 1) == 0) {
            args.push_back(Symbol::var(currentVar));
            args.push_back(Symbol::var(other));
        }
        else {
            args.push_back(Symbol::var(other));
            args.push_back(Symbol::var(currentVar));
        }
    }
    return std::make_unique<AtomicNode>(Symbol::pred(p.name, p.arity), std::move(args));
}


// buildEqualityAtom
std::unique_ptr<EqualityNode> FO2Generator::buildEqualityAtom(const std::string& currentVar) {
    const std::string other = nextVar(currentVar);
    
    return std::make_unique<EqualityNode>(Symbol::var(currentVar), Symbol::var(other));
}

std::unique_ptr<ASTNode> FO2Generator::buildComponentUNSAT(int depth, BudgetState& budget) {
    
    return build(depth, startVar(), budget);
}

