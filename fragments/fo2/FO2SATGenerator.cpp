#include "FO2SATGenerator.hpp"
#include <stdexcept>
#include <numeric>
#include <algorithm>

namespace {

    // converts Symbol vector to string vector of names
    std::vector<std::string> getArgNames(const std::vector<Symbol>& args) {
        std::vector<std::string> names;
        names.reserve(args.size());
        for (const auto& s : args) names.push_back(s.name);
        return names;
    }
}

// Generates FO2 arguments
std::vector<Symbol> FO2SATGenerator::generateFO2Args(int arity, const std::string& currentVar, const std::string& other) {
    std::vector<Symbol> args;
    if (arity == 1) {
        std::string chosenVar = (randInt(0, 1) == 0) ? currentVar : other;
        args.push_back(Symbol::var(chosenVar));
    }
    else {
        std::string arg1 = (randInt(0, 1) == 0) ? currentVar : other;
        std::string arg2 = (randInt(0, 1) == 0) ? currentVar : other;
        args.push_back(Symbol::var(arg1));
        args.push_back(Symbol::var(arg2));
    }
    return args;
}

// Generates all possible domain assignments for v1 and v2
Targets FO2SATGenerator::allAssignments(const std::vector<std::string>& domain) {
    Targets allAssign;
    allAssign.reserve(domain.size() * domain.size());
    for (const auto& e1 : domain) {
        for (const auto& e2 : domain) {
            allAssign.push_back({ {"v1", e1}, {"v2", e2} });
        }
    }
    return allAssign;
}

// Computes the complement of targets relative to all assignments
Targets FO2SATGenerator::complementTargets(const Targets& allAssign, const Targets& targets) {
    Targets complement;
    for (const auto& a : allAssign) {
        if (std::find(targets.begin(), targets.end(), a) == targets.end()) {
            complement.push_back(a);
        }
    }
    return complement;
}

// Evaluates AST node semantics recursively on a finite model
bool FO2SATGenerator::evaluateASTNode(const ASTNode& node, const Assignment& assign, const FiniteModel& model) {
    if (auto* a = dynamic_cast<const AtomicNode*>(&node)) {
        return model.evalAtom(a->predSymbol().name, a->predSymbol().arity, getArgNames(a->args()), assign);
    }
    if (auto* eqNode = dynamic_cast<const EqualityNode*>(&node)) {
        auto itL = assign.find(eqNode->lhs().name);
        auto itR = assign.find(eqNode->rhs().name);
        if (itL == assign.end() || itR == assign.end()) return false;
        return itL->second == itR->second;
    }
    if (auto* negNode = dynamic_cast<const NegNode*>(&node)) {
        return !evaluateASTNode(negNode->child(), assign, model);
    }
    if (auto* binConnNode = dynamic_cast<const BinaryConnNode*>(&node)) {
        bool lv = evaluateASTNode(binConnNode->left(), assign, model);
        bool rv = evaluateASTNode(binConnNode->right(), assign, model);
        switch (binConnNode->connSymbol().type) {
        case SymbolType::AND:     return lv && rv;
        case SymbolType::OR:      return lv || rv;
        case SymbolType::IMPLIES: return !lv || rv;
        default: return false;
        }
    }
    if (auto* quantNode = dynamic_cast<const QuantifierNode*>(&node)) {
        const std::string& vname = quantNode->var().name;
        bool isExists = (quantNode->quantSymbol().type == SymbolType::EXISTS);
        for (const auto& e : model.domain()) {
            Assignment ext = assign;
            ext[vname] = e;
            bool val = evaluateASTNode(quantNode->body(), ext, model);
            if (isExists && val)  return true;
            if (!isExists && !val) return false;
        }
        return !isExists;
    }
    return false;
}

// SAT Construction (only FO2SATGenerator)
std::unique_ptr<ASTNode> FO2SATGenerator::generateSAT(int depth, int domainSize, BudgetState& budget) {
    // Force a min domainSize if not specified 
    int actualDomainSize = (domainSize <= 0) ? 3 : domainSize;
    FiniteModel model(activeVocab_, rng_, actualDomainSize);
    const auto& D = model.domain();

    std::vector<std::string> domCopy = D;
    std::shuffle(domCopy.begin(), domCopy.end(), rng_);
    int ns = randInt(1, static_cast<int>(domCopy.size()));
    domCopy.resize(ns);

    bool rootExists = (randInt(0, 1) == 0);
    Targets bodyTargets;

    const auto& targetDomain = rootExists ? domCopy : D;
    for (const auto& e : targetDomain) {
        bodyTargets.push_back({ {"v1", e} });
    }

    int bodyDepth = (depth > 0) ? depth - 1 : 0;
    auto body = buildSAT(bodyDepth, bodyTargets, model, "v1", budget);

    auto rootQ = rootExists ? Symbol::exists() : Symbol::forall();
    return std::make_unique<QuantifierNode>(rootQ, Symbol::var("v1"), std::move(body));
}

