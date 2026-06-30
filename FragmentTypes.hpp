#pragma once
#include <string>
#include <vector>
#include <stdexcept>


struct PredInfo {
    std::string name;
    int         arity;
};

enum class GenMode { FREE, SAT, UNSAT, SATBUILD};

enum class TransformMode { NONE, NNF };

enum class OutputFormat { DEFAULT, TPTP };


// BudgetRange 
// range [min, max] of node types.
struct BudgetRange {
    int min = -1;
    int max = -1;

    [[nodiscard]] bool isConstrained() const {
        return min >= 0 || max >= 0;
    }

    void validate(const std::string& field) const {
        if (min >= 0 && max >= 0 && min > max)
            throw std::invalid_argument(
                "budget " + field + ": min (" + std::to_string(min) +
                ") > max (" + std::to_string(max) + ")");
    }
};


//  NodeBudget 
struct NodeBudget {
    BudgetRange and_count;
    BudgetRange or_count;
    BudgetRange not_count;
    BudgetRange exists_count;
    BudgetRange forall_count;
    BudgetRange implies_count;
    BudgetRange eq_count;

    [[nodiscard]] bool hasAnyConstraint() const {
        return and_count.isConstrained() || or_count.isConstrained() ||
            not_count.isConstrained() || exists_count.isConstrained() ||
            forall_count.isConstrained() || implies_count.isConstrained()|| eq_count.isConstrained();
    }

    void validate() const {
        and_count.validate("and");
        or_count.validate("or");
        not_count.validate("not");
        exists_count.validate("exists");
        forall_count.validate("forall");
        implies_count.validate("implies");
        eq_count.validate("eq");
    }

    [[nodiscard]] int totalMin() const {
        int t = 0;
        auto add = [&](const BudgetRange& r) { if (r.min >= 0) t += r.min; };
        add(and_count); add(or_count); add(not_count);
        add(exists_count); add(forall_count); add(implies_count); add(eq_count);

        return t;
    }
};

// GenConfig 
// Complete configuration
struct GenConfig {
    GenMode       mode = GenMode::FREE;
    int           depth = 3;
    int           domainSize = 0;
    TransformMode transform = TransformMode::NONE;
    OutputFormat  output = OutputFormat::DEFAULT;
    NodeBudget    budget;
    int           arityFilter = 0;
    std::vector<PredInfo> vocab;
};