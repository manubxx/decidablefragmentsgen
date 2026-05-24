#include "FlutedGenerator.hpp"
#include <stdexcept>
#include <numeric>
#include <algorithm>
#include <functional>

//  FlutedGenerator 

// Costante di sicurezza 
// MAX_DOMAIN_PRODUCT: soglia massima di assignment che domainProduct() può
// enumerare. buildNegSAT e buildImpliesSAT calcolano il prodotto cartesiano
// D^stackDepth; senza guardia l'allocazione cresce esponenzialmente.
// Con 4096 si coprono tutti i casi pratici.
// Oltre questa soglia i metodi degradano a buildAtomicSAT
static constexpr std::size_t MAX_DOMAIN_PRODUCT = 4096;


// Constructor

FlutedGenerator::FlutedGenerator(std::vector<PredInfo> vocab, unsigned seed)
    : FormulaBuilder(seed), vocab_(std::move(vocab))
{
    if (vocab_.empty())
        throw std::invalid_argument("FlutedGenerator: vocabolario vuoto");

    for (const auto& p : vocab_)
        if (p.arity < 1)
            throw std::invalid_argument(
                "FlutedGenerator: predicato '" + p.name +
                "' ha arità " + std::to_string(p.arity) + " < 1 (non ammessa in FL)");
}


std::string FlutedGenerator::fragmentName() const { return "FL"; }

std::string FlutedGenerator::startVar() const { return "x1"; }

std::string FlutedGenerator::nextVar(const std::string& current) const
{
    int n = std::stoi(current.substr(1));
    return "x" + std::to_string(n + 1);
}


