#pragma once
#include "../FragmentTypes.hpp"
#include <string>
#include <map>
#include <vector>
#include <random>
#include <stdexcept>

//  FiniteModel
//
// Per FO2  (arità <= 2): Mortimer (1975) garantisce la finite model property.
// Per FL   (arità qualsiasi): Pratt-Hartmann, Szwast, Tendera (2019)
// 
// Complessità costruzione: O(∑_P |D|^arità(P))
// Complessità evalAtom:    O(log(|D|^k)) 


class FiniteModel {
public:

    // Constructor
    FiniteModel(const std::vector<PredInfo>& vocab,
        std::mt19937& rng,
        int domainSize = 0)
    {
        if (domainSize <= 0)
            throw std::invalid_argument(
                "FiniteModel: domainSize non specificato. "
                "Usa --domain-size n per scegliere la dimensione del dominio.");

        if (domainSize < 2)
            throw std::invalid_argument(
                "FiniteModel: domainSize deve essere >= 2");

        // Costruisce il dominio D = {d0, d1, ..., d_{n-1}}
        domain_.reserve(domainSize);
        for (int i = 0; i < domainSize; ++i)
            domain_.push_back("d" + std::to_string(i));

        // Interpreta ogni predicato casualmente su D^arità(P)
        std::bernoulli_distribution coin(0.5);

        for (const auto& p : vocab) {
            if (p.arity < 1)
                throw std::invalid_argument(
                    "FiniteModel: predicato '" + p.name +
                    "' ha arità " + std::to_string(p.arity) + " < 1");

            // Genera tutte le tuple di D^k tramite prodotto cartesiano
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

            // Assegna un valore booleano casuale a ogni tupla
            auto& table = interp_[p.name];
            for (auto& tuple : tuples)
                table[tuple] = coin(rng);
        }
    }

    //  ──── Public Interface ────
    [[nodiscard]] const std::vector<std::string>& domain() const
    {
        return domain_;
    }

    [[nodiscard]] bool evalAtom(
        const std::string& pred,
        int               arity,
        const std::vector<std::string>& atomVars,
        const std::map<std::string, std::string>& varAssign) const
    {
        // Risolve le variabili in elementi del dominio
        std::vector<std::string> tuple;
        tuple.reserve(arity);
        for (int i = 0; i < arity; ++i)
            tuple.push_back(varAssign.at(atomVars[i]));

        return interp_.at(pred).at(tuple);
    }

    // Valuta un atomo con una tupla di elementi già risolti (senza varAssign).
    [[nodiscard]] bool evalAtomDirect(
        const std::string& pred,
        const std::vector<std::string>& tuple) const
    {
        return interp_.at(pred).at(tuple);
    }

private:
    std::vector<std::string> domain_;

    std::map<std::string,
        std::map<std::vector<std::string>, bool>> interp_;
};