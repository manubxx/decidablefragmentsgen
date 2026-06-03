#include "FO2Generator.hpp"
#include <stdexcept>
#include <algorithm>

//Constructor
FO2Generator::FO2Generator(std::vector<PredInfo> vocab, unsigned seed) : FormulaBuilder(seed), vocab_(std::move(vocab))
{
    if (vocab_.empty())
        throw std::invalid_argument("FO2Generator: vocabolario vuoto");
    for (const auto& p : vocab_)
        if (p.arity < 1 || p.arity > 2)
            throw std::invalid_argument("FO2Generator: predicato '" + p.name + "' ha arita' " + std::to_string(p.arity) + " (FO2 ammette solo arita' 1 o 2)");
}


// generateFormatted 
std::string FO2Generator::generateFormatted(const GenConfig& cfg)
{
    activeVocab_.clear();
    for (const auto& p : vocab_)
        if (cfg.arityFilter == 0 || p.arity == cfg.arityFilter)
            activeVocab_.push_back(p);

    if (activeVocab_.empty())
        throw std::invalid_argument("FO2Generator: nessun predicato con arita' " + std::to_string(cfg.arityFilter) + " nel vocabolario");

    return FormulaBuilder::generateFormatted(cfg);
}


// generateSAT 
// SAT is verified externally by Vampire using --verify.
std::unique_ptr<ASTNode> FO2Generator::generateSAT(int depth, int /*domainSize*/, BudgetState& budget)
{
    bool rootExists = (randInt(0, 1) == 0);
    int  bodyDepth = (depth > 0) ? depth - 1 : 0;

    auto body = build(bodyDepth, "v2", budget);
    auto rootQ = rootExists ? Symbol::exists() : Symbol::forall();
    return std::make_unique<QuantifierNode>(rootQ, Symbol::var("v1"), std::move(body));
}


// buildAtomic 
std::unique_ptr<AtomicNode> FO2Generator::buildAtomic(const std::string& currentVar)
{
    const std::string other = nextVar(currentVar);
    const auto& p = activeVocab_[randInt(0, static_cast<int>(activeVocab_.size()) - 1)];

    std::vector<Symbol> args;
    if (p.arity == 1) {
        args.push_back(Symbol::var(currentVar));
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
std::unique_ptr<EqualityNode> FO2Generator::buildEqualityAtom(const std::string& currentVar)
{
    const std::string other = nextVar(currentVar);
    const std::string lhs = (randInt(0, 1) == 0) ? currentVar : other;
    const std::string rhs = (randInt(0, 1) == 0) ? currentVar : other;
    return std::make_unique<EqualityNode>(Symbol::var(lhs), Symbol::var(rhs));
}


//  build (override) 
std::unique_ptr<ASTNode> FO2Generator::build(int depth, const std::string& currentVar,
    BudgetState& budget)
{
    if (depth == 0)
        return buildAtomic(currentVar);

    auto candidates = candidateTypes(depth, budget);
    if (candidates.empty())
        return buildAtomic(currentVar);

    SymbolType chosen = pickType(depth, budget);

    if (chosen == SymbolType::EQUALITY)
        return buildEqualityAtom(currentVar);

    switch (chosen) {
    case SymbolType::NEG:
        return std::make_unique<NegNode>(build(depth - 1, currentVar, budget));

    case SymbolType::AND:
        return std::make_unique<BinaryConnNode>(Symbol::and_(),
            build(depth - 1, currentVar, budget),
            build(depth - 1, currentVar, budget));

    case SymbolType::OR:
        return std::make_unique<BinaryConnNode>(Symbol::or_(),
            build(depth - 1, currentVar, budget),
            build(depth - 1, currentVar, budget));

    case SymbolType::IMPLIES:
        return std::make_unique<BinaryConnNode>(Symbol::implies(),
            build(depth - 1, currentVar, budget),
            build(depth - 1, currentVar, budget));

    case SymbolType::EXISTS: {
        std::string boundVar = nextVar(currentVar);
        return std::make_unique<QuantifierNode>(Symbol::exists(), Symbol::var(boundVar),
            build(depth - 1, boundVar, budget));
    }

    case SymbolType::FORALL: {
        std::string boundVar = nextVar(currentVar);
        return std::make_unique<QuantifierNode>(Symbol::forall(), Symbol::var(boundVar),
            build(depth - 1, boundVar, budget));
    }

    default:
        return buildAtomic(currentVar);
    }
}
