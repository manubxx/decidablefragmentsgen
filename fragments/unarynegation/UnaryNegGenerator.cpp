#include "UnaryNegGenerator.hpp"
#include <stdexcept>
#include <algorithm>
#include <cassert>


//  Constructor
UnaryNegGenerator::UnaryNegGenerator(std::vector<PredInfo> vocab, unsigned seed) : FormulaBuilder(seed), vocab_(std::move(vocab))
{
    if (vocab_.empty())
        throw std::invalid_argument("UNFO: empty vocabulary");

    for (const auto& p : vocab_)
        if (p.arity < 1)
            throw std::invalid_argument("UNFO: predicate '" + p.name + "' has arity < 1");
}

//  Fragment interface

std::string UnaryNegGenerator::fragmentName() const { return "UNFO"; }
std::string UnaryNegGenerator::startVar()     const { return "x1"; }

std::string UnaryNegGenerator::nextVar(const std::string& current) const {
    int n = std::stoi(current.substr(1));
    return "x" + std::to_string(n + 1);
}


//  generateFormatted  entry point
std::string UnaryNegGenerator::generateFormatted(const GenConfig& cfg)
{

    activeVocab_.clear();
    for (const auto& p : vocab_)
        if (cfg.arityFilter == 0 || p.arity == cfg.arityFilter)
            activeVocab_.push_back(p);

    if (activeVocab_.empty())
        throw std::invalid_argument("UNFO: no predicate matches the arity filter");


    auto finalize = [&](std::unique_ptr<ASTNode> f,
        const std::string& tptpRole) -> std::string {
            if (cfg.transform == TransformMode::NNF)
                f = f->toNNF(false);
            if (cfg.output == OutputFormat::TPTP)
                return "fof(f," + tptpRole + ",\n    " + f->toTPTP() + "\n).";
            return f->toString();
        };


    //  FREE / SAT  (SAT verified externally by Vampire)
    if (cfg.mode == GenMode::FREE || cfg.mode == GenMode::SAT) {
        static constexpr int MAX_RETRY = 500;
        std::unique_ptr<ASTNode> formula;
        bool budgetOk = false;

        for (int attempt = 0; attempt < MAX_RETRY; ++attempt) {
            BudgetState bs(cfg.budget, rng_);
            try {
                formula = buildUN(cfg.depth, {}, bs);
            }
            catch (const std::exception&) { continue; }

            if (!cfg.budget.hasAnyConstraint() || bs.satisfied()) {
                budgetOk = true;
                break;
            }
        }

        if (cfg.budget.hasAnyConstraint() && !budgetOk)
            throw std::invalid_argument("UNFO: unable to meet required budget with depth=" + std::to_string(cfg.depth));

        if (!formula) return "";

        return finalize(std::move(formula), "axiom");
    }


    //  UNSAT  —  φ ∧ ¬φ  
    if (cfg.mode == GenMode::UNSAT) {
        static constexpr int MAX_RETRY = 500;
        std::unique_ptr<ASTNode> formula;
        bool budgetOk = false;

        for (int attempt = 0; attempt < MAX_RETRY; ++attempt) {
            BudgetState bs(cfg.budget, rng_);
            try {
                formula = generateUNSAT(cfg.depth, bs);
            }
            catch (const std::exception&) {
                continue;
            }

            if (!cfg.budget.hasAnyConstraint() || bs.satisfied()) {
                budgetOk = true;
                break;
            }
        }

        if (cfg.budget.hasAnyConstraint() && !budgetOk)
            throw std::invalid_argument("UNFO UNSAT: unable to meet requested budget.");
        if (!formula) return "";

        return finalize(std::move(formula), "negated_conjecture");
    }

    throw std::invalid_argument("UNFO: unrecognized mode.");
}


//  generateSAT  (FormulaBuilder pure-virtual)

std::unique_ptr<ASTNode> UnaryNegGenerator::generateSAT(int depth, int /*domainSize*/, BudgetState& budget)
{
    return buildUN(depth, {}, budget);
}


