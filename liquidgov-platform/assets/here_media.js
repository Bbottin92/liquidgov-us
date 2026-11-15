/* Proximity voice and video on profile pages (spatial step 11, SPATIAL_EVOLUTION.md). Off until you turn it on: the Microphone and
   Camera switches in your drop's menu (walker.js) ask the browser for permission, then tell the server you are sharing. With someone else in the same
   instance who is also sharing, within RANGE.aware of you, a direct browser-to-browser connection (WebRTC) carries the
   sound and picture; its volume is full inside RANGE.near and falls in a straight line to nothing at RANGE.aware. Past
   that, or when either of you leaves the instance or stops sharing, the connection is closed and nothing is sent or
   received. The server only relays the setup messages, and only between people it lets talk (same instance, both
   sharing, in range, not blocked: here.cpp); this file hangs up on its own side too. Nothing is recorded.
   Distances are fractions of the page (the same coordinates the dots use), so RANGE.aware matches kMediaAware in here.cpp.
   No STUN or TURN server is configured, so two people behind different NATs may not connect.
   The picture (Brandon 2026-11-14): someone's camera is a small round bubble drawn over their own dot, moving with it
   (.lg-media-bubble, site.css). Click it to enlarge it, click again to shrink it; enlarged, a fullscreen button shows
   when the pointer is over it (always on a touch screen). Sound-only people get no bubble. */
