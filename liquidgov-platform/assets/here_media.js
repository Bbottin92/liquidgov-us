/* Proximity voice and video on profile pages (spatial step 11, SPATIAL_EVOLUTION.md). Off until you turn it on: the Microphone and
   Camera switches in your drop's menu (walker.js, LG_WALKER.media) ask the browser for permission and stay on across
   pages until you turn them off; this file tells the server what you are sharing. With someone else in the same
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
  var cfg = window.LG_HERE, api = window.LG_HERE_API, media = window.LG_WALKER && LG_WALKER.media;
  if (!cfg || !cfg.me || !api || !media || !window.RTCPeerConnection || !navigator.mediaDevices) return;
  var RANGE = { near: 0.15, aware: 0.5 }, RETRY_MS = 4000;
  var stream = media.stream(), peers = {}, pendingOffer = {}, pendingIce = {}, last = [];

  var held = document.createElement('div');
  held.id = 'lg-media-held';
  held.style.cssText = 'position:fixed;right:8px;bottom:8px;z-index:50;display:flex;gap:6px;max-width:60vw';
  document.body.appendChild(held);
  // Your camera and microphone are walker.js's (LG_WALKER.media: the switches in your drop's menu, kept on as you set
  // them across pages, Brandon 2026-11-15). When they change, every call here starts again with the new stream.
  function sharing() {   // what you are actually sending: the switches, and only what the browser gave
    var st = media.state();
    return { mic: !!(st.mic && stream && stream.getAudioTracks().length), cam: !!(st.cam && stream && stream.getVideoTracks().length) };
  }
  // (the next snapshot from the server starts the calls again: it already knows what you share by then)
  media.onChange(function (s) { closeAll(); stream = s; sendFlags(); });

  function gain(d) { return d <= RANGE.near ? 1 : d >= RANGE.aware ? 0 : (RANGE.aware - d) / (RANGE.aware - RANGE.near); }
  function sendFlags() { api.send(sharing()); }
  function closeAll() { Object.keys(peers).forEach(close); pendingOffer = {}; pendingIce = {}; }
  function sendSig(u, obj) { api.send({ to: u, sig: JSON.stringify(obj) }); }

  function close(u) {
    var p = peers[u];
    if (!p) return;
    try { p.pc.close(); } catch (e) { /* already closed */ }
    if (p.rippleOn) { LG_WALKER.ripples(p.rippleOn, null); p.rippleOn = null; }
    if (p.el) { p.el.srcObject = null; if (p.wrap.parentNode) p.wrap.parentNode.removeChild(p.wrap); }
    delete peers[u];
  }
  // The bubble sits inside that person's dot (here.js: #lg-here .lg-here-dot[data-u]), so it moves with it. Until the
  // dot is drawn, and for the moment here.js redraws it, the bubble waits in the corner strip.
  function home(p) {
    if (!p.wrap) return;
    var dot = document.querySelector('#lg-here .lg-here-dot[data-u="' + (window.CSS && CSS.escape ? CSS.escape(p.u) : p.u) + '"]');
    var to = dot || held;
    // their sound ripples off their dot (walker.js ripples), like yours off your drop
    var face = dot && dot.firstElementChild;
    if (face && p.rippleOn !== face && p.el && p.el.srcObject && p.el.srcObject.getAudioTracks().length) {
      if (p.rippleOn) LG_WALKER.ripples(p.rippleOn, null);
      LG_WALKER.ripples(face, p.el.srcObject);
      p.rippleOn = face;
    }
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
    var p = { pc: pc, u: u, el: null, wrap: null, vol: 0, remoteSet: false, t: Date.now() };
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
    var sh = sharing();
    if (!!self.mic !== sh.mic || !!self.cam !== sh.cam) sendFlags();   // a reconnect forgets what the server was told
    var live = {};
    people.forEach(function (p) {
      if (p.u === cfg.me || !stream || !(p.mic || p.cam)) return;
      var d = Math.hypot(p.x - self.x, p.y - self.y);
      if (d > RANGE.aware) return;
      live[p.u] = true;
      // an offer that got no answer (a setup message lost on the way, or wiped by the other side starting over) is made
      // again, rather than waiting for ever (2026-11-15: here_media M1 hung that way on a busy machine)
      var q = peers[p.u];
      if (q && !q.remoteSet && cfg.me < p.u && Date.now() - q.t > RETRY_MS) close(p.u);
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
      var st = media.state(), out = { want: { mic: st.mic, cam: st.cam }, self: null, peers: {}, elements: document.querySelectorAll('video[data-from]').length };
      last.forEach(function (p) { if (p.u === cfg.me) out.self = { mic: !!p.mic, cam: !!p.cam, instanceId: p.instanceId }; });
      Object.keys(peers).forEach(function (u) {
        out.peers[u] = { conn: peers[u].pc.connectionState, volume: peers[u].el ? peers[u].el.volume : null,
                         tracks: peers[u].el && peers[u].el.srcObject ? peers[u].el.srcObject.getTracks().length : 0 };
      });
      return out;
    }
  };
})();
