#!/usr/bin/env python3
"""
Convierte las texturas PNG de Hurrican al formato ETC1 (.pkm) que la GPU de Android puede
usar de forma nativa, junto con su mapa de transparencia en un segundo .pkm.

Hurrican usa desde hace tiempo este esquema en sus ports de consola (GCW Zero, Pandora,
RDU): cada textura se guarda como

    data/etc1/<nombre>.pkm         -> color RGB comprimido en ETC1
    data/etc1/<nombre>_alpha.pkm   -> canal alfa en escala de grises, tambien en ETC1

y el shader `shader_etc1_texture.frag` combina ambos:

    vec4(texture2D(u_Texture0, uv).rgb, texture2D(u_Texture1, uv).r)

Ventajas en moviles: la textura ocupa 4 bits/pixel en vez de 32 (8x menos memoria de GPU y
de ancho de banda) y no hay que decodificar PNG al arrancar cada nivel.

Requiere:
    - Python 3 con Pillow (pip install Pillow)
    - EtcTool de google/etc2comp compilado (ver tools/build_etctool.sh)

Uso:
    python3 tools/generate_etc1.py [--jobs N] [--force]
"""

import argparse
import os
import subprocess
import struct
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

try:
    from PIL import Image
except ImportError:
    sys.exit("ERROR: falta Pillow. Instala con: pip install Pillow")

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEXTURE_DIR = os.path.join(REPO_ROOT, "Hurrican", "data", "textures")
OUTPUT_DIR = os.path.join(REPO_ROOT, "Hurrican", "data", "etc1")

ETCTOOL = os.environ.get("ETCTOOL", os.path.join(REPO_ROOT, "tools", "bin", "EtcTool"))

PKM_HEADER = struct.Struct(">4sHHHHHH")  # magic, version, type, ext_w, ext_h, w, h


def write_pkm_from_etc1(etc1_bytes, width, height, path):
    """Envuelve un bloque ETC1 crudo en la cabecera PKM 1.0."""
    ext_w = (width + 3) // 4
    ext_h = (height + 3) // 4
    with open(path, "wb") as fh:
        fh.write(PKM_HEADER.pack(b"PKM ", 0x3130, 0, ext_w, ext_h, width, height))
        fh.write(etc1_bytes)


KTX_MAGIC = b"\xabKTX 11\xbb\r\n\x1a\n"
KTX_HEADER_SIZE = 64


def read_raw_etc1(path):
    """Extrae los datos ETC1 crudos de un archivo escrito por EtcTool.

    EtcTool emite KTX 1.1 (64 bytes de cabecera + un uint32 con el tamano de la imagen).
    Aceptamos tambien PKM por si se usa otra herramienta.
    """
    with open(path, "rb") as fh:
        data = fh.read()

    if data.startswith(KTX_MAGIC):
        width, height = struct.unpack("<II", data[36:44])
        image_size = struct.unpack("<I", data[64:68])[0]
        body = data[68:]
        if image_size not in (0, 0xFFFFFFFF):
            body = body[:image_size]
    elif data.startswith(b"PKM "):
        body = data[PKM_HEADER.size:]
        width = struct.unpack(">H", data[12:14])[0]
        height = struct.unpack(">H", data[14:16])[0]
    else:
        raise RuntimeError(f"{path}: formato desconocido (se esperaba KTX o PKM)")

    blocks = ((width + 3) // 4) * ((height + 3) // 4)
    if len(body) < blocks * 8:
        raise RuntimeError(
            f"{path}: datos ETC1 incompletos ({len(body)} bytes para {width}x{height})"
        )
    return body[: blocks * 8], width, height


def run_etctool(source_png, dest_block_file):
    """Comprime un PNG a ETC1 con EtcTool y devuelve la ruta del .pkm intermedio."""
    with tempfile.NamedTemporaryFile(suffix=".pkm", delete=False) as tmp:
        tmp_path = tmp.name
    try:
        cmd = [
            ETCTOOL,
            source_png,
            "-format", "ETC1",
            "-effort", "40",
            "-output", tmp_path,
        ]
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if proc.returncode != 0:
            raise RuntimeError(
                "EtcTool fallo en {}:\n{}".format(source_png, proc.stdout.decode("utf-8", "replace"))
            )
        raw, width, height = read_raw_etc1(tmp_path)
        write_pkm_from_etc1(raw, width, height, dest_block_file)
        return True
    finally:
        if os.path.exists(tmp_path):
            os.unlink(tmp_path)


def convert_texture(png_path, force):
    name = os.path.splitext(os.path.basename(png_path))[0]
    color_out = os.path.join(OUTPUT_DIR, name + ".pkm")
    alpha_out = os.path.join(OUTPUT_DIR, name + "_alpha.pkm")

    if not force and os.path.exists(color_out) and os.path.exists(alpha_out):
        return "omitido", name

    img = Image.open(png_path)
    img.load()

    if img.mode not in ("RGBA", "RGB"):
        img = img.convert("RGBA")
    if img.mode == "RGB":
        img = img.convert("RGBA")

    # Sin transparencia real no hace falta el segundo plano: se guarda un alfa opaco.
    alpha = img.getchannel("A")
    has_alpha = alpha.getextrema()[0] < 255

    with tempfile.TemporaryDirectory() as tmpdir:
        rgb_png = os.path.join(tmpdir, "rgb.png")
        img.convert("RGB").save(rgb_png, "PNG")
        run_etctool(rgb_png, color_out)

        if has_alpha:
            alpha_png = os.path.join(tmpdir, "alpha.png")
            alpha.convert("L").save(alpha_png, "PNG")
            run_etctool(alpha_png, alpha_out)
        else:
            # Alpha totalmente opaca: mismo tamano, ETC1 de un solo bloque gris claro.
            width, height = img.size
            ext_w, ext_h = (width + 3) // 4, (height + 3) // 4
            white_block = bytes.fromhex("ffffffffffffffff")
            write_pkm_from_etc1(white_block * (ext_w * ext_h), width, height, alpha_out)

    return "ok", name


def main():
    parser = argparse.ArgumentParser(description="Genera texturas ETC1 para Hurrican Android")
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 2)
    parser.add_argument("--force", action="store_true", help="regenerar aunque ya existan")
    parser.add_argument("--limit", type=int, default=0, help="solo procesar N texturas (pruebas)")
    args = parser.parse_args()

    if not os.path.exists(ETCTOOL):
        sys.exit(
            "ERROR: no se encuentra EtcTool en '{}'.\n"
            "Compilalo con tools/build_etctool.sh o define la variable ETCTOOL.".format(ETCTOOL)
        )
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    pngs = sorted(
        os.path.join(TEXTURE_DIR, f)
        for f in os.listdir(TEXTURE_DIR)
        if f.lower().endswith(".png")
    )
    if args.limit:
        pngs = pngs[: args.limit]

    print(f"Convirtiendo {len(pngs)} texturas a ETC1 en {OUTPUT_DIR}")
    ok = skipped = failed = 0
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for status, name in pool.map(lambda p: convert_texture(p, args.force), pngs):
            if status == "ok":
                ok += 1
                if ok % 25 == 0:
                    print(f"  ... {ok} convertidas")
            elif status == "omitido":
                skipped += 1
            else:
                failed += 1
                print(f"  FALLO: {name}")

    print(f"Listo: {ok} convertidas, {skipped} ya existentes, {failed} fallidas")


if __name__ == "__main__":
    main()
