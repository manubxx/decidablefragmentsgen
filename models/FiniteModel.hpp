#pragma once
#include "../FragmentTypes.hpp"
#include <string>
#include <unordered_map>
#include <map>
#include <vector>
#include <random>
#include <stdexcept>
#include <iostream>

class FiniteModel {
public:
    FiniteModel(const std::vector<PredInfo>& vocab, std::mt19937& rng, int totalExponent)
    {
        // overflow check, 16 is maximum that can be valued for a 32-bit integer
        if (totalExponent > 15) totalExponent = 15;
        int actualDomainSize = 1 << totalExponent;
        if (actualDomainSize < 2) actualDomainSize = 2;

        domain_.reserve(actualDomainSize);
        for (int i = 0; i < actualDomainSize; ++i) {
            domain_.push_back("d" + std::to_string(i));
        }

        std::bernoulli_distribution coin(0.5);

        // bit vectors and matrix
        for (const auto& p : vocab) {
            if (p.arity == 1) {
                auto& bitTable = unaryInterp_[p.name];
                bitTable.resize(actualDomainSize);
                for (int i = 0; i < actualDomainSize; ++i) {
                    bitTable[i] = coin(rng);
                }
            }
            else if (p.arity == 2) {
                long long totalCells = static_cast<long long>(actualDomainSize) * actualDomainSize;
                auto& bitTable = binaryInterp_[p.name];
                bitTable.resize(totalCells);
                for (long long i = 0; i < totalCells; ++i) {
                    bitTable[i] = coin(rng);
                }
            }
            else {
                throw std::invalid_argument("FiniteModel: This model supports only FO2 predicates");
            }
        }
    }

    [[nodiscard]] const std::vector<std::string>& domain() const {
        return domain_;
    }

    // Parsing on bit matrix
    [[nodiscard]] static inline int fastParseIndex(const std::string& s) {
        int res = 0;
        for (size_t i = 1; i < s.size(); ++i) {
            res = res * 10 + (s[i] - '0');
        }
        return res;
    }

    // O(1) access on matrix 
    [[nodiscard]] bool evalAtomDirect(const std::string& pred, const std::vector<std::string>& tuple) const
    {
        if (tuple.size() == 1) {
            int idx = fastParseIndex(tuple[0]);
            return unaryInterp_.at(pred)[idx];
        }
        else if (tuple.size() == 2) {
            int idx1 = fastParseIndex(tuple[0]);
            int idx2 = fastParseIndex(tuple[1]);
            long long finalIndex = static_cast<long long>(idx1) * domain_.size() + idx2;
            return binaryInterp_.at(pred)[finalIndex];
        }
        return false;
    }

    [[nodiscard]] bool evalAtom(const std::string& pred, int arity, const std::vector<std::string>& atomVars, const std::map<std::string, std::string>& varAssign) const
    {
        std::vector<std::string> tuple;
        tuple.reserve(arity);
        for (int i = 0; i < arity; ++i) {
            tuple.push_back(varAssign.at(atomVars[i]));
        }
        return evalAtomDirect(pred, tuple);
    }

private:
    std::vector<std::string> domain_;

    std::unordered_map<std::string, std::vector<bool>> unaryInterp_;
    std::unordered_map<std::string, std::vector<bool>> binaryInterp_;
};