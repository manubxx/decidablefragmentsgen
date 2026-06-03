
#include "FlutedGenerator.hpp"
#include <stdexcept>
#include <algorithm>
#include <climits>


// Constructor
FlutedGenerator::FlutedGenerator(std::vector<PredInfo> vocab, unsigned seed) : FormulaBuilder(seed) , vocab_(std::move(vocab))
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
    // arity filter on vocabulary
    activeVocab_.clear();
    for (const auto& p : vocab_)
        if (cfg.arityFilter == 0 || p.arity == cfg.arityFilter)
            activeVocab_.push_back(p);

    if (activeVocab_.empty())
        throw std::invalid_argument("FL: no predicate with this arity in the vocabulary");

    // lambda function for output
    auto finalize = [&](std::unique_ptr<ASTNode> f, const std::string& tptpRole) -> std::string {
        if (cfg.transform == TransformMode::NNF)
            f = f->toNNF(false);
        if (cfg.output == OutputFormat::TPTP)
            return "fof(f," + tptpRole + ",\n    " + f->toTPTP() + "\n).";
        return f->toString();
        };

    
    // FREE and SAT 
    // SAT is verified externally by Vampire using --verify.
    if (cfg.mode == GenMode::FREE || cfg.mode == GenMode::SAT) {
        static constexpr int MAX_RETRY = 500;
        std::unique_ptr<ASTNode> formula;
        bool budgetOk = false;

        for (int attempt = 0; attempt < MAX_RETRY; ++attempt) {
            BudgetState bs(cfg.budget, rng_);

            try { formula = buildFL(cfg.depth, 0, bs); }
         
            catch (const std::exception&) { continue;}

            if (!cfg.budget.hasAnyConstraint() || bs.satisfied()) {
                budgetOk = true;
                break; }
        }

        if (cfg.budget.hasAnyConstraint() && !budgetOk)
            throw std::invalid_argument(
                "FL: unable to meet required budget with depth=" + std::to_string(cfg.depth) ");

        if (!formula) return "";

        std::string role = (cfg.mode == GenMode::SAT) ? "axiom" : "axiom";
        return finalize(std::move(formula), role);
    }

    //  UNSAT  —  phi AND NOT phi
    if (cfg.mode == GenMode::UNSAT) {
        static constexpr int MAX_RETRY = 500;
        std::unique_ptr<ASTNode> formula;
        bool budgetOk = false;

        for (int attempt = 0; attempt < MAX_RETRY; ++attempt) {
            BudgetState bs(cfg.budget, rng_);
            try {
                auto phi = buildFL(cfg.depth, 0, bs);
                auto notPhi = std::make_unique<NegNode>(buildFL(cfg.depth, 0, bs));
                formula = std::make_unique<BinaryConnNode>(
                    Symbol::and_(), std::move(phi), std::move(notPhi));
            }
            catch (const std::exception&) { continue; }

            if (!cfg.budget.hasAnyConstraint() || bs.satisfied()) {
                budgetOk = true;
                break;
            }
        }

        if (cfg.budget.hasAnyConstraint() && !budgetOk)
            throw std::invalid_argument("FL UNSAT: unable to meet requested budget.");
        if (!formula) return "";

        return finalize(std::move(formula), "negated_conjecture");
    }

    throw std::invalid_argument("FL: unrecognized mode.");
}


//  generateSAT 
std::unique_ptr<ASTNode> FlutedGenerator::generateSAT(int /*depth*/, int /*domainSize*/, BudgetState& /*budget*/)
{
    throw std::logic_error("FL::generateSAT: not implemented. Use generateFormatted with GenMode::SAT.");
}


//  buildAtomic (override FormulaBuilder)
std::unique_ptr<AtomicNode> FlutedGenerator::buildAtomic(const std::string& currentVar)
{
    if (currentVar.empty() || currentVar[0] != 'x')
        throw std::invalid_argument("FL::buildAtomic: variable not FL-valid: '" +  currentVar + ");

    int stackDepth = std::stoi(currentVar.substr(1));
    return buildAtomicLeaf(stackDepth);
}


//  buildFL 
std::unique_ptr<ASTNode> FlutedGenerator::buildFL(int depth, int stackDepth, BudgetState& budget)
{
    auto admissible = admissiblePreds(stackDepth);

    // forced structural quantifier (not counted in budget) if no valid atom in this depth
    auto forcedQuant = [&](int currDepth, int currStDepth) -> std::unique_ptr<ASTNode> {   
        bool exists = (randInt(0, 1) == 0);
        int  nextStDepth = currStDepth + 1;
        auto varSym = Symbol::var(varName(nextStDepth));
        auto quantSym = exists ? Symbol::exists() : Symbol::forall();
        int nextDepth = currDepth > 0 ? currDepth - 1 : 0;
        auto body = buildFL(nextDepth, nextSD, budget);
        return std::make_unique<QuantifierNode>(quantSym, varSym, std::move(body));
        };

    // base case (leaf)
    if (depth == 0) {
        if (!admissible.empty()) return buildAtomicLeaf(stackDepth);
        return forcedQuant(0, stackDepth);
    }

    auto candidates = candidateTypes(depth, budget);
    if (candidates.empty()) {
        if (!admissible.empty()) return buildAtomicLeaf(stackDepth);
        return forcedQuant(depth, stackDepth);
    }

    SymbolType chosen = pickType(depth, budget);

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
        if (stackDepth >= 2)
            return buildEqualityLeaf(stackDepth);
        [[fallthrough]];

    default:
        if (!admissible.empty()) return buildAtomicLeaf(stackDepth);
        return forcedQuant(depth, stackDepth);
    }
}


//  buildAtomicLeaf / buildEqualityLeaf
std::unique_ptr<AtomicNode> FlutedGenerator::buildAtomicLeaf(int stackDepth)
{
    auto admissible = admissiblePreds(stackDepth);
    if (admissible.empty())
        throw std::logic_error(
            "FL::buildAtomicLeaf: no predicate allowed at stackDepth=" + std::to_string(stackDepth));

    int idx = admissible[randInt(0, static_cast<int>(admissible.size()) - 1)];
    const auto& p = activeVocab_[idx];
    auto args = flutedArgs(stackDepth, p.arity);
    return std::make_unique<AtomicNode>(Symbol::pred(p.name, p.arity), std::move(args));
}

std::unique_ptr<EqualityNode> FlutedGenerator::buildEqualityLeaf(int stackDepth)
{
    if (stackDepth < 2)
        throw std::logic_error("FL:buildEqualityLeaf: stackDepth=" + std::to_string(stackDepth) + " < 2");

    return std::make_unique<EqualityNode>(Symbol::var(varName(stackDepth - 1)) , Symbol::var(varName(stackDepth)));
}


// Utility

std::string FlutedGenerator::varName(int n)
{
    return "x" + std::to_string(n);
}

std::vector<Symbol> FlutedGenerator::flutedPredArgs(int stackDepth, int arity) const
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

int FlutedGenerator::minArity() const
{
    int m = INT_MAX;
    for (const auto& p : activeVocab_)
        if (p.arity < m) m = p.arity;
    return (m == INT_MAX) ? 1 : m;
}
