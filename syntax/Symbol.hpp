#pragma once
#include <string>
#include <stdexcept>

//  Tipi di simbolo del linguaggio FOL
enum class SymbolType {
    LPAREN,    // (
    RPAREN,    // )
    NEG,       // ¬    (stampato come ~)
    IMPLIES,   // →    (stampato come ->)
    FORALL,    // ∀   (stampato come A)
    EXISTS,    // ∃   (stampato come E)
    AND,       // ∧   (stampato come &)
    OR,        // ∨   (stampato come |)
    EQUALITY,  // = 
    PREDICATE, // P, Q, R, ...  (con arità)
    VARIABLE,  // x, y, v1, v2, ...
};

//  Symbol - unità del linguaggio.
struct Symbol {
    SymbolType  type;
    std::string name;
    int arity = 0; 

    [[nodiscard]] int kValue() const {
        switch (type) {
            case SymbolType::LPAREN:    return -1;
            case SymbolType::RPAREN:    return +1;
            case SymbolType::NEG:       return  0;
            case SymbolType::IMPLIES:   return -1;
            case SymbolType::FORALL:    return -1;
            case SymbolType::EXISTS:    return -1; 
            case SymbolType::AND:       return -1;  
            case SymbolType::OR:        return -1; 
            case SymbolType::PREDICATE: return  1 - arity;
            case SymbolType::VARIABLE:  return +1;
            case SymbolType::EQUALITY:  return -1;
        }
        throw std::logic_error("SymbolType sconosciuto");
    }

    // Static factories
    static Symbol lparen()  { return {SymbolType::LPAREN,   "("}; }
    static Symbol rparen()  { return {SymbolType::RPAREN,   ")"}; }
    static Symbol neg()     { return {SymbolType::NEG,      "~"}; }
    static Symbol implies() { return {SymbolType::IMPLIES,  "->"}; }
    static Symbol forall()  { return {SymbolType::FORALL,   "A"}; }
    static Symbol exists()  { return { SymbolType::EXISTS, "E" }; }
    static Symbol and_()    { return { SymbolType::AND,    "&" }; }
    static Symbol or_()     { return { SymbolType::OR,     "|" }; }
    static Symbol eq()      { return { SymbolType::EQUALITY, "=", 2 }; }

    // Il vincolo sul nome è responsabilità del generatore
    static Symbol var(const std::string& varName) {
        return {SymbolType::VARIABLE, varName};
    }
    static Symbol pred(const std::string& nm, int ar) {
        if (ar < 1)
            throw std::invalid_argument("L'arità del predicato deve essere >= 1");
        return {SymbolType::PREDICATE, nm, ar};
    }
};