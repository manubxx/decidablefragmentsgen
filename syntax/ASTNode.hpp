#pragma once

#include "Symbol.hpp"
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm> // Incluso per std::tolower/std::toupper se necessario

//  ASTNode // Gerarchia di nodi per l'albero sintattico astratto
class ASTNode {
public:
    virtual ~ASTNode() = default;

    [[nodiscard]] virtual std::string toString()  const = 0;
    [[nodiscard]] virtual std::string toTPTP()    const = 0;
    [[nodiscard]] virtual std::unique_ptr<ASTNode> clone()        const = 0;
    [[nodiscard]] virtual std::unique_ptr<ASTNode> toNNF(bool negated = false) const = 0;
};

// Dichiarazione anticipata (Forward Declaration) utile per i puntatori
class NegNode;

// AtomicNode
class AtomicNode final : public ASTNode {
public:
    AtomicNode(Symbol predSymbol, std::vector<Symbol> args)
        : pred_(std::move(predSymbol)), args_(std::move(args))
    {
        if (pred_.type != SymbolType::PREDICATE)
            throw std::invalid_argument("AtomicNode: il simbolo non e' un PREDICATE");
        if (static_cast<int>(args_.size()) != pred_.arity)
            throw std::invalid_argument(
                "AtomicNode: numero di argomenti (" +
                std::to_string(args_.size()) + ") != arita' (" +
                std::to_string(pred_.arity) + ")");
    }

    [[nodiscard]] const Symbol& predSymbol() const { return pred_; }
    [[nodiscard]] const std::vector<Symbol>& args()       const { return args_; }

    [[nodiscard]] std::string toString() const override {
        std::string s = pred_.name + "(";
        for (std::size_t i = 0; i < args_.size(); ++i) {
            if (i) s += ",";
            s += args_[i].name;
        }
        return s + ")";
    }

    // TPTP
    [[nodiscard]] std::string toTPTP() const override {
        std::string pname = pred_.name;
        for (auto& c : pname) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        std::string s = pname + "(";
        for (std::size_t i = 0; i < args_.size(); ++i) {
            if (i) s += ",";
            s += tptpVar(args_[i].name);
        }
        return s + ")";
    }

    [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
        return std::make_unique<AtomicNode>(pred_, args_);
    }

    [[nodiscard]] std::unique_ptr<ASTNode> toNNF(bool negated) const override;

private:
    Symbol              pred_;
    std::vector<Symbol> args_;

    static std::string tptpVar(const std::string& v) {
        if (v.empty()) return v;
        std::string r = v;
        r[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(r[0])));
        return r;
    }
};

// ──── EqualityNode ───
class EqualityNode final : public ASTNode {
public:
    EqualityNode(Symbol lhs, Symbol rhs)
        : lhs_(std::move(lhs)), rhs_(std::move(rhs))
    {
        if (lhs_.type != SymbolType::VARIABLE)
            throw std::invalid_argument("EqualityNode: lhs non e' una VARIABLE: " + lhs_.name);
        if (rhs_.type != SymbolType::VARIABLE)
            throw std::invalid_argument("EqualityNode: rhs non e' una VARIABLE: " + rhs_.name);
    }

    [[nodiscard]] const Symbol& lhs() const { return lhs_; }
    [[nodiscard]] const Symbol& rhs() const { return rhs_; }

    [[nodiscard]] std::string toString() const override {
        return "(" + lhs_.name + " = " + rhs_.name + ")";
    }

    [[nodiscard]] std::string toTPTP() const override {
        return tptpVar(lhs_.name) + " = " + tptpVar(rhs_.name);
    }

    [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
        return std::make_unique<EqualityNode>(lhs_, rhs_);
    }

    // L'implementazione è stata spostata in fondo al file per risolvere l'errore
    [[nodiscard]] std::unique_ptr<ASTNode> toNNF(bool negated) const override;

private:
    Symbol lhs_;
    Symbol rhs_;

    static std::string tptpVar(const std::string& v) {
        if (v.empty()) return v;
        std::string r = v;
        r[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(r[0])));
        return r;
    }
};

//  NegNode 
class NegNode final : public ASTNode {
public:
    explicit NegNode(std::unique_ptr<ASTNode> child) : child_(std::move(child)) {
        if (!child_) throw std::invalid_argument("NegNode: figlio nullo");
    }

    [[nodiscard]] const ASTNode& child() const { return *child_; }

    [[nodiscard]] std::string toString() const override {
        return "(~ " + child_->toString() + ")";
    }

    [[nodiscard]] std::string toTPTP() const override {
        return "(~" + child_->toTPTP() + ")";
    }

    [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
        return std::make_unique<NegNode>(child_->clone());
    }

    [[nodiscard]] std::unique_ptr<ASTNode> toNNF(bool negated) const override {
        return child_->toNNF(!negated);
    }

private:
    std::unique_ptr<ASTNode> child_;
};

