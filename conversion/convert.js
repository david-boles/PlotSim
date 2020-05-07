const util = require('util')
const fs = require('fs')
const Helper = require('dxf').Helper

const maxPlotMin = [-3000, -3000]
const maxPlotMax = [3000, 1500]
const maxPlotSize = [maxPlotMax[0] - maxPlotMin[0], maxPlotMax[1] - maxPlotMin[1]]
const plotCenter = [maxPlotMin[0] + (maxPlotSize[0]/2), maxPlotMin[1] + (maxPlotSize[1]/2)]

const text = fs.readFileSync(process.argv[2], "utf8")
const helper = new Helper(text)

const out = helper.toPolylines();
const dxfMin = [out.bbox.min.x, out.bbox.min.y]
const dxfMax = [out.bbox.max.x, out.bbox.max.y]
const dxfSize = [dxfMax[0] - dxfMin[0], dxfMax[1] - dxfMin[1]]

const reducePlotWidth = (maxPlotSize[0]/maxPlotSize[1]) > (dxfSize[0]/dxfSize[1])
const ratioFactor = reducePlotWidth? dxfSize[0]/dxfSize[1]: dxfSize[1]/dxfSize[0];

const plotSize = [reducePlotWidth? ratioFactor * maxPlotSize[1] : maxPlotSize[0], reducePlotWidth? maxPlotSize[1] : ratioFactor * maxPlotSize[0]]
const plotMin = [plotCenter[0] - (plotSize[0]/2), plotCenter[1] - (plotSize[1]/2)]

const scaleFactor = [plotSize[0] / dxfSize[0], plotSize[1] / dxfSize[1]]

const toPlotCoords = (dxfCoords) => {
  return [Math.round((dxfCoords[0] - dxfMin[0]) * scaleFactor[0] + plotMin[0]), Math.round((-(dxfCoords[1] - dxfMin[1]) + dxfSize[1]) * scaleFactor[1] + plotMin[1])]
}

const moves = []
for(let polyline of out.polylines) {
  let first = true
  for(let vertex of polyline.vertices) {
    moves[moves.length] = [first? 0 : 1, ...toPlotCoords(vertex)]
    first = false
  }
}
moves[moves.length] = [0, 0, 0]

console.log('static short state[][3] = \n{');
for(let move of moves) {
  console.log(`${move[0]}, ${move[1]}, ${move[2]},`)
}
console.log('};')