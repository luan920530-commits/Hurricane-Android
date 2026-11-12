#!/usr/bin/env python3
# ---------------------------------------------------------------------------------------
# Comprueba los shaders del juego sin necesitar una GPU ni el NDK.
#
# El motor inyecta constantes en el codigo GLSL antes de compilar (vease
# CShader::CompileShader): todas las que empiezan por `c_` son `const int` que se anaden al
# principio del fichero. Si un shader usa una `c_...` que el codigo C++ no le pasa, el shader
# no compila y el juego se queda sin imagen o sin escalado: es un fallo que solo se ve en el
# movil, asi que se comprueba aqui.
#
# Ademas hace comprobaciones basicas de sintaxis GLSL (parentesis y llaves equilibrados,
# que cada `varying`/`attribute` usado este declarado, que el fichero tenga main()).
#
# Uso: check_shaders.py <raiz del repositorio>
# ---------------------------------------------------------------------------------------

import re
import sys
from pathlib import Path

# Constantes que inyecta DX8Graphics.cpp (SetDeviceInfo / SetUpscale).
CONSTANTES_CONOCIDAS = {
    "c_WindowWidth",
    "c_WindowHeight",
    "c_curvature",
    "c_color_bleed",
    "c_scanlines",
    "c_noise",
    "c_upscale",
    "c_TexWidth",
    "c_TexHeight",
}

errores = []
avisos = []


def comprobar_fichero(path):
    texto = path.read_text(encoding="utf-8", errors="replace")

    # 1) Constantes inyectadas: las que se usan deben existir en el codigo C++.
    usadas = set(re.findall(r"\b(c_[A-Za-z_][A-Za-z0-9_]*)\b", texto))
    for nombre in sorted(usadas):
        if nombre not in CONSTANTES_CONOCIDAS:
            errores.append(f"{path}: usa {nombre}, que el motor no inyecta (revisa DX8Graphics.cpp)")

    # 2) main() presente.
    if not re.search(r"\bvoid\s+main\s*\(", texto):
        errores.append(f"{path}: no tiene main()")

    # 3) Equilibrio de llaves y parentesis (ignorando comentarios).
    limpio = re.sub(r"/\*.*?\*/", "", texto, flags=re.S)
    limpio = re.sub(r"//[^\n]*", "", limpio)
    if limpio.count("{") != limpio.count("}"):
        errores.append(f"{path}: llaves descompensadas ({limpio.count('{')} / {limpio.count('}')})")
    if limpio.count("(") != limpio.count(")"):
        errores.append(f"{path}: parentesis descompensados")

    # 4) Variables GLSL usadas sin declarar (solo las que interesan: varyings y atributos).
    declaradas = set(re.findall(r"\b(?:varying|attribute|in|out|uniform)\s+\w+\s+(\w+)\s*;", limpio))
    for nombre in re.findall(r"\bv_[A-Za-z0-9_]*\b|\ba_[A-Za-z0-9_]*\b", limpio):
        if nombre not in declaradas and not re.search(r"\b\w+\s+" + nombre + r"\s*[;,=)]", limpio):
            avisos.append(f"{path}: {nombre} se usa pero no aparece declarada")

    # 5) En GLSL 100 no puede aparecer `texture(` ni `in`/`out` en fragment... salvo que el
    #    fichero sea de la carpeta 320.
    if "shaders/100" in str(path).replace("\\", "/"):
        if re.search(r"\btexture\s*\(", limpio):
            errores.append(f"{path}: usa texture() en GLSL 100 (debe ser texture2D())")
        if re.search(r"^\s*out\s+vec4", limpio, flags=re.M):
            errores.append(f"{path}: declara `out` en GLSL 100 (debe ser gl_FragColor)")
    if "shaders/320" in str(path).replace("\\", "/"):
        if "gl_FragColor" in limpio:
            errores.append(f"{path}: usa gl_FragColor en GLSL 320")


def main():
    if len(sys.argv) < 2:
        print("uso: check_shaders.py <raiz del repositorio>")
        return 2

    raiz = Path(sys.argv[1])
    ficheros = sorted(raiz.glob("Hurrican/data/shaders/**/*.vert")) + sorted(
        raiz.glob("Hurrican/data/shaders/**/*.frag")
    )

    if not ficheros:
        print("No se han encontrado shaders: revisa la ruta.")
        return 1

    for f in ficheros:
        comprobar_fichero(f)

    print(f"Shaders comprobados: {len(ficheros)}")

    # Los avisos no rompen la compilacion; los errores si.
    for a in avisos:
        print(f"AVISO: {a}")
    for e in errores:
        print(f"::error::{e}")

    if errores:
        print(f"Fallos: {len(errores)}")
        return 1

    print("Sin errores.")
    # Aviso visible en el resumen del workflow (no falla la ejecucion).
    for a in avisos:
        print(f"::warning::{a}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