// BinaryConnNode 
class BinaryConnNode final : public ASTNode {
public:
    BinaryConnNode(Symbol connSymbol,
        std::unique_ptr<ASTNode> left,
        std::unique_ptr<ASTNode> right)
        : conn_(std::move(connSymbol)),
        left_(std::move(left)),
        right_(std::move(right))
    {
        if (conn_.type != SymbolType::AND &&
            conn_.type != SymbolType::OR &&
            conn_.type != SymbolType::IMPLIES)
            throw std::invalid_argument("BinaryConnNode: simbolo non binario: " + conn_.name);
        if (!left_ || !right_)
            throw std::invalid_argument("BinaryConnNode: figlio nullo");
    }

    [[nodiscard]] const Symbol& connSymbol() const { return conn_; }
    [[nodiscard]] const ASTNode& left()       const { return *left_; }
    [[nodiscard]] const ASTNode& right()      const { return *right_; }

    [[nodiscard]] std::string toString() const override {
        return "(" + left_->toString() + " " + conn_.name + " " + right_->toString() + ")";
    }

    [[nodiscard]] std::string toTPTP() const override {
        std::string op;
        switch (conn_.type) {
        case SymbolType::AND:     op = "&";  break;
        case SymbolType::OR:      op = "|";  break;
        case SymbolType::IMPLIES: op = "=>"; break;
        default: op = "?";
        }
        return "(" + left_->toTPTP() + " " + op + " " + right_->toTPTP() + ")";
    }

    [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
        return std::make_unique<BinaryConnNode>(conn_, left_->clone(), right_->clone());
    }

    [[nodiscard]] std::unique_ptr<ASTNode> toNNF(bool negated) const override;

private:
    Symbol                   conn_;
    std::unique_ptr<ASTNode> left_;
    std::unique_ptr<ASTNode> right_;
};

//  QuantifierNode 
class QuantifierNode final : public ASTNode {
public:
    QuantifierNode(Symbol quant, Symbol var, std::unique_ptr<ASTNode> body)
        : quant_(std::move(quant)), var_(std::move(var)), body_(std::move(body))
    {
        if (quant_.type != SymbolType::FORALL && quant_.type != SymbolType::EXISTS)
            throw std::invalid_argument("QuantifierNode: simbolo non quantificatore: " + quant_.name);
        if (var_.type != SymbolType::VARIABLE)
            throw std::invalid_argument("QuantifierNode: la variabile non e' VARIABLE: " + var_.name);
        if (!body_)
            throw std::invalid_argument("QuantifierNode: body nullo");
    }

    [[nodiscard]] const Symbol& quantSymbol() const { return quant_; }
    [[nodiscard]] const Symbol& var()         const { return var_; }
    [[nodiscard]] const ASTNode& body()        const { return *body_; }

    [[nodiscard]] std::string toString() const override {
        return "(" + quant_.name + " " + var_.name + " " + body_->toString() + ")";
    }

    [[nodiscard]] std::string toTPTP() const override {
        std::string q = (quant_.type == SymbolType::EXISTS) ? "?" : "!";
        std::string v = tptpVar(var_.name);
        return "(" + q + " [" + v + "] : " + body_->toTPTP() + ")";
    }

    [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
        return std::make_unique<QuantifierNode>(quant_, var_, body_->clone());
    }

    [[nodiscard]] std::unique_ptr<ASTNode> toNNF(bool negated) const override {
        Symbol newQ = negated
            ? (quant_.type == SymbolType::FORALL ? Symbol::exists() : Symbol::forall())
            : quant_;
        return std::make_unique<QuantifierNode>(newQ, var_, body_->toNNF(negated));
    }

private:
    Symbol                   quant_;
    Symbol                   var_;
    std::unique_ptr<ASTNode> body_;

    static std::string tptpVar(const std::string& v) {
        if (v.empty()) return v;
        std::string r = v;
        r[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(r[0])));
        return r;
    }
};


// ──── Definizioni delle funzioni inline in fondo per evitare problemi di dipendenza ────

inline std::unique_ptr<ASTNode> AtomicNode::toNNF(bool negated) const {
    if (!negated) return clone();
    return std::make_unique<NegNode>(clone());
}

// Ora che NegNode è stato interamente dichiarato, EqualityNode può usarlo qui!
inline std::unique_ptr<ASTNode> EqualityNode::toNNF(bool negated) const {
    if (!negated) return clone();
    return std::make_unique<NegNode>(clone());
}

inline std::unique_ptr<ASTNode> BinaryConnNode::toNNF(bool negated) const {
    if (conn_.type == SymbolType::IMPLIES) {
        auto equiv = std::make_unique<BinaryConnNode>(
            Symbol::or_(),
            std::make_unique<NegNode>(left_->clone()),
            right_->clone());
        return equiv->toNNF(negated);
    }
    Symbol newConn = conn_;
    if (negated)
        newConn = (conn_.type == SymbolType::AND) ? Symbol::or_() : Symbol::and_();
    return std::make_unique<BinaryConnNode>(
        newConn, left_->toNNF(negated), right_->toNNF(negated));
}