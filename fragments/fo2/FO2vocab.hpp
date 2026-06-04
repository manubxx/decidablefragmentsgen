#pragma once
#include "../../FragmentTypes.hpp"
#include <vector>

// FO2 Default Vocabulary  
// Coerente al 100% con predName(arity, index) con indici partenti da 0.
inline const std::vector<PredInfo> kVocabFO2 = {
    { "A0", 1 }, { "A1", 1 }, { "A2", 1 },
    { "B0", 2 }, { "B1", 2 }, { "B2", 2 }, { "B3", 2 }
};