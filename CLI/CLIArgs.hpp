#pragma once
#include "../FragmentTypes.hpp"
#include <string>
#include <vector>

//  AppArgs
//  Raccoglie tutti i parametri.
struct AppArgs {
    std::string fragment;           // fo2, fluted, guarded, unaryneg,
    GenConfig   cfg;                // mode, depth, transform,output, budget, arityFilter, vocab
    int         count  = 5;         
    unsigned    seed   = 0;         

    // Verifica con Vampire 
    bool        verify         = false;
    std::string vampirePath    = "";
    int         vampireTimeout = 10;
};
struct HelpRequest {};   // sentinella per --help

AppArgs parseArgs(int argc, char* argv[]);