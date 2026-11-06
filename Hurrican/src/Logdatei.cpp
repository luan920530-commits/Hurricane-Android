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
 **/
Logdatei::Logdatei(const std::string &filename)
    : filename_(filename), file(std::ofstream(filename)), delLogFile(false) {
    // Reduce unnecessary syncs
    std::cout.sync_with_stdio(false);

    if (!file) {
        std::cerr << "Unable to open logfile (" << filename << ")!" << std::endl;
        exit(EXIT_FAILURE);
    }
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
