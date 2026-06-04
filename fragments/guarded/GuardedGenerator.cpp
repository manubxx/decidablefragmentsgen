#include "GuardedGenerator.hpp"
#include <stdexcept>
#include <algorithm>
#include <cassert>
#include <iostream>


//Constructor
GuardedGenerator::GuardedGenerator(std::vector<PredInfo> vocab, unsigned seed) : FormulaBuilder(seed), vocab_(std::move(vocab))
{
    if (vocab_.empty())
        throw std::invalid_argument("GuardedGenerator: empty vocabulary");

    for (const auto& p : vocab_)
        if (p.arity < 1)
            throw std::invalid_argument("GuardedGenerator: predicate '" + p.name + "' has arity < 1 (not allowed)");
}

//  nextVar
std::string GuardedGenerator::nextVar(const std::string& current) const
{
    int n = std::stoi(current.substr(1));
    return varName(n + 1);
}

//  generateFormatted - generation entry point 
std::string GuardedGenerator::generateFormatted(const GenConfig& cfg)
{
    activeVocab_.clear();
    for (const auto& p : vocab_)
        if (cfg.arityFilter == 0 || p.arity == cfg.arityFilter)
            activeVocab_.push_back(p);

    if (activeVocab_.empty())
        throw std::invalid_argument("GF: no predicates with arity " + std::to_string(cfg.arityFilter));

    auto finalize = [&](std::unique_ptr<ASTNode> f, const std::string& tptpRole) -> std::string {
        if (cfg.transform == TransformMode::NNF)
            f = f->toNNF(false);

        Symbol rootQuant = (cfg.mode == GenMode::UNSAT)
            ? Symbol::forall()
            : Symbol::exists();
        f = std::make_unique<QuantifierNode>(rootQuant, Symbol::var("x1"), std::move(f));

        if (cfg.output == OutputFormat::TPTP)
            return "fof(f," + tptpRole + ",\n    " + f->toTPTP() + "\n).";
        return f->toString();
        };

    if (cfg.mode == GenMode::FREE || cfg.mode == GenMode::SAT) {
        static constexpr int MAX_RETRY = 500;
        std::unique_ptr<ASTNode> formula;
        bool budgetOk = false;

        for (int attempt = 0; attempt < MAX_RETRY; ++attempt) {
            BudgetState bs(cfg.budget, rng_);
            currScopeVars = { "x1" };

            try { formula = buildGF(cfg.depth, bs); }
            catch (const std::exception& e) {
                if (attempt < 3)
                    std::cerr << "[DEBUG retry " << attempt << "] exception: " << e.what() << "\n";
                continue;
            }

            if (!cfg.budget.hasAnyConstraint() || bs.satisfied()) {
                budgetOk = true;
                break;
            }
        }

        if (cfg.budget.hasAnyConstraint() && !budgetOk)
            throw std::invalid_argument("GF: unable to satisfy the requested budget with depth = " + std::to_string(cfg.depth));
        if (!formula) return "";

        return finalize(std::move(formula), "axiom");
    }

    if (cfg.mode == GenMode::UNSAT) {
        static constexpr int MAX_RETRY = 500;
        std::unique_ptr<ASTNode> formula;
        bool budgetOk = false;

        for (int attempt = 0; attempt < MAX_RETRY; ++attempt) {
            BudgetState bs(cfg.budget, rng_);
            try {
                currScopeVars = { "x1" };
                auto phi = buildGF(cfg.depth, bs);
                currScopeVars = { "x1" };
                auto notPhi = std::make_unique<NegNode>(buildGF(cfg.depth, bs));
                formula = std::make_unique<BinaryConnNode>(Symbol::and_(), std::move(phi), std::move(notPhi));
            }
            catch (const std::exception& e) {
                if (attempt < 3)
                    std::cerr << "[DEBUG retry " << attempt << "] exception: " << e.what() << "\n";
                continue;
            }

            if (!cfg.budget.hasAnyConstraint() || bs.satisfied()) {
                budgetOk = true;
                break;
            }
        }

        if (cfg.budget.hasAnyConstraint() && !budgetOk)
            throw std::invalid_argument("GF UNSAT: unable to satisfy the requested budget");
        if (!formula) return "";

        return finalize(std::move(formula), "negated_conjecture");
    }

    throw std::invalid_argument("GF: mode not recognized");
}




std::unique_ptr<ASTNode> GuardedGenerator::generateSAT(int depth, int /*domainSize*/, BudgetState& budget) {
    return buildGF(depth, budget);
}


