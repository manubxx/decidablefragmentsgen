#pragma once

#include "Symbol.hpp"
#include <memory>
#include <string>
#include <vector>

// Abstract Syntax Tree
class ASTNode {
public:
    virtual ~ASTNode() = default;

    [[nodiscard]] virtual std::string toString() const = 0;
    [[nodiscard]] virtual std::string toTPTP() const = 0;
    [[nodiscard]] virtual std::unique_ptr<ASTNode> clone() const = 0;
    [[nodiscard]] virtual std::unique_ptr<ASTNode> toNNF(bool negated = false) const = 0;
};

// AtomicNode 
class AtomicNode final : public ASTNode {
public:
    AtomicNode(Symbol predSymbol, std::vector<Symbol> args);

    [[nodiscard]] const Symbol& predSymbol() const { return pred_; }
    [[nodiscard]] const std::vector<Symbol>& args() const { return args_; }

    [[nodiscard]] std::string toString() const override;
    [[nodiscard]] std::string toTPTP() const override;
    [[nodiscard]] std::unique_ptr<ASTNode> clone() const override;
    [[nodiscard]] std::unique_ptr<ASTNode> toNNF(bool negated) const override;

private:
    Symbol pred_;
    std::vector<Symbol> args_;
    static std::string tptpVar(const std::string& v);
};

// EqualityNode
class EqualityNode final : public ASTNode {
public:
    EqualityNode(Symbol lhs, Symbol rhs);

    [[nodiscard]] const Symbol& lhs() const { return lhs_; }
    [[nodiscard]] const Symbol& rhs() const { return rhs_; }

    [[nodiscard]] std::string toString() const override;
    [[nodiscard]] std::string toTPTP() const override;
    [[nodiscard]] std::unique_ptr<ASTNode> clone() const override;
    [[nodiscard]] std::unique_ptr<ASTNode> toNNF(bool negated) const override;

private:
    Symbol lhs_;
    Symbol rhs_;
    static std::string tptpVar(const std::string& v);
};


//  NegNode
class NegNode final : public ASTNode {
public:
    explicit NegNode(std::unique_ptr<ASTNode> child);

    [[nodiscard]] const ASTNode& child() const { return *child_; }

    [[nodiscard]] std::string toString() const override;
    [[nodiscard]] std::string toTPTP() const override;
    [[nodiscard]] std::unique_ptr<ASTNode> clone() const override;
    [[nodiscard]] std::unique_ptr<ASTNode> toNNF(bool negated) const override;

private:
    std::unique_ptr<ASTNode> child_;
};

//  BinaryConnNode 
class BinaryConnNode final : public ASTNode {
public:
    BinaryConnNode(Symbol connSymbol, std::unique_ptr<ASTNode> left, std::unique_ptr<ASTNode> right);

    [[nodiscard]] const Symbol& connSymbol() const { return conn_; }
    [[nodiscard]] const ASTNode& left() const { return *left_; }
    [[nodiscard]] const ASTNode& right() const { return *right_; }

    [[nodiscard]] std::string toString() const override;
    [[nodiscard]] std::string toTPTP() const override;
    [[nodiscard]] std::unique_ptr<ASTNode> clone() const override;
    [[nodiscard]] std::unique_ptr<ASTNode> toNNF(bool negated) const override;

private:
    Symbol conn_;
    std::unique_ptr<ASTNode> left_;
    std::unique_ptr<ASTNode> right_;
};

// QuantifierNode 
class QuantifierNode final : public ASTNode {
public:
    QuantifierNode(Symbol quant, Symbol var, std::unique_ptr<ASTNode> body);

    [[nodiscard]] const Symbol& quantSymbol() const { return quant_; }
    [[nodiscard]] const Symbol& var() const { return var_; }
    [[nodiscard]] const ASTNode& body() const { return *body_; }

    [[nodiscard]] std::string toString() const override;
    [[nodiscard]] std::string toTPTP() const override;
    [[nodiscard]] std::unique_ptr<ASTNode> clone() const override;
    [[nodiscard]] std::unique_ptr<ASTNode> toNNF(bool negated) const override;

private:
    Symbol quant_;
    Symbol var_;
    std::unique_ptr<ASTNode> body_;
    static std::string tptpVar(const std::string& v);
};