// buildSAT routing
std::unique_ptr<ASTNode> FO2SATGenerator::buildSAT(int depth, const Targets& targets, const FiniteModel& model, const std::string& currentVar, BudgetState& budget) {
    if (depth == 0) return buildAtomicSAT(targets, model, currentVar);

    auto candidates = candidateTypes(depth, budget);
    if (candidates.empty()) return buildAtomicSAT(targets, model, currentVar);

    switch (pickType(depth, budget)) {
    case SymbolType::NEG:      return buildNegSAT(depth, targets, model, currentVar, budget);
    case SymbolType::AND:      return buildAndSAT(depth, targets, model, currentVar, budget);
    case SymbolType::OR:       return buildOrSAT(depth, targets, model, currentVar, budget);
    case SymbolType::IMPLIES:  return buildImpliesSAT(depth, targets, model, currentVar, budget);
    case SymbolType::EXISTS:   return buildExistsSAT(depth, targets, model, currentVar, budget);
    case SymbolType::FORALL:   return buildForallSAT(depth, targets, model, currentVar, budget);
    case SymbolType::EQUALITY: return buildEqualitySAT(targets, model, currentVar);
    default:                   return buildAtomicSAT(targets, model, currentVar);
    }
}

// Builds an atomic or negated atomic node satisfying targets
std::unique_ptr<ASTNode> FO2SATGenerator::buildAtomicSAT(const Targets& targets, const FiniteModel& model, const std::string& currentVar) {
    const std::string other = nextVar(currentVar);

    std::vector<int> idx(activeVocab_.size());
    std::iota(idx.begin(), idx.end(), 0);
    std::shuffle(idx.begin(), idx.end(), rng_);

    // 1: Look for a uniformly true or false atom
    for (int i : idx) {
        const auto& p = activeVocab_[i];
        auto args = generateFO2Args(p.arity, currentVar, other);
        auto argN = getArgNames(args);

        bool allTrue = true, allFalse = true;
        for (const auto& assign : targets) {
            bool val = model.evalAtom(p.name, p.arity, argN, assign);
            if (!val) allTrue = false;
            if (val)  allFalse = false;
        }

        auto atom = std::make_unique<AtomicNode>(Symbol::pred(p.name, p.arity), args);
        if (allTrue)  return atom;
        if (allFalse) return std::make_unique<NegNode>(std::move(atom));
    }

    // 2: Fallback: Maximize true satisfaction over targets
    int bestIdx = idx[0], bestCount = -1;
    for (int i : idx) {
        const auto& cand = activeVocab_[i];
        auto candNames = getArgNames(generateFO2Args(cand.arity, currentVar, other));
        int cnt = 0;
        for (const auto& assign : targets) {
            if (model.evalAtom(cand.name, cand.arity, candNames, assign)) ++cnt;
        }
        if (cnt > bestCount) { bestCount = cnt; bestIdx = i; }
    }

    const auto& bp = activeVocab_[bestIdx];
    auto ba = generateFO2Args(bp.arity, currentVar, other);
    auto atom = std::make_unique<AtomicNode>(Symbol::pred(bp.name, bp.arity), ba);
    if (bestCount == 0) {
        return std::make_unique<NegNode>(std::move(atom));
    }
    return atom;
}

// buildEqualitySAT
std::unique_ptr<ASTNode> FO2SATGenerator::buildEqualitySAT(const Targets& targets, const FiniteModel& model, const std::string& currentVar) {
    const std::string other = nextVar(currentVar);
    std::vector<std::pair<std::string, std::string>> combos = {
        {currentVar, currentVar}, {currentVar, other}, {other, currentVar}, {other, other}
    };
    std::shuffle(combos.begin(), combos.end(), rng_);

    for (const auto& [lhsName, rhsName] : combos) {
        bool allTrue = true, allFalse = true;
        for (const auto& assign : targets) {
            auto itL = assign.find(lhsName);
            auto itR = assign.find(rhsName);
            if (itL == assign.end() || itR == assign.end()) { allTrue = false; allFalse = false; break; }

            bool val = (itL->second == itR->second);
            if (!val) allTrue = false;
            if (val)  allFalse = false;
        }

        auto eq = std::make_unique<EqualityNode>(Symbol::var(lhsName), Symbol::var(rhsName));
        if (allTrue)  return eq;
        if (allFalse) return std::make_unique<NegNode>(std::move(eq));
    }

    return buildAtomicSAT(targets, model, currentVar);
}