(function () {
  var cfg = window.LG_HERE, api = window.LG_HERE_API;
  if (!cfg || !cfg.me || !api || !window.RTCPeerConnection || !navigator.mediaDevices) return;
  var RANGE = { near: 0.15, aware: 0.5 };
  var want = { mic: false, cam: false }, stream = null, peers = {}, pendingOffer = {}, pendingIce = {}, last = [];

  var held = document.createElement('div');
  held.id = 'lg-media-held';
  held.style.cssText = 'position:fixed;right:8px;bottom:8px;z-index:50;display:flex;gap:6px;max-width:60vw';
  document.body.appendChild(held);
  // The Microphone and Camera switches are in your drop's menu (walker.js, Brandon 2026-11-14: "the microphone/camera
  // buttons shouldnt be on the screen"): it reads and flips them through window.LG_MEDIA and repaints on
  // 'lg-media-change'.
  function paint() { document.dispatchEvent(new CustomEvent('lg-media-change', { detail: { mic: want.mic, cam: want.cam } })); }
  window.LG_MEDIA = {
    state: function () { return { mic: want.mic, cam: want.cam }; },
    toggle: function (kind) { if (kind === 'mic' || kind === 'cam') toggle(kind); }
  };

  function gain(d) { return d <= RANGE.near ? 1 : d >= RANGE.aware ? 0 : (RANGE.aware - d) / (RANGE.aware - RANGE.near); }
  function sendFlags() { api.send({ mic: want.mic, cam: want.cam }); }
  function stopLocal() { if (stream) stream.getTracks().forEach(function (t) { t.stop(); }); stream = null; }
  function closeAll() { Object.keys(peers).forEach(close); pendingOffer = {}; pendingIce = {}; }
  function sendSig(u, obj) { api.send({ to: u, sig: JSON.stringify(obj) }); }

  function toggle(kind) {
    var was = want[kind];
    want[kind] = !was;
    paint();
    if (!want.mic && !want.cam) { stopLocal(); closeAll(); sendFlags(); return; }
    // asks the browser for permission the first time; a refusal leaves it off
    navigator.mediaDevices.getUserMedia({ audio: want.mic, video: want.cam }).then(function (s) {
      stopLocal(); closeAll(); stream = s; sendFlags();
    }, function () { want[kind] = was; paint(); });
  }

  function close(u) {
    var p = peers[u];
    if (!p) return;
    try { p.pc.close(); } catch (e) { /* already closed */ }
    if (p.el) { p.el.srcObject = null; if (p.wrap.parentNode) p.wrap.parentNode.removeChild(p.wrap); }
    delete peers[u];
  }
  // The bubble sits inside that person's dot (here.js: #lg-here .lg-here-dot[data-u]), so it moves with it. Until the
  // dot is drawn, and for the moment here.js redraws it, the bubble waits in the corner strip.
  function home(p) {
    if (!p.wrap) return;
    var dot = document.querySelector('#lg-here .lg-here-dot[data-u="' + (window.CSS && CSS.escape ? CSS.escape(p.u) : p.u) + '"]');
    var to = dot || held;
    if (p.wrap.parentNode === to) return;
    to.appendChild(p.wrap);
    var play = p.el.play();   // a moved video element pauses
    if (play && play.catch) play.catch(function () { /* the browser will not autoplay yet */ });
  }
  function bubble(p) {
    var wrap = document.createElement('div'), full = document.createElement('button');
    wrap.className = 'lg-media-bubble';
    wrap.setAttribute('data-from', p.u);
    wrap.title = p.u;
    p.el = document.createElement('video');   // a video element plays sound too; the bubble is hidden while there is no picture
    p.el.autoplay = true; p.el.playsInline = true;
    p.el.setAttribute('data-from', p.u);
    p.el.volume = p.vol;
    full.type = 'button';
    full.className = 'lg-media-full';
    full.setAttribute('aria-label', 'Fullscreen');
    full.textContent = '\u26F6';
    full.addEventListener('click', function (e) {
      e.stopPropagation();
      var v = p.el, go = v.requestFullscreen || v.webkitRequestFullscreen || v.webkitEnterFullscreen;
      if (go) { var r = go.call(v); if (r && r.catch) r.catch(function () { /* refused by the browser */ }); }
    });
    wrap.addEventListener('click', function () { wrap.classList.toggle('lg-media-big'); });
    wrap.appendChild(p.el);
    wrap.appendChild(full);
    p.wrap = wrap;
  }
  function makePeer(u) {
    var pc = new RTCPeerConnection({ iceServers: [] });
    var p = { pc: pc, u: u, el: null, wrap: null, vol: 0, remoteSet: false };
    peers[u] = p;
    stream.getTracks().forEach(function (t) { pc.addTrack(t, stream); });
    pc.onicecandidate = function (e) { if (e.candidate) sendSig(u, { k: 'ice', c: e.candidate }); };
    pc.ontrack = function (e) {
      var rs = e.streams[0];
      if (!rs) return;
      if (!p.el) bubble(p);
      p.el.srcObject = rs;
      p.wrap.style.display = rs.getVideoTracks().length ? '' : 'none';
      rs.onaddtrack = function () { p.wrap.style.display = rs.getVideoTracks().length ? '' : 'none'; };
      home(p);
    };
    pc.onconnectionstatechange = function () { if (pc.connectionState === 'failed') close(u); };
    return p;
  }
  function flush(p) {
    p.remoteSet = true;
    (pendingIce[p.u] || []).forEach(function (c) { p.pc.addIceCandidate(c).catch(function () { /* a stale candidate */ }); });
    delete pendingIce[p.u];
  }
  function offerTo(u) {
    var p = makePeer(u);
    p.pc.createOffer().then(function (o) { return p.pc.setLocalDescription(o); })
      .then(function () { sendSig(u, { k: 'offer', sdp: p.pc.localDescription }); })
      .catch(function () { close(u); });
  }
  function answer(u, msg) {
    var p = makePeer(u);
    p.pc.setRemoteDescription(msg.sdp).then(function () { flush(p); return p.pc.createAnswer(); })
      .then(function (a) { return p.pc.setLocalDescription(a); })
      .then(function () { sendSig(u, { k: 'answer', sdp: p.pc.localDescription }); })
      .catch(function () { close(u); });
  }

  api.onSignal(function (m) {
    var msg, u = m.from;
    try { msg = JSON.parse(m.data); } catch (e) { return; }
    if (msg.k === 'offer') { pendingOffer[u] = msg; onPeople(last); }
    else if (msg.k === 'answer') {
      var p = peers[u];
      if (p && !p.remoteSet) p.pc.setRemoteDescription(msg.sdp).then(function () { flush(p); }, function () { close(u); });
    } else if (msg.k === 'ice') {
      var q = peers[u];
      if (q && q.remoteSet) q.pc.addIceCandidate(msg.c).catch(function () { /* a stale candidate */ });
      else (pendingIce[u] = pendingIce[u] || []).push(msg.c);
    }
  });

  function onPeople(people) {
    last = people;
    var self = null;
    people.forEach(function (p) { if (p.u === cfg.me) self = p; });
    if (!self) return;
    if (!!self.mic !== want.mic || !!self.cam !== want.cam) sendFlags();   // a reconnect forgets what the server was told
    var live = {};
    people.forEach(function (p) {
      if (p.u === cfg.me || !stream || !(p.mic || p.cam)) return;
      var d = Math.hypot(p.x - self.x, p.y - self.y);
      if (d > RANGE.aware) return;
      live[p.u] = true;
      if (!peers[p.u]) {
        if (pendingOffer[p.u]) { var o = pendingOffer[p.u]; delete pendingOffer[p.u]; answer(p.u, o); }
        else if (cfg.me < p.u) offerTo(p.u);   // the lower name makes the offer
      }
      if (peers[p.u]) { peers[p.u].vol = gain(d); if (peers[p.u].el) peers[p.u].el.volume = peers[p.u].vol; }
    });
    Object.keys(peers).forEach(function (u) { if (!live[u]) close(u); else home(peers[u]); });
    Object.keys(pendingOffer).forEach(function (u) { if (!live[u]) delete pendingOffer[u]; });
  }
  api.onPeople(onPeople);

  // what the check reads
  window.LG_HERE_MEDIA = {
    RANGE: RANGE,
    state: function () {
      var out = { want: { mic: want.mic, cam: want.cam }, self: null, peers: {}, elements: document.querySelectorAll('video[data-from]').length };
      last.forEach(function (p) { if (p.u === cfg.me) out.self = { mic: !!p.mic, cam: !!p.cam, instanceId: p.instanceId }; });
      Object.keys(peers).forEach(function (u) {
        out.peers[u] = { conn: peers[u].pc.connectionState, volume: peers[u].el ? peers[u].el.volume : null,
                         tracks: peers[u].el && peers[u].el.srcObject ? peers[u].el.srcObject.getTracks().length : 0 };
      });
      return out;
    }
  };
})();
