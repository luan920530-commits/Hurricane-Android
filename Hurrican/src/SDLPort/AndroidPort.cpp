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

#  include "Logdatei.hpp"

#  include <android/asset_manager.h>
#  include <android/asset_manager_jni.h>
#  include <android/log.h>
#  include <jni.h>
#  include <dirent.h>
#  include <sys/stat.h>
#  include <unistd.h>

#  include <cstdio>
#  include <cstdlib>
#  include <cstring>
#  include <sstream>
#  include <string>
#  include <filesystem>
#  include <fstream>
#  include <vector>

#  include <SDL.h>
#  include <SDL_system.h>

namespace fs = std::filesystem;

namespace {

/// Subir este numero cada vez que cambien los datos empaquetados en assets/ o la forma de
/// copiarlos. La version 4 actualizo los textos/creditos, la version 5 cambia los iconos
/// tactiles a los recursos v2 y la version 6 incorpora las hojas vectoriales del personaje
/// femenino. La version 7 incorpora los iconos tactiles Lucide; cada cambio fuerza a las
/// instalaciones existentes a recopiarlos.
constexpr const char *ANDROID_ASSET_VERSION = "7";

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
 * Copia los datos del APK al almacenamiento interno leyendo un INDICE de ficheros.
 *
 * Por que un indice y no un recorrido de directorios: la unica API del NDK para listar es
 * AAssetDir_getNextFileName(), y esa devuelve unicamente los FICHEROS de un directorio,
 * nunca los subdirectorios. Un recorrido recursivo con ella copiaria data/*.txt y se
 * dejaria fuera data/textures, data/levels, data/music... es decir, el juego entero. El
 * indice (assets/hurrican_index.txt) lo genera Gradle al empaquetar los datos, de modo que
 * siempre lista exactamente lo que hay dentro del APK.
 *
 * Cada linea tiene la forma "<tamano> <ruta relativa>", por ejemplo:
 *     32768 data/textures/level1.png
 * El tamano se usa para reservar de una vez y para poder avisar en el log si algun fichero
 * queda incompleto.
 */
bool CopyAssetsFromIndex(AAssetManager *manager, const std::string &files_dir) {
    if (manager == nullptr)
        return false;

    AAsset *index_asset =
        AAssetManager_open(manager, "hurrican_index.txt", AASSET_MODE_BUFFER);
    if (index_asset == nullptr) {
        LogError("No se encuentra hurrican_index.txt dentro del APK");
        return false;
    }

    const off_t index_size = AAsset_getLength(index_asset);
    std::string index(static_cast<size_t>(index_size), '\0');
    const int leido = AAsset_read(index_asset, &index[0], static_cast<size_t>(index_size));
    AAsset_close(index_asset);
    if (leido <= 0) {
        LogError("El indice de assets esta vacio");
        return false;
    }
    index.resize(static_cast<size_t>(leido));

    int copiados = 0;
    int fallos = 0;
    long long bytes = 0;

    std::istringstream lineas(index);
    std::string linea;
    std::vector<char> buffer(64 * 1024);

    while (std::getline(lineas, linea)) {
        // Tolerar finales de linea de Windows y lineas vacias.
        if (!linea.empty() && linea.back() == '\r')
            linea.pop_back();
        if (linea.empty())
            continue;

        const size_t espacio = linea.find(' ');
        if (espacio == std::string::npos)
            continue;
        const long long tamano = std::atoll(linea.substr(0, espacio).c_str());
        const std::string relativa = linea.substr(espacio + 1);

        // Nunca escribir fuera del directorio de datos, aunque el indice viniera corrupto.
        if (relativa.find("..") != std::string::npos || relativa.empty() ||
            relativa.front() == '/') {
            LogError("Ruta de asset no valida: " + relativa);
            ++fallos;
            continue;
        }

        const std::string destino = JoinPath(files_dir, relativa);

        // Crear los directorios intermedios (files/data/textures, files/lang, ...).
        std::error_code ec;
        fs::create_directories(fs::path(destino).parent_path(), ec);

        AAsset *asset =
            AAssetManager_open(manager, relativa.c_str(), AASSET_MODE_STREAMING);
        if (asset == nullptr) {
            LogError("No se pudo abrir el asset " + relativa);
            ++fallos;
            continue;
        }

        std::ofstream out(destino.c_str(), std::ios::binary | std::ios::trunc);
        if (!out) {
            AAsset_close(asset);
            LogError("No se pudo escribir " + destino);
            ++fallos;
            continue;
        }

        long long pendiente = tamano;
        bool completo = true;
        while (pendiente > 0) {
            const int a_leer =
                static_cast<int>(pendiente < (long long)buffer.size() ? pendiente
                                                                      : (long long)buffer.size());
            const int n = AAsset_read(asset, buffer.data(), static_cast<size_t>(a_leer));
            if (n <= 0) {
                completo = false;
                break;
            }
            out.write(buffer.data(), n);
            pendiente -= n;
        }
        AAsset_close(asset);
        out.close();

        if (!completo) {
            LogError("Copia incompleta de " + relativa);
            ++fallos;
            continue;
        }

        ++copiados;
        bytes += tamano;
    }

    LogInfo("Assets copiados: " + std::to_string(copiados) + " ficheros, " +
            std::to_string(bytes / (1024 * 1024)) + " MB, fallos: " + std::to_string(fallos));
    return fallos == 0 && copiados > 0;
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

        if (CopyAssetsFromIndex(manager, files_dir)) {
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

std::string Android_BootstrapStorage() {
    const std::string data_path = Android_InitStorage();

    if (g_save_path.empty())
        return data_path;

    // 1) El log: se reabre ahora que files/save existe.
    const std::string log_path = JoinPath(g_save_path, "Game_Log.txt");
    if (Protokoll.openFile(log_path)) {
        Protokoll << "\n=== Hurrican (Android) ===" << std::endl;
        LogInfo("Log en " + log_path);
    } else {
        LogError("No se pudo abrir " + log_path + "; se registra solo en logcat");
    }

    // 2) Directorio de trabajo = files/. En Android el proceso arranca en "/", que no se
    //    puede escribir, y el motor tiene rutas relativas heredadas de la version de
    //    escritorio (temp.map, el propio log...). Con esto caen dentro de la app.
    const std::string files_dir = GetFilesDir();
    if (!files_dir.empty()) {
        if (::chdir(files_dir.c_str()) == 0)
            LogInfo("Directorio de trabajo: " + files_dir);
        else
            LogError("No se pudo cambiar el directorio de trabajo a " + files_dir);
    }

    return data_path;
}

#endif /* ANDROID */
