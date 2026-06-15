#include "FO2SATGenerator.hpp"
#include <stdexcept>
#include <numeric>
#include <algorithm>

namespace {
   
    std::vector<std::string> getArgNames(const std::vector<Symbol>& args) {
        std::vector<std::string> names;
        names.reserve(args.size());
        for (const auto& s : args) names.push_back(s.name);
        return names;
    }
}

std::vector<Symbol> FO2SATGenerator::generateFO2Args(int arity, const std::string& currentVar, const std::string& other) {
    std::vector<Symbol> args;
    args.reserve(arity);
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


Targets FO2SATGenerator::complementTargets(const std::vector<std::string>& domain, const Targets& targets) {
    std::set<Assignment> targetSet(targets.begin(), targets.end());
    Targets complement;

    bool hasV1 = false;
    bool hasV2 = false;
    if (!targets.empty()) {
        hasV1 = targets[0].count("v1") > 0;
        hasV2 = targets[0].count("v2") > 0;
    }
    else {
        hasV1 = true; hasV2 = true;
    }

    if (hasV1 && hasV2) {
     
        unsigned long long expectedSize = static_cast<unsigned long long>(domain.size()) * domain.size() - targets.size();
        if (expectedSize < 5000000ULL) {
            complement.reserve(static_cast<size_t>(expectedSize));
        }
        for (const auto& e1 : domain) {
            for (const auto& e2 : domain) {
                Assignment a = { {"v1", e1}, {"v2", e2} };
                if (targetSet.find(a) == targetSet.end()) {
                    complement.push_back(std::move(a));
                }
            }
        }
    }
    else if (hasV1) {
        complement.reserve(domain.size() - targets.size());
        for (const auto& e1 : domain) {
            Assignment a = { {"v1", e1} };
            if (targetSet.find(a) == targetSet.end()) {
                complement.push_back(std::move(a));
            }
        }
    }
    else if (hasV2) {
        complement.reserve(domain.size() - targets.size());
        for (const auto& e2 : domain) {
            Assignment a = { {"v2", e2} };
            if (targetSet.find(a) == targetSet.end()) {
                complement.push_back(std::move(a));
            }
        }
    }
    return complement;
}

bool FO2SATGenerator::evaluateASTNode(const ASTNode& node, const Assignment& assign, const FiniteModel& model) {

    //evaluate atomic
    if (auto* a = dynamic_cast<const AtomicNode*>(&node)) {
        return model.evalAtom(a->predSymbol().name, a->predSymbol().arity, getArgNames(a->args()), assign);
    }

    //evaluate equality
    if (auto* eqNode = dynamic_cast<const EqualityNode*>(&node)) {
        auto itL = assign.find(eqNode->lhs().name);
        auto itR = assign.find(eqNode->rhs().name);
        if (itL == assign.end() || itR == assign.end()) return false;
        return itL->second == itR->second;
    }

    //evaluate negation
    if (auto* negNode = dynamic_cast<const NegNode*>(&node)) {
        return !evaluateASTNode(negNode->child(), assign, model);
    }

    //evaluate binary connective
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

    //evaluate quantifier
    if (auto* quantNode = dynamic_cast<const QuantifierNode*>(&node)) {
        const std::string& vname = quantNode->var().name;
        bool isExists = (quantNode->quantSymbol().type == SymbolType::EXISTS);

    
        Assignment ext = assign;
        for (const auto& e : model.domain()) {
            ext[vname] = e;
            bool val = evaluateASTNode(quantNode->body(), ext, model);
            if (isExists && val)  return true;
            if (!isExists && !val) return false;
        }
        return !isExists;
    }
    return false;
}

std::unique_ptr<ASTNode> FO2SATGenerator::generateSAT(int depth, int domainSize, BudgetState& budget) {
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
    bodyTargets.reserve(targetDomain.size());
    for (const auto& e : targetDomain) {
        bodyTargets.push_back({ {"v1", e} });
    }

    int bodyDepth = (depth > 0) ? depth - 1 : 0;
    auto body = buildSAT(bodyDepth, bodyTargets, model, "v1", budget);

    auto rootQ = rootExists ? Symbol::exists() : Symbol::forall();
    return std::make_unique<QuantifierNode>(rootQ, Symbol::var("v1"), std::move(body));
}

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

std::unique_ptr<ASTNode> FO2SATGenerator::buildAtomicSAT(const Targets& targets, const FiniteModel& model, const std::string& currentVar) {
    const std::string other = nextVar(currentVar);

    std::vector<int> idx(activeVocab_.size());
    std::iota(idx.begin(), idx.end(), 0);
    std::shuffle(idx.begin(), idx.end(), rng_);

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

std::unique_ptr<ASTNode> FO2SATGenerator::buildNegSAT(int depth, const Targets& targets, const FiniteModel& model, const std::string& currentVar, BudgetState& budget) {
    auto complement = complementTargets(model.domain(), targets);

    if (complement.empty()) {
        return buildAtomicSAT(targets, model, currentVar);
    }

    auto child = buildSAT(depth - 1, complement, model, currentVar, budget);

    for (const auto& t : targets) {
        if (evaluateASTNode(*child, t, model)) {
            return buildAtomicSAT(targets, model, currentVar);
        }
    }

    return std::make_unique<NegNode>(std::move(child));
}

std::unique_ptr<ASTNode> FO2SATGenerator::buildAndSAT(int depth, const Targets& targets, const FiniteModel& model, const std::string& currentVar, BudgetState& budget) {
    return std::make_unique<BinaryConnNode>(Symbol::and_(),
        buildSAT(depth - 1, targets, model, currentVar, budget),
        buildSAT(depth - 1, targets, model, currentVar, budget));
}

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

std::unique_ptr<ASTNode> FO2SATGenerator::buildImpliesSAT(int depth, const Targets& targets, const FiniteModel& model, const std::string& currentVar, BudgetState& budget) {
    Targets leftCoverTargets, rightTargets;
    for (const auto& t : targets) {
        int coin = randInt(0, 2);
        if (coin != 2) leftCoverTargets.push_back(t);
        if (coin != 0) rightTargets.push_back(t);
    }
    if (leftCoverTargets.empty()) leftCoverTargets = { targets[randInt(0, static_cast<int>(targets.size()) - 1)] };
    if (rightTargets.empty())     rightTargets = { targets[randInt(0, static_cast<int>(targets.size()) - 1)] };

    auto antecedentTargets = complementTargets(model.domain(), leftCoverTargets);

    if (antecedentTargets.empty()) {
        return buildAtomicSAT(targets, model, currentVar);
    }

    return std::make_unique<BinaryConnNode>(Symbol::implies(),
        buildSAT(depth - 1, antecedentTargets, model, currentVar, budget),
        buildSAT(depth - 1, rightTargets, model, currentVar, budget));
}

std::unique_ptr<ASTNode> FO2SATGenerator::buildExistsSAT(int depth, const Targets& targets, const FiniteModel& model, const std::string& currentVar, BudgetState& budget) {
    const std::string boundVar = nextVar(currentVar);
    const auto& D = model.domain();

    Targets bodyTargets;
    bodyTargets.reserve(targets.size());
    for (const auto& t : targets) {
        Assignment ex = t;
        ex[boundVar] = D[randInt(0, static_cast<int>(D.size()) - 1)];
        bodyTargets.push_back(std::move(ex));
    }

    return std::make_unique<QuantifierNode>(Symbol::exists(), Symbol::var(boundVar),
        buildSAT(depth - 1, bodyTargets, model, boundVar, budget));
}

std::unique_ptr<ASTNode> FO2SATGenerator::buildForallSAT(int depth, const Targets& targets, const FiniteModel& model, const std::string& currentVar, BudgetState& budget) {
    const std::string boundVar = nextVar(currentVar);
    const auto& D = model.domain();

    Targets bodyTargets;
    bodyTargets.reserve(targets.size() * D.size());
    for (const auto& t : targets) {
        for (const auto& e : D) {
            Assignment ex = t;
            ex[boundVar] = e;
            bodyTargets.push_back(std::move(ex));
        }
    }

    return std::make_unique<QuantifierNode>(Symbol::forall(), Symbol::var(boundVar),
        buildSAT(depth - 1, bodyTargets, model, boundVar, budget));
}