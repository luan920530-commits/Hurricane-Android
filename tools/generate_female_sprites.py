#!/usr/bin/env python3
"""Draw Hurrican's female player as smooth, anime-tactical SVG sprite sheets.

The artwork is authored from vector paths (not traced from the old pixel sprites)
and rasterized with 4x supersampling to the exact cell layouts expected by the
current game. The visual target is a detailed anime commando: long hair, olive
camouflage, plate carrier, gloves, kneepads, boots, and a rifle.

Requirements: Pillow and Node.js with @resvg/resvg-js installed in tools/.
"""
from __future__ import annotations

import math
import os
from pathlib import Path
import subprocess
import tempfile
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
TEXTURES = ROOT / "Hurrican" / "data" / "textures"
TOOLS = Path(__file__).resolve().parent
WORK = ROOT / ".work-sprites"
SUPERSAMPLE = 4

# frame width, frame height, columns, rows, motion family
SHEETS = {
    "idle": (70, 80, 5, 4, "idle"),
    "kucken": (70, 80, 2, 1, "look"),
    "idleumkuck": (70, 80, 14, 4, "look"),
    "laufen": (70, 80, 5, 4, "run"),
    "shootdiagonal": (70, 80, 2, 1, "diagonal"),
    "shootup": (70, 80, 2, 1, "up"),
    "crouch": (70, 80, 2, 1, "crouch"),
    "jump": (70, 80, 4, 3, "jump"),
    "jumpschraeg": (70, 80, 4, 3, "jump_diagonal"),
    "jumpup": (70, 80, 4, 3, "jump_up"),
    "rundumschuss": (70, 80, 7, 6, "spin"),
    "pissen": (70, 80, 7, 3, "idle"),
    "ride": (90, 120, 5, 4, "ride"),
    "rad": (35, 35, 4, 2, "wheel"),
}

DEFS = '''
<defs>
 <linearGradient id="skin" x1="0" y1="0" x2="0.8" y2="1"><stop stop-color="#ffe2cc"/><stop offset="1" stop-color="#c98d76"/></linearGradient>
 <linearGradient id="hair" x1="0" y1="0" x2="0.9" y2="1"><stop stop-color="#26303b"/><stop offset="0.45" stop-color="#151c26"/><stop offset="1" stop-color="#603343"/></linearGradient>
 <linearGradient id="vest" x1="0" y1="0" x2="0.9" y2="1"><stop stop-color="#c9b28a"/><stop offset="0.55" stop-color="#8b795c"/><stop offset="1" stop-color="#554c3c"/></linearGradient>
 <linearGradient id="rifle" x1="0" y1="0" x2="0" y2="1"><stop stop-color="#53565a"/><stop offset="0.42" stop-color="#24282d"/><stop offset="1" stop-color="#101318"/></linearGradient>
 <linearGradient id="boot" x1="0" y1="0" x2="0.7" y2="1"><stop stop-color="#777360"/><stop offset="1" stop-color="#373a35"/></linearGradient>
 <pattern id="camo" width="11" height="9" patternUnits="userSpaceOnUse">
  <rect width="11" height="9" fill="#48523d"/>
  <path d="M0 1L4 0L6 2L4 4L0 4ZM7 5L11 3L11 7L8 9L6 7Z" fill="#29372e"/>
  <path d="M4 4L7 3L9 5L7 6L5 8L2 7Z" fill="#6b7050"/>
  <path d="M0 8L2 7L4 9H0Z" fill="#89906a" opacity=".75"/>
 </pattern>
 <pattern id="webbing" width="5" height="5" patternUnits="userSpaceOnUse"><rect width="5" height="5" fill="#978563"/><path d="M0 1H5M0 4H5" stroke="#574e3f" stroke-width=".55"/></pattern>
</defs>
'''
OUTLINE = "#181c20"