//  buildAtomic  (FormulaBuilder pure-virtual)
std::unique_ptr<AtomicNode> UnaryNegGenerator::buildAtomic(const std::string& currentVar)
{
    return buildAtomicUN({ currentVar });
}


std::unique_ptr<ASTNode> UnaryNegGenerator::buildComponentUNSAT(int depth, BudgetState& budget) {
    return buildUN(depth, {}, budget);
}

//  buildUN  —  recursive builder
//  freeVars : variables that are free in the current sub-formula.
std::unique_ptr<ASTNode> UnaryNegGenerator::buildUN(int depth, const std::vector<std::string>& currFreeVars, BudgetState& budget)
{
    // If no variables are in scope yet, we must introduce one
    // with a (structural, budget-free) quantifier before we can build any atom.
    auto forcedQuant = [&](int d, const std::vector<std::string>& scope) -> std::unique_ptr<ASTNode>
        {
            bool existential = (randInt(0, 1) == 0);
            std::string newVar;
            if (scope.empty()) {
                newVar = startVar();
            }
            else {
                int maxIdx = 0;
                for (const auto& v : scope) {
                    int idx = std::stoi(v.substr(1));
                    if (idx > maxIdx) maxIdx = idx;
                }
                newVar = "x" + std::to_string(maxIdx + 1);
            }

            std::vector<std::string> newScope = scope;
            newScope.push_back(newVar);

            auto quantSym = existential ? Symbol::exists() : Symbol::forall();
            int nextDepth = (d > 0) ? d - 1 : 0;
            auto body = buildUN(nextDepth, newScope, budget);
            return std::make_unique<QuantifierNode>(
                quantSym, Symbol::var(newVar), std::move(body));
        };


    // Base case: depth == 0  
    if (depth == 0) {
        auto admPreds = admissiblePreds(currFreeVars);
        if (!admPreds.empty())
            return buildAtomicUN(currFreeVars);
        return forcedQuant(0, currFreeVars);
    }

    auto candidates = candidateTypesUN(depth, currFreeVars, budget);

    if (candidates.empty()) {
        auto admPreds = admissiblePreds(currFreeVars);
        if (!admPreds.empty())
            return buildAtomicUN(currFreeVars);
        return forcedQuant(depth, currFreeVars);
    }

    SymbolType chosen = pickType(depth, budget, candidates);

    if (chosen == SymbolType::PREDICATE) {
        auto admPreds = admissiblePreds(currFreeVars);
        if (!admPreds.empty())
            return buildAtomicUN(currFreeVars);
        return forcedQuant(depth, currFreeVars);
    }

    switch (chosen) {

    case SymbolType::NEG:
        return buildNegatedBody(depth - 1, currFreeVars, budget);

    case SymbolType::AND:
        return std::make_unique<BinaryConnNode>(Symbol::and_(),
            buildUN(depth - 1, currFreeVars, budget),
            buildUN(depth - 1, currFreeVars, budget));

    case SymbolType::OR:
        return std::make_unique<BinaryConnNode>(Symbol::or_(),
            buildUN(depth - 1, currFreeVars, budget),
            buildUN(depth - 1, currFreeVars, budget));

    case SymbolType::IMPLIES:
        return std::make_unique<BinaryConnNode>(Symbol::implies(),
            buildUN(depth - 1, currFreeVars, budget),
            buildUN(depth - 1, currFreeVars, budget));

    case SymbolType::EXISTS:
    case SymbolType::FORALL: {
        int maxIdx = 0;
        for (const auto& v : currFreeVars) {
            int idx = std::stoi(v.substr(1));
            if (idx > maxIdx) maxIdx = idx;
        }
        std::string newVar = "x" + std::to_string(maxIdx + 1);

        std::vector<std::string> newScope = currFreeVars;
        newScope.push_back(newVar);

        auto quantSym = (chosen == SymbolType::EXISTS) ? Symbol::exists() : Symbol::forall();
        return std::make_unique<QuantifierNode>(quantSym, Symbol::var(newVar),   buildUN(depth - 1, newScope, budget));
    }

    case SymbolType::EQUALITY: {
        if (currFreeVars.size() >= 2) {
            int i = randInt(0, static_cast<int>(currFreeVars.size()) - 1);
            int j;
            do { j = randInt(0, static_cast<int>(currFreeVars.size()) - 1); } while (j == i);
            return std::make_unique<EqualityNode>(Symbol::var(currFreeVars[i]), Symbol::var(currFreeVars[j]));
        }
        [[fallthrough]];
    }

    default: {
        auto admPreds = admissiblePreds(currFreeVars);
        if (!admPreds.empty())
            return buildAtomicUN(currFreeVars);
        return forcedQuant(depth, currFreeVars);
    }
    }
}


