/* Proximity voice and video on profile pages (spatial step 11, SPATIAL_EVOLUTION.md). Off until you turn it on: the Mic and
   Camera buttons ask the browser for permission, then tell the server you are sharing. With someone else in the same
   instance who is also sharing, within RANGE.aware of you, a direct browser-to-browser connection (WebRTC) carries the
   sound and picture; its volume is full inside RANGE.near and falls in a straight line to nothing at RANGE.aware. Past
   that, or when either of you leaves the instance or stops sharing, the connection is closed and nothing is sent or
   received. The server only relays the setup messages, and only between people it lets talk (same instance, both
   sharing, in range, not blocked: here.cpp); this file hangs up on its own side too. Nothing is recorded.
   Distances are fractions of the page (the same coordinates the dots use), so RANGE.aware matches kMediaAware in here.cpp.
   No STUN or TURN server is configured, so two people behind different NATs may not connect. */
(function () {
  var cfg = window.LG_HERE, api = window.LG_HERE_API;
  if (!cfg || !cfg.me || !api || !window.RTCPeerConnection || !navigator.mediaDevices) return;
  var RANGE = { near: 0.15, aware: 0.5 };
  var want = { mic: false, cam: false }, stream = null, peers = {}, pendingOffer = {}, pendingIce = {}, last = [];

  var box = document.createElement('div');
  box.id = 'lg-media';
  box.style.cssText = 'position:fixed;left:8px;bottom:8px;z-index:50;display:flex;gap:6px';
  var held = document.createElement('div');
  held.id = 'lg-media-held';
  held.style.cssText = 'position:fixed;right:8px;bottom:8px;z-index:50;display:flex;gap:6px;max-width:60vw';
  var buttons = {};
  [['mic', 'Mic'], ['cam', 'Camera']].forEach(function (k) {
    var b = document.createElement('button');
    b.type = 'button';
    b.setAttribute('data-kind', k[0]);
    b.addEventListener('click', function () { toggle(k[0]); });
    buttons[k[0]] = { el: b, label: k[1] };
    box.appendChild(b);
  });
  document.body.appendChild(box);
  document.body.appendChild(held);
  function paint() {
    Object.keys(buttons).forEach(function (k) {
      buttons[k].el.textContent = buttons[k].label + (want[k] ? ' on' : ' off');
      buttons[k].el.setAttribute('aria-pressed', want[k] ? 'true' : 'false');
    });
  }
  paint();

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
    if (p.el) { p.el.srcObject = null; if (p.el.parentNode) p.el.parentNode.removeChild(p.el); }
    delete peers[u];
  }
  function makePeer(u) {
    var pc = new RTCPeerConnection({ iceServers: [] });
    var p = { pc: pc, u: u, el: null, vol: 0, remoteSet: false };
    peers[u] = p;
    stream.getTracks().forEach(function (t) { pc.addTrack(t, stream); });
    pc.onicecandidate = function (e) { if (e.candidate) sendSig(u, { k: 'ice', c: e.candidate }); };
    pc.ontrack = function (e) {
      var rs = e.streams[0];
      if (!rs) return;
      if (!p.el) {
        p.el = document.createElement('video');   // a video element plays sound too; it is hidden while there is no picture
        p.el.autoplay = true; p.el.playsInline = true;
        p.el.setAttribute('data-from', u);
        p.el.style.cssText = 'width:140px;border-radius:8px;background:#000';
        p.el.volume = p.vol;
        held.appendChild(p.el);
      }
      p.el.srcObject = rs;
      p.el.style.display = rs.getVideoTracks().length ? '' : 'none';
      rs.onaddtrack = function () { p.el.style.display = rs.getVideoTracks().length ? '' : 'none'; };
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
    Object.keys(peers).forEach(function (u) { if (!live[u]) close(u); });
    Object.keys(pendingOffer).forEach(function (u) { if (!live[u]) delete pendingOffer[u]; });
  }
  api.onPeople(onPeople);

  // what the check reads
  window.LG_HERE_MEDIA = {
    RANGE: RANGE,
    state: function () {
      var out = { want: { mic: want.mic, cam: want.cam }, self: null, peers: {}, elements: held.querySelectorAll('video').length };
      last.forEach(function (p) { if (p.u === cfg.me) out.self = { mic: !!p.mic, cam: !!p.cam, instanceId: p.instanceId }; });
      Object.keys(peers).forEach(function (u) {
        out.peers[u] = { conn: peers[u].pc.connectionState, volume: peers[u].el ? peers[u].el.volume : null,
                         tracks: peers[u].el && peers[u].el.srcObject ? peers[u].el.srcObject.getTracks().length : 0 };
      });
      return out;
    }
  };
})();
