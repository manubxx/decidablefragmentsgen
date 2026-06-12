#pragma once
#include "../FragmentTypes.hpp"
#include <string>
#include <map>
#include <vector>
#include <random>
#include <stdexcept>

// For FO2 (arity <= 2): Mortimer (1975) guarantees the finite model property.
// Construction Complexity: O(∑_P |D|^arity(P))
// evalAtom Complexity:     O(log(|D|^k))

class FiniteModel {
public:

    // Constructor
    FiniteModel(const std::vector<PredInfo>& vocab, std::mt19937& rng, int domainSize = 0)
    {
        if (domainSize <= 0)
            throw std::invalid_argument("FiniteModel: domainSize not specified. Use --domain-size n to choose the domain size.");

        if (domainSize < 2)
            throw std::invalid_argument("FiniteModel: domainSize must be >= 2");

        // Builds the domain D = {d0, d1, ..., d_{n-1}}
        domain_.reserve(domainSize);
        for (int i = 0; i < domainSize; ++i)
            domain_.push_back("d" + std::to_string(i));

        // Interprets each predicate randomly over D^arity(P)
        std::bernoulli_distribution coin(0.5);

        for (const auto& p : vocab) {
            if (p.arity < 1)
                throw std::invalid_argument("FiniteModel: predicate '" + p.name +"' has arity " + std::to_string(p.arity) + " < 1");

            // Generates all tuples of D^k via Cartesian product
            std::vector<std::vector<std::string>> tuples = { {} };
            for (int dim = 0; dim < p.arity; ++dim) {
                std::vector<std::vector<std::string>> expanded;
                expanded.reserve(tuples.size() * domain_.size());
                for (const auto& t : tuples)
                    for (const auto& e : domain_) {
                        auto ext = t;
                        ext.push_back(e);
                        expanded.push_back(std::move(ext));
                    }
                tuples = std::move(expanded);
            }

            // Assigns a random boolean value to each tuple
            auto& table = interp_[p.name];
            for (auto& tuple : tuples)
                table[tuple] = coin(rng);
        }
    }

    // Public Interface
    [[nodiscard]] const std::vector<std::string>& domain() const
    {
        return domain_;
    }

    [[nodiscard]] bool evalAtom(const std::string& pred, int arity, const std::vector<std::string>& atomVars, const std::map<std::string, std::string>& varAssign) const
    {
        // Resolves variables into domain elements
        std::vector<std::string> tuple;
        tuple.reserve(arity);
        for (int i = 0; i < arity; ++i)
            tuple.push_back(varAssign.at(atomVars[i]));

        return interp_.at(pred).at(tuple);
    }

    // Evaluates an atom using a tuple of already resolved elements
    [[nodiscard]] bool evalAtomDirect(const std::string& pred, const std::vector<std::string>& tuple) const
    {
        return interp_.at(pred).at(tuple);
    }

private:
    std::vector<std::string> domain_;
    std::map<std::string, std::map<std::vector<std::string>, bool>> interp_;
};