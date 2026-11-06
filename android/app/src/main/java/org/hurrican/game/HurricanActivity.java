package org.hurrican.game;

import android.util.Log;

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
     * Red de seguridad para que un fallo no cierre la app en silencio.
     *
     * El motor ya avisa de sus propios fallos por logcat y por Game_Log.txt, pero una
     * excepcion no controlada de Java (por ejemplo dentro de SDLActivity al crear la
     * superficie) terminaba el proceso sin dejar rastro. Con esto, el motivo queda en
     * logcat y en el mismo fichero de log del juego.
     */
    @Override
    public void onCreate(android.os.Bundle savedInstanceState) {
        // Se guarda el handler del sistema ANTES de instalar el nuestro, para poder
        // devolverle el control y que la app se cierre como siempre.
        previousHandler = Thread.getDefaultUncaughtExceptionHandler();

        Thread.setDefaultUncaughtExceptionHandler(new Thread.UncaughtExceptionHandler() {
            @Override
            public void uncaughtException(Thread thread, Throwable error) {
                reportCrash("Excepcion de Java: " + Log.getStackTraceString(error));
                if (previousHandler != null) {
                    previousHandler.uncaughtException(thread, error);
                }
            }
        });

        super.onCreate(savedInstanceState);
    }

    private Thread.UncaughtExceptionHandler previousHandler;

    /**
     * Anade el motivo a files/save/Game_Log.txt, que es el fichero que el jugador puede leer
     * con "adb shell run-as org.hurrican.game cat files/save/Game_Log.txt".
     */
    private void reportCrash(String message) {
        Log.e("Hurrican", message);
        try {
            java.io.File dir = new java.io.File(getFilesDir(), "save");
            if (!dir.exists()) {
                //noinspection ResultOfMethodCallIgnored
                dir.mkdirs();
            }
            java.io.File log = new java.io.File(dir, "Game_Log.txt");
            java.io.FileWriter writer = new java.io.FileWriter(log, true);
            writer.write("\n*** FALLO ***\n" + message + "\n");
            writer.close();
        } catch (Exception e) {
            Log.e("Hurrican", "No se pudo escribir el informe de fallo: " + e);
        }
    }

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
