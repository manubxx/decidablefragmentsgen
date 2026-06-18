#pragma once

#include "../syntax/ASTNode.hpp"
#include "../FragmentTypes.hpp"
#include <memory>
#include <vector>
#include <random>

//  FormulaBuilder — First generation

//  Responsible for generating the logical structure of a formula.
//  Each fragment inherits from FormulaBuilder and implement:
//   buildAtomic()  : constructs a leaf node respecting fragment rules
//   generateSAT()  : handles the fragment SAT generation
//   fragmentName() : returns the fragment name
//   startVar()     : initial variable
//   nextVar()      : next variable to be bound


class BudgetRetryException : public std::exception {
public:
    [[nodiscard]] const char* what() const noexcept override {
        return "Budget constraints error. Retrying generation.";
    }
};

class FormulaBuilder {
public:
    explicit FormulaBuilder(unsigned seed);
    virtual ~FormulaBuilder() = default;

    [[nodiscard]] virtual std::string generateFormatted(const GenConfig& cfg);
    [[nodiscard]] virtual std::string fragmentName() const = 0;

protected:
  
     //BudgetState
     // Mutable copy of operational counters for a single building session.
     // Counter semantics:
     // -1  -> unconstrained/free: canUse() = true,
     // 0   -> forbidden/exhausted: canUse() = false
     // >0  -> constrained/limited: canUse() = true,  consume() decrements value

    static constexpr int DEFAULT_MARGIN = 20;
     
    struct BudgetState {
        int and_left;
        int or_left;
        int not_left;
        int exists_left;
        int forall_left;
        int implies_left;
        int eq_left;

        // Precise value assignment or random selection in the defined range
        static int sample(const BudgetRange& r, std::mt19937& rng) {

            if (!r.isConstrained()) return -1;

            // min == max, exact quantity
            if (r.min >= 0 && r.max >= 0 && r.min == r.max) {
                return r.min;
            }
            // else
            int lo = (r.min >= 0) ? r.min : 0;
            int hi = (r.max >= 0) ? r.max : (lo + DEFAULT_MARGIN);
            std::uniform_int_distribution<int> dist(lo, hi);
            return dist(rng);
        }

        // Constructor accepting the sampling
        explicit BudgetState(const NodeBudget& b, std::mt19937& rng)
            : and_left(sample(b.and_count, rng))
            , or_left(sample(b.or_count, rng))
            , not_left(sample(b.not_count, rng))
            , exists_left(sample(b.exists_count, rng))
            , forall_left(sample(b.forall_count, rng))
            , implies_left(sample(b.implies_count, rng))
            , eq_left(sample(b.eq_count, rng))
        {}

        [[nodiscard]] bool canUse(SymbolType t) const {
            switch (t) {
            case SymbolType::AND:      return and_left != 0;
            case SymbolType::OR:       return or_left != 0;
            case SymbolType::NEG:      return not_left != 0;
            case SymbolType::EXISTS:   return exists_left != 0;
            case SymbolType::FORALL:   return forall_left != 0;
            case SymbolType::IMPLIES:  return implies_left != 0;
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
            case SymbolType::AND:      if (and_left > 0) --and_left;     break;
            case SymbolType::OR:       if (or_left > 0) --or_left;      break;
            case SymbolType::NEG:      if (not_left > 0) --not_left;     break;
            case SymbolType::EXISTS:   if (exists_left > 0) --exists_left;  break;
            case SymbolType::FORALL:   if (forall_left > 0) --forall_left;  break;
            case SymbolType::IMPLIES:  if (implies_left > 0) --implies_left; break;
            case SymbolType::EQUALITY: if (eq_left > 0) --eq_left; break;
            default: break;
            }
        }

        [[nodiscard]] bool satisfied() const { return remaining() == 0; }

    };

    // Pure virtual methods
    [[nodiscard]] virtual std::unique_ptr<AtomicNode> buildAtomic(const std::string& currentVar) = 0;

    [[nodiscard]] virtual std::unique_ptr<ASTNode> generateSAT(int depth, int domainSize, BudgetState& budget) = 0;

    [[nodiscard]] virtual std::unique_ptr<ASTNode> buildComponentUNSAT(int depth, BudgetState& budget) = 0; 

    

    [[nodiscard]] virtual std::string startVar() const = 0;
    [[nodiscard]] virtual std::string nextVar(const std::string& currentVar) const = 0;

    // Shared methods
    [[nodiscard]] virtual std::unique_ptr<ASTNode> build(int depth, const std::string& currentVar, BudgetState& budget);

    [[nodiscard]] std::unique_ptr<ASTNode> generateUNSAT(int depth, BudgetState& budget);

    [[nodiscard]] virtual std::vector<SymbolType> candidateTypes(int depth, const BudgetState& budget) const;

    SymbolType pickType(int depth, BudgetState& budget);
    SymbolType pickType(int depth, BudgetState& budget, const std::vector<SymbolType>& candidates); //overload 

    int randInt(int lo, int hi);

    std::mt19937 rng_;
};