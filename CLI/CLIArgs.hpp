#pragma once


//  Parsing e validazione completa della riga di comando. 

#include "../FragmentTypes.hpp"

#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  AppArgs
//
//  Raccoglie tutti i parametri applicativi dopo parsing e validazione.
//  main() non deve più leggere argv direttamente.
// ─────────────────────────────────────────────────────────────────────────────

struct AppArgs {
    // ── Identificativo frammento ─────────────────────────────────────────────
    std::string fragment;           // "fo2" | "fluted"

    // ── Configurazione generatore ────────────────────────────────────────────
    GenConfig   cfg;                // mode, depth, domainSize, transform,
                                    // output, budget, arityFilter, vocab

    // ── Contatori sessione ───────────────────────────────────────────────────
    int         count  = 5;         // numero di formule da generare
    unsigned    seed   = 0;         // seme RNG (se non fornito applica std::random_device)

    // ── Verifica con Vampire ─────────────────────────────────────────────────
    bool        verify         = false;
    std::string vampirePath    = "";
    int         vampireTimeout = 10;
};


// ─────────────────────────────────────────────────────────────────────────────

struct HelpRequest {};   // eccezione sentinella per --help

AppArgs parseArgs(int argc, char* argv[]);