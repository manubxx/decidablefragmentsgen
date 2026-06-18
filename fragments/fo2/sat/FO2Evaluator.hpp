#pragma once

#include "syntax/ASTNode.hpp"
#include "models/FiniteModel.hpp"

class FO2Evaluator : public ASTVisitor {
private:
    Assignment currentAssign;
    const FiniteModel& model_;
    bool result_ = false;

    
    static Variable varFromName(const std::string& name) {
        return (name == "v1" || name == "0") ? 0 : 1;
    }

public:
    FO2Evaluator(const Assignment& assign, const FiniteModel& model)  : currentAssign(assign), model_(model) {
    }

    [[nodiscard]] bool getResult() const { return result_; }

    void visit(const AtomicNode& node) override {
        const auto& args = node.args();
        Variable arg1 = varFromName(args[0].name);
        Variable arg2 = (args.size() > 1) ? varFromName(args[1].name) : 0;
        result_ = model_.evalAtom(node.predSymbol().name, node.predSymbol().arity, currentAssign, arg1, arg2);
    }

    void visit(const EqualityNode& node) override {
        Variable v1 = varFromName(node.lhs().name);
        Variable v2 = varFromName(node.rhs().name);
        result_ = (currentAssign[v1] == currentAssign[v2]);
    }

    void visit(const NegNode& node) override {
        node.child().accept(*this);
        result_ = !result_;
    }

    void visit(const BinaryConnNode& node) override {
        node.left().accept(*this);
        bool lv = result_;
        node.right().accept(*this);
        bool rv = result_;

        switch (node.connSymbol().type) {
        case SymbolType::AND:     result_ = lv && rv; break;
        case SymbolType::OR:      result_ = lv || rv; break;
        case SymbolType::IMPLIES: result_ = !lv || rv; break;
        default:                  result_ = false;
        }
    }

    void visit(const QuantifierNode& node) override {
        Variable v = varFromName(node.var().name);
        bool isExists = (node.quantSymbol().type == SymbolType::EXISTS);

        Assignment ext = currentAssign;
        int dSize = model_.domainSize();
        for (int e = 0; e < dSize; ++e) {
            ext[v] = e;

            // Backtracking dell'assegnamento (salvataggio stato)
            Assignment oldAssign = currentAssign;
            currentAssign = ext;

            node.body().accept(*this);
            bool val = result_;

            currentAssign = oldAssign;

            if (isExists && val) {
                result_ = true;
                return;
            }
            if (!isExists && !val) {
                result_ = false;
                return;
            }
        }
        result_ = !isExists;
    }
};