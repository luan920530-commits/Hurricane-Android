#!/usr/bin/env python3
"""
Comprueba que todos los #include "..." del codigo apuntan a ficheros que existen.

Por que existe: al traer el codigo al repositorio, un fichero puede quedarse fuera por una
regla de .gitignore sin que nadie se de cuenta (fue justo lo que paso con
src/loaders/mod.h de libxmp: en local existia y en CI no). El fallo solo aparece al
compilar en otro sitio, y el mensaje de error no dice nada del .gitignore. Con esta
comprobacion el problema se ve en el primer paso, con el nombre exacto del fichero.

Los #include <...> no se revisan: son cabeceras del sistema o de terceros.
"""

import os
import re
import sys

# Zonas del repositorio que se compilan.
RAICES = [
    "Hurrican/src",
    "android/app/src/main/jni/SDL_mixer/external/libxmp",
]

# Directorios donde buscar, ademas del propio directorio del fichero que incluye.
# Copiar solo los directorios REALES del compilador: si se anaden de mas, la comprobacion
# se vuelve ciega (encontraria ficheros que clang no encontraria). glm y SDL no aparecen
# porque sus cabeceras se incluyen con <...>.
# Espejo exacto de los -I que usa la compilacion (ver android/app/src/main/jni/CMakeLists.txt
# y los CMakeLists de SDL_mixer), mas el directorio del propio fichero, que es lo que hace
# el compilador para los #include "...".
EXTRA = [
    "Hurrican/src",
    "Hurrican/src/SDLPort",
    "Hurrican/3rdparty/glm",
    "android/app/src/main/jni/SDL/include",
    "android/app/src/main/jni/SDL_image/include",
    "android/app/src/main/jni/SDL_mixer/include",
    "android/app/src/main/jni/SDL_mixer/external/libxmp/include",
    "android/app/src/main/jni/SDL_mixer/external/libxmp/src",
    "android/app/src/main/jni/SDL_mixer/external/libxmp/src/loaders",
]

SUFIJO = (".c", ".h", ".cpp", ".hpp", ".cc")

# Cabeceras de la libc/libstdc++ que se incluyen con comillas pero las resuelve el
# compilador, no el directorio del fichero.
CABECERAS_DEL_SISTEMA = {
    "math.h", "assert.h", "stdlib.h", "stdio.h", "string.h", "time.h", "errno.h",
    "limits.h", "ctype.h", "unistd.h", "signal.h", "stdint.h", "stddef.h", "fcntl.h",
}

# Plataformas que no se compilan para Android (Raspberry Pi, vtables de pruebas...).
# prowizard/ no se compila: libxmp se configura con LIBXMP_NO_PROWIZARD (dos de sus
# ficheros incluyen un ptktable.h que no existe ni en libxmp 4.6.0).
EXCLUIR = ("SDLPort/eglport", "external/libxmp/test/", "loaders/prowizard/")
PATRON = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.MULTILINE)


def main():
    raiz_repo = sys.argv[1] if len(sys.argv) > 1 else "."
    bases = [os.path.join(raiz_repo, d) for d in EXTRA]
    bases += [os.path.join(raiz_repo, d) for d in RAICES]

    faltan, revisados = [], 0
    for raiz in RAICES:
        for actual, _, ficheros in os.walk(os.path.join(raiz_repo, raiz)):
            for nombre in ficheros:
                if not nombre.endswith(SUFIJO):
                    continue
                ruta = os.path.join(actual, nombre)
                if any(x in ruta.replace(os.sep, "/") for x in EXCLUIR):
                    continue
                try:
                    with open(ruta, "r", encoding="utf-8", errors="replace") as f:
                        contenido = f.read()
                except OSError:
                    continue
                for incluido in PATRON.findall(contenido):
                    revisados += 1
                    nombre = os.path.basename(incluido)
                    # math.h, assert.h y compania existen en bionic pero no en el
                    # directorio del fichero: no son un problema de empaquetado.
                    if nombre in CABECERAS_DEL_SISTEMA:
                        continue
                    candidatos = [os.path.join(actual, incluido)]
                    candidatos += [os.path.join(b, incluido) for b in bases]
                    if not any(os.path.isfile(c) for c in candidatos):
                        faltan.append((ruta, incluido))

    if faltan:
        print(f"::error::{len(faltan)} #include apuntan a ficheros que no estan en el repositorio")
        for ruta, incluido in faltan[:40]:
            print(f"  {ruta}: no se encuentra \"{incluido}\"")
        print(
            "\nSuele ser un fichero que existe en local pero esta excluido por .gitignore.\n"
            "Se arregla con: git add -f <ruta>"
        )
        return 1

    print(f"OK: {revisados} #include revisados, todos resuelven a un fichero del repositorio.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
