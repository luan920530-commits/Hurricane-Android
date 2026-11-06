/**
 * Implementacion del dialogo de error de Android. Vease AndroidError.hpp.
 *
 * (c) 2026 - Port de Android, misma licencia que el resto del codigo (MIT).
 **/

#include "AndroidError.hpp"

#if defined(ANDROID)

#  include <android/log.h>

#  include <SDL.h>

#  include <filesystem>
#  include <fstream>

#  include "AndroidPort.hpp"

namespace {

/// Ultimas lineas del log, para que el usuario pueda pasarlas al reportar el fallo.
std::string ReadLogTail(std::size_t max_lines) {
    const std::string path = Android_GetSavePath() + "/Game_Log.txt";
    std::ifstream file(path.c_str());
    if (!file)
        return std::string();

    std::string line;
    std::string tail;
    std::size_t count = 0;
    while (std::getline(file, line)) {
        tail += line;
        tail += '\n';
        if (++count > 400) {
            // Ventana deslizante sencilla: se queda con lo ultimo leido.
            const std::size_t keep_from = tail.find('\n') + 1;
            tail.erase(0, keep_from);
            count = max_lines;
        }
    }
    return tail;
}

}  // namespace

void Android_ShowFatalError(const std::string &title, const std::string &message,
                            const std::string &log_tail) {
    std::string detail = message;
    detail += "\n\nEl registro completo esta en:\n";
    detail += Android_GetSavePath();
    detail += "/Game_Log.txt";

    const std::string tail = log_tail.empty() ? ReadLogTail(20) : log_tail;
    if (!tail.empty()) {
        detail += "\n\nUltimas lineas:\n";
        detail += tail;
    }

    __android_log_print(ANDROID_LOG_ERROR, "Hurrican", "%s: %s", title.c_str(), message.c_str());

    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title.c_str(), detail.c_str(), nullptr);
}

#endif /* ANDROID */
