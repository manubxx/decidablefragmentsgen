#pragma once
#include "../../FragmentTypes.hpp"
#include <vector>

//  Vocabolario di default per il Fluted Fragment 

// Questo vocabolario è usato quando l'utente non specifica --preds.
// Per vocabolari personalizzati usa:
//   --preds 3/1,2/2,1/3,...   (3 pred. arità 1, 2 arità 2, 1 arità 3,...)


inline const std::vector<PredInfo> kVocabFL = {
    // arità 1 — ammissibili da stackDepth >= 1
    { "Q", 1 },
    { "S", 1 },
    { "U", 1 },
    // arità 2 — ammissibili da stackDepth >= 2
    { "P", 2 },
    { "R", 2 },
    { "T", 2 },
};