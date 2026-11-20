// The map globes' page (/globe/<kind>, map globes G1, Brandon 2026-11-20 "a real street map"): draws the OpenStreetMap
// street map in #globemap with Leaflet (assets/leaflet.js, 1.9.4, BSD-2, see leaflet_license.txt) and one dot per
// place in data-points (JSON: lat, lon, name, href), centred on data-center (your city) or the whole country.
// Names are set as text, never as HTML. Tiles come from tile.openstreetmap.org (their attribution is shown).
(function () {
  var el = document.getElementById('globemap');
  if (!el || !window.L) return;
  var pts = [];
  try { pts = JSON.parse(el.getAttribute('data-points') || '[]'); } catch (e) { pts = []; }   // err-ok: no dots
  var c = (el.getAttribute('data-center') || '').split(',').map(Number);
  var map = L.map(el, { scrollWheelZoom: false, worldCopyJump: true });
  L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png', {
    maxZoom: 19, attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors'
  }).addTo(map);
  // the credit line without Leaflet's default flag: a civic site keeps its own chrome neutral
  map.attributionControl.setPrefix('<a href="https://leafletjs.com">Leaflet</a>');
  var group = [];
  pts.forEach(function (p) {
    if (typeof p.lat !== 'number' || typeof p.lon !== 'number') return;
    var dot = L.circleMarker([p.lat, p.lon], { radius: 8, weight: 2, color: '#7d5a17', fillColor: '#d1a85c', fillOpacity: 0.9 });
    var tip = document.createElement('span');
    tip.textContent = p.name || '';
    dot.bindTooltip(tip);
    if (p.href && p.href.charAt(0) === '/') dot.on('click', function () { location.assign(p.href); });
    dot.addTo(map);
    group.push([p.lat, p.lon]);
  });
  if (c.length === 2 && isFinite(c[0]) && isFinite(c[1])) {
    L.circle([c[0], c[1]], { radius: 900, weight: 1, color: '#4f8cff', fillOpacity: 0.12 }).addTo(map);
    map.setView([c[0], c[1]], group.length ? 9 : 11);
  } else if (group.length) {
    map.fitBounds(group, { padding: [30, 30], maxZoom: 11 });
  } else {
    map.setView([39.5, -98.35], 4);   // the whole country until your city is known
  }
})();