def path(d: str, fill: str, stroke: str = OUTLINE, width: float = 0.7, extra: str = "") -> str:
    stroke_attr = "none" if stroke == "none" else f'stroke="{stroke}" stroke-width="{width}" stroke-linejoin="round" stroke-linecap="round"'
    return f'<path d="{d}" fill="{fill}" {stroke_attr} {extra}/>'


def line(d: str, stroke: str, width: float = 0.55, extra: str = "") -> str:
    return f'<path d="{d}" fill="none" stroke="{stroke}" stroke-width="{width}" stroke-linejoin="round" stroke-linecap="round" {extra}/>'


def limb(points: tuple[tuple[float, float], ...], width: float, fill: str = "url(#camo)", seam: str = "#b9b291") -> str:
    d = f'M {points[0][0]:.2f},{points[0][1]:.2f} ' + " ".join(f'L {x:.2f},{y:.2f}' for x, y in points[1:])
    return (f'<path d="{d}" fill="none" stroke="{OUTLINE}" stroke-width="{width+1.2:.2f}" stroke-linecap="round" stroke-linejoin="round"/>'
            f'<path d="{d}" fill="none" stroke="{fill}" stroke-width="{width:.2f}" stroke-linecap="round" stroke-linejoin="round"/>'
            f'<path d="{d}" fill="none" stroke="{seam}" stroke-width="0.32" stroke-linecap="round" opacity=".7"/>')


def leg_art(back: bool, phase: float, pose: str, shift_y: float) -> str:
    hip_x = 29.4 if back else 34.6
    hip_y = 48.2 + shift_y
    if pose in ("run", "spin"):
        stride = math.sin(phase)
        knee_x = hip_x + (-7.5 if back else 8.0) * stride
        knee_y = 59.1 + (2.0 if math.cos(phase) < 0 else 0) + shift_y
        ankle_x = knee_x + (-3.5 if back else -4.0) * math.cos(phase)
        ankle_y = 69.2 + (1.6 if math.sin(phase) > 0 else -1.0) + shift_y
    elif pose in ("jump", "jump_diagonal", "jump_up"):
        knee_x = hip_x + (-7.2 if back else 6.8) + math.sin(phase) * 1.1
        knee_y = 57.2 + shift_y
        ankle_x = knee_x + (-3.0 if back else 5.0)
        ankle_y = 63.7 + math.cos(phase) * 1.7 + shift_y
    elif pose == "crouch":
        hip_y += 3.5
        knee_x = hip_x + (-7.0 if back else 8.5)
        knee_y = 63.0 + shift_y
        ankle_x = knee_x + (7.0 if back else -6.5)
        ankle_y = 69.2 + shift_y
    elif pose == "ride":
        knee_x = hip_x + (2.5 if back else 7.0)
        knee_y = 56.0 + shift_y
        ankle_x = knee_x + (4.0 if back else -3.0)
        ankle_y = 60.0 + shift_y
    else:
        knee_x = hip_x + (-1.5 if back else 2.2) + math.sin(phase) * 0.35
        knee_y = 59.5 + shift_y
        ankle_x = hip_x + (-2.2 if back else 2.6)
        ankle_y = 70.0 + shift_y

    points = ((hip_x, hip_y), (knee_x, knee_y), (ankle_x, ankle_y))
    boot_dir = -1.0 if pose in ("run", "jump", "jump_diagonal", "jump_up") and (back ^ (math.sin(phase) > 0)) else 1.0
    toe_x = ankle_x + 5.2 * boot_dir
    toe_y = ankle_y + (1.4 if pose not in ("jump", "jump_diagonal", "jump_up") else 1.0)
    result = limb(points, 5.5 if back else 6.1)
    result += f'<ellipse cx="{knee_x:.2f}" cy="{knee_y:.2f}" rx="2.75" ry="3.25" fill="#373b3e" stroke="{OUTLINE}" stroke-width=".75"/>'
    result += f'<ellipse cx="{knee_x:.2f}" cy="{knee_y-0.25:.2f}" rx="1.95" ry="2.35" fill="#565c5b" stroke="#252a2b" stroke-width=".45"/>'
    result += f'<path d="M {ankle_x-2.1:.2f} {ankle_y-1.9:.2f} Q {ankle_x:.2f} {ankle_y-3.0:.2f} {ankle_x+2.0:.2f} {ankle_y-1.2:.2f} L {toe_x+2.5:.2f} {toe_y+0.2:.2f} Q {toe_x+3.7:.2f} {toe_y+2.2:.2f} {toe_x+1.4:.2f} {toe_y+2.7:.2f} L {toe_x-4.0:.2f} {toe_y+1.8:.2f} Z" fill="url(#boot)" stroke="{OUTLINE}" stroke-width=".8" stroke-linejoin="round"/>'
    result += line(f'M {ankle_x-1.6:.2f} {ankle_y+0.3:.2f} L {toe_x+1.6:.2f} {toe_y+1.5:.2f}', "#afa687", .42)
    result += line(f'M {toe_x-1.8:.2f} {toe_y+2.2:.2f} L {toe_x+2.2:.2f} {toe_y+2.4:.2f}', "#282b2a", .55)
    return result