//  buildNegatedBody
std::unique_ptr<ASTNode> UnaryNegGenerator::buildNegatedBody(int depth, const std::vector<std::string>& currFreeVars, BudgetState& budget)
{
    std::vector<std::string> unaryScope;
    if (!currFreeVars.empty()) {
        int pick = randInt(0, static_cast<int>(currFreeVars.size()) - 1);
        unaryScope = { currFreeVars[pick] };
    }

    auto body = buildUN(depth, unaryScope, budget);
    return std::make_unique<NegNode>(std::move(body));
}


//  candidateTypesUN
std::vector<SymbolType> UnaryNegGenerator::candidateTypesUN(int depth, const std::vector<std::string>& currFreeVars, const BudgetState& bs) const
{
    if (depth == 0) return {};

    static const SymbolType pool[] = {
        SymbolType::AND, SymbolType::OR, SymbolType::NEG,
        SymbolType::IMPLIES, SymbolType::EXISTS, SymbolType::FORALL,
        SymbolType::EQUALITY,
    };

    std::vector<SymbolType> candidates;
    for (auto t : pool) {
        if (!bs.canUse(t)) continue;

        if (t == SymbolType::NEG && currFreeVars.size() > 1)
            continue;   //illegal in UNFO
        if (t == SymbolType::EQUALITY && currFreeVars.size() < 2)
            continue;   // x=y atleast two vars in scope 

        candidates.push_back(t);
    }
    return candidates;
}


//  buildAtomicLeaf
std::unique_ptr<AtomicNode> UnaryNegGenerator::buildAtomicUN(const std::vector<std::string>& scope)
{
    auto adm = admissiblePreds(scope);
    if (adm.empty())
        throw std::logic_error("UNFO::buildAtomicLeaf: no predicate admissible at scope size=" + std::to_string(scope.size()));

    int idx = adm[randInt(0, static_cast<int>(adm.size()) - 1)];
    const auto& p = activeVocab_[idx];
    auto args = predArgNames(scope, p.arity);
    return std::make_unique<AtomicNode>(Symbol::pred(p.name, p.arity), std::move(args));
}


//  admissiblePreds
std::vector<int> UnaryNegGenerator::admissiblePreds(const std::vector<std::string>& scope) const
{
    std::vector<int> idx;
    for (int i = 0; i < static_cast<int>(activeVocab_.size()); ++i)
        if (static_cast<int>(scope.size()) >= activeVocab_[i].arity)
            idx.push_back(i);
    return idx;
}


std::vector<Symbol> UnaryNegGenerator::predArgNames(const std::vector<std::string>& scope, int arity)
{
    std::vector<Symbol> args;
    args.reserve(arity);
    for (int i = 0; i < arity; ++i) {
        int pick = randInt(0, static_cast<int>(scope.size()) - 1);
        args.push_back(Symbol::var(scope[pick]));
    }
    return args;
}


std::string UnaryNegGenerator::varName(int n) {
    return "x" + std::to_string(n);
}