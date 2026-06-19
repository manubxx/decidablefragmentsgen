#pragma once
#include "../FragmentTypes.hpp"
#include <string>
#include <vector>

//  AppArgs
struct AppArgs {
    std::string fragment;           // fo2, fluted, guarded, unaryneg,
    GenConfig   cfg;                // mode, depth, transform, output, budget, arityFilter,vocab
    int         count  = 5;         
    unsigned    seed   = 0;         

    // Vampire verification 
    bool        verify         = false;
    std::string vampirePath    = "";
    int         vampireTimeout = 10;

    //tests
    std::string testMode = "";
};
struct HelpRequest {};   // --help guard

AppArgs parseArgs(int argc, char* argv[]);