// buildNegSAT
std::unique_ptr<ASTNode> FO2SATGenerator::buildNegSAT(int depth, const Targets& targets, const FiniteModel& model, const std::string& currentVar, BudgetState& budget) {
    auto allAssign = allAssignments(model.domain());
    auto complement = complementTargets(allAssign, targets);

    if (complement.empty()) {
        return buildAtomicSAT(targets, model, currentVar);
    }

    auto child = buildSAT(depth - 1, complement, model, currentVar, budget);

    // Fallback
    for (const auto& t : targets) {
        if (evaluateASTNode(*child, t, model)) {
            return buildAtomicSAT(targets, model, currentVar);
        }
    }

    return std::make_unique<NegNode>(std::move(child));
}

// buildAndSAT
std::unique_ptr<ASTNode> FO2SATGenerator::buildAndSAT(int depth, const Targets& targets, const FiniteModel& model, const std::string& currentVar, BudgetState& budget) {
    return std::make_unique<BinaryConnNode>(Symbol::and_(),
        buildSAT(depth - 1, targets, model, currentVar, budget),
        buildSAT(depth - 1, targets, model, currentVar, budget));
}

//buildOrSAT
std::unique_ptr<ASTNode> FO2SATGenerator::buildOrSAT(int depth, const Targets& targets, const FiniteModel& model, const std::string& currentVar, BudgetState& budget) {
    Targets leftT, rightT;
    for (const auto& t : targets) {
        int coin = randInt(0, 2);
        if (coin != 2) leftT.push_back(t);
        if (coin != 0) rightT.push_back(t);
    }
    if (leftT.empty())  leftT = { targets[randInt(0, static_cast<int>(targets.size()) - 1)] };
    if (rightT.empty()) rightT = { targets[randInt(0, static_cast<int>(targets.size()) - 1)] };

    return std::make_unique<BinaryConnNode>(Symbol::or_(),
        buildSAT(depth - 1, leftT, model, currentVar, budget),
        buildSAT(depth - 1, rightT, model, currentVar, budget));
}

// buildImpliesSAT
std::unique_ptr<ASTNode> FO2SATGenerator::buildImpliesSAT(int depth, const Targets& targets, const FiniteModel& model, const std::string& currentVar, BudgetState& budget) {
    Targets leftCoverTargets, rightTargets;
    for (const auto& t : targets) {
        int coin = randInt(0, 2);
        if (coin != 2) leftCoverTargets.push_back(t);
        if (coin != 0) rightTargets.push_back(t);
    }
    if (leftCoverTargets.empty()) leftCoverTargets = { targets[randInt(0, static_cast<int>(targets.size()) - 1)] };
    if (rightTargets.empty())     rightTargets = { targets[randInt(0, static_cast<int>(targets.size()) - 1)] };

    auto allAssign = allAssignments(model.domain());
    auto antecedentTargets = complementTargets(allAssign, leftCoverTargets);

    if (antecedentTargets.empty()) {
        return buildAtomicSAT(targets, model, currentVar);
    }

    return std::make_unique<BinaryConnNode>(Symbol::implies(),
        buildSAT(depth - 1, antecedentTargets, model, currentVar, budget),
        buildSAT(depth - 1, rightTargets, model, currentVar, budget));
}

// Builds an existential quantifier node extending variables mapping randomly
std::unique_ptr<ASTNode> FO2SATGenerator::buildExistsSAT(int depth, const Targets& targets, const FiniteModel& model, const std::string& currentVar, BudgetState& budget) {
    const std::string boundVar = nextVar(currentVar);
    const auto& D = model.domain();

    Targets bodyTargets;
    bodyTargets.reserve(targets.size());
    for (const auto& t : targets) {
        Assignment ex = t;
        ex[boundVar] = D[randInt(0, static_cast<int>(D.size()) - 1)];
        bodyTargets.push_back(ex);
    }

    return std::make_unique<QuantifierNode>(Symbol::exists(), Symbol::var(boundVar),
        buildSAT(depth - 1, bodyTargets, model, boundVar, budget));
}

// Builds a universal quantifier node targets with full domain
std::unique_ptr<ASTNode> FO2SATGenerator::buildForallSAT(int depth, const Targets& targets, const FiniteModel& model, const std::string& currentVar, BudgetState& budget) {
    const std::string boundVar = nextVar(currentVar);
    const auto& D = model.domain();

    Targets bodyTargets;
    bodyTargets.reserve(targets.size() * D.size());
    for (const auto& t : targets) {
        for (const auto& e : D) {
            Assignment ex = t;
            ex[boundVar] = e;
            bodyTargets.push_back(ex);
        }
    }

    return std::make_unique<QuantifierNode>(Symbol::forall(), Symbol::var(boundVar),
        buildSAT(depth - 1, bodyTargets, model, boundVar, budget));
}