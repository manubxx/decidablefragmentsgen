#pragma once
#include "Symbol.hpp"
#include <vector>
#include <string>


//  Formula — sequenza ordinata di Symbol
class Formula {
public:
    using Tokens = std::vector<Symbol>;

    explicit Formula(Tokens t) : tokens_(std::move(t)) {}

    // accesso read-only
    [[nodiscard]] const Tokens& tokens() const { return tokens_; }
    [[nodiscard]] std::size_t   size()   const { return tokens_.size(); }

    // funzione K somma
    [[nodiscard]] int totalKValue() const {
        int sum = 0;
        for (const auto& s : tokens_) sum += s.kValue();
        return sum;
    }

    //  Validazione
    //  Condizioni necessarie e sufficienti per essere una wff:
    //   1) La sequenza non è vuota
    //   2) Ogni prefisso PROPRIO ha somma K <= 0
    //   3) La somma TOTALE è esattamente 1
    [[nodiscard]] bool isValid() const {
        if (tokens_.empty()) return false;

        int sum = 0;
        const std::size_t n = tokens_.size();
        for (std::size_t i = 0; i < n; ++i) {
            sum += tokens_[i].kValue();
            if (i < n - 1 && sum > 0) return false;
        }
        return sum == 1;
    }

    // Stampa
    [[nodiscard]] std::string toString() const {
        std::string out;
        for (std::size_t i = 0; i < tokens_.size(); ++i) {
            if (i > 0) out += ' ';
            out += tokens_[i].name;
        }
        return out;
    }

    // Debug: K simbolo per simbolo
    [[nodiscard]] std::string debugKTrace() const {
        std::string out;
        int running = 0;
        for (const auto& s : tokens_) {
            running += s.kValue();
            out += s.name
                + "[k="   + std::to_string(s.kValue())
                + ",cumulative=" + std::to_string(running) + "] ";
        }
        return out;
    }

private:
    Tokens tokens_;
};
