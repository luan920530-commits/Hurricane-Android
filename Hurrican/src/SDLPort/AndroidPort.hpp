/**
 * Rutas de almacenamiento y utilidades propias de Android.
 *
 * Hurrican fue escrito para Windows y asume que puede leer y escribir ficheros junto al
 * ejecutable. En Android el sistema de ficheros es de solo lectura dentro del APK y cada
 * aplicacion tiene sus propios directorios privados, asi que este modulo centraliza:
 *
 *   - la copia inicial ("bootstrap") de los datos del juego desde los assets del APK al
 *     almacenamiento interno, que es el unico sitio donde el motor puede leer con rutas
 *     normales y sin pasar por el AssetManager de Java;
 *   - la eleccion de los directorios de datos y de guardado;
 *   - la redireccion del log a logcat (std::cout no existe en Android).
 *
 * (c) 2026 - Port de Android, misma licencia que el resto del codigo (MIT).
 **/

#ifndef _ANDROIDPORT_HPP_
#define _ANDROIDPORT_HPP_

#if defined(ANDROID)

#  include <string>

/**
 * Prepara el almacenamiento del juego y devuelve el directorio que contiene data/ y lang/.
 *
 * Primera ejecucion: extrae los assets a <almacenamiento interno>/data y
 * <almacenamiento interno>/lang. Ejecuciones posteriores: reutiliza la copia existente y
 * solo la refresca si la version empaquetada cambio (vease ANDROID_ASSET_VERSION).
 *
 * @return ruta absoluta del directorio de datos, o cadena vacia si algo fallo.
 */
std::string Android_InitStorage();

/** Directorio donde el juego puede escribir (config, partidas, records, log). */
const std::string &Android_GetSavePath();

/**
 * Punto de entrada de la app en Android.
 *
 * Hace dos cosas antes de que el motor toque ningun fichero:
 *
 *  1. Redirige el log a files/save/Game_Log.txt. El objeto de log global se construye antes
 *     de SDL_main(), cuando el directorio de guardado aun no existe, asi que alli no se
 *     puede abrir: se reabre aqui.
 *  2. Cambia el directorio de trabajo del proceso a files/, para que cualquier ruta
 *     relativa que use el motor ("Game_Log.txt", "temp.map", ficheros de configuracion
 *     heredados) acabe en un sitio escribible en lugar de en "/", que es de solo lectura.
 *
 * @return ruta absoluta del directorio de datos (el mismo que Android_InitStorage()).
 */
std::string Android_BootstrapStorage();

/**
 * Devuelve la version de los datos instalados, leyendo un fichero marcador. Vacio si no
 * existe todavia (primera ejecucion o instalacion incompleta).
 */
std::string Android_GetInstalledDataVersion();

#endif /* ANDROID */

#endif /* _ANDROIDPORT_HPP_ */
