/**
 * Implementacion del dialogo de error de Android. Vease AndroidError.hpp.
 *
 * (c) 2026 - Port de Android, misma licencia que el resto del codigo (MIT).
 **/

#include "AndroidError.hpp"

#if defined(ANDROID)

#  include <android/log.h>

#  include <SDL.h>
#  include <GLES2/gl2.h>

#  include <filesystem>
#  include <fstream>

#  include "AndroidPort.hpp"
#  include "DX8Graphics.hpp"
#  include "Main.hpp"

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

/*
 * Cartel de arranque.
 *
 * Se dibuja un rectangulo de color y una barra con la barra de progreso de carga usando el
 * vertex shader de color del motor (DrawColorVertices), asi que no depende de que las
 * texturas ya esten cargadas: es justo lo que hace falta antes de que el motor pinte su
 * primer fotograma.
 */
void Android_ShowStartupScreen(const std::string &title, const std::string &status) {
    (void)title;
    (void)status;

    __android_log_print(ANDROID_LOG_INFO, "Hurrican", "Pantalla de carga: %s", status.c_str());

    /*
     * Si la grafica todavia no esta inicializada no hay shaders con los que dibujar; en ese
     * caso se deja el framebuffer limpio (que ya es distinto de "no haber presentado nada").
     */
    const int w = DirectGraphics.GetWindowWidth();
    const int h = DirectGraphics.GetWindowHeight();
    if (w <= 0 || h <= 0)
        return;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, w, h);

    const glm::mat4x4 window_matrix = DirectGraphics.GetWindowProjection();

    // Fondo.
    VERTEX2D fondo[4] = {
        VERTEX2D{0.0f, 0.0f, HCOLOR(0xFF101018), 0.0f, 0.0f},
        VERTEX2D{0.0f, static_cast<float>(h), HCOLOR(0xFF101018), 0.0f, 0.0f},
        VERTEX2D{static_cast<float>(w), 0.0f, HCOLOR(0xFF101018), 0.0f, 0.0f},
        VERTEX2D{static_cast<float>(w), static_cast<float>(h), HCOLOR(0xFF101018), 0.0f, 0.0f},
    };
    DirectGraphics.DrawColorVertices(GL_TRIANGLE_STRIP, 4, fondo, window_matrix);

    // Barra de "cargando" centrada: da sensacion de actividad mientras se copian los datos.
    const float bw = static_cast<float>(w) * 0.40f;
    const float bh = static_cast<float>(h) * 0.028f;
    const float bx = (static_cast<float>(w) - bw) * 0.5f;
    const float by = static_cast<float>(h) * 0.52f;

    VERTEX2D barra[4] = {
        VERTEX2D{bx, by, HCOLOR(0xFF303040), 0.0f, 0.0f},
        VERTEX2D{bx, by + bh, HCOLOR(0xFF303040), 0.0f, 0.0f},
        VERTEX2D{bx + bw, by, HCOLOR(0xFF303040), 0.0f, 0.0f},
        VERTEX2D{bx + bw, by + bh, HCOLOR(0xFF303040), 0.0f, 0.0f},
    };
    DirectGraphics.DrawColorVertices(GL_TRIANGLE_STRIP, 4, barra, window_matrix);

    // Relleno parcial: es un indicador estatico (no hay bucle de animacion aqui), pero deja
    // claro que la app esta viva.
    const float relleno = bw * 0.35f;
    VERTEX2D progreso[4] = {
        VERTEX2D{bx, by, HCOLOR(0xFFE0A030), 0.0f, 0.0f},
        VERTEX2D{bx, by + bh, HCOLOR(0xFFE0A030), 0.0f, 0.0f},
        VERTEX2D{bx + relleno, by, HCOLOR(0xFFE0A030), 0.0f, 0.0f},
        VERTEX2D{bx + relleno, by + bh, HCOLOR(0xFFE0A030), 0.0f, 0.0f},
    };
    DirectGraphics.DrawColorVertices(GL_TRIANGLE_STRIP, 4, progreso, window_matrix);

    // Presentar ya: el motor aun no ha llamado a ShowBackBuffer().
    DirectGraphics.PresentNow();
}

#endif /* ANDROID */
