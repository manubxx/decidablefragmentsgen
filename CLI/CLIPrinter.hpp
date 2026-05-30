#pragma once

#include "../FragmentTypes.hpp"
#include "../vampire/VampireRunner.hpp"

#include <string>
#include <vector>



//  printHeader
//  Stampa il riepilogo della sessione prima della generazione.

void printHeader(const std::string&           fragment,
                 const GenConfig&             cfg,
                 int                          count,
                 unsigned                     seed,
                 const std::vector<PredInfo>& vocab,
                 bool                         verify,
                 const std::string&           vampirePath,
                 int                          vampireTimeout);


//  printFormula
//
//  Stampa una singola formula con il suo indice e, se richiesto,
//  lancia la verifica con Vampire e ne stampa il risultato.
//
//  Parametri:
//    idx           - indice  della formula nella sessione
//    cfg           — configurazione (usata per mode e output)
//    formulaStr    — formula già serializzata da generateFormatted()
//    verify        — se true, esegue la verifica vampire
//    runner        — puntatore al runner Vampire
//    vampireTimeout— timeout in secondi per Vampire
// ─────────────────────────────────────────────────────────────────────────────

void printFormula(int                  idx,
                  const GenConfig&     cfg,
                  const std::string&   formulaStr,
                  bool                 verify,
                  const VampireRunner* runner,
                  int                  vampireTimeout);