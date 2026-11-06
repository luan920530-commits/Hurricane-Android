#!/usr/bin/env python3
"""
Publica el contexto de un fallo de compilacion como anotaciones de GitHub Actions.

Por que existe este fichero: el log completo de una ejecucion vive en un almacenamiento
que no siempre responde, y muchas veces lo unico que se puede consultar por API son las
anotaciones del paso. Este script recorta el log de Gradle alrededor de los mensajes
interesantes (errores del compilador, del enlazador, el bloque "What went wrong"...) y los
imprime como anotaciones de error, troceadas para respetar los limites de GitHub:
10 anotaciones por paso y unos 64 KB de texto por anotacion.

Uso:
    python3 annotate_log.py /ruta/al/gradle.log [--title "Gradle"]

Si no hay ningun mensaje reconocible, publica el final del log, que es donde Gradle
escribe el resumen del fallo.
"""

import re
import sys

# Limites de GitHub: 10 anotaciones por paso, y conviene quedarse bastante por debajo de
# los 64 KB por anotacion (el texto va escapado, que lo puede triplicar).
TROZOS_MAX = 10
TAMANO_TROZO = 18000

# Lineas que merecen la pena ver, y cuantas lineas de contexto alrededor de cada una.
PATRON = re.compile(
    r"error:|Error:|FAILURE:|What went wrong|ninja: |undefined reference"
    r"|No such file|fatal error|FAILED|CMake Error|cannot find|not found"
)
CONTEXTO = 6


def recortar(lineas):
    """Devuelve las lineas de interes con su contexto, sin repetir ninguna."""
    interes = [i for i, l in enumerate(lineas) if PATRON.search(l)]
    if not interes:
        # Sin pistas claras: el final del log es el resumen del fallo.
        return lineas[-150:]

    vistas, seleccion = set(), []
    for i in interes:
        for j in range(max(0, i - CONTEXTO), min(len(lineas), i + CONTEXTO + 1)):
            if j not in vistas:
                vistas.add(j)
                seleccion.append(j)
    return [lineas[j] for j in sorted(seleccion)]


def escapar(texto):
    """Escapa el texto para el formato de comandos de GitHub (workflow commands)."""
    return texto.replace("%", "%25").replace("\r", "").replace("\n", "%0A")


def main():
    if len(sys.argv) < 2:
        print("uso: annotate_log.py <log> [--title TITULO]", file=sys.stderr)
        return 2

    ruta = sys.argv[1]
    titulo = "Gradle"
    if "--title" in sys.argv:
        titulo = sys.argv[sys.argv.index("--title") + 1]

    try:
        with open(ruta, "r", encoding="utf-8", errors="replace") as f:
            lineas = [l.rstrip() for l in f]
    except OSError as e:
        print(f"::error::No se pudo leer el log ({ruta}): {e}")
        return 1

    seleccion = recortar(lineas)

    # Se trocea por lineas (no por caracteres) para no partir un mensaje del compilador a
    # la mitad si se puede evitar.
    trozos, actual = [], ""
    for linea in seleccion:
        if len(actual) + len(linea) + 1 > TAMANO_TROZO and actual:
            trozos.append(actual)
            actual = ""
        actual += linea + "\n"
    if actual:
        trozos.append(actual)

    # Si se ha recortado por el limite de anotaciones, se avisa en vez de callarlo.
    recortado = len(trozos) > TROZOS_MAX
    trozos = trozos[:TROZOS_MAX]

    for n, trozo in enumerate(trozos, 1):
        print(f"::error title={titulo} {n}/{len(trozos)}::{escapar(trozo)}")
    if recortado:
        print(f"::warning::El contexto era mayor que {TROZOS_MAX} anotaciones; se ha recortado.")

    # Resumen en el log normal, que ayuda cuando las anotaciones no bastan.
    print(f"({len(seleccion)} lineas de contexto publicadas en {len(trozos)} anotaciones)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
