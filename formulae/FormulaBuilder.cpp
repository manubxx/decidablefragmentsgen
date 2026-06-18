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
            case GenMode::SATBUILD:
                // polymorphism makes sure that fragments implement their satbuild
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
        case GenMode::SAT:
        case GenMode::SATBUILD: role = "axiom";              break; 
        case GenMode::UNSAT:    role = "negated_conjecture"; break;
        case GenMode::FREE:     role = "axiom";              break;
        }
        return "fof(f," + role + ",\n    " + formula->toTPTP() + "\n).";
    }
    return formula->toString();
}

std::unique_ptr<ASTNode> FormulaBuilder::generateUNSAT(int depth, BudgetState& budget) {

    if (budget.and_left == 0) throw std::logic_error("generateUNSAT: budget AND exhausted");
    if (budget.not_left == 0) throw std::logic_error("generateUNSAT: budget NOT exhausted");

    // Validation
    if (budget.and_left > 0 && budget.and_left % 2 != 1)
        throw std::invalid_argument("UNSAT generation with exact budget requires an ODD total number of ANDs.");
    if (budget.not_left > 0 && budget.not_left % 2 != 1)
        throw std::invalid_argument("UNSAT generation with exact budget requires an ODD total number of NEGs.");

    auto checkEven = [](int val, const std::string& name) {
        if (val > 0 && val % 2 != 0)
            throw std::invalid_argument("UNSAT generation with exact budget requires an EVEN total number of " + name );
        };
    checkEven(budget.or_left, "OR");
    checkEven(budget.exists_left, "EXISTS");
    checkEven(budget.forall_left, "FORALL");
    checkEven(budget.implies_left, "IMPLIES");
    checkEven(budget.eq_left, "EQUALITY");

    // budget snapshot
    BudgetState originalBudget = budget;

    budget.consume(SymbolType::AND);
    budget.consume(SymbolType::NEG);

    int childDepth = (depth > 0) ? depth - 1 : 0;

    // phiBudget copy 
    BudgetState phiBudget = budget;
    phiBudget.and_left = (budget.and_left > 0) ? budget.and_left / 2 : budget.and_left;
    phiBudget.or_left = (budget.or_left > 0) ? budget.or_left / 2 : budget.or_left;
    phiBudget.not_left = (budget.not_left > 0) ? budget.not_left / 2 : budget.not_left;
    phiBudget.exists_left = (budget.exists_left > 0) ? budget.exists_left / 2 : budget.exists_left;
    phiBudget.forall_left = (budget.forall_left > 0) ? budget.forall_left / 2 : budget.forall_left;
    phiBudget.implies_left = (budget.implies_left > 0) ? budget.implies_left / 2 : budget.implies_left;
    phiBudget.eq_left = (budget.eq_left > 0) ? budget.eq_left / 2 : budget.eq_left;

    auto phi = buildComponentUNSAT(childDepth, phiBudget);
    auto copy = phi->clone();

    auto applyDoubleDelta = [](int& realBudget, int B, int finalPhiBudget) {
        if (B <= 0) return;
        int allocated = B / 2;
        int remainder = B - allocated * 2;
        int consumedByPhi = allocated - finalPhiBudget;
        int totalConsumed = consumedByPhi * 2;
        realBudget = std::max(0, B - totalConsumed - remainder);
        };

    applyDoubleDelta(budget.and_left, originalBudget.and_left, phiBudget.and_left);
    applyDoubleDelta(budget.or_left, originalBudget.or_left, phiBudget.or_left);
    applyDoubleDelta(budget.not_left, originalBudget.not_left, phiBudget.not_left);
    applyDoubleDelta(budget.exists_left, originalBudget.exists_left, phiBudget.exists_left);
    applyDoubleDelta(budget.forall_left, originalBudget.forall_left, phiBudget.forall_left);
    applyDoubleDelta(budget.implies_left, originalBudget.implies_left, phiBudget.implies_left);
    applyDoubleDelta(budget.eq_left, originalBudget.eq_left, phiBudget.eq_left);

    return std::make_unique<BinaryConnNode>(
        Symbol::and_(),
        std::move(phi),
        std::make_unique<NegNode>(std::move(copy))
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

// pickType
SymbolType FormulaBuilder::pickType(int depth, BudgetState& budget, const std::vector<SymbolType>& candidates) {
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

    const std::vector<SymbolType>* pick = &candidates;
    if (!forced.empty()) {
        if (randInt(1, 100) <= 85) {
            pick = &forced;
        }
    }

    SymbolType chosen = (*pick)[randInt(0, static_cast<int>(pick->size()) - 1)];

    budget.consume(chosen);
    return chosen;
}

// pickType — wrapper retrocompatibile
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