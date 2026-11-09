const fs = require('fs');
const { Resvg } = require('@resvg/resvg-js');
const [svgPath, pngPath, scaleText] = process.argv.slice(2);
if (!svgPath || !pngPath) {
  console.error('Usage: node render_vector_sheet.js input.svg output.png [supersample]');
  process.exit(2);
}
const scale = Number(scaleText || 4);
const svg = fs.readFileSync(svgPath);
const png = new Resvg(svg, { fitTo: { mode: 'zoom', value: scale } }).render().asPng();
fs.writeFileSync(pngPath, png);
