#include "FO2SATGenerator.hpp"
#include <stdexcept>
#include <numeric>
#include <algorithm>
#include <array>
#include <unordered_set>

std::vector<Symbol> FO2SATGenerator::generateFO2Args(int arity, Variable currentVar, Variable other) {
    std::vector<Symbol> args;
    args.reserve(arity);
    if (arity == 1) {
        Variable chosenVar = (randInt(0, 1) == 0) ? currentVar : other;
        args.push_back(Symbol::var(nameFromVar(chosenVar)));
    }
    else {
        Variable arg1 = (randInt(0, 1) == 0) ? currentVar : other;
        Variable arg2 = (randInt(0, 1) == 0) ? currentVar : other;
        args.push_back(Symbol::var(nameFromVar(arg1)));
        args.push_back(Symbol::var(nameFromVar(arg2)));
    }
    return args;
}



Targets FO2SATGenerator::complementTargets(int domainSize, const Targets& targets, Variable currentVar) {
    Targets complement;
    if (targets.empty()) return complement;

    const size_t MAX_TARGETS = 1000; // Il tuo valore perfetto

    Variable active = currentVar;
    Variable other = 1 - currentVar;

    int referenceValue = targets[0][other];
    bool isSingleRowC = true;
    for (const auto& a : targets) {
        if (a[other] != referenceValue) {
            isSingleRowC = false;
            break;
        }
    }

    if (!isSingleRowC) {
        
        std::unordered_set<long long> visited;
        visited.reserve(targets.size());

        for (const auto& a : targets) {
            // Mappiamo la coppia in un singolo intero univoco
            long long key = static_cast<long long>(a[0]) * domainSize + a[1];
            visited.insert(key);
        }

        for (int e1 = 0; e1 < domainSize; ++e1) {
            for (int e2 = 0; e2 < domainSize; ++e2) {
                if (complement.size() >= MAX_TARGETS) return complement;

                long long key = static_cast<long long>(e1) * domainSize + e2;
                if (visited.find(key) == visited.end()) {
                    complement.push_back({ e1, e2 });
                }
            }
        }
    }
    else {
        // Il caso 1D è già piccolo e non crea colli di bottiglia
        std::vector<bool> visited(domainSize, false);
        for (const auto& a : targets) {
            visited[a[active]] = true;
        }
        for (int e = 0; e < domainSize; ++e) {
            if (complement.size() >= MAX_TARGETS) return complement;
            if (!visited[e]) {
                Assignment c;
                c[active] = e;
                c[other] = referenceValue;
                complement.push_back(c);
            }
        }
    }
    return complement;
}

bool FO2SATGenerator::evaluateASTNode(const ASTNode& node, const Assignment& assign, const FiniteModel& model) {
    FO2Evaluator evaluator(assign, model);
    node.accept(evaluator);
    return evaluator.getResult();
}

std::unique_ptr<ASTNode> FO2SATGenerator::generateSAT(int depth, int domainSize, BudgetState& budget) {
    int actualDomainSize = (domainSize <= 0) ? 3 : domainSize;
    FiniteModel model(activeVocab_, rng_, actualDomainSize);
    int dSize = model.domainSize();

    std::vector<int> domCopy(dSize);
    std::iota(domCopy.begin(), domCopy.end(), 0);
    std::shuffle(domCopy.begin(), domCopy.end(), rng_);
    int ns = randInt(1, dSize);
    domCopy.resize(ns);

    bool rootExists = (randInt(0, 1) == 0);
    Targets bodyTargets;

  
    std::vector<int> fullDom;
    if (!rootExists) { //FORALL
        fullDom.resize(dSize);
        std::iota(fullDom.begin(), fullDom.end(), 0);
    }
    const auto& targetDomain = rootExists ? domCopy : fullDom;

    bodyTargets.reserve(targetDomain.size());
    for (int e : targetDomain) {
        bodyTargets.push_back({ e, 0 }); 
    }

    int bodyDepth = (depth > 0) ? depth - 1 : 0;
    auto body = buildSAT(bodyDepth, bodyTargets, model, 0, budget); 

    auto rootQ = rootExists ? Symbol::exists() : Symbol::forall();
    return std::make_unique<QuantifierNode>(rootQ, Symbol::var("v1"), std::move(body));
}

