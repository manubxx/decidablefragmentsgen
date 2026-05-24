cmake_minimum_required(VERSION 3.16)
project(DecidableFragmentsGen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR})

add_executable(DecidableFragmentsGen
    main.cpp
    "fragments/fo2/FO2Generator.cpp"
    "fragments/fo2/FO2vocab.hpp"
    "fragments/fluted/FlutedGenerator.cpp"
    "fragments/fluted/Flutedvocab.hpp"
    "formulae/FormulaBuilder.cpp"
    "vampire/VampireRunner.cpp"
    "CLI/CLIPrinter.cpp"
    "CLI/CLIArgs.cpp"  

)

if(MSVC)    
    target_compile_options(DecidableFragmentsGen PRIVATE /EHsc)
endif()

# Vampire: cerca prima in third_party/, poi nel PATH
find_program(VAMPIRE_EXE NAMES vampire vampire.exe
   HINTS "${CMAKE_SOURCE_DIR}/utils/vampire-Windows-X64")

if(VAMPIRE_EXE)    
    message(STATUS "Vampire trovato: ${VAMPIRE_EXE}")    
    target_compile_definitions(DecidableFragmentsGen PRIVATE
        VAMPIRE_DEFAULT_PATH="${VAMPIRE_EXE}")    
    add_custom_command(TARGET DecidableFragmentsGen POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VAMPIRE_EXE}" "$<TARGET_FILE_DIR:DecidableFragmentsGen>")
else()    
    message(WARNING "Vampire non trovato: --verify non funzionerà")
endif()