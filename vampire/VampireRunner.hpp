#pragma once
#include <string>

//  VampireRunner

//  La formula deve essere già in formato TPTP 
// VampireRunner la scrive su un file temporaneo e lancia Vampire come sottoprocesso

//  Status SZS restituiti da Vampire:
//    Theorem             — formula valida (UNSAT del negato)
//    CounterSatisfiable  — formula soddisfacibile (SAT)
//    Satisfiable         — formula soddisfacibile 
//    Unsatisfiable       — formula insoddisfacibile
//    Timeout             — limite di tempo superato
//    Unknown             — Vampire non ha trovato una risposta
//    Error               — errore interno al runner (Vampire non trovato, ecc.)


class VampireRunner {
public:

    struct Result {
        std::string status;   // Status SZS 
        std::string rawOutput; //debug
        bool        timedOut = false;
        bool        runError = false; // true se Vampire non è stato avviato
    };

  
    // VAMPIRE_DEFAULT_PATH viene definito da CMakeLists.txt
    explicit VampireRunner(std::string vampirePath = "");

    // Esegue Vampire sulla formula TPTP fornita come stringa.
    // timeLimitSec: timeout in secondi (passato a --time_limit di Vampire).
    Result run(const std::string& tptpFormula, int timeLimitSec = 10) const;

    bool isAvailable() const;

private:
    std::string vampirePath_;

    // Estrae lo status SZS dall'output di Vampire.
    static std::string extractSZSStatus(const std::string& output);
};