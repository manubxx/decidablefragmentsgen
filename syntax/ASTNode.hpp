#pragma once

#include "Symbol.hpp"
#include <memory>
#include <string>
#include <vector>

//  Visitor pattern Forward declarations
class AtomicNode;
class EqualityNode;
class NegNode;
class BinaryConnNode;
class QuantifierNode;

class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;

    virtual void visit(const AtomicNode& node) = 0;
    virtual void visit(const EqualityNode& node) = 0;
    virtual void visit(const NegNode& node) = 0;
    virtual void visit(const BinaryConnNode& node) = 0;
    virtual void visit(const QuantifierNode& node) = 0;
};

// Abstract Syntax Tree 
class ASTNode {
public:
    virtual ~ASTNode() = default;

    virtual std::string toString() const = 0;
    virtual std::string toTPTP() const = 0;
    virtual std::unique_ptr<ASTNode> clone() const = 0;
    virtual std::unique_ptr<ASTNode> toNNF(bool negated = false) const = 0;

    
    virtual void accept(ASTVisitor& visitor) const = 0;
protected:

};

// AtomicNode 
class AtomicNode final : public ASTNode {
public:
    AtomicNode(Symbol predSymbol, std::vector<Symbol> args);

    const Symbol& predSymbol() const { return pred_; }
    const std::vector<Symbol>& args() const { return args_; }

    std::string toString() const override;
    std::string toTPTP() const override;
    std::unique_ptr<ASTNode> clone() const override;
    std::unique_ptr<ASTNode> toNNF(bool negated) const override;

    void accept(ASTVisitor& visitor) const override;

private:
    Symbol pred_;
    std::vector<Symbol> args_;
    static std::string tptpVar(const std::string& v);
};

// EqualityNode
class EqualityNode final : public ASTNode {
public:
    EqualityNode(Symbol lhs, Symbol rhs);

    const Symbol& lhs() const { return lhs_; }
    const Symbol& rhs() const { return rhs_; }

    std::string toString() const override;
    std::string toTPTP() const override;
    std::unique_ptr<ASTNode> clone() const override;
    std::unique_ptr<ASTNode> toNNF(bool negated) const override;

    void accept(ASTVisitor& visitor) const override;

private:
    Symbol lhs_;
    Symbol rhs_;
    static std::string tptpVar(const std::string& v);
};

// NegNode
class NegNode final : public ASTNode {
public:
    explicit NegNode(std::unique_ptr<ASTNode> child);

    const ASTNode& child() const { return *child_; }

    std::string toString() const override;
    std::string toTPTP() const override;
    std::unique_ptr<ASTNode> clone() const override;
    std::unique_ptr<ASTNode> toNNF(bool negated) const override;

    void accept(ASTVisitor& visitor) const override;

private:
    std::unique_ptr<ASTNode> child_;
};

// BinaryConnNode 
class BinaryConnNode final : public ASTNode {
public:
    BinaryConnNode(Symbol connSymbol, std::unique_ptr<ASTNode> left, std::unique_ptr<ASTNode> right);

    const Symbol& connSymbol() const { return conn_; }
    const ASTNode& left() const { return *left_; }
    const ASTNode& right() const { return *right_; }

    std::string toString() const override;
    std::string toTPTP() const override;
    std::unique_ptr<ASTNode> clone() const override;
    std::unique_ptr<ASTNode> toNNF(bool negated) const override;

    void accept(ASTVisitor& visitor) const override;

private:
    Symbol conn_;
    std::unique_ptr<ASTNode> left_;
    std::unique_ptr<ASTNode> right_;
};

// QuantifierNode 
class QuantifierNode final : public ASTNode {
public:
    QuantifierNode(Symbol quant, Symbol var, std::unique_ptr<ASTNode> body);

    const Symbol& quantSymbol() const { return quant_; }
    const Symbol& var() const { return var_; }
    const ASTNode& body() const { return *body_; }

    std::string toString() const override;
    std::string toTPTP() const override;
    std::unique_ptr<ASTNode> clone() const override;
    std::unique_ptr<ASTNode> toNNF(bool negated) const override;

    void accept(ASTVisitor& visitor) const override;

private:
    Symbol quant_;
    Symbol var_;
    std::unique_ptr<ASTNode> body_;
    static std::string tptpVar(const std::string& v);
};