//  buildAtomic  (override FormulaBuilder)
std::unique_ptr<AtomicNode> GuardedGenerator::buildAtomic(const std::string& /*currentVar*/)
{
    return buildAtomicLeaf(currScopeVars);
}


//  buildGF — recursive
std::unique_ptr<ASTNode> GuardedGenerator::buildGF(int depth, BudgetState& budget)
{
    if (depth == 0)
        return buildAtomicLeaf(currScopeVars);

    auto candidates = candidateTypes(depth, budget);
    if (candidates.empty())
        return buildAtomicLeaf(currScopeVars);

    SymbolType chosen = pickTypeGF(depth, budget);

    switch (chosen) {

    case SymbolType::NEG:
        return std::make_unique<NegNode>(buildGF(depth - 1, budget));

    case SymbolType::AND:
        return std::make_unique<BinaryConnNode>(Symbol::and_(),
            buildGF(depth - 1, budget),
            buildGF(depth - 1, budget));

    case SymbolType::OR:
        return std::make_unique<BinaryConnNode>(Symbol::or_(),
            buildGF(depth - 1, budget),
            buildGF(depth - 1, budget));

    case SymbolType::IMPLIES:
        return std::make_unique<BinaryConnNode>(Symbol::implies(),
            buildGF(depth - 1, budget),
            buildGF(depth - 1, budget));

    case SymbolType::EQUALITY: {
        if (currScopeVars.size() >= 2) {
            int i = randInt(0, static_cast<int>(currScopeVars.size()) - 1);
            int j = randInt(0, static_cast<int>(currScopeVars.size()) - 1);
            return std::make_unique<EqualityNode>(Symbol::var(currScopeVars[i]), Symbol::var(currScopeVars[j]));
        }
        return buildAtomicLeaf(currScopeVars);
    }

    case SymbolType::EXISTS: {
        int maxK = std::min(3, depth);
        int k = randInt(1, maxK);
        auto boundVars = nextVarNames(k);
        auto guard = buildGuard(currScopeVars, boundVars);

        auto snapScopeVars = currScopeVars;
        for (const auto& y : boundVars)
            currScopeVars.push_back(y);

        auto body = buildGF(depth - 1, budget);
        currScopeVars = snapScopeVars;

        auto guardAndBody = std::make_unique<BinaryConnNode>(Symbol::and_(), std::move(guard), std::move(body));
        return wrapQuantifiers(Symbol::exists(), boundVars, std::move(guardAndBody));
    }

    case SymbolType::FORALL: {
        int maxK = std::min(3, depth);
        int k = randInt(1, maxK);
        auto boundVars = nextVarNames(k);
        auto guard = buildGuard(currScopeVars, boundVars);

        auto snapScopeVars = currScopeVars;
        for (const auto& y : boundVars)
            currScopeVars.push_back(y);

        auto body = buildGF(depth - 1, budget);
        currScopeVars = snapScopeVars;

        auto guardImpliesBody = std::make_unique<BinaryConnNode>(Symbol::implies(), std::move(guard), std::move(body));
        return wrapQuantifiers(Symbol::forall(), boundVars, std::move(guardImpliesBody));
    }
    case SymbolType::PREDICATE:
    case SymbolType::VARIABLE:
    default:
        return buildAtomicLeaf(currScopeVars);
    }
}


//  buildAtomicLeaf
std::unique_ptr<AtomicNode> GuardedGenerator::buildAtomicLeaf(const std::vector<std::string>& vars)
{
    auto admissible = admissibleAtoms(static_cast<int>(vars.size()));
    if (admissible.empty())
        throw std::logic_error("GuardedGenerator::buildAtomicLeaf: no predicate found for scope " + std::to_string(vars.size()));

    int idx = admissible[randInt(0, static_cast<int>(admissible.size()) - 1)];
    const auto& p = activeVocab_[idx];

    std::vector<Symbol> args;
    args.reserve(p.arity);
    int start = static_cast<int>(vars.size()) - p.arity;
    for (int i = start; i < static_cast<int>(vars.size()); ++i)
        args.push_back(Symbol::var(vars[i]));

    return std::make_unique<AtomicNode>(Symbol::pred(p.name, p.arity), std::move(args));
}