def rifle(angle: float) -> str:
    return f'''<g transform="rotate({angle:.2f} 43 27)">
      <path d="M 32.5 23.5 L 38 22 L 42.2 23.2 L 42.2 27.2 L 37.5 28.4 L 35.1 27.2 L 31 27.5 L 29.5 26 L 30 24.2 Z" fill="url(#rifle)" stroke="#0c1014" stroke-width=".75" stroke-linejoin="round"/>
      <path d="M 38 22.5 L 49.5 22.4 L 50.5 24.2 L 59.4 24.3 L 60.2 25.6 L 49 26.1 L 46.4 27.4 L 39.1 27.2 Z" fill="url(#rifle)" stroke="#0b0e12" stroke-width=".68" stroke-linejoin="round"/>
      <path d="M 43 22.7 L 47.4 22.7 L 47.1 25.3 L 42.9 25.5 Z" fill="#43484d" stroke="#101318" stroke-width=".45"/>
      <path d="M 44 25.6 L 46 25.5 L 45.2 30.4 L 42.7 30.7 L 42 29 Z" fill="#1e2226" stroke="#0b0e12" stroke-width=".5"/>
      <path d="M 48.3 24.7 L 52.3 24.7 L 52.1 25.7 L 48.1 25.7 Z M 53.1 24.7 L 56.4 24.8 L 56.3 25.5 L 53 25.4 Z" fill="#72777b" stroke="none"/>
      <path d="M 59.2 24.4 L 67.2 24.6 L 68.8 25.2 L 68.8 26 L 59.3 26 Z" fill="#272b30" stroke="#090c10" stroke-width=".55"/>
      <path d="M 66.6 24.5 L 68.9 24.8 L 68.9 26 L 66.5 26.1 Z" fill="#555b5e" stroke="#0c1014" stroke-width=".4"/>
      <path d="M 38.8 22.2 L 40 20.7 L 44 20.7 L 44.6 22.3 Z" fill="#22272c" stroke="#0b0e12" stroke-width=".5"/>
      <path d="M 35.5 27.3 L 37.8 27.3 L 38.1 29.2 L 36.5 29.8 L 35 28.6 Z" fill="#24292d" stroke="#0b0e12" stroke-width=".45"/>
      <circle cx="49.1" cy="23.3" r=".42" fill="#d2bd8e"/>
    </g>'''


