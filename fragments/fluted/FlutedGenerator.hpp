#ifndef FLUTED_GENERATOR_HPP
#define FLUTED_GENERATOR_HPP
#include "../../formulae/FormulaBuilder.hpp"
#include <vector>
#include <string>
#include <memory>
class FlutedGenerator : public FormulaBuilder {
public:
   
    FlutedGenerator(std::vector<PredInfo> vocab, unsigned seed);
    ~FlutedGenerator() override = default;
    // FormulaBuilder Interface
    std::string fragmentName() const override;
    std::string startVar()     const override;
    std::string nextVar(const std::string& current) const override;
   

    std::string generateFormatted(const GenConfig& cfg) override;
    std::unique_ptr<ASTNode> generateSAT(int depth, int domainSize, BudgetState& budget) override;
    
    std::unique_ptr<AtomicNode> buildAtomic(const std::string& currentVar) override;
private:

    std::vector<PredInfo> vocab_;      
    std::vector<PredInfo> activeVocab_; 
   

    std::unique_ptr<ASTNode> buildFL(int depth, int stackDepth, BudgetState& budget);

    std::unique_ptr<AtomicNode> buildAtomicLeaf(int stackDepth);
   
    std::unique_ptr<EqualityNode> buildEqualityLeaf(int stackDepth);

    // Utility
    static std::string varName(int n);
    std::vector<Symbol> flutedArgs(int stackDepth, int arity) const;
    std::vector<int> admissiblePreds(int stackDepth) const;
    int minArity() const;
};
#endif // FLUTED_GENERATOR_HPP
