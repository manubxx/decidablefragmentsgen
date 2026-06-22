#include "ASTNode.hpp"
#include <stdexcept>
#include <algorithm>
#include <cctype>


// AtomicNode Implementation
// ==========================================
AtomicNode::AtomicNode(Symbol predSymbol, std::vector<Symbol> args) : pred_(std::move(predSymbol)), args_(std::move(args))
{
    if (pred_.type != SymbolType::PREDICATE)
        throw std::invalid_argument("AtomicNode: symbol is not PREDICATE");

    if (static_cast<int>(args_.size()) != pred_.arity)
        throw std::invalid_argument("AtomicNode: args number != preds arity: (" + std::to_string(pred_.arity) + ")");
}

std::string AtomicNode::toString() const {
    std::string s = pred_.name + "(";
    for (std::size_t i = 0; i < args_.size(); ++i) {
        if (i) s += ",";
        s += args_[i].name;
    }
    return s + ")";
}

std::string AtomicNode::toTPTP() const {
    std::string pname = pred_.name;
    for (auto& c : pname) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    std::string s = pname + "(";
    for (std::size_t i = 0; i < args_.size(); ++i) {
        if (i) s += ",";
        s += tptpVar(args_[i].name);
    }
    return s + ")";
}

std::unique_ptr<ASTNode> AtomicNode::clone() const {
    return std::make_unique<AtomicNode>(pred_, args_);
}

std::unique_ptr<ASTNode> AtomicNode::toNNF(bool negated) const {
    if (!negated) return clone();
    return std::make_unique<NegNode>(clone());
}

std::string AtomicNode::tptpVar(const std::string& v) {
    if (v.empty()) return v;
    std::string r = v;
    r[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(r[0])));
    return r;
}

void AtomicNode::accept(ASTVisitor& visitor) const {
    visitor.visit(*this);
}


// EqualityNode Implementation
// ==========================================
EqualityNode::EqualityNode(Symbol lhs, Symbol rhs) : lhs_(std::move(lhs)), rhs_(std::move(rhs))
{
    if (lhs_.type != SymbolType::VARIABLE)
        throw std::invalid_argument("EqualityNode: lhs is not a VARIABLE: " + lhs_.name);
    if (rhs_.type != SymbolType::VARIABLE)
        throw std::invalid_argument("EqualityNode: rhs is not a VARIABLE: " + rhs_.name);
}

std::string EqualityNode::toString() const {
    return "(" + lhs_.name + " = " + rhs_.name + ")";
}

std::string EqualityNode::toTPTP() const {
    return tptpVar(lhs_.name) + " = " + tptpVar(rhs_.name);
}

std::unique_ptr<ASTNode> EqualityNode::clone() const {
    return std::make_unique<EqualityNode>(lhs_, rhs_);
}

std::unique_ptr<ASTNode> EqualityNode::toNNF(bool negated) const {
    if (!negated) return clone();
    return std::make_unique<NegNode>(clone());
}

std::string EqualityNode::tptpVar(const std::string& v) {
    if (v.empty()) return v;
    std::string r = v;
    r[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(r[0])));
    return r;
}

void EqualityNode::accept(ASTVisitor& visitor) const {
    visitor.visit(*this);
}


// NegNode Implementation
// ==========================================

NegNode::NegNode(std::unique_ptr<ASTNode> child) : child_(std::move(child)) {
    if (!child_) throw std::invalid_argument("NegNode: null child");
}

std::string NegNode::toString() const {
    return "(~ " + child_->toString() + ")";
}

std::string NegNode::toTPTP() const {
    return "(~" + child_->toTPTP() + ")";
}

std::unique_ptr<ASTNode> NegNode::clone() const {
    return std::make_unique<NegNode>(child_->clone());
}

std::unique_ptr<ASTNode> NegNode::toNNF(bool negated) const {
    return child_->toNNF(!negated);
}

void NegNode::accept(ASTVisitor& visitor) const {
    visitor.visit(*this);
}


