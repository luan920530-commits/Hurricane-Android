/**
 * This implements logging to a file and cout.
 *
 * (c) 2002 Jörg M. Winterstein
 * (c) 2017 Stefan Schmidt
 **/

#include "Logdatei.hpp"
#include <iostream>
#include <filesystem>
#include <cstdlib>
#if defined(ANDROID)
#  include <android/log.h>
#  include <unistd.h>
#endif
namespace fs = std::filesystem;

/**
 * Construct the Logger object and open the logfile
 *
 * IMPORTANTE (Android): este objeto es global, asi que se construye ANTES de que se ejecute
 * SDL_main() y antes de que exista el directorio de guardado. Abrir "Game_Log.txt" con una
 * ruta relativa dependia del directorio de trabajo del proceso, que en Android es "/" (no
 * se puede escribir): el ofstream fallaba y el exit(EXIT_FAILURE) de abajo cerraba la app
 * nada mas abrirla, sin mensaje y sin log. Ahora, si el fichero no se puede abrir, el log
 * sigue funcionando en memoria + logcat en lugar de matar el proceso.
 **/
Logdatei::Logdatei(const std::string &filename)
    : filename_(filename), file(), delLogFile(false) {
    // Reduce unnecessary syncs
    std::cout.sync_with_stdio(false);

    openFile(filename);

    if (!file.is_open()) {
#if defined(ANDROID)
        __android_log_print(ANDROID_LOG_WARN, "Hurrican",
                            "No se pudo abrir el fichero de log '%s'; se registra solo en logcat",
                            filename.c_str());
#else
        std::cerr << "Unable to open logfile (" << filename << ")! Logging to console only." << std::endl;
#endif
    }
}

/**
 * Abre (o reabre) el fichero de log.
 *
 * En Android hace falta reabrir el fichero cuando ya se conoce el directorio de guardado
 * (files/save): la primera vez que se construye este objeto ese directorio aun no existe.
 * Devuelve true si el fichero quedo abierto.
 **/
bool Logdatei::openFile(const std::string &path) {
    if (file.is_open())
        file.close();

    file.open(path, std::ios::out | std::ios::trunc);
    if (file.is_open()) {
        filename_ = path;
        return true;
    }
    return false;
}

Logdatei::~Logdatei() {
    // make sure no output gets lost
    flush();
    if (file.is_open())
        file.close();

    // Kein Fehler im Game? Dann Logfile löschen
    if (delLogFile)
        fs::remove(fs::path(filename_));
}

/**
 * Puts the internal buffer into the output-streams, flushes them and empties the buffer
 **/
void Logdatei::flush() {
#if defined(ANDROID)
    /*
     * std::cout no llega a ningun sitio util en Android: se manda todo a logcat con la
     * etiqueta "Hurrican" (adb logcat -s Hurrican) ademas del fichero de log.
     */
    const std::string text = this->str();
    if (!text.empty()) {
        __android_log_write(ANDROID_LOG_INFO, "Hurrican", text.c_str());
    }
    if (file.is_open()) {
        file << text;
        file << std::flush;
    }
    this->str("");
    return;
#else
    // print to both outputs
    std::cout << this->str();
    this->file << this->str();

    // flush both streams to ensure they are up-to-date
    std::cout << std::flush;
    this->file << std::flush;

    // empty internal buffer
    this->str("");
#endif
}