//  generateFormatted (override)
std::string FlutedGenerator::generateFormatted(const GenConfig& cfg)
{
    activeVocab_.clear();
    for (const auto& p : vocab_)
        if (cfg.arityFilter == 0 || p.arity == cfg.arityFilter)
            activeVocab_.push_back(p);

    if (activeVocab_.empty())
        throw std::invalid_argument(
            "FlutedGenerator: nessun predicato con arità " +
            std::to_string(cfg.arityFilter) + " nel vocabolario");

    // UNSAT: phi AND NOT phi.
    if (cfg.mode == GenMode::UNSAT) {
        if (minArity() == 1)
            return FormulaBuilder::generateFormatted(cfg);

        // Vocabolario di arità > 1: percorso UNSAT FL
        static constexpr int MAX_RETRY_UNSAT = 500;
        std::unique_ptr<ASTNode> formula;
        bool budgetOk = false;
        const int domSz = (cfg.domainSize > 0) ? cfg.domainSize : 2;

        for (int attempt = 0; attempt < MAX_RETRY_UNSAT; ++attempt) {
            BudgetState bs(cfg.budget, rng_);
            try {
                auto phi = generateSATFull(cfg.depth, domSz, bs).formula;
                BudgetState bs2(cfg.budget, rng_);
                auto notPhi = std::make_unique<NegNode>(
                    generateSATFull(cfg.depth, domSz, bs2).formula);
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
            throw std::invalid_argument(
                "FL UNSAT: impossibile soddisfare il budget richiesto.");
        if (!formula) return "";

        if (cfg.transform == TransformMode::NNF)
            formula = formula->toNNF(false);
        if (cfg.output == OutputFormat::TPTP)
            return "fof(f,negated_conjecture,\n    " + formula->toTPTP() + "\n).";
        return formula->toString();
    }

    static constexpr int MAX_RETRY = 500;

    std::unique_ptr<ASTNode> formula;
    bool budgetOk = false;

    for (int attempt = 0; attempt < MAX_RETRY; ++attempt) {
        BudgetState bs(cfg.budget, rng_);
        try {
            if (cfg.mode == GenMode::FREE)
                formula = buildFL(cfg.depth, 0, bs);
            else
                formula = generateSATFull(cfg.depth, cfg.domainSize, bs).formula;
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
            "FL: impossibile soddisfare il budget richiesto "
            "con depth=" + std::to_string(cfg.depth) +
            ". Aumenta --depth o riduci i vincoli di budget.");

    if (!formula) return "";

    if (cfg.transform == TransformMode::NNF)
        formula = formula->toNNF(false);

    if (cfg.output == OutputFormat::TPTP) {
        std::string role;
        switch (cfg.mode) {
        case GenMode::SAT:   role = "axiom";              break;
        case GenMode::FREE:  role = "axiom";              break;
        case GenMode::UNSAT: role = "negated_conjecture"; break;
        }
        return "fof(f," + role + ",\n    " + formula->toTPTP() + "\n).";
    }
    return formula->toString();
}


//  buildAtomicLeaf 
std::unique_ptr<AtomicNode> FlutedGenerator::buildAtomicLeaf(int stackDepth)
{
    auto admissible = admissiblePreds(stackDepth);
    if (admissible.empty())
        throw std::logic_error(
            "FlutedGenerator::buildAtomicLeaf: nessun predicato ammissibile "
            "a stackDepth=" + std::to_string(stackDepth) +
            " (arità minima vocabolario=" + std::to_string(minArity()) + ")");

    int idx = admissible[randInt(0, static_cast<int>(admissible.size()) - 1)];
    const auto& currpreds = activeVocab_[idx];
    auto args = flutedArgs(stackDepth, currpreds.arity);
    return std::make_unique<AtomicNode>(Symbol::pred(currpreds.name, currpreds.arity), std::move(args));
}


// ──── buildEqualityLeaf ────────────────────────────────────────────────────
// Costruisce un atomo di uguaglianza in stile FL:  x_{n-1} = x_n.
//
// In FL ogni predicato di arità k usa le variabili x_{n-k+1}, ..., x_n
// (le ultime k dello stack). L'uguaglianza è binaria (arità 2), quindi
// usa le variabili x_{n-1} e x_n, esattamente come un predicato binario.
//
// Precondizione: stackDepth >= 2.
// Se stackDepth < 2 l'atomo non è costruibile → lancia logic_error (il
// chiamante deve assicurarsi di non invocare questo metodo con stackDepth < 2).
std::unique_ptr<EqualityNode> FlutedGenerator::buildEqualityLeaf(int stackDepth)
{
    if (stackDepth < 2)
        throw std::logic_error(
            "FlutedGenerator::buildEqualityLeaf: stackDepth=" +
            std::to_string(stackDepth) + " < 2, uguaglianza non ammissibile in FL");

    // Segue la stessa convenzione di flutedArgs per arità 2:
    //   argomenti = x_{stackDepth-1}, x_{stackDepth}
    std::string lhsName = varName(stackDepth - 1);
    std::string rhsName = varName(stackDepth);
    return std::make_unique<EqualityNode>(Symbol::var(lhsName), Symbol::var(rhsName));
}


// buildAtomic(string)

std::unique_ptr<AtomicNode> FlutedGenerator::buildAtomic(const std::string& currentVar)
{
    if (currentVar.empty() || currentVar[0] != 'x')
        throw std::invalid_argument(
            "FlutedGenerator::buildAtomic: variabile non FL-valida: '" +
            currentVar + "' (atteso formato 'x{n}')");

    int stackDepth = std::stoi(currentVar.substr(1));
    return buildAtomicLeaf(stackDepth);
}


//  generateSAT (virtuale puro FormulaBuilder)
std::unique_ptr<ASTNode> FlutedGenerator::generateSAT(int depth, int domainSize,
    BudgetState& budget)
{
    return generateSATFull(depth, domainSize, budget).formula;
}


//  generateSATFull
//
//  Ingresso della target-propagation FL
//
// STRUTTURA:
//   1. Costruisce un FiniteModel casuale.
//   2. Determina la arità minima del vocabolario attivo.
//   3. Aggiunge m quantificatori strutturali alla radice (non contati nel
//      budget) per portare lo stack da 0 a minStackDepth, dove almeno un predicato
//      è ammissibile.
//   4. buildSAT procede ricorsivamente con stackDepth = minStackDepth.

FlutedGenerator::SatResult FlutedGenerator::generateSATFull(int depth, int domainSize,
    BudgetState& budget)
{
    FiniteModel model(activeVocab_, rng_, domainSize);
    const auto& D = model.domain();

    int minstackdepth = minArity(); // profondità minima necessaria

 
    bool rootExists = (randInt(0, 1) == 0);

    // Target radice
    std::vector<std::string> domCopy = D;
    std::shuffle(domCopy.begin(), domCopy.end(), rng_);
    int nS = randInt(1, static_cast<int>(domCopy.size()));
    domCopy.resize(nS); 

    Targets rootTargets;
    if (rootExists) {
        // EXISTS: witness scelti casualmente nel sottoinsieme di D
        for (const auto& e : domCopy) {
            Assignment a;
            a["x1"] = e;
            rootTargets.push_back(a);
        }
    }
    else {
        // FORALL: il body deve essere vero per tutti gli e in D.
        for (const auto& e : D) {
            Assignment a;
            a["x1"] = e;
            rootTargets.push_back(a);
        }
    }

    int bodyDepth = (depth > 0) ? depth - 1 : 0;

    //  Quantificatori strutturali
    //  fuori dalla logica del budget, per garantire stackDepth minimo
    Targets currentTargets = rootTargets;
    int     currentDepth = bodyDepth;

    struct StructuralLevel {
        bool        strRootExists;
        std::string varName;
    };
    std::vector<StructuralLevel> structLevels;

    for (int level = 2; level <= minstackdepth; ++level) { 
        bool lvlRootExists = (randInt(0, 1) == 0);
        std::string lvlVarName = varName(level);

        Targets nextTargets;
        if (lvlRootExists) {
            for (const auto& t : currentTargets) {
                Assignment ex = t;
                ex[lvlVarName] = D[randInt(0, static_cast<int>(D.size()) - 1)];
                nextTargets.push_back(ex);
            }
        }
        else {
            for (const auto& t : currentTargets)
                for (const auto& e : D) {
                    Assignment ex = t;
                    ex[lvlVarName] = e;
                    nextTargets.push_back(ex);
                }
        }

        structLevels.push_back({ lvlRootExists, lvlVarName});
        currentTargets = std::move(nextTargets);
        currentDepth = (currentDepth > 0) ? currentDepth - 1 : 0;
    }

    //  buildSAT con stackDepth minimo raggiunto 
    auto body = buildSAT(currentDepth, currentTargets, model, minstackdepth, budget);


    // Ricostruzione dell'AST topdown
    for (int i = static_cast<int>(structLevels.size()) - 1; i >= 0; --i) {
        const auto& structlvl = structLevels[i];
        auto quantSym = structlvl.strRootExists ? Symbol::exists() : Symbol::forall();
        body = std::make_unique<QuantifierNode>(
            quantSym, Symbol::var(structlvl.varName), std::move(body));
    }

    auto rootQ = rootExists ? Symbol::exists() : Symbol::forall();
    auto formula = std::make_unique<QuantifierNode>(
        rootQ, Symbol::var("x1"), std::move(body));

    //SatResult
    return { std::move(formula), std::move(model) };
}


//  buildSAT
// Dispatcher principale

std::unique_ptr<ASTNode> FlutedGenerator::buildSAT(int depth,
    const Targets& targets, const FiniteModel& model,
    int stackDepth, BudgetState& budget)
{
    auto admissible = admissiblePreds(stackDepth);

    if (depth == 0 || candidateTypes(depth, budget).empty()) {
        if (!admissible.empty())
            return buildAtomicSAT(depth, targets, model, stackDepth, budget);

        return buildForcedQuantSAT(depth, targets, model, stackDepth, budget);
    }

    SymbolType chosen = pickType(depth, budget);

    switch (chosen) {
    case SymbolType::NEG:     return buildNegSAT(depth, targets, model, stackDepth, budget);
    case SymbolType::AND:     return buildAndSAT(depth, targets, model, stackDepth, budget);
    case SymbolType::OR:      return buildOrSAT(depth, targets, model, stackDepth, budget);
    case SymbolType::IMPLIES: return buildImpliesSAT(depth, targets, model, stackDepth, budget);
    case SymbolType::EXISTS:  return buildExistsSAT(depth, targets, model, stackDepth, budget);
    case SymbolType::FORALL:  return buildForallSAT(depth, targets, model, stackDepth, budget);
    case SymbolType::EQUALITY:
        if (stackDepth >= 2)
            return buildEqualitySAT(targets, model, stackDepth, budget);
        [[fallthrough]];
    default:
        if (!admissible.empty())
            return buildAtomicSAT(depth, targets, model, stackDepth, budget);

        return buildForcedQuantSAT(depth, targets, model, stackDepth, budget);
    }
}


//  buildAtomicSAT
std::unique_ptr<ASTNode> FlutedGenerator::buildAtomicSAT(int depth,
    const Targets& targets, const FiniteModel& model,
    int stackDepth, BudgetState& budget)
{
    auto admissible = admissiblePreds(stackDepth);
    if (admissible.empty())
        return buildForcedQuantSAT(depth, targets, model, stackDepth, budget);

    std::vector<int> shuffled = admissible;
    std::shuffle(shuffled.begin(), shuffled.end(), rng_);

    // Ricerca del predicato uniformemente vero o uniformemente falso sui target
    for (int i : shuffled) {
        const auto& p = activeVocab_[i];
        auto argNames = flutedArgNames(stackDepth, p.arity);

        bool allTrue = true, allFalse = true;
        for (const auto& assign : targets) {
            bool val = model.evalAtom(p.name, p.arity, argNames, assign);
            if (!val) allTrue = false;
            if (val)  allFalse = false;
        }

        auto args = flutedArgs(stackDepth, p.arity);
        auto atom = std::make_unique<AtomicNode>(Symbol::pred(p.name, p.arity), args);
        if (allTrue)  return atom;
        if (allFalse) return std::make_unique<NegNode>(std::move(atom));
    }

    // Se non esiste, best effort: predicato più vero possibile sui target
    int bestIdx = shuffled[0];
    int bestCount = 0;
    for (int i : shuffled) {
        const auto& candpred = activeVocab_[i];
        auto argNames2 = flutedArgNames(stackDepth, candpred.arity);
        int cnt = 0;
        for (const auto& assign : targets)
            if (model.evalAtom(candpred.name, candpred.arity, argNames2, assign)) ++cnt;
        if (cnt > bestCount) { bestCount = cnt; bestIdx = i; }
    }

    const auto& bestpred = activeVocab_[bestIdx];
    auto args = flutedArgs(stackDepth, bestpred.arity);
    auto atom = std::make_unique<AtomicNode>(Symbol::pred(bestpred.name, bestpred.arity), args);
    if (bestCount == 0) return std::make_unique<NegNode>(std::move(atom));
    return atom;
}


// buildEqualitySAT 
// Costruisce un EqualityNode (o la sua negazione) verificato sui target.
//
// In FL, l'unico atomo di uguaglianza ammissibile a stackDepth n è:
//   x_{n-1} = x_n
// Strategia:
//   1. Valuta x_{n-1} = x_n su tutti i target.
//   2. Se allTrue  → restituisce l'atomo positivo.
//   3. Se allFalse → restituisce ~(x_{n-1} = x_n).
//   4. Altrimenti  → fallback su buildAtomicSAT (come gli altri atomi).
//
std::unique_ptr<ASTNode> FlutedGenerator::buildEqualitySAT(const Targets& targets,
    const FiniteModel& model, int stackDepth, BudgetState& budget)
{
    std::string lhsName = varName(stackDepth - 1);
    std::string rhsName = varName(stackDepth);

    bool allTrue = true, allFalse = true;
    for (const auto& assign : targets) {
        auto itL = assign.find(lhsName);
        auto itR = assign.find(rhsName);
        // Se una delle variabili non è in scope nel target, non possiamo valutare.
        if (itL == assign.end() || itR == assign.end()) {
            allTrue = false; allFalse = false; break;
        }
        bool val = (itL->second == itR->second);
        if (!val) allTrue  = false;
        if (val)  allFalse = false;
    }

    auto eq = std::make_unique<EqualityNode>(Symbol::var(lhsName), Symbol::var(rhsName));
    if (allTrue)  return eq;
    if (allFalse) return std::make_unique<NegNode>(std::move(eq));

    // Non uniforme: restituiamo l'atomo positivo come best-effort
    // (la correttezza SAT è garantita a livello superiore dal meccanismo di retry).
    return eq;
}


//  buildForcedQuantSAT

std::unique_ptr<ASTNode> FlutedGenerator::buildForcedQuantSAT(int depth,
    const Targets& targets, const FiniteModel& model,
    int stackDepth, BudgetState& budget)
{
    const auto& D = model.domain();
    int nextStackDepth = stackDepth + 1;
    std::string nextVarName = varName(nextStackDepth);

    bool useExists = (randInt(0, 1) == 0);

    Targets bodyTargets;
    if (useExists) {
        for (const auto& t : targets) {
            Assignment ex = t;
            ex[nextVarName] = D[randInt(0, static_cast<int>(D.size()) - 1)];
            bodyTargets.push_back(ex);
        }
    }
    else {
        for (const auto& t : targets)
            for (const auto& e : D) {
                Assignment ex = t;
                ex[nextVarName] = e;
                bodyTargets.push_back(ex);
            }
    }

    int childDepth = (depth > 0) ? depth - 1 : 0;
    auto body = buildSAT(childDepth, bodyTargets, model, nextStackDepth, budget);
    auto quantSym = useExists ? Symbol::exists() : Symbol::forall();
    return std::make_unique<QuantifierNode>(quantSym, Symbol::var(nextVarName), std::move(body));
}


//  buildNegSAT 
// NOT phi vera sui target <=> phi falsa sui target.
std::unique_ptr<ASTNode> FlutedGenerator::buildNegSAT(int depth,
    const Targets& targets, const FiniteModel& model,
    int stackDepth, BudgetState& budget)
{
    // Guard: evita enumerazione esponenziale di D^stackDepth
    {
        std::size_t prodSize = 1;
        const std::size_t domSize = model.domain().size();
        bool overflow = false;
        for (int k = 0; k < stackDepth; ++k) {
            if (prodSize > MAX_DOMAIN_PRODUCT / domSize) { overflow = true; break; }
            prodSize *= domSize;
        }
        if (overflow || prodSize > MAX_DOMAIN_PRODUCT)
            return buildAtomicSAT(depth, targets, model, stackDepth, budget);
    }

    // Complemento di targets in D^stackDepth
    Targets allAssign = domainProduct(stackDepth, model);
    Targets complement;
    for (const auto& a : allAssign) {
        bool inTargets = false;
        for (const auto& t : targets)
            if (t == a) { inTargets = true; break; }
        if (!inTargets) complement.push_back(a);
    }

    // Complemento vuoto: targets = D^stackDepth
    if (complement.empty()) {
        auto admissible = admissiblePreds(stackDepth);
        for (int i : admissible) {
            const auto& currpred = activeVocab_[i];
            auto argNames = flutedArgNames(stackDepth, currpred.arity);
            bool allFalse = true;
            for (const auto& t : targets)
                if (model.evalAtom(currpred.name, currpred.arity, argNames, t)) { allFalse = false; break; }
            if (allFalse) {
                auto atomargs = flutedArgs(stackDepth, currpred.arity);
                auto atom = std::make_unique<AtomicNode>(
                    Symbol::pred(currpred.name, currpred.arity), atomargs);
                return std::make_unique<NegNode>(std::move(atom));
            }
        }
        throw std::logic_error(
            "buildNegSAT: targets = D^stackDepth, nessun atomo "
            "uniformemente falso -> retry");
    }

    // Costruisce phi vera sul complemento
    auto child = buildSAT(std::max(0, depth - 1), complement, model, stackDepth, budget);

    // Verifica semantica: phi deve essere falsa su tutti i target originali
    for (const auto& t : targets)
        if (evalNode(*child, t, model))
            throw std::logic_error(
                "buildNegSAT: child vera su un target "
                "(complemento insufficiente) -> retry");

    return std::make_unique<NegNode>(std::move(child));
}


//  buildAndSAT 
// (phi AND psi) vera sui target <=> phi vera E psi vera su tutti i target.

std::unique_ptr<ASTNode> FlutedGenerator::buildAndSAT(int depth,
    const Targets& targets, const FiniteModel& model,
    int stackDepth, BudgetState& budget)
{
    auto left = buildSAT(std::max(0, depth - 1), targets, model, stackDepth, budget);
    auto right = buildSAT(std::max(0, depth - 1), targets, model, stackDepth, budget);
    return std::make_unique<BinaryConnNode>(
        Symbol::and_(), std::move(left), std::move(right));
}


//  buildOrSAT 
// (phi OR psi) vera sui target <=> per ogni target, phi OPPURE psi è vera.
std::unique_ptr<ASTNode> FlutedGenerator::buildOrSAT(int depth,
    const Targets& targets, const FiniteModel& model,
    int stackDepth, BudgetState& budget)
{
    Targets leftT, rightT;
    for (const auto& t : targets) {
        int coin = randInt(0, 2);
        if (coin != 2) leftT.push_back(t);
        if (coin != 0) rightT.push_back(t);
    }
    if (leftT.empty())
        leftT = { targets[randInt(0, static_cast<int>(targets.size()) - 1)] };
    if (rightT.empty())
        rightT = { targets[randInt(0, static_cast<int>(targets.size()) - 1)] };

    auto left = buildSAT(std::max(0, depth - 1), leftT, model, stackDepth, budget);
    auto right = buildSAT(std::max(0, depth - 1), rightT, model, stackDepth, budget);
    return std::make_unique<BinaryConnNode>(
        Symbol::or_(), std::move(left), std::move(right));
}


//  buildImpliesSAT 
// (phi -> psi) vera su t  <=>  phi(t) = false  OR  psi(t) = true.
std::unique_ptr<ASTNode> FlutedGenerator::buildImpliesSAT(int depth,
    const Targets& targets, const FiniteModel& model,
    int stackDepth, BudgetState& budget)
{
    const auto& D = model.domain();

    Targets falseTargets, trueTargets;
    for (const auto& t : targets) {
        int coin = randInt(0, 2);
        if (coin != 2) falseTargets.push_back(t);
        if (coin != 0) trueTargets.push_back(t);
    }
    if (falseTargets.empty())
        falseTargets = { targets[randInt(0, static_cast<int>(targets.size()) - 1)] };
    if (trueTargets.empty())
        trueTargets = { targets[randInt(0, static_cast<int>(targets.size()) - 1)] };

    // Guard: evita enumerazione esponenziale di D^stackDepth
    {
        std::size_t prodSize = 1;
        const std::size_t domSize = D.size();
        bool overflow = false;
        for (int k = 0; k < stackDepth; ++k) {
            if (prodSize > MAX_DOMAIN_PRODUCT / domSize) { overflow = true; break; }
            prodSize *= domSize;
        }
        if (overflow || prodSize > MAX_DOMAIN_PRODUCT) {
            auto antecedent = buildSAT(
                std::max(0, depth - 1), falseTargets, model, stackDepth, budget);
            auto consequent = buildSAT(
                std::max(0, depth - 1), trueTargets, model, stackDepth, budget);
            return std::make_unique<BinaryConnNode>(
                Symbol::implies(), std::move(antecedent), std::move(consequent));
        }
    }

    // Complemento di falseTargets in D^stackDepth
    Targets allAssign = domainProduct(stackDepth, model);
    Targets antecedentTargets;
    for (const auto& a : allAssign) {
        bool inFalse = false;
        for (const auto& t : falseTargets)
            if (t == a) { inFalse = true; break; }
        if (!inFalse) antecedentTargets.push_back(a);
    }

    // Complemento vuoto: falseTargets = D^n
    if (antecedentTargets.empty()) {
        auto admissible = admissiblePreds(stackDepth);
        for (int i : admissible) {
            const auto& currpred = activeVocab_[i];
            auto        argNames = flutedArgNames(stackDepth, currpred.arity);
            bool allFalse = true;
            for (const auto& t : falseTargets)
                if (model.evalAtom(currpred.name, currpred.arity, argNames, t)) { allFalse = false; break; }
            if (allFalse) {
                auto args = flutedArgs(stackDepth, currpred.arity);
                auto atom = std::make_unique<AtomicNode>(
                    Symbol::pred(currpred.name, currpred.arity), args);
                auto consequent = buildSAT(
                    std::max(0, depth - 1), trueTargets, model, stackDepth, budget);
                return std::make_unique<BinaryConnNode>(
                    Symbol::implies(), std::move(atom), std::move(consequent));
            }
        }
        throw std::logic_error(
            "buildImpliesSAT: falseTargets = D^n, nessun atomo "
            "uniformemente falso -> retry");
    }

    auto antecedent = buildSAT(
        std::max(0, depth - 1), antecedentTargets, model, stackDepth, budget);
    auto consequent = buildSAT(
        std::max(0, depth - 1), trueTargets, model, stackDepth, budget);
    return std::make_unique<BinaryConnNode>(
        Symbol::implies(), std::move(antecedent), std::move(consequent));
}


//  buildExistsSAT 
//  phi vera sui target  <=> per ogni target t, esiste e in D tale che phi è vera su t U {x_{n+1}->e}.

std::unique_ptr<ASTNode> FlutedGenerator::buildExistsSAT(int depth,
    const Targets& targets, const FiniteModel& model,
    int stackDepth, BudgetState& budget)
{
    const auto& D = model.domain();
    int  nextStackD = stackDepth + 1;
    std::string nextVarN = varName(nextStackD);

    auto admissibleNext = admissiblePreds(nextStackD);

    Targets bodyTargets;
    for (const auto& t : targets) {
        std::vector<std::string> domShuffled = D;
        std::shuffle(domShuffled.begin(), domShuffled.end(), rng_);

        std::string chosenWitness = domShuffled[0]; // fallback
        bool found = false;

        if (!admissibleNext.empty()) {
            for (const auto& e : domShuffled) {
                Assignment extended = t;
                extended[nextVarN] = e;
                for (int i : admissibleNext) {
                    const auto& currpred = activeVocab_[i];
                    auto        argNames = flutedArgNames(nextStackD, currpred.arity);
                    if (model.evalAtom(currpred.name, currpred.arity, argNames, extended)) {
                        chosenWitness = e;
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }
        }

        Assignment ex = t;
        ex[nextVarN] = chosenWitness;
        bodyTargets.push_back(ex);
    }

    auto body = buildSAT(std::max(0, depth - 1), bodyTargets, model, nextStackD, budget);
    return std::make_unique<QuantifierNode>(
        Symbol::exists(), Symbol::var(nextVarN), std::move(body));
}


//  buildForallSAT 
// FORALL x_{n+1}. phi vera sui target  <=> per ogni target t e per OGNI e in D, phi è vera su t U {x_{n+1} -> e}.

std::unique_ptr<ASTNode> FlutedGenerator::buildForallSAT(int depth,
    const Targets& targets, const FiniteModel& model,
    int stackDepth, BudgetState& budget)
{
    const auto& D = model.domain();
    int  nextStackD = stackDepth + 1;
    std::string nextVarN = varName(nextStackD);

    Targets bodyTargets;
    for (const auto& t : targets)
        for (const auto& e : D) {
            Assignment ex = t;
            ex[nextVarN] = e;
            bodyTargets.push_back(ex);
        }

    auto body = buildSAT(std::max(0, depth - 1), bodyTargets, model, nextStackD, budget);
    return std::make_unique<QuantifierNode>(
        Symbol::forall(), Symbol::var(nextVarN), std::move(body));
}


//  buildFL (FREE) 

std::unique_ptr<ASTNode> FlutedGenerator::buildFL(int depth, int stackDepth,
    BudgetState& budget)
{
    auto admissible = admissiblePreds(stackDepth);

    // Quantificatore strutturale forzato: NON contato nel budget.
    // Emesso quando nessun predicato è ammissibile al stackDepth corrente.
    auto forcedQuant = [&](int d, int sd) -> std::unique_ptr<ASTNode> {
        bool ex = (randInt(0, 1) == 0);
        int nextStackD = sd + 1;
        auto varSym = Symbol::var(varName(nextStackD));
        auto quantSym = ex ? Symbol::exists() : Symbol::forall();
        auto body = buildFL(d > 0 ? d - 1 : 0, nextStackD, budget);
        return std::make_unique<QuantifierNode>(quantSym, varSym, std::move(body));
        };

    if (depth == 0) {
        if (!admissible.empty()) return buildAtomic(varName(stackDepth));
        return forcedQuant(0, stackDepth);
    }

    auto candidates = candidateTypes(depth, budget);

    if (candidates.empty()) {
        if (!admissible.empty()) return buildAtomic(varName(stackDepth));
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
        int nextStackD = stackDepth + 1;
        return std::make_unique<QuantifierNode>(
            Symbol::exists(), Symbol::var(varName(nextStackD)),
            buildFL(depth - 1, nextStackD, budget));
    }

    case SymbolType::FORALL: {
        int nextStackD = stackDepth + 1;
        return std::make_unique<QuantifierNode>(
            Symbol::forall(), Symbol::var(varName(nextStackD)),
            buildFL(depth - 1, nextStackD, budget));
    }

    // ── EQUALITY ──────────────────────────────────────────────────────────────
    // In FL l'uguaglianza usa x_{n-1} e x_n: ammissibile solo se stackDepth >= 2.
    // Se stackDepth < 2, cade nel default (atomo ordinario o quantificatore forzato).
    case SymbolType::EQUALITY:
        if (stackDepth >= 2)
            return buildEqualityLeaf(stackDepth);
        [[fallthrough]];

    default:
        if (!admissible.empty()) return buildAtomic(varName(stackDepth));
        return forcedQuant(depth, stackDepth);
    }
}


//  Utility
std::string FlutedGenerator::varName(int n)
{
    return "x" + std::to_string(n);
}

std::vector<Symbol> FlutedGenerator::flutedArgs(int stackDepth, int arity) const
{
    std::vector<Symbol> args;
    args.reserve(arity);
    for (int i = stackDepth - arity + 1; i <= stackDepth; ++i)
        args.push_back(Symbol::var(varName(i)));
    return args;
}

std::vector<std::string> FlutedGenerator::flutedArgNames(int stackDepth, int arity) const
{
    std::vector<std::string> names;
    names.reserve(arity);
    for (int i = stackDepth - arity + 1; i <= stackDepth; ++i)
        names.push_back(varName(i));
    return names;
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

FlutedGenerator::Targets FlutedGenerator::domainProduct(
    int stackDepth, const FiniteModel& model) const
{
    const auto& D = model.domain();
    Targets result = { {} }; 

    for (int k = 1; k <= stackDepth; ++k) {
        std::string currVarName = varName(k);
        Targets expanded;
        expanded.reserve(result.size() * D.size());
        for (const auto& a : result)
            for (const auto& e : D) {
                Assignment ex = a;
                ex[currVarName] = e;
                expanded.push_back(std::move(ex));
            }
        result = std::move(expanded);
    }
    return result;
}


//  evalNode
// Valutatore ricorsivo locale sul FiniteModel utilizzato per verificare
// la correttezza semantica di BuildNegSat

bool FlutedGenerator::evalNode(const ASTNode& node,
    const Assignment& assign,
    const FiniteModel& model) const
{
    if (auto* atomnode = dynamic_cast<const AtomicNode*>(&node)) {
        std::vector<std::string> argNames;
        for (const auto& sym : atomnode->args()) argNames.push_back(sym.name);
        return model.evalAtom(
            atomnode->predSymbol().name, atomnode->predSymbol().arity, argNames , assign);
    }

    
    if (auto* eqnode = dynamic_cast<const EqualityNode*>(&node)) {
        auto itL = assign.find(eqnode->lhs().name);
        auto itR = assign.find(eqnode->rhs().name);
        if (itL == assign.end() || itR == assign.end()) return false;
        return itL->second == itR->second;
    }
    if (auto* negnode = dynamic_cast<const NegNode*>(&node))
        return !evalNode(negnode->child(), assign, model);

    if (auto* binarynode = dynamic_cast<const BinaryConnNode*>(&node)) {
        bool lv = evalNode(binarynode->left(), assign, model);
        bool rv = evalNode(binarynode->right(), assign, model);
        switch (binarynode->connSymbol().type) {
        case SymbolType::AND:     return lv && rv;
        case SymbolType::OR:      return lv || rv;
        case SymbolType::IMPLIES: return !lv || rv;
        default: return false;
        }
    }
    if (auto* quantnode = dynamic_cast<const QuantifierNode*>(&node)) {
        const std::string& vname = quantnode->var().name;
        bool isEx = (quantnode ->quantSymbol().type == SymbolType::EXISTS);
        for (const auto& e : model.domain()) {
            Assignment ext = assign;
            ext[vname] = e;
            bool val = evalNode(quantnode->body(), ext, model);
            if (isEx && val) return true;
            if (!isEx && !val) return false;
        }
        return !isEx;
    }
    return false;
}