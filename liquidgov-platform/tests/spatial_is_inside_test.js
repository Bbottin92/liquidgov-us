// Spatial step 2: the one focused test of isInside(droplet, region). Run: node tests/spatial_is_inside_test.js
'use strict';
var assert = require('assert');
var isInside = require('../assets/spatial.js').isInside;
var region = { cx: 100, cy: 50, hw: 40, hh: 18 };   // a box centered at (100, 50), 80 wide and 36 tall
assert.strictEqual(isInside({ x: 100, y: 50 }, region), true, 'the center is inside');
assert.strictEqual(isInside({ x: 140, y: 68 }, region), true, 'the corner (edge) counts as inside');
assert.strictEqual(isInside({ x: 141, y: 50 }, region), false, 'just past the right edge is outside');
assert.strictEqual(isInside({ x: 100, y: 31 }, region), false, 'just above the top edge is outside');
assert.strictEqual(isInside({ x: 100, y: 50 }, null), false, 'no region: not inside');
console.log('PASS isInside(droplet, region): center and edge inside, past an edge outside');
