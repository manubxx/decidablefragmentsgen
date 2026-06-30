cmake_minimum_required(VERSION 3.16)
project(DecidableFragmentsGen LANGUAGES CXX)

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
 "tests/benchmark.cpp")


target_compile_options(DecidableFragmentsGen PRIVATE
    -Wall
    -Wextra
    -pedantic
    -O3
    -fsanitize=address
    -g        
)

target_link_options(DecidableFragmentsGen PRIVATE 
    -fsanitize=address
)

find_program(VAMPIRE_EXE NAMES vampire
   HINTS "${CMAKE_SOURCE_DIR}/utils"
)

if(VAMPIRE_EXE)
    message(STATUS "Vampire: ${VAMPIRE_EXE}")
    target_compile_definitions(DecidableFragmentsGen PRIVATE
        VAMPIRE_DEFAULT_PATH="${VAMPIRE_EXE}"
    )
    
    add_custom_command(TARGET DecidableFragmentsGen POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${VAMPIRE_EXE}" "$<TARGET_FILE_DIR:DecidableFragmentsGen>"
    )
else()
    message(WARNING "Vampire not found in utils! --verify will search 'vampire' in the global PATH")
endif()