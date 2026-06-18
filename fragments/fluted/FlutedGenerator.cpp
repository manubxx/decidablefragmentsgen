#include "FlutedGenerator.hpp"
#include <stdexcept>
#include <algorithm>
#include <climits>


// Constructor
FlutedGenerator::FlutedGenerator(std::vector<PredInfo> vocab, unsigned seed) : FormulaBuilder(seed), vocab_(std::move(vocab))
{
    if (vocab_.empty())
        throw std::invalid_argument("FL: empty vocabulary");

    for (const auto& p : vocab_)
        if (p.arity < 1)
            throw std::invalid_argument("FL: predicate '" + p.name + "' has arity < 1 (not allowed in FL)");
}

std::string FlutedGenerator::fragmentName() const { return "FL"; }
std::string FlutedGenerator::startVar()     const { return "x1"; }
std::string FlutedGenerator::nextVar(const std::string& current) const
{
    int n = std::stoi(current.substr(1));
    return "x" + std::to_string(n + 1);
}


// generateFormatted entry point
std::string FlutedGenerator::generateFormatted(const GenConfig& cfg)
{
    activeVocab_.clear();
    for (const auto& p : vocab_)
        if (cfg.arityFilter == 0 || p.arity == cfg.arityFilter)
            activeVocab_.push_back(p);

    if (activeVocab_.empty())
        throw std::invalid_argument("FL: no predicate with this arity in the vocabulary");

    auto finalize = [&](std::unique_ptr<ASTNode> f, const std::string& tptpRole) -> std::string {
        if (cfg.transform == TransformMode::NNF)
            f = f->toNNF(false);
        if (cfg.output == OutputFormat::TPTP)
            return "fof(f," + tptpRole + ",\n    " + f->toTPTP() + "\n).";
        return f->toString();
        };

    // FREE and SAT
    if (cfg.mode == GenMode::FREE || cfg.mode == GenMode::SAT) {
        static constexpr int MAX_RETRY = 500;
        std::unique_ptr<ASTNode> formula;
        bool budgetOk = false;

        for (int attempt = 0; attempt < MAX_RETRY; ++attempt) {
            BudgetState bs(cfg.budget, rng_);

            try { formula = buildFL(cfg.depth, 0, bs); }
            catch (const BudgetRetryException&) {
                continue;
            }

            if (!cfg.budget.hasAnyConstraint() || bs.satisfied()) {
                budgetOk = true;
                break;
            }
        }

        if (cfg.budget.hasAnyConstraint() && !budgetOk)
            throw std::invalid_argument("FL: unable to meet required budget with depth=" + std::to_string(cfg.depth));

        if (!formula)
            throw std::logic_error("FL: formula is null after successful budget loop");

        
        return finalize(std::move(formula), "axiom");
    }

    if (cfg.mode == GenMode::UNSAT)
        return FormulaBuilder::generateFormatted(cfg);

    throw std::invalid_argument("FL: unrecognized mode.");
}


std::unique_ptr<ASTNode> FlutedGenerator::generateSAT(int depth, int /*domainSize*/, BudgetState& budget)
{
    return buildFL(depth, 0, budget);
}


//  buildAtomic (override FormulaBuilder)
std::unique_ptr<AtomicNode> FlutedGenerator::buildAtomic(const std::string& currentVar)
{
    if (currentVar.empty() || currentVar[0] != 'x')
        throw std::invalid_argument("FL::buildAtomic: variable not FL-valid: '" + currentVar + "'");

    int stackDepth = std::stoi(currentVar.substr(1));
    return buildAtomicFL(stackDepth);
}

std::unique_ptr<ASTNode> FlutedGenerator::buildComponentUNSAT(int depth, BudgetState& budget) {
   
    return buildFL(depth, 0, budget);
}

