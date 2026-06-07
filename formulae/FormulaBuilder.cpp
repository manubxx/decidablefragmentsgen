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

        FormulaBuilder::BudgetState bs(cfg.budget, rng_);
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
            "Cannot satisfy the requested budget. Increase --depth or reduce constraints.");

    if (!formula) return "";

    // Transformations
    if (cfg.transform == TransformMode::NNF)
        formula = formula->toNNF(false);

    // TPTP Formatting
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

std::unique_ptr<ASTNode> FormulaBuilder::generateUNSAT(int depth, BudgetState& budget) {
    if (budget.and_left == 0)
        throw std::logic_error("generateUNSAT: budget AND exhausted");
    if (budget.not_left == 0)
        throw std::logic_error("generateUNSAT: budget NOT exhausted");

    budget.consume(SymbolType::AND);
    budget.consume(SymbolType::NEG);

    int childDepth = (depth > 0) ? depth - 1 : 0;

    // Halved budget; phi will never consume more budget than clone 
    BudgetState phiBudget = budget;
    phiBudget.and_left /= 2;
    phiBudget.or_left /= 2;
    phiBudget.not_left /= 2;
    phiBudget.exists_left /= 2;
    phiBudget.forall_left /= 2;
    phiBudget.implies_left /= 2;
    phiBudget.eq_left /= 2;

    auto phi = buildComponentUNSAT(childDepth, phiBudget);
    auto copy = phi->clone();


    auto applyDoubleDelta = [](int& realBudget, int oldPhi, int newPhi) {
        int consumed = oldPhi - newPhi;
        realBudget = std::max(0, realBudget - (consumed * 2));
        };

    BudgetState startPhiBudget = budget; // before /= 2

    applyDoubleDelta(budget.and_left, startPhiBudget.and_left / 2, phiBudget.and_left);
    applyDoubleDelta(budget.or_left,  startPhiBudget.or_left / 2, phiBudget.or_left);
    applyDoubleDelta(budget.not_left, startPhiBudget.not_left / 2, phiBudget.not_left);
    applyDoubleDelta(budget.exists_left, startPhiBudget.exists_left / 2, phiBudget.exists_left);
    applyDoubleDelta(budget.forall_left, startPhiBudget.forall_left / 2, phiBudget.forall_left);
    applyDoubleDelta(budget.implies_left, startPhiBudget.implies_left / 2, phiBudget.implies_left);
    applyDoubleDelta(budget.eq_left, startPhiBudget.eq_left / 2, phiBudget.eq_left);

    // UNSAT: phi AND (NOT copy)
    return std::make_unique<BinaryConnNode>(Symbol::and_(), std::move(phi), std::make_unique<NegNode>(std::move(copy))
    );
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

// pickType — overload principale: riceve i candidati già filtrati dal chiamante.
// Ogni frammento costruisce la propria lista (es. candidateTypesUN, pickTypeGF)
// e delega qui tutta la logica forced/random + budget.consume.
SymbolType FormulaBuilder::pickType(int depth, BudgetState& budget,
    const std::vector<SymbolType>& candidates) {
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

// pickType — wrapper retrocompatibile per build() e chi non ha filtri aggiuntivi.
SymbolType FormulaBuilder::pickType(int depth, BudgetState& budget) {
    auto candidates = candidateTypes(depth, budget);
    return pickType(depth, budget, candidates);
}

// build 
std::unique_ptr<ASTNode> FormulaBuilder::build(int depth, const std::string& currentVar, BudgetState& budget) {
    if (depth == 0)
        return buildAtomic(currentVar);

    auto candidates = candidateTypes(depth, budget);
    if (candidates.empty())
        return buildAtomic(currentVar);

    SymbolType chosen = pickType(depth, budget, candidates);

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