//  buildGuard
std::unique_ptr<AtomicNode> GuardedGenerator::buildGuard(const std::vector<std::string>& outerScopeVars, const std::vector<std::string>& boundVars)
{


    for (int k = static_cast<int>(boundVars.size()); k >= 0; --k) {
        int totalArity = static_cast<int>(outerScopeVars.size()) + k;
        if (totalArity < 1) continue;

        auto guards = admissibleGuards(totalArity);
      

        if (guards.empty()) continue;

        int idx = guards[randInt(0, static_cast<int>(guards.size()) - 1)];
        const auto& p = activeVocab_[idx];

        std::vector<Symbol> args;
        args.reserve(p.arity);
        for (const auto& v : outerScopeVars)
            args.push_back(Symbol::var(v));
        for (int i = 0; i < k; ++i)
            args.push_back(Symbol::var(boundVars[i]));

        return std::make_unique<AtomicNode>(Symbol::pred(p.name, p.arity), std::move(args));
    }

    throw std::logic_error(
        "GuardedGenerator::buildGuard: no guard predicate found for outerScope="
        + std::to_string(outerScopeVars.size())
        + " + boundVars=" + std::to_string(boundVars.size()));
}


//  wrapQuantifiers
std::unique_ptr<ASTNode> GuardedGenerator::wrapQuantifiers(Symbol quantSym, const std::vector<std::string>& boundVars, std::unique_ptr<ASTNode> body)
{
    std::unique_ptr<ASTNode> result = std::move(body);
    for (int i = static_cast<int>(boundVars.size()) - 1; i >= 0; --i)
        result = std::make_unique<QuantifierNode>(quantSym, Symbol::var(boundVars[i]), std::move(result));
    return result;
}


SymbolType GuardedGenerator::pickTypeGF(int depth, BudgetState& budget)
{
    auto candidates = candidateTypes(depth, budget);

    if (currScopeVars.size() < 2)
        candidates.erase(std::remove(candidates.begin(), candidates.end(), SymbolType::EQUALITY), candidates.end());

    int outerSize = static_cast<int>(currScopeVars.size());
    bool canQuant = false;
    for (int k = 1; k <= 3; ++k)
        if (!admissibleGuards(outerSize + k).empty()) { canQuant = true; break; }
    if (!canQuant) {
        candidates.erase(std::remove(candidates.begin(), candidates.end(), SymbolType::EXISTS), candidates.end());
        candidates.erase(std::remove(candidates.begin(), candidates.end(), SymbolType::FORALL), candidates.end());
    }

    if (candidates.empty()) return SymbolType::PREDICATE;

    std::vector<SymbolType> forced;
    for (auto t : candidates) {
        int left = 0;
        switch (t) {
        case SymbolType::AND:      left = budget.and_left;     break;
        case SymbolType::OR:       left = budget.or_left;      break;
        case SymbolType::NEG:      left = budget.not_left;     break;
        case SymbolType::EXISTS:   left = budget.exists_left;  break;
        case SymbolType::FORALL:   left = budget.forall_left;  break;
        case SymbolType::IMPLIES:  left = budget.implies_left; break;
        case SymbolType::EQUALITY: left = budget.eq_left;      break;
        default: break;
        }
        if (left > 0) forced.push_back(t);
    }

    auto& pick = (budget.remaining() > depth && !forced.empty()) ? forced : candidates;
    SymbolType chosen = pick[randInt(0, static_cast<int>(pick.size()) - 1)];
    budget.consume(chosen);
    return chosen;
}

//  Utility

std::string GuardedGenerator::varName(int n) {
    return "x" + std::to_string(n);
}

std::vector<int> GuardedGenerator::admissibleGuards(int totalVars) const
{
    std::vector<int> idx;
    for (int i = 0; i < static_cast<int>(activeVocab_.size()); ++i)
        if (activeVocab_[i].arity == totalVars)
            idx.push_back(i);
    return idx;
}

std::vector<int> GuardedGenerator::admissibleAtoms(int maxArity) const
{
    std::vector<int> idx;
    for (int i = 0; i < static_cast<int>(activeVocab_.size()); ++i)
        if (activeVocab_[i].arity <= maxArity)
            idx.push_back(i);
    return idx;
}

std::vector<std::string> GuardedGenerator::nextVarNames(int k) const
{
    int nextIdx = static_cast<int>(currScopeVars.size()) + 1;
    std::vector<std::string> vars;
    vars.reserve(k);
    for (int i = 0; i < k; ++i)
        vars.push_back(varName(nextIdx + i));
    return vars;
}