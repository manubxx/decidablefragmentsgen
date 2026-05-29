#include "VampireRunner.hpp"
#include <filesystem>
#include <cstdio>
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <array>
#include <iostream>

#ifdef _WIN32
#  include <windows.h>
#  define POPEN  _popen
#  define PCLOSE _pclose
#else
#  include <cstring>
#  define POPEN  popen
#  define PCLOSE pclose
#endif

// Constructor

VampireRunner::VampireRunner(std::string vampirePath)
{
    if (!vampirePath.empty()) {
        vampirePath_ = std::move(vampirePath);
        return;
    }

    // Nessun percorso fornito: usa VAMPIRE_DEFAULT_PATH se definito da CMake,
    // altrimenti fallback a "vampire" (ricerca nel PATH di sistema).
#ifdef VAMPIRE_DEFAULT_PATH
    vampirePath_ = VAMPIRE_DEFAULT_PATH;
#else
    vampirePath_ = "vampire";
#endif
}

//isAvailable 

bool VampireRunner::isAvailable() const
{
    return std::filesystem::exists(vampirePath_);
}

// run

VampireRunner::Result VampireRunner::run(const std::string& tptpFormula,
    int timeLimitSec) const
{
    Result result;

    // Scrive la formula su un file temporaneo
    //  basato sul PID per evitare collisioni in run paralleli.
#ifdef _WIN32
    

    char tmpPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tmpPath);
    // Sostituire backslash con forward slash per la conversione WSL
    for (char* p = tmpPath; *p; ++p) if (*p == '\\') *p = '/';
    std::string inputFile = std::string(tmpPath) + "vamp_input_"
        + std::to_string(GetCurrentProcessId()) + ".p";

    
#else
    std::string inputFile = "/tmp/vamp_input_"
        + std::to_string(getpid()) + ".p";
#endif

    {
        std::ofstream ofs(inputFile);
        if (!ofs.is_open()) {
            result.runError = true;
            result.status = "Error";
            result.rawOutput = "Impossibile creare il file temporaneo: " + inputFile;
            return result;
        }
        ofs << tptpFormula << "\n";
    }

    // Comando Vampire
    //
    //  Opzioni usate:
    //    --mode casc          : modalita' di prova standard (CASC)
    //    --time_limit <n>     : timeout in secondi
    //    --output_axiom_names on : mantiene i nomi degli assiomi nell'output
   
   // Converte vampirePath_ in path WSL
    std::string wslVampire = vampirePath_;
    if (wslVampire.size() >= 2 && wslVampire[1] == ':') {
        char d = std::tolower(wslVampire[0]);
        wslVampire = "/mnt/" + std::string(1, d) + wslVampire.substr(2);
    }
    std::replace(wslVampire.begin(), wslVampire.end(), '\\', '/');

    // Converte inputFile in path WSL
    std::string wslInput = inputFile;
    if (wslInput.size() >= 2 && wslInput[1] == ':') {
        char d = std::tolower(wslInput[0]);
        wslInput = "/mnt/" + std::string(1, d) + wslInput.substr(2);
    }
    std::replace(wslInput.begin(), wslInput.end(), '\\', '/');

    std::ostringstream cmdStream;
    cmdStream << "wsl vampire"
        << " --mode casc"
        << " --time_limit " << timeLimitSec
        << " " << wslInput
        << " 2>&1";

    std::string cmd = cmdStream.str();
    

    // Esegue e restituisce l'output 

    FILE* pipe = POPEN(cmd.c_str(), "r");
    if (!pipe) {
        result.runError = true;
        result.status = "Error";
        result.rawOutput = "Impossibile avviare Vampire. "
            "Verifica che '" + vampirePath_ + "' sia nel PATH.";
        std::remove(inputFile.c_str());
        return result;
    }

    std::ostringstream outputStream;
    std::array<char, 256> buf;
    while (std::fgets(buf.data(), static_cast<int>(buf.size()), pipe))
        outputStream << buf.data();

    PCLOSE(pipe);
    std::remove(inputFile.c_str());

    result.rawOutput = outputStream.str();

    // Estrae lo status SZS
    result.status = extractSZSStatus(result.rawOutput);

    if (result.status == "Timeout")
        result.timedOut = true;

    // Se Vampire ha restituito un output vuoto 
    // il runner non è riuscito ad avviare il prover.
    if (result.rawOutput.empty()) {
        result.runError = true;
        result.status = "Error";
        result.rawOutput = "Nessun output da Vampire. "
            "Verifica che '" + vampirePath_ + "' sia nel PATH.";
    }

    return result;
}

// extractSZSStatus
// Vampire stampa lo status nel formato:
//   % SZS status <Status> for <problem>

std::string VampireRunner::extractSZSStatus(const std::string& output)
{
    const std::string marker = "SZS status ";
    auto pos = output.find(marker);
    if (pos == std::string::npos) {
        // Cerca fallback per timeout esplicito nel testo
        if (output.find("Time limit") != std::string::npos ||
            output.find("time limit") != std::string::npos)
            return "Timeout";
        return "Unknown";
    }

    pos += marker.size();
    // Legge fino al primo spazio o fine riga
    auto end = output.find_first_of(" \t\r\n", pos);
    if (end == std::string::npos)
        return output.substr(pos);

    return output.substr(pos, end - pos);
}