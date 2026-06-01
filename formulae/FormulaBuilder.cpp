#include "FormulaBuilder.hpp"
#include <stdexcept>
#include <algorithm>

static constexpr int MAX_RETRY = 500;

// Constructor
FormulaBuilder::FormulaBuilder(unsigned seed)
    : rng_(seed) {
}

// generateFormatted 
std::string FormulaBuilder::generateFormatted(const GenConfig& cfg) {
    std::unique_ptr<ASTNode> formula;
    bool budgetOk = false;

    for (int attempt = 0; attempt < MAX_RETRY; ++attempt) {

        BudgetState bs(cfg.budget, rng_);
        try {
            switch (cfg.mode) {
            case GenMode::FREE:
                formula = build(cfg.depth, startVar(), bs);
                break;
            case GenMode::SAT:
                formula = generateSAT(cfg.depth, cfg.domainSize, bs);
                break;
            case GenMode::UNSAT:
                formula = generateUNSAT(cfg.depth, bs);
                break;
            }
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
        throw std::invalid_argument(
            fragmentName() + ": impossibile soddisfare il budget richiesto "
            "con depth=" + std::to_string(cfg.depth) +
            ". Aumenta --depth o riduci i vincoli.");

    if (!formula) return "";

    // Trasformazione
    if (cfg.transform == TransformMode::NNF)
        formula = formula->toNNF(false);

    // Formattazione TPTP
    if (cfg.output == OutputFormat::TPTP) {
        std::string role;
        switch (cfg.mode) {
        case GenMode::SAT:   role = "axiom";              break;
        case GenMode::UNSAT: role = "negated_conjecture"; break;
        case GenMode::FREE:  role = "axiom";              break;
        }
        return "fof(f," + role + ",\n    " + formula->toTPTP() + "\n).";
    }
    return formula->toString();
}


// generateUNSAT
std::unique_ptr<ASTNode> FormulaBuilder::generateUNSAT(int depth, BudgetState& budget) {
    if (budget.and_left == 0)
        throw std::logic_error("generateUNSAT: budget AND esaurito");
    if (budget.not_left == 0)
        throw std::logic_error("generateUNSAT: budget NOT esaurito");

    budget.consume(SymbolType::AND);
    budget.consume(SymbolType::NEG);

    int childDepth = (depth > 0) ? depth - 1 : 0;

    BudgetState lastbudget = budget;
    auto phi = build(childDepth, startVar(), budget);
    auto copy = phi->clone();

    // Applica il delta consumato una seconda volta per il clone.
    auto applyDelta = [](int& current, int budget_old, int budget_new) {
        if (budget_old > 0) current = std::max(0, current - (budget_old - budget_new));
        };

    applyDelta(budget.and_left,     lastbudget.and_left,     budget.and_left);
    applyDelta(budget.or_left,      lastbudget.or_left,      budget.or_left);
    applyDelta(budget.not_left,     lastbudget.not_left,     budget.not_left);
    applyDelta(budget.exists_left,  lastbudget.exists_left,  budget.exists_left);
    applyDelta(budget.forall_left,  lastbudget.forall_left,  budget.forall_left);
    applyDelta(budget.implies_left, lastbudget.implies_left, budget.implies_left);
    applyDelta(budget.eq_left,      lastbudget.eq_left,      budget.eq_left);

    return std::make_unique<BinaryConnNode>(
        Symbol::and_(), std::move(phi), std::make_unique<NegNode>(std::move(copy)));
}

// candidateTypes
std::vector<SymbolType> FormulaBuilder::candidateTypes(int depth, const BudgetState& bs) const {
    if (depth == 0) return {};

    static const SymbolType pool[] = {
        SymbolType::AND, SymbolType::OR, SymbolType::NEG,
        SymbolType::IMPLIES, SymbolType::EXISTS, SymbolType::FORALL,
        SymbolType::EQUALITY,
    };

    std::vector<SymbolType> candidates;
    for (auto t : pool)
        if (bs.canUse(t)) candidates.push_back(t);

    return candidates;
}

// pickType
SymbolType FormulaBuilder::pickType(int depth, BudgetState& budget) {
    auto candidates = candidateTypes(depth, budget);

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

    if (pick.empty()) return SymbolType::PREDICATE;

    SymbolType chosen = pick[randInt(0, static_cast<int>(pick.size()) - 1)];
    budget.consume(chosen);
    return chosen;
}

// build 
std::unique_ptr<ASTNode> FormulaBuilder::build(int depth, const std::string& currentVar, BudgetState& budget) {
    if (depth == 0)
        return buildAtomic(currentVar);

    auto candidates = candidateTypes(depth, budget);
    if (candidates.empty())
        return buildAtomic(currentVar);

    SymbolType chosen = pickType(depth, budget);

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
    case SymbolType::EQUALITY: {
        std::string other = nextVar(currentVar);
        return std::make_unique<EqualityNode>(
            Symbol::var(currentVar), Symbol::var(other));
    }

    default:
        return buildAtomic(currentVar);
    }
}

// randInt 
int FormulaBuilder::randInt(int lo, int hi) {
    return std::uniform_int_distribution<int>(lo, hi)(rng_);
}