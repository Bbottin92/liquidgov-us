/* Spatial step 2 (SPATIAL_EVOLUTION.md): is a droplet inside a box's region? Pure: no page, no state.
   droplet: { x, y }. region: { cx, cy, hw, hh } (center and half extents, the shape lgmap.js gives each box as
   box.region). Both in the same units. The edge counts as inside. Not loaded by any page yet. */
(function (root) {
  'use strict';
  function isInside(droplet, region) {
    if (!droplet || !region) return false;
    return Math.abs(droplet.x - region.cx) <= region.hw && Math.abs(droplet.y - region.cy) <= region.hh;
  }
  var api = { isInside: isInside };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.LG_SPATIAL = api;
})(typeof window !== 'undefined' ? window : this);