def character(pose: str, phase: float, frame_w: int, frame_h: int) -> str:
    bob = math.sin(phase * 2) * .45 if pose in ("idle", "look") else 0
    if pose == "run":
        bob = -abs(math.sin(phase * 2)) * 1.15
    if pose in ("jump", "jump_diagonal", "jump_up"):
        bob = -1.5 + math.sin(phase) * .65
    if pose == "crouch":
        bob = 2.0
    if pose == "ride":
        bob = math.sin(phase * 2) * .5
    torso_y = (3.2 if pose == "crouch" else 0) + bob
    lean = 2.0 if pose == "run" else 0.0
    if pose == "jump_diagonal":
        lean = 1.0
    head_y = 12.2 + torso_y - lean * .25
    hair_sway = math.sin(phase) * (2.5 if pose in ("run", "jump", "jump_diagonal", "jump_up") else .8)
    aim = {"diagonal": -29, "up": -67, "jump_diagonal": -27, "jump_up": -53}.get(pose, 0)
    if pose == "spin":
        aim = math.sin(phase * 1.5) * 18
    if pose == "ride":
        aim = -4

    parts = []
    # Pack/glider silhouette for the existing flying-ride animation.
    if pose == "ride":
        parts += [
            '<path d="M 18 26 L 25 22 L 28 43 L 22 48 L 17 43 Z" fill="#343c40" stroke="#15191d" stroke-width="1"/>',
            '<path d="M 17 29 L 21 28 L 22 40 L 18 41 Z" fill="#8c7958" stroke="#252722" stroke-width=".6"/>',
            '<path d="M 18 47 Q 33 43 47 48 L 51 56 Q 35 62 19 57 Z" fill="#3f4a46" stroke="#161b1c" stroke-width=".9"/>',
            '<path d="M 22 50 L 44 49 L 43 54 L 23 55 Z" fill="url(#vest)" stroke="#211f1b" stroke-width=".55"/>'
        ]

    # Long tied-back hair behind the body, shaped like an anime ponytail.
    parts.append(f'<path d="M 27 {head_y+1:.2f} C 20 {head_y+2+hair_sway:.2f} 17 {head_y+9+hair_sway:.2f} 18 {head_y+17+hair_sway:.2f} C 19 {head_y+25+hair_sway:.2f} 24 {head_y+30+hair_sway:.2f} 26 {head_y+33+hair_sway:.2f} C 29 {head_y+28+hair_sway:.2f} 28 {head_y+23:.2f} 25 {head_y+19:.2f} C 23 {head_y+14:.2f} 27 {head_y+8:.2f} 32 {head_y+5:.2f} Z" fill="url(#hair)" stroke="#11171e" stroke-width=".85" stroke-linejoin="round"/>')
    parts.append(line(f'M 23 {head_y+6:.2f} C 18 {head_y+14+hair_sway:.2f} 20 {head_y+24+hair_sway:.2f} 25 {head_y+30+hair_sway:.2f}', "#87909a", .5, 'opacity=".68"'))
    parts.append(line(f'M 27 {head_y+7:.2f} C 23 {head_y+15+hair_sway:.2f} 25 {head_y+23+hair_sway:.2f} 27 {head_y+27+hair_sway:.2f}', "#923e58", .55))

    # Rear leg, fitted camo trousers, knee pad, and boot.
    parts.append(leg_art(True, phase + math.pi, pose, bob + torso_y * .15))

    # Neck and shirt body. Narrow waist under the plate carrier gives the uniform a fitted silhouette.
    parts.append(f'<path d="M 25 {21+torso_y:.2f} Q 29 {19+torso_y:.2f} 33 {21+torso_y:.2f} L 38 {27+torso_y:.2f} L 36 {38+torso_y:.2f} Q 33 {42+torso_y:.2f} 37 {47+torso_y:.2f} L 34 {51+torso_y:.2f} L 24 {49+torso_y:.2f} Q 27 {44+torso_y:.2f} 25 {40+torso_y:.2f} L 22 {28+torso_y:.2f} Z" fill="url(#camo)" stroke="{OUTLINE}" stroke-width=".95" stroke-linejoin="round"/>')
    # Collar and plate carrier, with layered tan pouches and webbing.
    parts.append(f'<path d="M 25 {22+torso_y:.2f} L 29 {20+torso_y:.2f} L 33 {22+torso_y:.2f} L 37 {27+torso_y:.2f} L 36 {40+torso_y:.2f} Q 31 {43+torso_y:.2f} 25 {40+torso_y:.2f} L 23 {27+torso_y:.2f} Z" fill="url(#vest)" stroke="#292a25" stroke-width=".85" stroke-linejoin="round"/>')
    parts.append(f'<path d="M 26 {23+torso_y:.2f} L 29 {22+torso_y:.2f} L 29 {40+torso_y:.2f} L 26 {40+torso_y:.2f} Z M 33 {22+torso_y:.2f} L 35 {24+torso_y:.2f} L 35 {40+torso_y:.2f} L 32 {40+torso_y:.2f} Z" fill="url(#webbing)" stroke="#524a3b" stroke-width=".45"/>')
    parts.append(f'<path d="M 28 {24+torso_y:.2f} Q 31 {22+torso_y:.2f} 34 {25+torso_y:.2f} L 34 {36+torso_y:.2f} Q 31 {38+torso_y:.2f} 28 {36+torso_y:.2f} Z" fill="#69715b" stroke="#33382f" stroke-width=".6"/>')
    # Four utility pouches with seams and flap buttons.
    for px, py, pw in ((25.5, 33.0, 3.4), (29.2, 34.1, 3.6), (32.7, 33.0, 3.3), (27.1, 37.0, 3.7), (31.0, 37.1, 3.8)):
        py += torso_y
        parts.append(f'<path d="M {px:.2f} {py:.2f} Q {px+pw/2:.2f} {py-.45:.2f} {px+pw:.2f} {py+.2:.2f} L {px+pw-.15:.2f} {py+3.0:.2f} Q {px+pw/2:.2f} {py+3.5:.2f} {px+.1:.2f} {py+3.0:.2f} Z" fill="url(#vest)" stroke="#38382e" stroke-width=".48"/>')
        parts.append(f'<path d="M {px+.35:.2f} {py+.7:.2f} L {px+pw-.35:.2f} {py+.7:.2f}" stroke="#d4c49b" stroke-width=".34"/>')
        parts.append(f'<circle cx="{px+pw/2:.2f}" cy="{py+1.15:.2f}" r=".23" fill="#242722"/>')
    parts.append(f'<path d="M 24 {43+torso_y:.2f} Q 31 {45+torso_y:.2f} 37 {42+torso_y:.2f} L 38 {45+torso_y:.2f} Q 32 {48+torso_y:.2f} 24 {46+torso_y:.2f} Z" fill="#282f2e" stroke="#11181b" stroke-width=".65"/>')
    parts.append(f'<path d="M 26 {44+torso_y:.2f} Q 31 {46+torso_y:.2f} 36 {44+torso_y:.2f}" fill="none" stroke="#c7b891" stroke-width=".48"/>')
    parts.append(f'<rect x="30.3" y="{44.4+torso_y:.2f}" width="2.5" height="1.8" rx=".25" fill="#b7a57e" stroke="#34362f" stroke-width=".42"/>')
    # Side holster and radio on the belt.
    parts.append(f'<path d="M 36 {42+torso_y:.2f} L 39 {43+torso_y:.2f} L 38 {49+torso_y:.2f} L 35 {48+torso_y:.2f} Z" fill="#4a4d40" stroke="#202522" stroke-width=".6"/>')
    parts.append(f'<path d="M 25 {25+torso_y:.2f} L 23 {27+torso_y:.2f} L 25 {35+torso_y:.2f} L 27 {34+torso_y:.2f} Z" fill="#333d3b" stroke="#1c2222" stroke-width=".55"/>')

    # Camo sleeves and forearms, placed behind the weapon.
    if pose in ("jump", "jump_diagonal", "jump_up"):
        far_arm = ((25.0, 25.0+torso_y), (30.0, 29.3+torso_y), (39.2, 27.7+torso_y))
        near_arm = ((32.4, 24.8+torso_y), (35.2, 28.6+torso_y), (43.3, 27.4+torso_y))
    elif pose == "crouch":
        far_arm = ((24.5, 28.0+torso_y), (30.3, 32.1+torso_y), (39.0, 30.7+torso_y))
        near_arm = ((32.2, 28.0+torso_y), (36.0, 31.0+torso_y), (43.2, 30.1+torso_y))
    else:
        far_arm = ((24.5, 24.6+torso_y), (29.8, 29.0+torso_y), (39.3, 27.0+torso_y))
        near_arm = ((32.0, 24.2+torso_y), (35.8, 28.0+torso_y), (43.0, 27.2+torso_y))
    parts.append(limb(far_arm, 4.15, "url(#camo)", "#8e9973"))
    parts.append(limb(near_arm, 4.35, "url(#camo)", "#b7b28e"))
    parts.append(f'<path d="M 22 {23+torso_y:.2f} Q 25 {21+torso_y:.2f} 28 {24+torso_y:.2f} L 29 {28+torso_y:.2f} Q 26 {31+torso_y:.2f} 23 {29+torso_y:.2f} Z" fill="#5b6547" stroke="#252b25" stroke-width=".72"/>')
    parts.append(f'<path d="M 32 {23+torso_y:.2f} Q 35 {22+torso_y:.2f} 37 {25+torso_y:.2f} L 36 {29+torso_y:.2f} Q 33 {30+torso_y:.2f} 31 {27+torso_y:.2f} Z" fill="#4b5945" stroke="#202721" stroke-width=".7"/>')
    parts.append(f'<path d="M 22.6 {27+torso_y:.2f} Q 25 {29+torso_y:.2f} 28 {27.5+torso_y:.2f} L 29 {29+torso_y:.2f} Q 26 {32+torso_y:.2f} 23 {30+torso_y:.2f} Z" fill="#3f4d40" stroke="#1d2523" stroke-width=".55"/>')
    # Gloves grip fore-end and pistol grip.
    parts.append(f'<path d="M 38.5 {26+torso_y:.2f} Q 40 {25+torso_y:.2f} 41.4 {26.5+torso_y:.2f} L 41.2 {28+torso_y:.2f} Q 39.5 {29+torso_y:.2f} 38.2 {28+torso_y:.2f} Z" fill="#363a39" stroke="#171b1c" stroke-width=".5"/>')
    parts.append(f'<path d="M 42 {26.5+torso_y:.2f} Q 43.5 {25.5+torso_y:.2f} 44.8 {27+torso_y:.2f} L 44.1 {28.8+torso_y:.2f} Q 42.7 {29+torso_y:.2f} 41.6 {28+torso_y:.2f} Z" fill="#353a39" stroke="#151a1d" stroke-width=".48"/>')

    # Rifle silhouette in front, matching the reference's compact assault-carbine profile.
    parts.append(rifle(aim))

    # Foreground leg, long camouflaged trousers and compact tactical boot.
    parts.append(leg_art(False, phase, pose, bob + torso_y * .12))

    # Pack straps and shoulder pads.
    parts.append(f'<path d="M 24 {22+torso_y:.2f} Q 22 {23+torso_y:.2f} 22 {27+torso_y:.2f} L 24 {28+torso_y:.2f} L 27 {23+torso_y:.2f} Z" fill="#534b3d" stroke="#222622" stroke-width=".55"/>')
    parts.append(f'<path d="M 34 {22+torso_y:.2f} L 37 {24+torso_y:.2f} L 36 {28+torso_y:.2f} L 33 {24+torso_y:.2f} Z" fill="#675d49" stroke="#232722" stroke-width=".55"/>')

    # Head, anime eye and long side fringe. Small head-to-body ratio follows the supplied reference.
    face_cx = 27.2 + lean * .35
    face_cy = head_y
    parts.append(f'<path d="M 24 {face_cy-6.0:.2f} Q 26 {face_cy-9.3:.2f} 31 {face_cy-7.0:.2f} Q 34 {face_cy-5.4:.2f} 34 {face_cy-1.4:.2f} L 35.8 {face_cy+.6:.2f} L 34.3 {face_cy+2.2:.2f} Q 33.4 {face_cy+6.1:.2f} 29.7 {face_cy+7.0:.2f} Q 25.3 {face_cy+6.1:.2f} 23.6 {face_cy+2.8:.2f} L 23 {face_cy-2:.2f} Z" fill="url(#skin)" stroke="#171a1d" stroke-width=".78" stroke-linejoin="round"/>')
    # Ear and neck guard.
    parts.append(f'<ellipse cx="23.8" cy="{face_cy+1.1:.2f}" rx="1.35" ry="1.8" fill="url(#skin)" stroke="#232326" stroke-width=".48"/>')
    parts.append(f'<path d="M 26 {face_cy+6:.2f} L 31 {face_cy+6:.2f} L 32 {21+torso_y:.2f} L 26 {21+torso_y:.2f} Z" fill="url(#skin)" stroke="#171a1d" stroke-width=".55"/>')
    # Swept dark hair with separate highlight locks and a long tied ponytail.
    parts.append(f'<path d="M 22.8 {face_cy-1.8:.2f} C 20 {face_cy-7.1:.2f} 23 {face_cy-10.6:.2f} 28 {face_cy-10.0:.2f} C 32 {face_cy-10.0:.2f} 35 {face_cy-7.0:.2f} 35 {face_cy-3.6:.2f} Q 32 {face_cy-5.0:.2f} 30 {face_cy-3.2:.2f} Q 27 {face_cy-1.0:.2f} 24 {face_cy-2.8:.2f} L 24 {face_cy+1.0:.2f} Q 22 {face_cy+0.2:.2f} 22.8 {face_cy-1.8:.2f} Z" fill="url(#hair)" stroke="#0d1218" stroke-width=".72" stroke-linejoin="round"/>')
    parts.append(line(f'M 23 {face_cy-4.1:.2f} Q 27 {face_cy-8.6:.2f} 32 {face_cy-7.4:.2f}', "#73808c", .48, 'opacity=".75"'))
    parts.append(f'<path d="M 32.2 {face_cy-1.3:.2f} Q 33.8 {face_cy-2.2:.2f} 34.4 {face_cy-.4:.2f} Q 33.1 {face_cy+.55:.2f} 32.2 {face_cy-1.3:.2f} Z" fill="#e8dfd2" stroke="#27242a" stroke-width=".36"/>')
    parts.append(f'<ellipse cx="33.25" cy="{face_cy-.35:.2f}" rx=".45" ry=".83" fill="#536a73" stroke="#24252a" stroke-width=".23"/>')
    parts.append(f'<circle cx="33.35" cy="{face_cy-.42:.2f}" r=".2" fill="#11171c"/>')
    parts.append(f'<circle cx="33.12" cy="{face_cy-.72:.2f}" r=".16" fill="#fff"/>')
    parts.append(line(f'M 34.6 {face_cy+1.5:.2f} l 1.1 .15', "#8b5a56", .34))
    parts.append(line(f'M 30.6 {face_cy+3.1:.2f} q 1.1 .75 2.1 0', "#a96763", .34))
    # Tiny radio earpiece and cheek detail.
    parts.append(f'<circle cx="23.7" cy="{face_cy+1.0:.2f}" r=".75" fill="#8c978f" stroke="#1a2023" stroke-width=".38"/>')

    if pose == "spin":
        parts.insert(0, f'<path d="M 8 37 C 12 15 47 11 61 26" fill="none" stroke="#b9d0aa" stroke-width=".65" opacity=".75" stroke-dasharray="2 2" transform="rotate({phase*22:.1f} 35 39)"/>')
        parts.insert(0, f'<path d="M 13 56 C 29 67 52 58 58 44" fill="none" stroke="#d5b886" stroke-width=".55" opacity=".65" stroke-dasharray="2 2"/>')

    if pose == "ride":
        parts.append('<path d="M 20 54 Q 16 62 21 67 L 28 66 L 30 59 M 41 54 Q 47 58 48 65 L 43 67 L 38 59" fill="none" stroke="#303a38" stroke-width="4.2" stroke-linecap="round" stroke-linejoin="round"/>')
        parts.append('<path d="M 19 65 L 28 65 L 29 67 L 18 68 Z M 42 64 L 49 64 L 51 66 L 42 67 Z" fill="url(#boot)" stroke="#171b1b" stroke-width=".58"/>')

    scale = min(frame_w / 70.0, frame_h / 80.0)
    tx = (frame_w - 70 * scale) / 2.0
    ty = (frame_h - 80 * scale) / 2.0
    return f'<g transform="translate({tx:.2f} {ty:.2f}) scale({scale:.4f})">' + "".join(parts) + "</g>"


