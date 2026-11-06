package org.hurrican.game;

import org.libsdl.app.SDLActivity;

/**
 * Actividad principal de Hurrican para Android.
 *
 * Hereda de SDLActivity, que es quien crea la superficie OpenGL, carga las librerias nativas
 * y lleva todo el ciclo de vida (pausa, reanudacion, mandos, audio). Lo unico que se ajusta
 * aqui es la lista de librerias a cargar.
 *
 * La orientacion horizontal, el tema a pantalla completa y los permisos estan en
 * AndroidManifest.xml.
 */
public class HurricanActivity extends SDLActivity {

    /**
     * Librerias nativas, en orden de carga.
     *
     * libmain.so es el juego y ya depende de libSDL2 / libSDL2_image (se enlazan al
     * compilar), pero se declaran explicitamente: si alguna ABI no se genero bien, el error
     * de dlopen senala exactamente cual falta en vez de fallar al cargar "main".
     *
     * Importante: la ultima entrada de esta lista es la que SDLActivity usa como objeto
     * principal, del que busca el simbolo SDL_main. "main" debe quedarse al final.
     */
    @Override
    protected String[] getLibraries() {
        return new String[]{
                "SDL2",
                "SDL2_image",
                "main"
        };
    }

    /*
     * No se sobreescriben getMainSharedObject() ni getMainFunction(): los valores por
     * defecto de SDLActivity ya son los correctos para nosotros.
     *
     * getMainSharedObject() devuelve la RUTA COMPLETA del .so (usando
     * nativeLibraryDir + la ultima entrada de getLibraries(), o sea libmain.so). Devolver
     * solo "libmain.so" haria que dlopen() no lo encontrase, porque el directorio de
     * librerias nativas del APK no esta en la ruta de busqueda por defecto.
     * getMainFunction() devuelve ya "SDL_main", que es el simbolo que exporta Main.cpp.
     */
}
