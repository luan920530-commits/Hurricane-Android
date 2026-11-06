#!/usr/bin/env python3
"""
Comprueba que la APK generada es realmente jugable.

Que Gradle termine bien solo significa que la APK se ha empaquetado, no que se pueda
jugar. Los fallos tipicos que se escapan asi son una APK sin los datos del juego (el motor
arranca y se queda en negro, o se cierra al no encontrar las texturas) o sin la libreria
nativa libmain.so. Ambos se detectan aqui, antes de publicar el artefacto.

Uso:
    python3 check_apk.py ruta/app-debug.apk
"""

import sys
import zipfile

# Los datos del juego son ~91 MB y ~600 ficheros; se deja margen por si se recorta el
# arbol de datos en el futuro, pero lo suficiente para detectar una APK vacia.
MIN_FICHEROS_DATOS = 600
MIN_FICHEROS_IDIOMA = 5
MIN_BYTES_ASSETS = 60 * 1024 * 1024


def main():
    if len(sys.argv) < 2:
        print("uso: check_apk.py <apk>", file=sys.stderr)
        return 2

    apk = sys.argv[1]
    try:
        z = zipfile.ZipFile(apk)
    except (OSError, zipfile.BadZipFile) as e:
        print(f"::error title=APK ilegible::No se pudo abrir {apk}: {e}")
        return 1

    nombres = z.namelist()
    assets = [n for n in nombres if n.startswith("assets/hurrican/")]
    datos = [n for n in assets if n.startswith("assets/hurrican/data/")]
    idioma = [n for n in assets if n.startswith("assets/hurrican/lang/")]
    indice = [n for n in assets if n.endswith("hurrican_index.txt")]
    bytes_assets = sum(z.getinfo(n).file_size for n in assets)
    librerias = sorted(n for n in nombres if n.startswith("lib/"))

    print(
        f"Assets: {len(assets)} ficheros ({len(datos)} de datos, {len(idioma)} de idioma), "
        f"{bytes_assets / 1024 / 1024:.1f} MB sin comprimir"
    )
    print(f"Indice de assets: {'presente' if indice else 'FALTA'}")
    print("Librerias nativas:", ", ".join(librerias) or "ninguna")
    print("Ejemplos de datos:", ", ".join(datos[:3]) or "ninguno")

    problemas = []
    if len(datos) < MIN_FICHEROS_DATOS:
        problemas.append(f"solo {len(datos)} ficheros de datos (minimo {MIN_FICHEROS_DATOS})")
    if len(idioma) < MIN_FICHEROS_IDIOMA:
        problemas.append(f"solo {len(idioma)} ficheros de idioma (minimo {MIN_FICHEROS_IDIOMA})")
    if bytes_assets < MIN_BYTES_ASSETS:
        problemas.append(
            f"solo {bytes_assets / 1024 / 1024:.0f} MB de assets "
            f"(minimo {MIN_BYTES_ASSETS // 1024 // 1024} MB)"
        )
    if not indice:
        problemas.append("falta hurrican_index.txt (la copia de datos en el movil no funcionaria)")
    if not any(l.endswith("/libmain.so") for l in librerias):
        problemas.append("falta lib/libmain.so (el juego no arrancaria)")

    if problemas:
        print("::error title=APK incompleta::" + "; ".join(problemas))
        return 1

    print("APK correcta: lleva los datos del juego y las librerias nativas.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
