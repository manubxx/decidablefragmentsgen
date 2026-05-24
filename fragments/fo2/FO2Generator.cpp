#include "FO2Generator.hpp"
#include <stdexcept>
#include <numeric>
#include <algorithm>
#include <functional>


FO2Generator::FO2Generator(std::vector<PredInfo> vocab, unsigned seed)
    : FormulaBuilder(seed), vocab_(std::move(vocab))
{
    if (vocab_.empty())
        throw std::invalid_argument("FO2Generar: vocabolario vuoto");
    for (const auto& p : vocab_)
        if (p.arity < 1 || p.arity > 2)
            throw std::invalid_argument(
                "FO2Generator: predicato '" + p.name + "' ha arita' " +
                std::to_string(p.arity) + " (FO2 ammette solo arita' 1 o 2)");
}


std::string FO2Generator::generateFormatted(const GenConfig& cfg)
{
    activeVocab_.clear();
    for (const auto& p : vocab_)
        if (cfg.arityFilter == 0 || p.arity == cfg.arityFilter)
            activeVocab_.push_back(p);

    if (activeVocab_.empty())
        throw std::invalid_argument(
            "FO2Generator: nessun predicato con arita' " +
            std::to_string(cfg.arityFilter) + " nel vocabolario");

    return FormulaBuilder::generateFormatted(cfg);
}


// ──── buildAtomic ────
// In FO2 le variabili in scope sono currentVar e nextVar(currentVar).
// L'uguaglianza viene generata qui, in alternativa casuale ai predicati,
// quando il budget eq_left lo consente (il consumo è già avvenuto in pickType,
// ma buildAtomic può essere chiamata anche direttamente al caso base depth==0:
// in quel caso non consuma budget, genera solo un atomo predicativo ordinario).
std::unique_ptr<AtomicNode> FO2Generator::buildAtomic(const std::string& currentVar)
{
    const std::string other = nextVar(currentVar);
    const auto& p = activeVocab_[randInt(0, static_cast<int>(activeVocab_.size()) - 1)];

    std::vector<Symbol> args;
    if (p.arity == 1) {
        args.push_back(Symbol::var(currentVar));
    }
    else {
        if (randInt(0, 1) == 0) {
            args.push_back(Symbol::var(currentVar));
            args.push_back(Symbol::var(other));
        }
        else {
            args.push_back(Symbol::var(other));
            args.push_back(Symbol::var(currentVar));
        }
    }
    return std::make_unique<AtomicNode>(Symbol::pred(p.name, p.arity), std::move(args));
}


// ──── buildEqualityAtom ────
// Costruisce un EqualityNode in FO2 con le due variabili disponibili.
// Sceglie casualmente tra le quattro combinazioni possibili:
//   v1=v1,  v1=v2,  v2=v1,  v2=v2.
// (v1=v1 e v2=v2 sono tautologie, ma sono sintatticamente lecite in FO2.)
std::unique_ptr<EqualityNode> FO2Generator::buildEqualityAtom(const std::string& currentVar)
{
    const std::string other = nextVar(currentVar);

    // Sceglie casualmente lhs e rhs fra currentVar e other
    const std::string lhs = (randInt(0, 1) == 0) ? currentVar : other;
    const std::string rhs = (randInt(0, 1) == 0) ? currentVar : other;

    return std::make_unique<EqualityNode>(Symbol::var(lhs), Symbol::var(rhs));
}


// ──── build (override) ────
// Intercetta il caso EQUALITY prima di delegare al base.
// Necessario perché FormulaBuilder::build produce EqualityNode tramite
// nextVar, che in FO2 è corretto, ma qui vogliamo usare buildEqualityAtom
// per avere la scelta casuale su tutte le combinazioni (incluso lhs/rhs invertiti).
std::unique_ptr<ASTNode> FO2Generator::build(int depth, const std::string& currentVar,
    BudgetState& budget)
{
    if (depth == 0)
        return buildAtomic(currentVar);

    auto candidates = candidateTypes(depth, budget);
    if (candidates.empty())
        return buildAtomic(currentVar);

    SymbolType chosen = pickType(depth, budget);

    if (chosen == SymbolType::EQUALITY)
        return buildEqualityAtom(currentVar);


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
    case SymbolType::EQUALITY:
    return buildEqualityAtom(currentVar);
    

    default:
        return buildAtomic(currentVar);
    }
}


