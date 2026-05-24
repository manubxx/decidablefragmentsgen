#pragma once

//  Il Fluted Fragment (FL) è il frammento di FOL caratterizzato dalla
//  fluting condition:
//
//      Un atomo P(y1,...,yk) è ammissibile in un contesto con stack di
//      variabili attive [x1,...,xn] se e solo se:
//
//          (y1,...,yk) = (x_{n-k+1},...,xn)   con n >= k
//  Riferimento teorico:Pratt-Hartmann, Szwast, Tendera — "The Fluted Fragment Revisited"



#include "../../formulae/FormulaBuilder.hpp"
#include "../../models/FiniteModel.hpp"
#include "../../syntax/ASTNode.hpp"

#include <memory>
#include <string>
#include <vector>
#include <map>


class FlutedGenerator : public FormulaBuilder {
public:
    // Constructor
    explicit FlutedGenerator(std::vector<PredInfo> vocab, unsigned seed);

    // Public Interface 
    [[nodiscard]] std::string generateFormatted(const GenConfig& cfg) override;
    [[nodiscard]] std::string fragmentName() const override;

protected:

    using Assignment = std::map<std::string, std::string>;
    using Targets = std::vector<Assignment>;

    // Risultato: formula + modello
    struct SatResult {
        std::unique_ptr<ASTNode> formula;
        FiniteModel              model;
    };

    // Pure virtual methods from FormulaBuilder

    [[nodiscard]] std::unique_ptr<AtomicNode>
        buildAtomic(const std::string& currentVar) override;


    [[nodiscard]] std::unique_ptr<ASTNode>
        generateSAT(int depth, int domainSize, BudgetState& budget) override;

    // Variabili FL: x1, x2, ..., xn (stack).
    [[nodiscard]] std::string startVar()                           const override;
    [[nodiscard]] std::string nextVar(const std::string& current)  const override;

    // SAT: Target propagation in FL

    // Genera formula + modello.
    SatResult generateSATFull(int depth, int domainSize, BudgetState& budget);

    // Dispatcher principale
    std::unique_ptr<ASTNode> buildSAT(int depth, const Targets& targets,
        const FiniteModel& model,
        int stackDepth, BudgetState& budget);

    // Foglia SAT
    std::unique_ptr<ASTNode> buildAtomicSAT(int depth, const Targets& targets,
        const FiniteModel& model,
        int stackDepth, BudgetState& budget);


    std::unique_ptr<ASTNode> buildForcedQuantSAT(int depth, const Targets& targets,
        const FiniteModel& model,
        int stackDepth, BudgetState& budget);


    std::unique_ptr<ASTNode> buildNegSAT(int depth, const Targets& targets,
        const FiniteModel& model,
        int stackDepth, BudgetState& budget);

    std::unique_ptr<ASTNode> buildAndSAT(int depth, const Targets& targets,
        const FiniteModel& model,
        int stackDepth, BudgetState& budget);

    std::unique_ptr<ASTNode> buildOrSAT(int depth, const Targets& targets,
        const FiniteModel& model,
        int stackDepth, BudgetState& budget);

    std::unique_ptr<ASTNode> buildImpliesSAT(int depth, const Targets& targets,
        const FiniteModel& model,
        int stackDepth, BudgetState& budget);

    std::unique_ptr<ASTNode> buildExistsSAT(int depth, const Targets& targets,
        const FiniteModel& model,
        int stackDepth, BudgetState& budget);

    std::unique_ptr<ASTNode> buildForallSAT(int depth, const Targets& targets,
        const FiniteModel& model,
        int stackDepth, BudgetState& budget);
    std::unique_ptr<ASTNode> buildEqualitySAT(const Targets& targets,
        const FiniteModel& model,
        int stackDepth, BudgetState& budget);
    std::unique_ptr<EqualityNode> buildEqualityLeaf(int stackDepth);

    //  FREE generation
    [[nodiscard]] std::unique_ptr<ASTNode> buildFL(int depth, int stackDepth,
        BudgetState& budget);

    //  Utility

    // Nucleo puro della generazione di un atomo FL
    [[nodiscard]] std::unique_ptr<AtomicNode> buildAtomicLeaf(int stackDepth);

    static std::string varName(int n);

    // Restituisce i Symbol degli argomenti FL per un predicato di arità k
    std::vector<Symbol> flutedArgs(int stackDepth, int arity) const;

    std::vector<std::string> flutedArgNames(int stackDepth, int arity) const;

    // Restituisce gli indici dei predicati ammissibili
    std::vector<int> admissiblePreds(int stackDepth) const;

    // Arità minima tra tutti i predicati in activeVocab_.
    int minArity() const;

    // Genera il prodotto cartesiano D^stackDepth come vettore di Assignment.
    Targets domainProduct(int stackDepth, const FiniteModel& model) const;

    // Valutatore ricorsivo locale: usato da buildNegSAT per verificare
    // semanticamente che la formula costruita sia falsa su tutti i target.
    bool evalNode(const ASTNode& node, const Assignment& assign,
        const FiniteModel& model) const;

  
    std::vector<PredInfo> vocab_;
    std::vector<PredInfo> activeVocab_;
};