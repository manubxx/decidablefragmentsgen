#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <random>
#include <algorithm>
#include <array>
#include "../FragmentTypes.hpp" 


using Element = int;                       // An element of the finite domain {0, 1, ..., domainSize - 1}
using Variable = int;                      // Variable index for FO^2 (0 for x, 1 for y)
using Assignment = std::array<Element, 2>; // Assignment function in the stack: assign[0] and assign[1]
using Targets = std::vector<Assignment>;   // Set of inherited semantic truth constraints

class FiniteModel {
public:
 
    FiniteModel(const std::vector<PredInfo>& vocab, std::mt19937& rng, int n) {

        //types counting
        int numUnary = 0;
        int numBinary = 0;
        for (const auto& p : vocab) {
            if (p.arity == 1) numUnary++;
            else if (p.arity == 2) numBinary++;
        }

        // 2 ^ (n * (unary + binary))
        long long exponent = static_cast<long long>(n) * (numUnary + numBinary);

   
        if (exponent > 13) exponent = 13;
        if (exponent < 1)  exponent = 1;

        domainSize_ = static_cast<int>(1LL << exponent);
        std::bernoulli_distribution coin(0.5);

        for (const auto& p : vocab) {
            if (p.arity == 1) {
     
                unaryInterp_[p.name].resize(domainSize_);
                for (int i = 0; i < domainSize_; ++i) {
                    unaryInterp_[p.name][i] = coin(rng);
                }
            }
            else if (p.arity == 2) {
                
                size_t totalCells = static_cast<size_t>(domainSize_) * domainSize_;
                binaryInterp_[p.name].resize(totalCells);
                for (size_t i = 0; i < totalCells; ++i) {
                    binaryInterp_[p.name][i] = coin(rng);
                }
            }
        }
    }

    [[nodiscard]] int domainSize() const { return domainSize_; }

  
    [[nodiscard]] bool evalAtom(const std::string& predName, int arity, const Assignment& assign, Variable arg1, Variable arg2 = 0) const {
        if (arity == 1) {
            Element e = assign[arg1];
            return unaryInterp_.at(predName)[e];
        }
        else {
            Element e1 = assign[arg1];
            Element e2 = assign[arg2];
           
            return binaryInterp_.at(predName)[static_cast<size_t>(e1) * domainSize_ + e2];
        }
    }

private:
    int domainSize_;
    std::unordered_map<std::string, std::vector<bool>> unaryInterp_;
    std::unordered_map<std::string, std::vector<bool>> binaryInterp_;
};