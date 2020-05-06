const util = require('util')
const fs = require('fs')
const DxfParser = require('dxf-parser')

const scale = 6
const plotterOffset = [-5000, 5000]

const text = fs.readFileSync(process.argv[2], "utf8")
const parsed = new DxfParser().parseSync(text);



 // [[0/1, 0.01cm int, 0.01cm int], ...]
const moves = []

// {x: dxf float, y: dxf float} -> new move
const addMove = (penDown, pathSegment) => {
  moves[moves.length] = [penDown? 1 : 0, Math.round((pathSegment.x * scale) + plotterOffset[0]), Math.round((-pathSegment.y * scale) + plotterOffset[1])]
}

// [{x: dxf float, y: dxf float}, ...] -> new move
const addPath = (path) => {
  let first = true
  for(let segment of path) {
    addMove(!first, segment)
    first = false
  }
}

const isVisible = (ent) => {
  return (ent.lineType ? ent.lineType.indexOf('HIDDEN') == -1 : true) && (ent.layer? ent.layer == 'CO3' /*|| ent.layer == 'CO2'*/ : false);
}

const handlers = {
  LINE: (ent) => addPath(ent.vertices),
  LWPOLYLINE: (ent) => addPath(ent.vertices),
}

for(let entity of parsed.entities) {
  const handler = handlers[entity.type]
  if(handler && isVisible(entity)) {
    console.error("handling ", entity)
    handler(entity)
  }else {
    // console.error("No handler for entity: ", entity)
  }
}

moves[moves.length] = [0, 0, 0]

console.log('static short state[][3] = \n{');
for(let move of moves) {
  console.log(`${move[0]}, ${move[1]}, ${move[2]},`)
}
console.log('};')

// console.log(util.inspect(parsed, {showHidden: false, depth: null}))
//console.log(parsed)