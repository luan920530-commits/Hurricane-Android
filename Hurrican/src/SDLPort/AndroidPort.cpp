/**
 * Implementacion de AndroidPort.hpp
 *
 * Estrategia de almacenamiento
 * ----------------------------
 * En Android el APK es de solo lectura y los assets (todo lo que hay bajo
 * app/src/main/assets) solo se pueden leer a traves del AssetManager. El motor, en cambio,
 * lee cientos de ficheros con std::ifstream y std::filesystem, asi que la opcion mas
 * robusta y menos invasiva es copiar una vez los datos a un directorio normal:
 *
 *     /data/data/<paquete>/files/data   <- texturas, niveles, musica, sonidos, shaders
 *     /data/data/<paquete>/files/lang   <- traducciones
 *     /data/data/<paquete>/files/save   <- Hurrican.cfg, partidas, records, Game_Log.txt
 *
 * El primer arranque tarda unos segundos (son ~90 MB); despues se reutiliza la copia. Si se
 * publica una actualizacion con datos nuevos basta con subir ANDROID_ASSET_VERSION para
 * forzar la recopia.
 *
 * (c) 2026 - Port de Android, misma licencia que el resto del codigo (MIT).
 **/

#if defined(ANDROID)

#  include "AndroidPort.hpp"

#  include <android/asset_manager.h>
#  include <android/asset_manager_jni.h>
#  include <android/log.h>
#  include <jni.h>
#  include <dirent.h>
#  include <sys/stat.h>
#  include <unistd.h>

#  include <cstdio>
#  include <cstring>
#  include <filesystem>
#  include <fstream>
#  include <vector>

#  include <SDL.h>
#  include <SDL_system.h>

namespace fs = std::filesystem;

namespace {

/// Subir este numero cada vez que cambien los datos empaquetados en assets/.
constexpr const char *ANDROID_ASSET_VERSION = "1";

constexpr const char *LOG_TAG = "Hurrican";

std::string g_data_path;
std::string g_save_path;

void LogInfo(const std::string &msg) {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", msg.c_str());
}

void LogError(const std::string &msg) {
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "%s", msg.c_str());
}

std::string JoinPath(const std::string &a, const std::string &b) {
    if (a.empty())
        return b;
    if (a.back() == '/')
        return a + b;
    return a + "/" + b;
}

std::string GetFilesDir() {
    const char *path = SDL_AndroidGetInternalStoragePath();
    if (path == nullptr || path[0] == '\0')
        return std::string();
    return std::string(path);
}

/// Lee un fichero de texto pequeno completo (o devuelve "" si no existe).
std::string ReadWholeFile(const std::string &path) {
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file)
        return std::string();
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

bool WriteWholeFile(const std::string &path, const std::string &contents) {
    std::ofstream file(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!file)
        return false;
    file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    return file.good();
}

/**
 * Copia recursivamente un directorio de assets al almacenamiento interno.
 *
 * Se usa AAssetDir para enumerar, que es la unica API disponible: el NDK no puede listar
 * directorios dentro del APK como si fueran un sistema de ficheros normal.
 */
bool CopyAssetDir(AAssetManager *manager, const std::string &asset_dir, const std::string &dest_dir,
                  int depth = 0) {
    if (manager == nullptr)
        return false;

    AAssetDir *dir = AAssetManager_openDir(manager, asset_dir.c_str());
    if (dir == nullptr) {
        LogError("No se pudo abrir el directorio de assets: " + asset_dir);
        return false;
    }

    ::mkdir(dest_dir.c_str(), 0770);

    bool ok = true;
    const char *entry = nullptr;
    while ((entry = AAssetDir_getNextFileName(dir)) != nullptr) {
        const std::string name(entry);
        const std::string asset_path = asset_dir.empty() ? name : asset_dir + "/" + name;
        const std::string dest_path = JoinPath(dest_dir, name);

        AAsset *asset = AAssetManager_open(manager, asset_path.c_str(), AASSET_MODE_STREAMING);
        if (asset != nullptr) {
            const off_t size = AAsset_getLength(asset);
            std::ofstream out(dest_path.c_str(), std::ios::binary | std::ios::trunc);
            if (!out) {
                AAsset_close(asset);
                LogError("No se pudo escribir " + dest_path);
                ok = false;
                continue;
            }

            constexpr int CHUNK = 64 * 1024;
            std::vector<char> buffer(CHUNK);
            off_t remaining = size;
            while (remaining > 0) {
                const int to_read = static_cast<int>(remaining < CHUNK ? remaining : CHUNK);
                const int read = AAsset_read(asset, buffer.data(), static_cast<size_t>(to_read));
                if (read <= 0)
                    break;
                out.write(buffer.data(), read);
                remaining -= read;
            }
            AAsset_close(asset);
            continue;
        }

        // No es un fichero: puede ser un subdirectorio. AAssetManager_openDir no distingue
        // de forma fiable, asi que se intenta descender y se ignora si esta vacio.
        if (depth < 8) {
            CopyAssetDir(manager, asset_path, dest_path, depth + 1);
        }
    }

    AAssetDir_close(dir);
    return ok;
}

}  // namespace