// BinaryConnNode Implementation
// ==========================================
BinaryConnNode::BinaryConnNode(Symbol connSymbol, std::unique_ptr<ASTNode> left, std::unique_ptr<ASTNode> right)
    : conn_(std::move(connSymbol)), left_(std::move(left)), right_(std::move(right))
{
    if (conn_.type != SymbolType::AND && conn_.type != SymbolType::OR && conn_.type != SymbolType::IMPLIES)
        throw std::invalid_argument("BinaryConnNode: not binary symbol " + conn_.name);
    if (!left_ || !right_)
        throw std::invalid_argument("BinaryConnNode: null child");
}

std::string BinaryConnNode::toString() const {
    return "(" + left_->toString() + " " + conn_.name + " " + right_->toString() + ")";
}

std::string BinaryConnNode::toTPTP() const {
    std::string op;
    switch (conn_.type) {
    case SymbolType::AND:     op = "&";  break;
    case SymbolType::OR:      op = "|";  break;
    case SymbolType::IMPLIES: op = "=>"; break;
    default:                  op = "?";
    }
    return "(" + left_->toTPTP() + " " + op + " " + right_->toTPTP() + ")";
}

std::unique_ptr<ASTNode> BinaryConnNode::clone() const {
    return std::make_unique<BinaryConnNode>(conn_, left_->clone(), right_->clone());
}

std::unique_ptr<ASTNode> BinaryConnNode::toNNF(bool negated) const {
    if (conn_.type == SymbolType::IMPLIES) {
        auto equiv = std::make_unique<BinaryConnNode>(
            Symbol::or_(),
            std::make_unique<NegNode>(left_->clone()),
            right_->clone());
        return equiv->toNNF(negated);
    }
    Symbol newConn = conn_;
    if (negated) {
        newConn = (conn_.type == SymbolType::AND) ? Symbol::or_() : Symbol::and_();
    }
    return std::make_unique<BinaryConnNode>(
        newConn, left_->toNNF(negated), right_->toNNF(negated));
}

void BinaryConnNode::accept(ASTVisitor& visitor) const {
    visitor.visit(*this);
}

// QuantifierNode Implementation
// ==========================================

QuantifierNode::QuantifierNode(Symbol quant, Symbol var, std::unique_ptr<ASTNode> body) : quant_(std::move(quant)), var_(std::move(var)), body_(std::move(body))
{
    if (quant_.type != SymbolType::FORALL && quant_.type != SymbolType::EXISTS)
        throw std::invalid_argument("QuantifierNode: symbol is not a quantifier: " + quant_.name);
    if (var_.type != SymbolType::VARIABLE)
        throw std::invalid_argument("QuantifierNode: var is not a variable " + var_.name);
    if (!body_)
        throw std::invalid_argument("QuantifierNode: null body");
}

std::string QuantifierNode::toString() const {
    return "(" + quant_.name + " " + var_.name + " " + body_->toString() + ")";
}

std::string QuantifierNode::toTPTP() const {
    std::string q = (quant_.type == SymbolType::EXISTS) ? "?" : "!";
    std::string v = tptpVar(var_.name);
    return "(" + q + " [" + v + "] : " + body_->toTPTP() + ")";
}

std::unique_ptr<ASTNode> QuantifierNode::clone() const {
    return std::make_unique<QuantifierNode>(quant_, var_, body_->clone());
}

std::unique_ptr<ASTNode> QuantifierNode::toNNF(bool negated) const {
    Symbol newQ = negated ? (quant_.type == SymbolType::FORALL ? Symbol::exists() : Symbol::forall()) : quant_;
    return std::make_unique<QuantifierNode>(newQ, var_, body_->toNNF(negated));
}

std::string QuantifierNode::tptpVar(const std::string& v) {
    if (v.empty()) return v;
    std::string r = v;
    r[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(r[0])));
    return r;
}

void QuantifierNode::accept(ASTVisitor& visitor) const {
    visitor.visit(*this);
}