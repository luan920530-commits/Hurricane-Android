// Hurrican vector-operator concept draft.
// Authored with Vex (https://github.com/uilianks/vex, MIT); source artwork here is original.
// Canvas per frame: 70x80 px, matching Hurrican's player cell size.

pal({
  ink: "#18251e", ink2: "#26372d", skin: "#dcae8b", skinHi: "#f0c5a1",
  hair: "#30251f", hairHi: "#594235", helmet: "#465842", helmetHi: "#75805d",
  cloth: "#485c43", clothHi: "#72805a", clothDark: "#2b3c31", vest: "#33483b",
  vestHi: "#596b4d", web: "#859066", metal: "#9ca99b", metalHi: "#d4d8c8",
  black: "#202923", boot: "#342f26", accent: "#bd7b4e", muzzle: "#f6c967",
})
const OL = { outline: "ink", ow: 0.8 }
const limb = (a, b, width, color) => line([a[0], a[1], b[0], b[1]], { stroke: color, sw: width, cap: "round" })

function buildOperator({ female = false } = {}) {
  const hair = female ? "hair" : "ink2"
  const upper = female ? "clothHi" : "cloth"
  const accessory = female ? "hairHi" : "helmetHi"
  const body = rig({
    pelvis: { pivot: [35, 49], z: 4, items: [
      path("M29 44H41L42 51H28Z", { fill: "clothDark" }),
      rect(31, 46, 9, 3.5, { fill: "vest", r: 1 }),
      rect(34, 47, 2, 2, { fill: "web", r: 0.5 }),
    ] },
    legRear: { parent: "pelvis", pivot: [35, 49], z: 1, items: [
      path("M30 49L35 49L34 61L29 63L28 59Z", { fill: "clothDark" }),
      line([30, 58, 34, 58], { stroke: "web", sw: 1 }),
    ] },
    bootRear: { parent: "legRear", pivot: [32, 61], z: 1, items: [
      path("M29 60L34 60L34 68L39 71L39 74L27 74L28 69Z", { fill: "boot" }),
      line([28, 72, 38, 72], { stroke: "ink2", sw: 0.8 }),
    ] },
    legFront: { parent: "pelvis", pivot: [35, 49], z: 5, items: [
      path("M35 49L41 49L40 61L35 63L33 59Z", { fill: "cloth" }),
      path("M34 52L40 51L39 55L34 56Z", { fill: "clothHi" }),
      line([34, 59, 40, 58], { stroke: "web", sw: 1 }),
    ] },
    bootFront: { parent: "legFront", pivot: [38, 61], z: 5, items: [
      path("M35 60L40 60L40 68L46 71L46 74L34 74L34 68Z", { fill: "boot" }),
      line([35, 72, 45, 72], { stroke: "ink2", sw: 0.8 }),
      circle(44, 70, 0.7, "web"),
    ] },
    torso: { parent: "pelvis", pivot: [35, 49], z: 3, items: [
      path("M30 29L40 29L44 35L42 49L28 49L27 36Z", { fill: upper }),
      path("M30 31L35 34L33 47L29 47L28 37Z", { fill: "clothDark" }),
      // clean tactical vest silhouette and two readable pouches
      path("M32 31L39 31L42 36L41 47L30 47L30 36Z", { fill: "vest" }),
      path("M34 32L37 32L37 45L33 45Z", { fill: "vestHi" }),
      rect(29.5, 37, 4, 5, { fill: "clothHi", r: 0.7 }),
      rect(38, 37, 4, 5, { fill: "clothHi", r: 0.7 }),
      line([31, 38, 32.5, 38], { stroke: "web", sw: 0.8 }),
      line([39.5, 38, 41, 38], { stroke: "web", sw: 0.8 }),
      rect(33.8, 34, 3.8, 1.5, { fill: "web", r: 0.5 }),
      circle(35, 30.5, 1.3, "accent"),
      // shoulder pad, sleeve seam and a compact radio
      path("M39 30L43 32L44 36L40 37L38 34Z", { fill: "clothHi" }),
      rect(40.5, 32, 2.2, 2.8, { fill: "ink2", r: 0.4 }),
      line([41.5, 31, 42.5, 29], { stroke: "web", sw: 0.6 }),
    ] },
    armBack: { parent: "torso", pivot: [39, 33], z: 2, items: [
      limb([39, 33], [43, 38], 4.2, "clothDark"),
      circle(43, 38, 2, "clothHi"),
      limb([43, 38], [49, 37], 3.4, "clothDark"),
      circle(49, 37, 1.8, "skin"),
      line([42, 37, 44, 39], { stroke: "web", sw: 0.7 }),
    ] },
    armFront: { parent: "torso", pivot: [40, 34], z: 8, items: [
      limb([40, 34], [45, 36], 4.5, "cloth"),
      circle(45, 36, 2.1, "clothHi"),
      limb([45, 36], [51, 35], 3.7, "cloth"),
      circle(51, 35, 1.8, "skinHi"),
      line([44, 35, 46, 37], { stroke: "web", sw: 0.7 }),
    ] },
    head: { parent: "torso", pivot: [35, 30], z: 9, items: [
      // neck and face: slight profile toward the right
      path("M32 25H38V31H32Z", { fill: "skin" }),
      path("M31 15Q31 11 36 11Q41 12 41 17L40.5 23L38 26L32.5 26L30 22Z", { fill: "skinHi" }),
      path("M39 20Q43 19 42 22L39 24Z", { fill: "skin" }),
      ellipse(39.5, 18, 0.7, 1, "ink"),
      line([38, 23, 40, 23], { stroke: "accent", sw: 0.7 }),
      // hair/helmet differ while preserving one team silhouette
      ...(female ? [
        path("M30 17Q29 11 34 9Q40 8 42 13L41 17L38 15L35 16L32 14L30 20Z", { fill: hair }),
        path("M30 17L28 24L30 28L33 26L32 21Z", { fill: "hairHi" }),
        path("M30 22Q25 23 26 28L30 29L33 26Z", { fill: hair }),
        circle(30, 26, 1.2, "accent"),
        line([31, 11, 36, 9], { stroke: accessory, sw: 0.8 }),
      ] : [
        path("M29 16Q29 10 35 9Q41 9 43 13L42 17L39 15L35 16L31 14L30 19Z", { fill: "ink2" }),
        path("M29 14Q30 9 35 8Q41 8 44 12L42 14L31 14Z", { fill: "helmet" }),
        path("M31 12Q35 10 41 11L43 13L32 14Z", { fill: accessory }),
        rect(39, 14, 4, 2.1, { fill: "metal", r: 0.6 }),
        line([40, 15, 42, 15], { stroke: "ink", sw: 0.5 }),
      ]),
      // anime-inspired eye highlight kept small for the in-game scale
      circle(40, 18, 0.55, "metalHi"),
    ] },
    rifle: { parent: "torso", pivot: [48, 36], z: 10, items: [
      // compact rifle facing right, readable as a single small icon
      path("M45 33.2L59 33.2L61 34.5L59 36.2L48 36.2L46 38L43 37.2Z", { fill: "ink2" }),
      rect(48, 32.7, 10, 1.3, { fill: "metal", r: 0.4 }),
      line([50, 34, 59, 34], { stroke: "metalHi", sw: 0.5 }),
      rect(60, 34, 5, 0.9, { fill: "metal" }),
      rect(45, 32.3, 2, 2.1, { fill: "ink", r: 0.4 }),
      path("M49 36L52 36L51 41L48.5 41Z", { fill: "ink2" }),
      path("M45 36L47 36L45 40L43 39Z", { fill: "ink" }),
      circle(61, 33, 0.65, "metal"),
    ] },
    pack: { parent: "torso", pivot: [31, 34], z: 0, items: [
      path("M27 30L31 31L30 42L27 44L25 42L26 34Z", { fill: "clothDark" }),
      rect(26, 34, 3, 4, { fill: "vest", r: 0.6 }),
      line([26.5, 36, 28.5, 36], { stroke: "web", sw: 0.6 }),
    ] },
  })

  const idle = {
    torso: { rot: [[0, -1], [4, 1], [8, -1]] },
    head: { rot: [[0, 0], [4, -1], [8, 0]] },
    pelvis: { move: [[0, [0, 0]], [4, [0, -0.7]], [8, [0, 0]]] },
    legFront: { rot: [[0, -4], [8, -4]] }, legRear: { rot: [[0, 5], [8, 5]] },
    armFront: { rot: [[0, 0], [8, 0]] }, armBack: { rot: [[0, 0], [8, 0]] },
  }
  const run = {
    pelvis: { move: [[0, [0, 0]], [2, [0, -2]], [4, [0, 0]], [6, [0, -2]], [8, [0, 0]]] },
    torso: { rot: [[0, -8], [4, -11], [8, -8]] }, head: { rot: [[0, 7], [4, 6], [8, 7]] },
    legFront: { rot: [[0, -34], [4, 38], [8, -34]] }, bootFront: { rot: [[0, 10], [3, 35], [5, -8], [8, 10]] },
    legRear: { rot: [[0, 38], [4, -34], [8, 38]] }, bootRear: { rot: [[0, 20], [2, -10], [4, 12], [7, 30], [8, 20]] },
    armFront: { rot: [[0, -5], [4, 4], [8, -5]] }, armBack: { rot: [[0, 4], [4, -5], [8, 4]] },
  }
  const jump = {
    pelvis: { move: [[0, [0, 0]], [2, [0, 2]], [4, [0, -6]], [6, [0, -9]], [8, [0, 0]]] },
    torso: { rot: [[0, 0], [2, -8], [4, 3], [6, 0], [8, 0]] },
    legFront: { rot: [[0, -4], [2, 23], [4, -40], [6, -18], [8, -4]] }, bootFront: { rot: [[0, 0], [2, 45], [4, 70], [6, 38], [8, 0]] },
    legRear: { rot: [[0, 5], [2, 18], [4, -32], [6, -15], [8, 5]] }, bootRear: { rot: [[0, 0], [2, 45], [4, 60], [6, 30], [8, 0]] },
    armFront: { rot: [[0, 0], [2, -18], [4, -20], [6, 0], [8, 0]] }, armBack: { rot: [[0, 0], [2, 16], [4, 10], [6, 0], [8, 0]] },
  }
  const crouch = {
    pelvis: { move: [[0, [0, 0]], [2, [0, 4]], [4, [0, 4]]] },
    torso: { rot: [[0, 0], [2, 12], [4, 12]] }, head: { rot: [[0, 0], [2, -4], [4, -4]] },
    legFront: { rot: [[0, -4], [2, 28], [4, 28]] }, bootFront: { rot: [[0, 0], [2, -52], [4, -52]] },
    legRear: { rot: [[0, 5], [2, -42], [4, -42]] }, bootRear: { rot: [[0, 0], [2, 42], [4, 42]] },
    armFront: { rot: [[0, 0], [2, -6], [4, -6]] }, armBack: { rot: [[0, 0], [2, 4], [4, 4]] },
  }
  const shoot = {
    torso: { rot: [[0, -2], [2, -5], [4, -2]] }, head: { rot: [[0, 0], [2, -1], [4, 0]] },
    armFront: { rot: [[0, 0], [1, -3], [2, 0], [4, -2]] },
    armBack: { rot: [[0, 0], [1, 2], [2, 0], [4, 0]] },
    rifle: { rot: [[0, 0], [1, -1], [2, 0], [4, 0]] },
  }

  const key = female ? "woman" : "man"
  const naturalHead = (clip) => ({ ...clip, head: { ...(clip.head ?? {}), scale: [[0, 0.82]] } })
  sprite(`${key}_stand`, { w: 70, h: 80, ...OL }, pose(body, naturalHead({})))
  const common = { w: 70, h: 80, scale: 2, ...OL }
  anim(`${key}_idle`, { ...common, frames: 8, fps: 8 }, pose(body, naturalHead(idle)))
  anim(`${key}_run`, { ...common, frames: 8, fps: 12 }, pose(body, naturalHead(run)))
  anim(`${key}_jump`, { ...common, frames: 8, fps: 12 }, pose(body, naturalHead(jump)))
  anim(`${key}_crouch`, { ...common, frames: 4, fps: 6 }, pose(body, naturalHead(crouch)))
  anim(`${key}_shoot`, { ...common, frames: 5, fps: 10 }, pose(body, naturalHead(shoot)),
    (f) => f.i === 1 || f.i === 4 ? circle(65, 34, 1.8, { fill: "muzzle", glow: { r: 2, color: "muzzle", strength: 1.5 } }) : null)
  return body
}

const WOMAN = buildOperator({ female: true })
const MAN = buildOperator({ female: false })

scene("operators_preview", { w: 150, h: 112, bg: "#e8e5d9" },
  rect(0, 96, 150, 16, "#d5d0c1"),
  line([0, 96, 150, 96], { stroke: "#aba697", sw: 0.7 }),
  stamp("woman_stand", 43, 96, { anchor: [0.5, 1], scale: 1.1 }),
  stamp("man_stand", 108, 96, { anchor: [0.5, 1], scale: 1.1 }),
  text(43, 108, "WOMAN / OPERATOR", { fill: "#334238", size: 4.2, anchor: "middle", weight: "bold", font: "DejaVu Sans" }),
  text(108, 108, "MAN / OPERATOR", { fill: "#334238", size: 4.2, anchor: "middle", weight: "bold", font: "DejaVu Sans" }),
)