def wheel(frame_w: int, frame_h: int, phase: float) -> str:
    cx, cy = frame_w / 2, frame_h / 2
    r = min(frame_w, frame_h) * .41
    spokes = []
    for i in range(8):
        angle = phase + i * math.pi / 4
        x1, y1 = cx + math.cos(angle)*r*.38, cy + math.sin(angle)*r*.38
        x2, y2 = cx + math.cos(angle)*r*.88, cy + math.sin(angle)*r*.88
        spokes.append(f'<path d="M {x1:.2f} {y1:.2f} L {x2:.2f} {y2:.2f}" stroke="#b8beaa" stroke-width=".8"/>')
    return (f'<circle cx="{cx:.2f}" cy="{cy:.2f}" r="{r:.2f}" fill="#202629" stroke="#0c1113" stroke-width="1.5"/>'
            f'<circle cx="{cx:.2f}" cy="{cy:.2f}" r="{r*.77:.2f}" fill="none" stroke="#707965" stroke-width="1.3"/>'
            + "".join(spokes) +
            f'<circle cx="{cx:.2f}" cy="{cy:.2f}" r="{r*.29:.2f}" fill="#b9b08f" stroke="#111619" stroke-width=".8"/>')


def make_sheet(name: str, fw: int, fh: int, cols: int, rows: int, pose: str, temp_dir: Path) -> None:
    width, height = fw * cols, fh * rows
    pieces = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">', DEFS]
    total = cols * rows
    for index in range(total):
        row, col = divmod(index, cols)
        phase = (index / max(1, total - 1)) * math.tau
        drawing = wheel(fw, fh, phase) if name == "rad" else character(pose, phase, fw, fh)
        pieces.append(f'<g transform="translate({col*fw} {row*fh})">{drawing}</g>')
    pieces.append('</svg>')

    svg_path = temp_dir / f'{name}.svg'
    high_path = temp_dir / f'{name}-4x.png'
    output_path = TEXTURES / f'p1_female_hurri_{name}.png'
    svg_path.write_text("\n".join(pieces))
    env = os.environ.copy()
    env.setdefault("NODE_PATH", str(TOOLS / "node_modules"))
    subprocess.run([
        "node", str(TOOLS / "render_vector_sheet.js"), str(svg_path), str(high_path), str(SUPERSAMPLE)
    ], cwd=TOOLS, check=True, env=env)
    high = Image.open(high_path).convert("RGBA")
    high = high.convert("RGBa").resize((width, height), Image.Resampling.LANCZOS).convert("RGBA")
    high.save(output_path, optimize=True)
    print(f'{output_path.name}: {output_path.stat().st_size // 1024} KiB, {cols*rows} vector frames')


if __name__ == "__main__":
    WORK.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="female-vector-", dir=WORK) as temp:
        temp_dir = Path(temp)
        for sheet, (fw, fh, cols, rows, pose) in SHEETS.items():
            make_sheet(sheet, fw, fh, cols, rows, pose, temp_dir)