//  buildFL
std::unique_ptr<ASTNode> FlutedGenerator::buildFL(int depth, int stackDepth, BudgetState& budget)
{
    auto admissible = admissiblePreds(stackDepth);

    auto forcedQuant = [&](int currDepth, int currStDepth) -> std::unique_ptr<ASTNode> {
        const bool canExists = budget.canUse(SymbolType::EXISTS);
        const bool canForall = budget.canUse(SymbolType::FORALL);

        if (!canExists && !canForall)
            throw BudgetRetryException();

        const bool exists = canExists && (!canForall || randInt(0, 1) == 0);
        budget.consume(exists ? SymbolType::EXISTS : SymbolType::FORALL);
        int nextStDepth = currStDepth + 1;
        int nextDepth = currDepth > 0 ? currDepth - 1 : 0;
        auto quantSym = exists ? Symbol::exists() : Symbol::forall();
        auto varSym = Symbol::var(varName(nextStDepth));
        auto body = buildFL(nextDepth, nextStDepth, budget);

        return std::make_unique<QuantifierNode>(quantSym, varSym, std::move(body));
        };

    // base case (leaf)
    if (depth == 0) {
        if (!admissible.empty()) return buildAtomicFL(stackDepth);
        return forcedQuant(0, stackDepth);
    }


    auto candidates = candidateTypesFL(depth, stackDepth, budget);

    if (candidates.empty()) {
        if (!admissible.empty()) return buildAtomicFL(stackDepth);
        return forcedQuant(depth, stackDepth);
    }

    SymbolType chosen = pickType(depth, budget, candidates);

    switch (chosen) {
    case SymbolType::NEG:
        return std::make_unique<NegNode>(buildFL(depth - 1, stackDepth, budget));

    case SymbolType::AND:
        return std::make_unique<BinaryConnNode>(Symbol::and_(),
            buildFL(depth - 1, stackDepth, budget),
            buildFL(depth - 1, stackDepth, budget));

    case SymbolType::OR:
        return std::make_unique<BinaryConnNode>(Symbol::or_(),
            buildFL(depth - 1, stackDepth, budget),
            buildFL(depth - 1, stackDepth, budget));

    case SymbolType::IMPLIES:
        return std::make_unique<BinaryConnNode>(Symbol::implies(),
            buildFL(depth - 1, stackDepth, budget),
            buildFL(depth - 1, stackDepth, budget));

    case SymbolType::EXISTS: {
        int nextSD = stackDepth + 1;
        return std::make_unique<QuantifierNode>(Symbol::exists(), Symbol::var(varName(nextSD)),
            buildFL(depth - 1, nextSD, budget));
    }

    case SymbolType::FORALL: {
        int nextSD = stackDepth + 1;
        return std::make_unique<QuantifierNode>(Symbol::forall(), Symbol::var(varName(nextSD)),
            buildFL(depth - 1, nextSD, budget));
    }

    case SymbolType::EQUALITY:
        return buildEqualityAtom(stackDepth);

    default:
        if (!admissible.empty()) return buildAtomicFL(stackDepth);
        return forcedQuant(depth, stackDepth);
    }
}

//candidateTypes: FormulaBuilder::candidateTypes specification for FL
std::vector<SymbolType> FlutedGenerator::candidateTypesFL(int depth, int stackDepth, const BudgetState& bs) const
{
    auto candidates = candidateTypes(depth, bs); 

    if (stackDepth < 2)
        candidates.erase(
            std::remove(candidates.begin(), candidates.end(), SymbolType::EQUALITY),
            candidates.end());

    return candidates;
}


//  buildAtomicLeaf / buildEqualityLeaf
std::unique_ptr<AtomicNode> FlutedGenerator::buildAtomicFL(int stackDepth)
{
    auto admissible = admissiblePreds(stackDepth);
    if (admissible.empty())
        throw std::logic_error(
            "FL::buildAtomicLeaf: no predicate allowed at stackDepth=" + std::to_string(stackDepth));

    int idx = admissible[randInt(0, static_cast<int>(admissible.size()) - 1)];
    const auto& p = activeVocab_[idx];
    auto args = predArgNames(stackDepth, p.arity);
    return std::make_unique<AtomicNode>(Symbol::pred(p.name, p.arity), std::move(args));
}

std::unique_ptr<EqualityNode> FlutedGenerator::buildEqualityAtom(int stackDepth)
{
    if (stackDepth < 2)
        throw std::logic_error("FL:buildEqualityAtom: stackDepth=" + std::to_string(stackDepth) + " < 2");

    return std::make_unique<EqualityNode>(Symbol::var(varName(stackDepth - 1)), Symbol::var(varName(stackDepth)));
}


// Utility

std::string FlutedGenerator::varName(int n) {
    return "x" + std::to_string(n);
}

std::vector<Symbol> FlutedGenerator::predArgNames(int stackDepth, int arity) const
{
    std::vector<Symbol> predArgs;
    predArgs.reserve(arity);
    for (int i = stackDepth - arity + 1; i <= stackDepth; ++i)
        predArgs.push_back(Symbol::var(varName(i)));
    return predArgs;
}

std::vector<int> FlutedGenerator::admissiblePreds(int stackDepth) const
{
    std::vector<int> idx;
    for (int i = 0; i < static_cast<int>(activeVocab_.size()); ++i)
        if (stackDepth >= activeVocab_[i].arity)
            idx.push_back(i);
    return idx;
}