std::unique_ptr<ASTNode> FO2SATGenerator::buildSAT(int depth, const Targets& targets, const FiniteModel& model, Variable currentVar, BudgetState& budget) {
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

std::unique_ptr<ASTNode> FO2SATGenerator::buildAtomicSAT(const Targets& targets, const FiniteModel& model, Variable currentVar) {
    Variable other = 1 - currentVar;

    std::vector<int> idx(activeVocab_.size());
    std::iota(idx.begin(), idx.end(), 0);
    std::shuffle(idx.begin(), idx.end(), rng_);

    
    for (int i : idx) {
        const auto& p = activeVocab_[i];
        auto args = generateFO2Args(p.arity, currentVar, other);
        Variable arg1 = varFromName(args[0].name);
        Variable arg2 = (p.arity > 1) ? varFromName(args[1].name) : 0;

        bool allTrue = true, allFalse = true;
        for (const auto& assign : targets) {
            bool val = model.evalAtom(p.name, p.arity, assign, arg1, arg2);
            if (!val) allTrue = false;
            if (val)  allFalse = false;
        }

        auto atom = std::make_unique<AtomicNode>(Symbol::pred(p.name, p.arity), args);
        if (allTrue)  return atom;
        if (allFalse) return std::make_unique<NegNode>(std::move(atom));
    }

   // fallback: return a tautology
    return std::make_unique<EqualityNode>(Symbol::var(nameFromVar(currentVar)), Symbol::var(nameFromVar(currentVar))
    );
}

std::unique_ptr<ASTNode> FO2SATGenerator::buildEqualitySAT(const Targets& targets, const FiniteModel& model, Variable currentVar) {
    Variable other = 1 - currentVar;
    std::vector<std::pair<Variable, Variable>> combos = {
        {currentVar, currentVar}, {currentVar, other}, {other, currentVar}, {other, other}
    };
    std::shuffle(combos.begin(), combos.end(), rng_);

    for (const auto& [lhs, rhs] : combos) {
        bool allTrue = true, allFalse = true;
        for (const auto& assign : targets) {
            bool val = (assign[lhs] == assign[rhs]);
            if (!val) allTrue = false;
            if (val)  allFalse = false;
        }

        auto eq = std::make_unique<EqualityNode>(Symbol::var(nameFromVar(lhs)), Symbol::var(nameFromVar(rhs)));
        if (allTrue)  return eq;
        if (allFalse) return std::make_unique<NegNode>(std::move(eq));
    }

    return buildAtomicSAT(targets, model, currentVar);
}

std::unique_ptr<ASTNode> FO2SATGenerator::buildNegSAT(int depth, const Targets& targets, const FiniteModel& model, Variable currentVar, BudgetState& budget) {
    auto complement = complementTargets(model.domainSize(), targets, currentVar);

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

std::unique_ptr<ASTNode> FO2SATGenerator::buildAndSAT(int depth, const Targets& targets, const FiniteModel& model, Variable currentVar, BudgetState& budget) {
    return std::make_unique<BinaryConnNode>(Symbol::and_(),
        buildSAT(depth - 1, targets, model, currentVar, budget),
        buildSAT(depth - 1, targets, model, currentVar, budget));
}

std::unique_ptr<ASTNode> FO2SATGenerator::buildOrSAT(int depth, const Targets& targets, const FiniteModel& model, Variable currentVar, BudgetState& budget) {
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

std::unique_ptr<ASTNode> FO2SATGenerator::buildImpliesSAT(int depth, const Targets& targets, const FiniteModel& model, Variable currentVar, BudgetState& budget) {
    Targets leftCoverTargets, rightTargets;
    for (const auto& t : targets) {
        int coin = randInt(0, 2);
        if (coin != 2) leftCoverTargets.push_back(t);
        if (coin != 0) rightTargets.push_back(t);
    }
    if (leftCoverTargets.empty()) leftCoverTargets = { targets[randInt(0, static_cast<int>(targets.size()) - 1)] };
    if (rightTargets.empty())     rightTargets = { targets[randInt(0, static_cast<int>(targets.size()) - 1)] };

    auto antecedentTargets = complementTargets(model.domainSize(), leftCoverTargets, currentVar);

    if (antecedentTargets.empty()) {
        return buildAtomicSAT(targets, model, currentVar);
    }

    return std::make_unique<BinaryConnNode>(Symbol::implies(),
        buildSAT(depth - 1, antecedentTargets, model, currentVar, budget),
        buildSAT(depth - 1, rightTargets, model, currentVar, budget));
}

std::unique_ptr<ASTNode> FO2SATGenerator::buildExistsSAT(int depth, const Targets& targets, const FiniteModel& model, Variable currentVar, BudgetState& budget) {
    Variable boundVar = 1 - currentVar;
    int dSize = model.domainSize();

    Targets bodyTargets;
    const size_t MAX_TARGETS = 1000;
    bodyTargets.reserve(std::min(targets.size(), MAX_TARGETS));

    for (const auto& t : targets) {
        if (bodyTargets.size() >= MAX_TARGETS) break;
        Assignment ex = t;
        ex[boundVar] = randInt(0, dSize - 1);
        bodyTargets.push_back(ex);
    }

    return std::make_unique<QuantifierNode>(Symbol::exists(), Symbol::var(nameFromVar(boundVar)),
        buildSAT(depth - 1, bodyTargets, model, boundVar, budget));
}

std::unique_ptr<ASTNode> FO2SATGenerator::buildForallSAT(int depth, const Targets& targets, const FiniteModel& model, Variable currentVar, BudgetState& budget) {
    Variable boundVar = 1 - currentVar;
    int dSize = model.domainSize();

    Targets bodyTargets;
    const size_t MAX_TARGETS = 1000;
    size_t requestedSize = static_cast<size_t>(targets.size()) * static_cast<size_t>(dSize);
    bodyTargets.reserve(std::min(requestedSize, MAX_TARGETS));

    for (const auto& t : targets) {
        for (int e = 0; e < dSize; ++e) {
            if (bodyTargets.size() >= MAX_TARGETS) break;
            Assignment ex = t;
            ex[boundVar] = e;
            bodyTargets.push_back(ex);
        }
        if (bodyTargets.size() >= MAX_TARGETS) break;
    }

    return std::make_unique<QuantifierNode>(Symbol::forall(), Symbol::var(nameFromVar(boundVar)),
        buildSAT(depth - 1, bodyTargets, model, boundVar, budget));
}