std::string Android_GetInstalledDataVersion() {
    const std::string files_dir = GetFilesDir();
    if (files_dir.empty())
        return std::string();
    return ReadWholeFile(JoinPath(files_dir, "data_version"));
}

std::string Android_InitStorage() {
    const std::string files_dir = GetFilesDir();
    if (files_dir.empty()) {
        LogError("SDL_AndroidGetInternalStoragePath() devolvio una ruta vacia");
        g_data_path = ".";
        g_save_path = ".";
        return g_data_path;
    }

    g_save_path = JoinPath(files_dir, "save");
    ::mkdir(g_save_path.c_str(), 0770);

    const std::string version_file = JoinPath(files_dir, "data_version");
    const std::string installed_version = ReadWholeFile(version_file);
    const bool data_present = fs::is_directory(JoinPath(files_dir, "data/textures")) &&
                              fs::is_directory(JoinPath(files_dir, "lang"));
    const bool assets_changed = installed_version != ANDROID_ASSET_VERSION;

    if (!data_present || assets_changed) {
        // Si los datos son de otra version se copian encima; los ficheros sobrantes de una
        // version anterior son inofensivos y se dejan para no complicar el borrado.
        LogInfo(assets_changed ? "Instalando/actualizando los datos del juego..."
                               : "Primera ejecucion: copiando los datos del juego desde el APK...");

        /*
         * SDL no expone el AssetManager directamente. Lo mas estable es pedir el Context de
         * la actividad (getContext es estatico en SDLActivity) y de ahi getAssets(); asi no
         * dependemos de que la actividad concreta exponga nada especial.
         */
        AAssetManager *manager = nullptr;
        JNIEnv *env = static_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());
        jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
        if (env != nullptr && activity != nullptr) {
            jclass cls = env->GetObjectClass(static_cast<jobject>(activity));
            if (cls != nullptr) {
                jmethodID mid_get_assets =
                    env->GetMethodID(cls, "getAssets", "()Landroid/content/res/AssetManager;");
                if (mid_get_assets != nullptr) {
                    jobject asset_manager = env->CallObjectMethod(static_cast<jobject>(activity), mid_get_assets);
                    if (asset_manager != nullptr) {
                        manager = AAssetManager_fromJava(env, asset_manager);
                        env->DeleteLocalRef(asset_manager);
                    }
                } else {
                    // No hay metodo getAssets en la actividad: limpiar la excepcion pendiente.
                    env->ExceptionClear();
                }
                env->DeleteLocalRef(cls);
            }
            env->DeleteLocalRef(static_cast<jobject>(activity));
        }

        if (manager == nullptr) {
            LogError("No se pudo obtener el AssetManager de Android");
            g_data_path = ".";
            return g_data_path;
        }

        const bool data_ok = CopyAssetDir(manager, "data", JoinPath(files_dir, "data"));
        const bool lang_ok = CopyAssetDir(manager, "lang", JoinPath(files_dir, "lang"));
        if (data_ok && lang_ok) {
            WriteWholeFile(version_file, ANDROID_ASSET_VERSION);
            LogInfo("Datos del juego instalados correctamente");
        } else {
            LogError("La copia de datos termino con errores; el juego puede no arrancar");
        }
    } else {
        LogInfo("Usando los datos ya instalados (version " + installed_version + ")");
    }

    g_data_path = files_dir;
    return g_data_path;
}

const std::string &Android_GetSavePath() {
    return g_save_path;
}

#endif /* ANDROID */