// ──── generateSAT ────
std::unique_ptr<ASTNode> FO2Generator::generateSAT(int depth, int domainSize,
    BudgetState& budget)
{
    FiniteModel model(activeVocab_, rng_, domainSize);
    const auto& D = model.domain();

    std::vector<std::string> domCopy = D;
    std::shuffle(domCopy.begin(), domCopy.end(), rng_);
    int ns = randInt(1, static_cast<int>(domCopy.size()));
    domCopy.resize(ns);

    bool rootExists = (randInt(0, 1) == 0);

    Targets bodyTargets;
    if (rootExists) {
        for (const auto& e : domCopy) {
            Assignment a;
            a["v1"] = e;
            bodyTargets.push_back(a);
        }
    }
    else {
        for (const auto& e : D) {
            Assignment a;
            a["v1"] = e;
            bodyTargets.push_back(a);
        }
    }

    int bodyDepth = (depth > 0) ? depth - 1 : 0;
    auto body = buildSAT(bodyDepth, bodyTargets, model, "v1", budget);

    auto rootQ = rootExists ? Symbol::exists() : Symbol::forall();
    return std::make_unique<QuantifierNode>(rootQ, Symbol::var("v1"), std::move(body));
}


// ──── buildSAT ────
std::unique_ptr<ASTNode> FO2Generator::buildSAT(int depth,
    const Targets& targets, const FiniteModel& model,
    const std::string& currentVar, BudgetState& budget)
{
    if (depth == 0)
        return buildAtomicSAT(targets, model, currentVar);

    auto candidates = candidateTypes(depth, budget);
    if (candidates.empty())
        return buildAtomicSAT(targets, model, currentVar);

    SymbolType chosen = pickType(depth, budget);

    switch (chosen) {
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


// ──── buildAtomicSAT ────
std::unique_ptr<ASTNode> FO2Generator::buildAtomicSAT(const Targets& targets,
    const FiniteModel& model, const std::string& currentVar)
{
    const std::string other = nextVar(currentVar);

    auto atomArgs = [&](int arity) -> std::vector<Symbol> {
        std::vector<Symbol> a;
        if (arity == 1) {
            a.push_back(Symbol::var(currentVar));
        }
        else {
            if (randInt(0, 1) == 0) {
                a.push_back(Symbol::var(currentVar));
                a.push_back(Symbol::var(other));
            }
            else {
                a.push_back(Symbol::var(other));
                a.push_back(Symbol::var(currentVar));
            }
        }
        return a;
        };

    auto atomArgNames = [](const std::vector<Symbol>& args) -> std::vector<std::string> {
        std::vector<std::string> names;
        for (const auto& s : args) names.push_back(s.name);
        return names;
        };

    std::vector<int> idx(activeVocab_.size());
    std::iota(idx.begin(), idx.end(), 0);
    std::shuffle(idx.begin(), idx.end(), rng_);

    for (int i : idx) {
        const auto& p = activeVocab_[i];
        auto args = atomArgs(p.arity);
        auto argN = atomArgNames(args);

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

    int bestIdx = idx[0], bestCount = 0;
    for (int i : idx) {
        const auto& candidates_ = activeVocab_[i];
        auto        candidatesArgs = atomArgs(candidates_.arity);
        auto        candidateNames = atomArgNames(candidatesArgs);
        int cnt = 0;
        for (const auto& assign : targets)
            if (model.evalAtom(candidates_.name, candidates_.arity, candidateNames, assign)) ++cnt;
        if (cnt > bestCount) { bestCount = cnt; bestIdx = i; }
    }
    const auto& bestpred = activeVocab_[bestIdx];
    auto        bestargs = atomArgs(bestpred.arity);
    auto        bestatom = std::make_unique<AtomicNode>(Symbol::pred(bestpred.name, bestpred.arity), bestargs);
    if (bestCount == 0) return std::make_unique<NegNode>(std::move(bestatom));
    return bestatom;
}


// ──── buildEqualitySAT ────
// Cerca una combinazione (lhs, rhs) tra le quattro possibili in FO2
// tale che l'atomo di uguaglianza (o la sua negazione) sia vero su tutti i target.

std::unique_ptr<ASTNode> FO2Generator::buildEqualitySAT(const Targets& targets,
    const FiniteModel& model, const std::string& currentVar)
{
    const std::string other = nextVar(currentVar);

    // Le quattro combinazioni possibili in FO2
    const std::pair<std::string, std::string> combos[4] = {
        {currentVar, currentVar},
        {currentVar, other},
        {other,      currentVar},
        {other,      other},
    };

    // Ordine di visita casuale
    int order[4] = {0, 1, 2, 3};
    for (int i = 3; i > 0; --i) {
        int j = randInt(0, i);
        std::swap(order[i], order[j]);
    }

    for (int k : order) {
        const auto& [lhsName, rhsName] = combos[k];

        bool allTrue = true, allFalse = true;
        for (const auto& assign : targets) {
            // Recupera i valori nell'assignment; se la variabile non è presente si salta.
            auto itL = assign.find(lhsName);
            auto itR = assign.find(rhsName);
            if (itL == assign.end() || itR == assign.end()) {
                // Variabile fuori scope: non possiamo valutare, saltiamo questa combo.
                allTrue = false; allFalse = false; break;
            }
            bool val = (itL->second == itR->second);
            if (!val) allTrue  = false;
            if (val)  allFalse = false;
        }

        auto eq = std::make_unique<EqualityNode>(Symbol::var(lhsName), Symbol::var(rhsName));
        if (allTrue)  return eq;
        if (allFalse) return std::make_unique<NegNode>(std::move(eq));
    }

    // Nessuna combinazione uniforme: fallback su atomo predicativo
    return buildAtomicSAT(targets, model, currentVar);
}


// ──── buildNegSAT ────
std::unique_ptr<ASTNode> FO2Generator::buildNegSAT(int depth,
    const Targets& targets, const FiniteModel& model,
    const std::string& currentVar, BudgetState& budget)
{
    const auto& D = model.domain();

    Targets allAssign;
    for (const auto& e1 : D)
        for (const auto& e2 : D)
            allAssign.push_back({ {"v1", e1}, {"v2", e2} });

    Targets complement;
    for (const auto& a : allAssign) {
        bool found = false;
        for (const auto& t : targets) if (t == a) { found = true; break; }
        if (!found) complement.push_back(a);
    }

    if (complement.empty()) {
        std::vector<int> idx2(activeVocab_.size());
        std::iota(idx2.begin(), idx2.end(), 0);
        std::shuffle(idx2.begin(), idx2.end(), rng_);
        for (int i : idx2) {
            const auto& p = activeVocab_[i];
            std::vector<std::string> argN;
            if (p.arity == 1) {
                argN = { currentVar };
            }
            else {
                argN = (randInt(0, 1) == 0)
                    ? std::vector<std::string>{ currentVar, nextVar(currentVar) }
                : std::vector<std::string>{ nextVar(currentVar), currentVar };
            }
            bool allFalse = true;
            for (const auto& t : targets)
                if (model.evalAtom(p.name, p.arity, argN, t)) { allFalse = false; break; }
            if (allFalse) {
                std::vector<Symbol> args;
                for (const auto& n : argN) args.push_back(Symbol::var(n));
                auto atom = std::make_unique<AtomicNode>(Symbol::pred(p.name, p.arity), args);
                return std::make_unique<NegNode>(std::move(atom));
            }
        }
        throw std::logic_error(
            "buildNegSAT: complemento vuoto e nessun atomo uniformemente falso -> retry");
    }

    auto child = buildSAT(depth - 1, complement, model, currentVar, budget);

    std::function<bool(const ASTNode&, const Assignment&)> evalNode;
    evalNode = [&](const ASTNode& node, const Assignment& assign) -> bool {
        if (auto* a = dynamic_cast<const AtomicNode*>(&node)) {
            std::vector<std::string> argN;
            for (const auto& s : a->args()) argN.push_back(s.name);
            return model.evalAtom(a->predSymbol().name, a->predSymbol().arity, argN, assign);
        }
        // ── Uguaglianza ──
        if (auto* e = dynamic_cast<const EqualityNode*>(&node)) {
            auto itL = assign.find(e->lhs().name);
            auto itR = assign.find(e->rhs().name);
            if (itL == assign.end() || itR == assign.end()) return false;
            return itL->second == itR->second;
        }
        if (auto* n = dynamic_cast<const NegNode*>(&node))
            return !evalNode(n->child(), assign);
        if (auto* b = dynamic_cast<const BinaryConnNode*>(&node)) {
            bool lv = evalNode(b->left(), assign);
            bool rv = evalNode(b->right(), assign);
            switch (b->connSymbol().type) {
            case SymbolType::AND:     return lv && rv;
            case SymbolType::OR:      return lv || rv;
            case SymbolType::IMPLIES: return !lv || rv;
            default: return false;
            }
        }
        if (auto* q = dynamic_cast<const QuantifierNode*>(&node)) {
            const std::string& vname = q->var().name;
            bool isExists = (q->quantSymbol().type == SymbolType::EXISTS);
            for (const auto& e : model.domain()) {
                Assignment ext = assign;
                ext[vname] = e;
                bool val = evalNode(q->body(), ext);
                if (isExists && val) return true;
                if (!isExists && !val) return false;
            }
            return !isExists;
        }
        return false;
        };

    for (const auto& t : targets)
        if (evalNode(*child, t))
            throw std::logic_error("buildNegSAT: child vera su un target -> retry");

    return std::make_unique<NegNode>(std::move(child));
}


// ──── buildAndSAT ────
std::unique_ptr<ASTNode> FO2Generator::buildAndSAT(int depth,
    const Targets& targets, const FiniteModel& model,
    const std::string& currentVar, BudgetState& budget)
{
    auto left = buildSAT(depth - 1, targets, model, currentVar, budget);
    auto right = buildSAT(depth - 1, targets, model, currentVar, budget);
    return std::make_unique<BinaryConnNode>(Symbol::and_(), std::move(left), std::move(right));
}


// ──── buildOrSAT ────
std::unique_ptr<ASTNode> FO2Generator::buildOrSAT(int depth,
    const Targets& targets, const FiniteModel& model,
    const std::string& currentVar, BudgetState& budget)
{
    Targets leftT, rightT;
    for (const auto& t : targets) {
        int coin = randInt(0, 2);
        if (coin != 2) leftT.push_back(t);
        if (coin != 0) rightT.push_back(t);
    }
    if (leftT.empty())  leftT = { targets[randInt(0, static_cast<int>(targets.size()) - 1)] };
    if (rightT.empty()) rightT = { targets[randInt(0, static_cast<int>(targets.size()) - 1)] };

    auto left = buildSAT(depth - 1, leftT, model, currentVar, budget);
    auto right = buildSAT(depth - 1, rightT, model, currentVar, budget);
    return std::make_unique<BinaryConnNode>(Symbol::or_(), std::move(left), std::move(right));
}


// ──── buildImpliesSAT ────
std::unique_ptr<ASTNode> FO2Generator::buildImpliesSAT(int depth,
    const Targets& targets, const FiniteModel& model,
    const std::string& currentVar, BudgetState& budget)
{
    const auto& D = model.domain();

    Targets leftCoverTargets, rightTargets;
    for (const auto& t : targets) {
        int coin = randInt(0, 2);
        if (coin != 2) leftCoverTargets.push_back(t);
        if (coin != 0) rightTargets.push_back(t);
    }
    if (leftCoverTargets.empty())
        leftCoverTargets = { targets[randInt(0, static_cast<int>(targets.size()) - 1)] };
    if (rightTargets.empty())
        rightTargets = { targets[randInt(0, static_cast<int>(targets.size()) - 1)] };

    Targets allAssign;
    for (const auto& e1 : D)
        for (const auto& e2 : D)
            allAssign.push_back({ {"v1", e1}, {"v2", e2} });

    Targets antecedentTargets;
    for (const auto& a : allAssign) {
        bool inLeft = false;
        for (const auto& t : leftCoverTargets) if (t == a) { inLeft = true; break; }
        if (!inLeft) antecedentTargets.push_back(a);
    }

    if (antecedentTargets.empty()) {
        int keep = std::max(1, static_cast<int>(leftCoverTargets.size()) / 2);
        std::shuffle(leftCoverTargets.begin(), leftCoverTargets.end(), rng_);
        leftCoverTargets.resize(keep);
        antecedentTargets.clear();
        for (const auto& a : allAssign) {
            bool inLeft = false;
            for (const auto& t : leftCoverTargets) if (t == a) { inLeft = true; break; }
            if (!inLeft) antecedentTargets.push_back(a);
        }
        if (antecedentTargets.empty())
            throw std::logic_error(
                "buildImpliesSAT: impossibile costruire antecedente non vuoto -> retry");
    }

    auto antecedent = buildSAT(depth - 1, antecedentTargets, model, currentVar, budget);
    auto consequent = buildSAT(depth - 1, rightTargets, model, currentVar, budget);
    return std::make_unique<BinaryConnNode>(
        Symbol::implies(), std::move(antecedent), std::move(consequent));
}


// ──── buildExistsSAT ────
std::unique_ptr<ASTNode> FO2Generator::buildExistsSAT(int depth,
    const Targets& targets, const FiniteModel& model,
    const std::string& currentVar, BudgetState& budget)
{
    const std::string boundVar = nextVar(currentVar);
    const auto& D = model.domain();

    Targets bodyTargets;
    for (const auto& t : targets) {
        Assignment ex = t;
        ex[boundVar] = D[randInt(0, static_cast<int>(D.size()) - 1)];
        bodyTargets.push_back(ex);
    }

    auto body = buildSAT(depth - 1, bodyTargets, model, boundVar, budget);
    return std::make_unique<QuantifierNode>(
        Symbol::exists(), Symbol::var(boundVar), std::move(body));
}


// ──── buildForallSAT ────
std::unique_ptr<ASTNode> FO2Generator::buildForallSAT(int depth,
    const Targets& targets, const FiniteModel& model,
    const std::string& currentVar, BudgetState& budget)
{
    const std::string boundVar = nextVar(currentVar);
    const auto& D = model.domain();

    Targets bodyTargets;
    for (const auto& t : targets)
        for (const auto& e : D) {
            Assignment ex = t;
            ex[boundVar] = e;
            bodyTargets.push_back(ex);
        }

    auto body = buildSAT(depth - 1, bodyTargets, model, boundVar, budget);
    return std::make_unique<QuantifierNode>(
        Symbol::forall(), Symbol::var(boundVar), std::move(body));
}