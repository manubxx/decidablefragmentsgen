cmake_minimum_required(VERSION 3.16)
project(DecidableFragmentsGen LANGUAGES CXX)

# Ho aggiornato a 20 come nel tuo Makefile (se preferisci mantieni 17)
set(CMAKE_CXX_STANDARD 20) 
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR})

add_executable(DecidableFragmentsGen
    main.cpp
    "fragments/fo2/FO2Generator.cpp"
    "fragments/fo2/FO2vocab.hpp"
    "fragments/fo2/sat/FO2SATGenerator.cpp"
    "fragments/fluted/FlutedGenerator.cpp"
    "fragments/fluted/FLvocab.hpp"
    "fragments/guarded/GuardedGenerator.cpp"
    "fragments/guarded/GFvocab.hpp"
    "fragments/unarynegation/UnaryNegGenerator.cpp"
    "fragments/unarynegation/UNvocab.hpp"
    "formulae/FormulaBuilder.cpp"
    "vampire/VampireRunner.cpp"
    "CLI/CLIPrinter.cpp"
    "CLI/CLIArgs.cpp"  
    "syntax/ASTNode.cpp"  
    "tests/fo2/testFO2.cpp"
)

# --- CONFIGURAZIONE FLAG COMPILATORE ---
# --- CONFIGURAZIONE FLAG COMPILATORE ---
if(MSVC)
    # Flag per Windows (compilatore Visual Studio)
    target_compile_options(DecidableFragmentsGen PRIVATE 
        /EHsc      # Gestione eccezioni standard C++
        /W4        # Warning di alto livello
        # Nota: ho rimosso /O2. CMake gestisce le ottimizzazioni automaticamente
        # quando decidi di compilare in modalità "Release".
    )
else()
    # Flag per Linux (compilatori GCC o Clang) o MinGW
    target_compile_options(DecidableFragmentsGen PRIVATE
        -Wall
        -pedantic
        -O3
        -fsanitize=address
        -g         # Utile per avere informazioni esatte sulla riga
    )
    # Su Linux, AddressSanitizer richiede di essere passato anche al linker!
    target_link_options(DecidableFragmentsGen PRIVATE 
        -fsanitize=address
    )
endif()

# --- CONFIGURAZIONE VAMPIRE ---
# Cerca prima in third_party/, poi nel PATH
find_program(VAMPIRE_EXE NAMES vampire vampire.exe
   HINTS "${CMAKE_SOURCE_DIR}/utils/vampire-Windows-X64"
)

if(VAMPIRE_EXE)
    message(STATUS "Vampire trovato: ${VAMPIRE_EXE}")
    target_compile_definitions(DecidableFragmentsGen PRIVATE
        VAMPIRE_DEFAULT_PATH="${VAMPIRE_EXE}"
    )
    add_custom_command(TARGET DecidableFragmentsGen POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${VAMPIRE_EXE}" "$<TARGET_FILE_DIR:DecidableFragmentsGen>"
    )
else()
    message(WARNING "Vampire non trovato: --verify non funzionerà")
endif()