#pragma once
#include "../syntax/ASTNode.hpp"
#include "../FragmentTypes.hpp"
#include <memory>
#include <vector>
#include <random>

// ─────────────────────────────────────────────────────────────────────────────
//  FormulaBuilder — Quadrante Generazione
//
//  Componente comune a tutti i frammenti decidibili.
//  Genera la struttura logica di una formula (connettivi, quantificatori,
//  profondità, budget) 
//
//  Ogni frammento eredita da FormulaBuilder e implementa:
//    - buildAtomic()  : costruisce una foglia rispettando le regole
//    - generateSAT()  : target propagation specifico del frammento
//    - fragmentName() : nome del frammento ("FO2", "Fluted", ...)
//    - startVar()     : variabile iniziale
//    - nextVar()      : prossima variabile da legare
// ─────────────────────────────────────────────────────────────────────────────

class FormulaBuilder {
public:
    explicit FormulaBuilder(unsigned seed);
    virtual ~FormulaBuilder() = default;

    [[nodiscard]] virtual std::string generateFormatted(const GenConfig& cfg);
    [[nodiscard]] virtual std::string fragmentName() const = 0;

protected:

    // ──── BudgetState ────
    // Copia mutabile dei contatori per una singola build.

    // Semantica dei contatori dopo il campionamento:
    //   -1  -> tipo libero:    canUse() = true,  consume() è no-op
    //    0  -> tipo proibito:  canUse() = false
    //   >0  -> tipo vincolato: canUse() = true,  consume() decrementa
    //
    struct BudgetState {
        int and_left;
        int or_left;
        int not_left;
        int exists_left;
        int forall_left;
        int implies_left;
        int eq_left; 


        // Gestisce la scelta precisa o casuale dal range
        static int sample(const BudgetRange& r, std::mt19937& rng) {
            if (!r.isConstrained()) return -1;

            // Se min == max, l'utente ha inserito un valore preciso
            if (r.min >= 0 && r.max >= 0 && r.min == r.max) {
                return r.min;
            }

            // Altrimenti campiona nel range definito
            int lo = (r.min >= 0) ? r.min : 0;
            int hi = (r.max >= 0) ? r.max : lo + 20;
            std::uniform_int_distribution<int> dist(lo, hi);
            return dist(rng);
        }

        // Costruttore che accetta il generatore casuale per il campionamento
        explicit BudgetState(const NodeBudget& b, std::mt19937& rng)
            : and_left(sample(b.and_count, rng))
            , or_left(sample(b.or_count, rng))
            , not_left(sample(b.not_count, rng))
            , exists_left(sample(b.exists_count, rng))
            , forall_left(sample(b.forall_count, rng))
            , implies_left(sample(b.implies_count, rng))
            , eq_left(sample(b.eq_count, rng))
        {
        }

        [[nodiscard]] bool canUse(SymbolType t) const {
            switch (t) {
            case SymbolType::AND:     return and_left != 0;
            case SymbolType::OR:      return or_left != 0;
            case SymbolType::NEG:     return not_left != 0;
            case SymbolType::EXISTS:  return exists_left != 0;
            case SymbolType::FORALL:  return forall_left != 0;
            case SymbolType::IMPLIES: return implies_left != 0;
            case SymbolType::EQUALITY: return eq_left != 0;
            default: return true;
            }
        }

        [[nodiscard]] int remaining() const {
            int r = 0;
            if (and_left > 0) r += and_left;
            if (or_left > 0) r += or_left;
            if (not_left > 0) r += not_left;
            if (exists_left > 0) r += exists_left;
            if (forall_left > 0) r += forall_left;
            if (implies_left > 0) r += implies_left;
            if (eq_left > 0) r += eq_left;
            return r;
        }

        void consume(SymbolType t) {
            switch (t) {
            case SymbolType::AND:     if (and_left > 0) --and_left;     break;
            case SymbolType::OR:      if (or_left > 0) --or_left;      break;
            case SymbolType::NEG:     if (not_left > 0) --not_left;     break;
            case SymbolType::EXISTS:  if (exists_left > 0) --exists_left;  break;
            case SymbolType::FORALL:  if (forall_left > 0) --forall_left;  break;
            case SymbolType::IMPLIES: if (implies_left > 0) --implies_left; break;
            case SymbolType::EQUALITY: if (eq_left > 0) --eq_left; break;
            default: break;
            }
        }

        [[nodiscard]] bool satisfied() const { return remaining() == 0; }
    };

    // ──── Metodi virtuali puri implementati da ogni frammento ────

    [[nodiscard]] virtual std::unique_ptr<AtomicNode>
        buildAtomic(const std::string& currentVar) = 0;

    [[nodiscard]] virtual std::unique_ptr<ASTNode>
        generateSAT(int depth, int domainSize, BudgetState& budget) = 0;

    [[nodiscard]] virtual std::string startVar() const = 0;
    [[nodiscard]] virtual std::string nextVar(const std::string& currentVar) const = 0;

    // ──── Metodi condivisi ────

    [[nodiscard]] std::unique_ptr<ASTNode> build(int depth,
        const std::string& currentVar,
        BudgetState& budget);

    [[nodiscard]] std::unique_ptr<ASTNode> generateUNSAT(int depth,
        BudgetState& budget);

    [[nodiscard]] std::vector<SymbolType> candidateTypes(int depth,
        const BudgetState& budget) const;

    SymbolType pickType(int depth, BudgetState& budget);

    int randInt(int lo, int hi);

    std::mt19937 rng_;
};