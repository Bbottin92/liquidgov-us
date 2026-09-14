/* Juketube — shared radio. Playback stays in this widget. */
(function () {
  var lastBeacon = {};
  function lgBeacon(ev, detail) {
    var now = Date.now();
    var prev = lastBeacon[ev] || 0;
    if (now - prev < 2500) return;
    lastBeacon[ev] = now;
    var body = "ev=" + encodeURIComponent(ev || "");
    if (detail) body += "&d=" + encodeURIComponent(String(detail).slice(0, 120));
    try {
      if (navigator.sendBeacon) {
        navigator.sendBeacon("/log", new Blob([body], { type: "application/x-www-form-urlencoded" }));
      } else {
        fetch("/log", { method: "POST", credentials: "same-origin",
          headers: { "Content-Type": "application/x-www-form-urlencoded" }, body: body, keepalive: true });
      }
    } catch (e) {}
  }

  if (document.getElementById("jbtoggle")) return;

  var root = document.getElementById("juketube-root");
  if (!root) {
    root = document.createElement("div");
    root.id = "juketube-root";
    document.body.appendChild(root);
  }

  var ICON =
    '<svg viewBox="0 0 24 24" aria-hidden="true"><rect x="4" y="3" width="16" height="18" rx="3" fill="currentColor" opacity=".95"/><circle cx="12" cy="13" r="5" fill="none" stroke="var(--paper,#f7f1e4)" stroke-width="1.6"/><circle cx="12" cy="13" r="1.4" fill="var(--paper,#f7f1e4)"/><rect x="8" y="5.2" width="8" height="1.6" rx=".4" fill="var(--paper,#f7f1e4)"/></svg>';

  root.innerHTML =
    '<button id="jbtoggle" type="button" aria-label="Juketube" aria-expanded="false">' + ICON + "</button>" +
    '<div id="jbhint">Juketube — search, stations, history. Tap a track and everyone on that station hears it.</div>' +
    '<div id="jbpanel" role="dialog" aria-label="Juketube">' +
    '<div id="jbstations" hidden>' +
    '<div class="jb-head">' +
    '<p class="jb-name">Stations</p>' +
    '<button type="button" id="jbstations-close" aria-label="Close stations">×</button>' +
    "</div>" +
    '<div id="jbstationlist"></div>' +
    '<div class="jb-follow">' +
    '<input id="jbfollow" placeholder="Follow a username" autocomplete="off">' +
    '<button type="button" id="jbfollowbtn">Follow</button>' +
    "</div>" +
    '<p class="muted" id="jbfollowerr" hidden></p>' +
    "</div>" +
    '<div class="jb-head">' +
    '<p class="jb-name">Juketube</p>' +
    '<button type="button" id="jbstations-toggle">Stations</button>' +
    "</div>" +
    '<div class="jb-bar">' +
    '<select id="jbscope" aria-label="Search in">' +
    '<option value="all">All</option>' +
    '<option value="groups">Groups</option>' +
    '<option value="friends">Friends</option>' +
    '<option value="others">Others</option>' +
    '<option value="self">Self</option>' +
    '<option value="history">History</option>' +
    "</select>" +
    "</div>" +
    '<div class="jb-searchwrap">' +
    '<input id="jbsearch" type="search" placeholder="Search" autocomplete="off" enterkeyhint="search">' +
    '<div id="jbresults"></div>' +
    "</div>" +
    '<div id="jbnow"><span class="muted">Connecting…</span></div>' +
    '<div id="jbtube-wrap"><div id="jbtube-player"></div><div id="jbtube-block" hidden></div></div>' +
    '<div class="jb-controls">' +
    '<button type="button" id="jbpause">Pause</button>' +
    '<button type="button" id="jbskip">Skip</button>' +
    '<button type="button" id="jbmute">Mute</button>' +
    '<input id="jbvol" type="range" min="0" max="100" value="80" aria-label="Volume">' +
    '<button type="button" id="jbcc">CC</button>' +
    '<button type="button" id="jbfs">Full</button>' +
    '<button type="button" id="jbcopy">Copy URL</button>' +
    "</div>" +
    '<div id="jbupcoming"></div>' +
    "</div>";

  var toggle = document.getElementById("jbtoggle");
  var panel = document.getElementById("jbpanel");
  var hint = document.getElementById("jbhint");
  var nowEl = document.getElementById("jbnow");
  var resultsEl = document.getElementById("jbresults");
  var upcomingEl = document.getElementById("jbupcoming");
  var search = document.getElementById("jbsearch");
  var pauseBtn = document.getElementById("jbpause");
  var skipBtn = document.getElementById("jbskip");
  var muteBtn = document.getElementById("jbmute");
  var vol = document.getElementById("jbvol");
  var ccBtn = document.getElementById("jbcc");
  var fsBtn = document.getElementById("jbfs");
  var copyBtn = document.getElementById("jbcopy");
  var blockEl = document.getElementById("jbtube-block");
  var scopeEl = document.getElementById("jbscope");
  var stToggle = document.getElementById("jbstations-toggle");
  var stClose = document.getElementById("jbstations-close");
  var stationsUi = document.getElementById("jbstations");
  var stationList = document.getElementById("jbstationlist");
  var followInput = document.getElementById("jbfollow");
  var followBtn = document.getElementById("jbfollowbtn");
  var followErr = document.getElementById("jbfollowerr");

  if (localStorage.getItem("lg-jb-hint") === "1") hint.style.display = "none";

  function syncViewport() {
    var vv = window.visualViewport;
    var kb = 0, vvh = window.innerHeight;
    if (vv) {
      kb = Math.max(0, Math.round(window.innerHeight - vv.height - vv.offsetTop));
      vvh = Math.round(vv.height);
    }
    document.documentElement.style.setProperty("--kb", kb + "px");
    document.documentElement.style.setProperty("--vvh", vvh + "px");
    panel.classList.toggle("kb", kb > 60);
  }
  if (window.visualViewport) {
    visualViewport.addEventListener("resize", syncViewport);
    visualViewport.addEventListener("scroll", syncViewport);
  }
  window.addEventListener("resize", syncViewport);
  syncViewport();

  function setOpen(open) {
    panel.classList.toggle("open", open);
    panel.style.display = open ? "flex" : "none";
    toggle.setAttribute("aria-expanded", open ? "true" : "false");
    if (open) {
      hint.style.display = "none";
      localStorage.setItem("lg-jb-hint", "1");
      ensurePlayer(function () {
        applySound();
        try {
          var st = ytPlayer.getPlayerState();
          if (lastId && st !== YT.PlayerState.PLAYING) ytPlayer.playVideo();
        } catch (e) {}
      });
      if (stationId.indexOf("u:") !== 0 && (!ws || ws.readyState > 1)) connectWs();
      syncViewport();
    }
  }

  toggle.addEventListener("click", function (e) {
    e.stopPropagation();
    setOpen(!panel.classList.contains("open"));
  });
  hint.addEventListener("click", function (e) {
    e.stopPropagation();
    setOpen(true);
  });
  document.addEventListener("pointerdown", function (e) {
    if (!panel.classList.contains("open")) return;
    var t = e.target;
    if (!t || !document.documentElement.contains(t)) return;
    if (panel.contains(t) || toggle.contains(t)) return;
    setOpen(false);
  });

  function setStationsOpen(open) {
    stationsUi.hidden = !open;
    stToggle.classList.toggle("on", open);
  }
  stToggle.addEventListener("click", function (e) {
    e.stopPropagation();
    setStationsOpen(stationsUi.hidden);
  });
  stClose.addEventListener("click", function (e) {
    e.stopPropagation();
    setStationsOpen(false);
  });
  panel.addEventListener("click", function (e) {
    if (stationsUi.hidden) return;
    if (stationsUi.contains(e.target) || stToggle.contains(e.target)) return;
    setStationsOpen(false);
  });

  var ytPlayer = null;
  var lastId = null;
  var lastElapsed = 0;
  var lastDuration = 0;
  var ignoreEndedUntil = 0;
  var loadedAt = 0;
  var seekedOnce = false;
  var emptyTimer = null;
  var ws = null;
  var pollTimer = null;
  var gen = 0;
  var pendingPlay = null;
  var pendingAdds = [];
  var me = "";
  var stations = [{ id: "", name: "Site-wide", kind: "site" }];
  var friends = [];
  var groups = [];
  var stationId = "";
  var titles = {};
  var localQueue = [];
  var nowTitle = "";
  var holdId = "";
  var holdUntil = 0;

  function personalOwner() {
    return stationId.indexOf("u:") === 0 ? stationId.slice(2) : null;
  }
  function vidOf(it) {
    if (!it) return "";
    if (typeof it === "string") return it;
    return (it.id && it.id.videoId) || it.video_id || it.videoId || "";
  }
  function ytUrl(id) {
    return "https://www.youtube.com/watch?v=" + id;
  }
  function saneStart(elapsed, duration) {
    elapsed = Number(elapsed);
    duration = Number(duration);
    if (!isFinite(elapsed) || elapsed < 0) return 0;
    if (elapsed > 3 * 3600) return 0;
    if (isFinite(duration) && duration > 1 && elapsed >= duration - 1) return 0;
    return elapsed;
  }
  function stationName(id) {
    for (var i = 0; i < stations.length; i++) if (stations[i].id === id) return stations[i].name;
    if (id.indexOf("u:") === 0) return id.slice(2);
    return id || "Site-wide";
  }

  function ensureYT(cb) {
    if (window.YT && YT.Player) {
      cb();
      return;
    }
    var prev = window.onYouTubeIframeAPIReady;
    window.onYouTubeIframeAPIReady = function () {
      if (typeof prev === "function") prev();
      cb();
    };
    if (!document.getElementById("yt-iframe-api")) {
      var s = document.createElement("script");
      s.id = "yt-iframe-api";
      s.src = "https://www.youtube.com/iframe_api";
      document.head.appendChild(s);
    }
  }

  function ensurePlayer(cb) {
    ensureYT(function () {
      if (ytPlayer) {
        cb();
        return;
      }
      ytPlayer = new YT.Player("jbtube-player", {
        width: "100%",
        height: "100%",
        playerVars: {
          autoplay: 1, rel: 0, modestbranding: 1, playsinline: 1,
          origin: location.origin, controls: 0, disablekb: 1, fs: 1, iv_load_policy: 3
        },
        events: {
          onReady: function () {
            applySound();
            cb();
          },
          onStateChange: function (e) {
            if (e.data === YT.PlayerState.PLAYING) {
              pauseBtn.textContent = "Pause";
              try {
                var d = ytPlayer.getDuration();
                if (d > 1) lastDuration = d;
                if (!seekedOnce && lastElapsed > 2) {
                  var tgt = saneStart(lastElapsed, lastDuration);
                  var cur = ytPlayer.getCurrentTime() || 0;
                  if (tgt > 2 && lastDuration > 1 && tgt < lastDuration - 1.5 && Math.abs(cur - tgt) > 2) {
                    seekedOnce = true;
                    ignoreEndedUntil = Date.now() + 2000;
                    ytPlayer.seekTo(tgt, true);
                  } else {
                    seekedOnce = true;
                  }
                }
              } catch (err) {}
            }
            if (e.data === YT.PlayerState.PAUSED) pauseBtn.textContent = "Play";
            if (e.data === YT.PlayerState.ENDED) reportEnded();
          }
        }
      });
    });
  }

  function reportEnded() {
    if (Date.now() < ignoreEndedUntil) return;
    var d = 0, t = 0;
    try {
      d = ytPlayer.getDuration() || 0;
      t = ytPlayer.getCurrentTime() || 0;
    } catch (e) {}
    if (d > 1) lastDuration = d;
    if (d < 1 || t < Math.max(2, d - 1.5) || Date.now() - loadedAt < 4000) return;
    var endedId = lastId;
    var owner = personalOwner();
    if (owner && endedId) {
      fetch("/u/" + encodeURIComponent(owner) + "/jukebox/complete", {
        method: "POST",
        credentials: "same-origin",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: "video_id=" + encodeURIComponent(endedId)
      });
      return;
    }
    if (ws && ws.readyState === 1) ws.send(JSON.stringify({ ended: true }));
  }

  function loadTrack(id, elapsed) {
    if (!id) return;
    lastDuration = 0;
    lastId = id;
    lastElapsed = saneStart(elapsed, 0);
    loadedAt = Date.now();
    seekedOnce = lastElapsed <= 2;
    ignoreEndedUntil = Date.now() + 5000;
    ensurePlayer(function () {
      try {
        ytPlayer.loadVideoById({ videoId: id, startSeconds: 0 });
        applySound();
      } catch (e) {}
    });
  }

  function rememberTitle(id, title) {
    if (!id || !title || title === id) return;
    titles[id] = title;
  }
  function labelOf(id) { return titles[id] || id; }
  function renderNow() {
    var t = nowTitle || (lastId ? labelOf(lastId) : "");
    nowEl.textContent = t || (stationName(stationId) + " — nothing playing.");
  }
  function renderQueue() {
    upcomingEl.textContent = "";
    if (!localQueue.length) return;
    var h = document.createElement("p");
    h.className = "jb-label";
    h.textContent = "Up next · " + localQueue.length;
    upcomingEl.appendChild(h);
    localQueue.forEach(function (it) {
      var row = document.createElement("div");
      row.className = "jb-q";
      row.textContent = it.title || labelOf(it.id);
      upcomingEl.appendChild(row);
    });
  }
  function fillTitle(id) {
    if (!id || (titles[id] && titles[id] !== id)) return;
    function apply(title) {
      if (!title) return;
      title = String(title).replace(/&#(\d+);/g, function (_, n) { return String.fromCharCode(+n); })
                           .replace(/&/g, "&").replace(/"/g, '"').replace(/&#39;/g, "'");
      rememberTitle(id, title);
      if (lastId === id) { nowTitle = title; renderNow(); }
      localQueue.forEach(function (it) {
        if (it.id === id) it.title = title;
      });
      renderQueue();
      var labs = resultsEl.querySelectorAll('.jb-hit[data-vid="' + id + '"] .jb-hit-t');
      for (var i = 0; i < labs.length; i++) labs[i].textContent = title;
    }
    fetch("https://www.youtube.com/oembed?format=json&url=" + encodeURIComponent(ytUrl(id)))
      .then(function (r) { return r.ok ? r.json() : null; })
      .then(function (data) {
        if (data && data.title) { apply(data.title); return; }
        return fetch("https://noembed.com/embed?url=" + encodeURIComponent(ytUrl(id)))
          .then(function (r) { return r.ok ? r.json() : null; })
          .then(function (d) { if (d && d.title) apply(d.title); });
      })
      .catch(function () {
        fetch("https://noembed.com/embed?url=" + encodeURIComponent(ytUrl(id)))
          .then(function (r) { return r.ok ? r.json() : null; })
          .then(function (d) { if (d && d.title) apply(d.title); })
          .catch(function () {});
      });
  }

  function applySnapshot(data) {
    if (!data) return;
    if (holdId && Date.now() < holdUntil) {
      if (!data.now || data.now.video_id !== holdId) return;
      holdId = "";
    }
    if (data.now && data.now.video_id) {
      if (emptyTimer) {
        clearTimeout(emptyTimer);
        emptyTimer = null;
      }
      var who = data.now.queued_by ? " — " + data.now.queued_by : "";
      if (data.now.title) rememberTitle(data.now.video_id, data.now.title);
      nowTitle = labelOf(data.now.video_id) || data.now.video_id;
      if (nowTitle && nowTitle !== data.now.video_id) nowEl.textContent = nowTitle + who;
      else nowEl.textContent = stationName(stationId) + who;
      fillTitle(data.now.video_id);
      for (var qi = 0; qi < localQueue.length; qi++) {
        if (localQueue[qi].id === data.now.video_id) {
          localQueue = localQueue.slice(qi + 1);
          break;
        }
      }
      var el = Number(data.now.elapsed) || 0;
      if (data.now.video_id !== lastId) {
        loadTrack(data.now.video_id, el);
      } else {
        lastElapsed = saneStart(el, lastDuration);
      }
    } else {
      nowEl.textContent = stationName(stationId) + " — nothing playing.";
      nowTitle = "";
      localQueue = [];
      if (emptyTimer) return;
      emptyTimer = setTimeout(function () {
        emptyTimer = null;
        lastId = null;
        lastElapsed = 0;
        if (ytPlayer) {
          try { ytPlayer.stopVideo(); } catch (e) {}
        }
      }, 1500);
    }
    if (Array.isArray(data.queue)) {
      localQueue = data.queue.map(function (x) {
        var qid = vidOf(x);
        rememberTitle(qid, x.title);
        return { id: qid, title: x.title || labelOf(qid) };
      }).filter(function (x) { return x.id && x.id !== lastId && x.id !== (data.now && data.now.video_id); });
      localQueue.forEach(function (it) { fillTitle(it.id); });
    }
    renderQueue();
  }

  function teardownWs() {
    if (ws) {
      try { ws.close(); } catch (e) {}
      ws = null;
    }
  }
  function teardownPoll() {
    if (pollTimer) {
      clearInterval(pollTimer);
      pollTimer = null;
    }
  }

  function connectWs() {
    teardownPoll();
    if (ws && (ws.readyState === 0 || ws.readyState === 1)) return;
    var proto = location.protocol === "https:" ? "wss:" : "ws:";
    var path = stationId ? "/c/" + encodeURIComponent(stationId) + "/jukebox/ws" : "/community/ws";
    ws = new WebSocket(proto + "//" + location.host + path);
    ws.onopen = function () {
      if (pendingPlay) {
        ws.send(JSON.stringify({ play: pendingPlay }));
        pendingPlay = null;
      }
      pendingAdds.forEach(function (u) { ws.send(JSON.stringify({ add: u })); });
      pendingAdds = [];
    };
    ws.onmessage = function (ev) {
      var data;
      try { data = JSON.parse(ev.data); } catch (e) { return; }
      applySnapshot(data);
    };
    ws.onclose = function () {
      ws = null;
      lgBeacon("ws-close", stationId || "site");
      if (personalOwner()) return;
      setTimeout(function () {
        if (!personalOwner() && !ws) connectWs();
      }, 2000);
    };
  }

  function pollPersonal() {
    var owner = personalOwner();
    if (!owner) return;
    fetch("/u/" + encodeURIComponent(owner) + "/jukebox/now", { credentials: "same-origin" })
      .then(function (r) { return r.ok ? r.json() : null; })
      .then(function (data) { if (data) applySnapshot(data); })
      .catch(function () {});
  }

  function isShared() { return stationId !== ("u:" + me); }
  function applySound() {
    if (!ytPlayer) return;
    try {
      ytPlayer.setVolume(Number(vol.value) || 0);
      if (muted) ytPlayer.mute(); else ytPlayer.unMute();
    } catch (e) {}
  }
  function applyChrome() {
    if (panel) panel.classList.toggle("jb-shared", isShared());
    if (blockEl) blockEl.hidden = !isShared();
    applySound();
  }

  function loadHistory() {
    var q = search.value.trim().toLowerCase();
    var my = ++gen;
    resultsEl.textContent = "Loading history…";
    fetch("/jukebox/history?station=" + encodeURIComponent(stationId), { credentials: "same-origin" })
      .then(function (r) { return r.ok ? r.json() : { items: [] }; })
      .then(function (data) {
        if (my !== gen) return;
        resultsEl.textContent = "";
        var items = data.items || [];
        var n = 0;
        items.forEach(function (it) {
          var id = vidOf(it);
          if (!id) return;
          var label = it.title || ((it.queued_by || "track") + " · " + id);
          if (q && label.toLowerCase().indexOf(q) === -1 && id.toLowerCase().indexOf(q) === -1) return;
          ytHit({ video_id: id, title: label });
          n++;
        });
        if (!n) resultsEl.textContent = q ? "No history matches." : "No history yet.";
      })
      .catch(function () {
        if (my === gen) resultsEl.textContent = "History failed.";
      });
  }

  function tune(id, opts) {
    opts = opts || {};
    if (id === stationId && !opts.force) {
      if (opts.open) setOpen(true);
      return;
    }
    stationId = id == null ? "" : id;
    lastId = null;
    lastElapsed = 0;
    seekedOnce = false;
    localQueue = [];
    nowTitle = "";
    teardownWs();
    teardownPoll();
    renderStations();
    if (opts.open) setOpen(true);
    applyChrome();
    if (personalOwner()) {
      pollPersonal();
      pollTimer = setInterval(pollPersonal, 4000);
    } else {
      connectWs();
    }
  }

  window.__lgTuneStation = function (id) {
    tune(id, { open: true, force: true });
  };

  function sendPlay(it) {
    var id = vidOf(it);
    if (!id || id.length < 8) return;
    var url = ytUrl(id);
    rememberTitle(id, (it && it.title) || (it && it.snippet && it.snippet.title));
    nowTitle = labelOf(id);
    localQueue = [];
    resultsEl.textContent = "";
    gen++;
    if (t) clearTimeout(t);
    try { search.blur(); } catch (e) {}
    holdId = id;
    holdUntil = Date.now() + 4000;
    lgBeacon("play", id);
    renderNow();
    renderQueue();
    fillTitle(id);
    loadTrack(id, 0);
    var owner = personalOwner();
    if (owner) {
      fetch("/u/" + encodeURIComponent(owner) + "/jukebox/add", {
        method: "POST",
        credentials: "same-origin",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: "url=" + encodeURIComponent(url)
      });
      return;
    }
    if (ws && ws.readyState === 1) ws.send(JSON.stringify({ play: url }));
    else {
      pendingPlay = url;
      connectWs();
    }
  }

  function sendNext(it) {
    var id = vidOf(it);
    if (!id || id.length < 8) return;
    var url = ytUrl(id);
    rememberTitle(id, (it && it.title) || (it && it.snippet && it.snippet.title));
    localQueue.push({ id: id, title: labelOf(id) });
    renderQueue();
    fillTitle(id);
    var owner = personalOwner();
    if (owner) {
      fetch("/u/" + encodeURIComponent(owner) + "/jukebox/add", {
        method: "POST",
        credentials: "same-origin",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: "url=" + encodeURIComponent(url)
      });
      return;
    }
    if (ws && ws.readyState === 1) ws.send(JSON.stringify({ add: url }));
    else {
      pendingAdds.push(url);
      connectWs();
    }
  }

  pauseBtn.addEventListener("click", function () {
    if (isShared()) return;
    if (!ytPlayer) return;
    try {
      var st = ytPlayer.getPlayerState();
      if (st === YT.PlayerState.PLAYING) ytPlayer.pauseVideo();
      else ytPlayer.playVideo();
    } catch (e) {}
  });
  skipBtn.addEventListener("click", function () {
    if (isShared()) return;
    ignoreEndedUntil = Date.now() + 1500;
    var owner = personalOwner();
    var id = lastId;
    lastId = null;
    lastElapsed = 0;
    if (owner && id) {
      fetch("/u/" + encodeURIComponent(owner) + "/jukebox/complete", {
        method: "POST",
        credentials: "same-origin",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: "video_id=" + encodeURIComponent(id) + "&skip=1"
      }).then(function () { pollPersonal(); }).catch(function () {});
      return;
    }
    if (ws && ws.readyState === 1) ws.send(JSON.stringify({ ended: true }));
  });
  var muted = localStorage.getItem("lg-jb-muted") === "1";
  vol.value = localStorage.getItem("lg-jb-vol") || "80";
  muteBtn.textContent = muted ? "Unmute" : "Mute";
  muteBtn.addEventListener("click", function () {
    muted = !muted;
    localStorage.setItem("lg-jb-muted", muted ? "1" : "0");
    applySound();
    muteBtn.textContent = muted ? "Unmute" : "Mute";
  });
  vol.addEventListener("input", function () {
    localStorage.setItem("lg-jb-vol", String(vol.value || "0"));
    if (muted && Number(vol.value) > 0) {
      muted = false;
      localStorage.setItem("lg-jb-muted", "0");
      muteBtn.textContent = "Mute";
    }
    applySound();
  });
  ccBtn.addEventListener("click", function () {
    if (!ytPlayer) return;
    try {
      var on = ccBtn.classList.toggle("on");
      if (on) { ytPlayer.loadModule("captions"); ytPlayer.setOption("captions", "track", { languageCode: "en" }); }
      else ytPlayer.unloadModule("captions");
    } catch (e) {}
  });
  fsBtn.addEventListener("click", function () {
    var el = document.getElementById("jbtube-wrap");
    if (!el) return;
    var req = el.requestFullscreen || el.webkitRequestFullscreen;
    if (req) req.call(el);
  });
  copyBtn.addEventListener("click", function () {
    if (!lastId) return;
    var u = ytUrl(lastId);
    if (navigator.clipboard && navigator.clipboard.writeText) navigator.clipboard.writeText(u);
    copyBtn.textContent = "Copied";
    setTimeout(function () { copyBtn.textContent = "Copy URL"; }, 1000);
  });

  function playChannel(ch) {
    setOpen(true);
    var my = ++gen;
    setTimeout(function () {
      if (my === gen) resultsEl.textContent = "Loading channel…";
    }, 0);
    fetch("/jukebox/channel?id=" + encodeURIComponent(ch), { credentials: "same-origin" })
      .then(function (r) { return r.ok ? r.json() : { items: [] }; })
      .then(function (data) {
        if (my !== gen) return;
        var items = data.items || [];
        if (!items.length) { resultsEl.textContent = data.error || "No videos on that channel."; return; }
        resultsEl.textContent = "";
        var head = document.createElement("p");
        head.className = "jb-label";
        head.textContent = "Channel videos";
        resultsEl.appendChild(head);
        var playAll = document.createElement("button");
        playAll.type = "button";
        playAll.textContent = "Play all";
        playAll.style.margin = "0 0 .4rem";
        playAll.addEventListener("click", function (e) {
          e.stopPropagation();
          sendPlay(items[0]);
          for (var k = 1; k < items.length; k++) sendNext(items[k]);
        });
        resultsEl.appendChild(playAll);
        items.forEach(function (it) {
          var id = vidOf(it);
          ytHit({ video_id: id, title: it.title || "Loading…" });
          fillTitle(id);
        });
      })
      .catch(function () {
        if (my === gen) resultsEl.textContent = "Channel failed.";
      });
  }
  function ytHit(it) {
    var ch = (it.id && it.id.channelId) || it.channel_id || "";
    var id = vidOf(it);
    var title = (it.snippet && it.snippet.title) || it.title || "untitled";
    title = title.replace(/&#(\d+);/g, function (_, n) { return String.fromCharCode(+n); })
                 .replace(/&/g, "&").replace(/"/g, '"').replace(/&#39;/g, "'");
    var th = it.snippet && it.snippet.thumbnails && (it.snippet.thumbnails.default || it.snippet.thumbnails.medium);
    var thumb = (th && th.url) || (id ? "https://img.youtube.com/vi/" + id + "/default.jpg" : "");
    if (!id && !ch) return;
    var row = document.createElement("div");
    row.className = "jb-hit";
    row.tabIndex = 0;
    if (id) row.setAttribute("data-vid", id);
    if (thumb) {
      var img = document.createElement("img");
      img.src = thumb;
      img.alt = "";
      row.appendChild(img);
    }
    var lab = document.createElement("span");
    lab.className = "jb-hit-t";
    lab.textContent = title;
    row.appendChild(lab);
    if (ch) {
      lab.textContent = title + " · channel";
      row.addEventListener("pointerdown", function (e) { e.stopPropagation(); });
      row.addEventListener("click", function (e) {
        e.preventDefault();
        e.stopPropagation();
        setOpen(true);
        playChannel(ch);
      });
      row.addEventListener("keydown", function (e) { if (e.key === "Enter") playChannel(ch); });
    } else {
      var nx = document.createElement("button");
      nx.type = "button";
      nx.className = "next";
      nx.textContent = "Next";
      nx.addEventListener("click", function (e) {
        e.stopPropagation();
        sendNext({ video_id: id, title: title });
      });
      row.appendChild(nx);
      row.addEventListener("click", function () { sendPlay({ video_id: id, title: title }); });
      row.addEventListener("keydown", function (e) {
        if (e.key === "Enter") sendPlay({ video_id: id, title: title });
      });
    }
    resultsEl.appendChild(row);
  }

  function personHit(p) {
    var name = p.name || p.id || "";
    if (!name) return;
    var row = document.createElement("div");
    row.className = "jb-hit jb-person";
    row.tabIndex = 0;
    var img = document.createElement("img");
    img.className = "port";
    img.src = "/portrait/" + encodeURIComponent(name);
    img.alt = "";
    row.appendChild(img);
    var lab = document.createElement("span");
    lab.textContent = name + (p.meta ? " · " + p.meta : "");
    row.appendChild(lab);
    row.addEventListener("click", function () {
      resultsEl.textContent = "";
      gen++;
      tune("u:" + name, { open: true });
    });
    resultsEl.appendChild(row);
  }

  function groupHit(g) {
    var row = document.createElement("div");
    row.className = "jb-hit";
    row.tabIndex = 0;
    var lab = document.createElement("span");
    lab.textContent = g.name || g.id;
    row.appendChild(lab);
    row.addEventListener("click", function () {
      resultsEl.textContent = "";
      gen++;
      tune(g.id, { open: true });
    });
    resultsEl.appendChild(row);
  }

  function filterLocal(list, q, key) {
    q = (q || "").toLowerCase();
    if (!q) return list.slice();
    return list.filter(function (x) {
      var s = (typeof x === "string" ? x : x[key] || x.name || x.id || "");
      return s.toLowerCase().indexOf(q) !== -1;
    });
  }

  function runSearch() {
    var q = search.value.trim();
    var scope = scopeEl.value;
    var my = ++gen;
    resultsEl.textContent = "";
    if (scope === "history") {
      loadHistory();
      return;
    }
    if (scope === "all" || scope === "self") {
      if (!q) {
        if (scope === "self") resultsEl.textContent = "Type to add to your Juketube.";
        return;
      }
      resultsEl.textContent = "Searching…";
      fetch("/jukebox/search?q=" + encodeURIComponent(q), { credentials: "same-origin" })
        .then(function (r) {
          if (r.status === 401) throw new Error("sign-in");
          if (!r.ok) throw new Error("http " + r.status);
          return r.json();
        })
        .then(function (data) {
          if (my !== gen) return;
          resultsEl.textContent = "";
          var items = data.items || [];
          if (!items.length) {
            resultsEl.textContent = data.error || "No matches.";
            lgBeacon("search-empty", q.slice(0, 40));
            return;
          }
          if (scope === "self" && me) tune("u:" + me);
          items.forEach(ytHit);
        })
        .catch(function (err) {
          if (my === gen) {
            resultsEl.textContent = (err && err.message === "sign-in") ? "Sign in to search." : "Search failed.";
            lgBeacon("search-fail", (err && err.message) || "fail");
          }
        });
      return;
    }
    if (scope === "friends") {
      var fl = filterLocal(friends, q);
      if (!fl.length) {
        resultsEl.textContent = q ? "No friends match." : "No friends yet.";
        return;
      }
      fl.forEach(function (n) { personHit({ name: n, meta: "friend" }); });
      return;
    }
    if (scope === "groups") {
      var gl = filterLocal(groups, q, "name");
      if (!gl.length) {
        resultsEl.textContent = q ? "No groups match." : "You are not in a group yet.";
        return;
      }
      gl.forEach(groupHit);
      return;
    }
    if (scope === "others") {
      if (q.length < 1) {
        resultsEl.textContent = "Type a name.";
        return;
      }
      resultsEl.textContent = "Searching…";
      fetch("/jukebox/people?scope=others&q=" + encodeURIComponent(q), { credentials: "same-origin" })
        .then(function (r) { return r.ok ? r.json() : { items: [] }; })
        .then(function (data) {
          if (my !== gen) return;
          resultsEl.textContent = "";
          var items = data.items || [];
          if (!items.length) {
            resultsEl.textContent = "No one matches.";
            return;
          }
          items.forEach(personHit);
        })
        .catch(function () {
          if (my === gen) resultsEl.textContent = "Search failed.";
        });
    }
  }

  var t = null;
  function scheduleSearch() {
    clearTimeout(t);
    t = setTimeout(runSearch, 180);
  }
  new MutationObserver(function () {
    panel.classList.toggle("has-results", !!resultsEl.firstChild);
  }).observe(resultsEl, { childList: true, subtree: true });
  search.addEventListener("input", scheduleSearch);
  search.addEventListener("search", runSearch);
  search.addEventListener("focus", function () {
    setTimeout(syncViewport, 50);
    setTimeout(syncViewport, 320);
  });
  search.addEventListener("blur", function () {
    setTimeout(syncViewport, 50);
    setTimeout(syncViewport, 320);
  });
  scopeEl.addEventListener("change", runSearch);

  function renderStations() {
    stationList.innerHTML = "";
    stations.forEach(function (st) {
      var b = document.createElement("button");
      b.type = "button";
      b.className = "jb-station" + (st.id === stationId ? " on" : "");
      b.textContent = st.name + (st.kind === "group" ? " · group" : st.kind === "person" ? " · person" : "");
      b.addEventListener("click", function (e) {
        e.stopPropagation();
        tune(st.id, { open: true });
        setStationsOpen(false);
      });
      stationList.appendChild(b);
    });
  }

  followBtn.addEventListener("click", function () {
    var name = followInput.value.trim();
    followErr.hidden = true;
    if (!name) return;
    fetch("/u/" + encodeURIComponent(name) + "/jukebox/subscribe", {
      method: "POST",
      credentials: "same-origin"
    })
      .then(function (r) {
        if (!r.ok) throw new Error("follow failed");
        return fetch("/jukebox/meta", { credentials: "same-origin" });
      })
      .then(function (r) { return r.ok ? r.json() : null; })
      .then(function (data) {
        if (data) applyMeta(data);
        followInput.value = "";
        tune("u:" + name, { open: true });
      })
      .catch(function () {
        followErr.textContent = "Could not follow that user.";
        followErr.hidden = false;
      });
  });

  function applyMeta(data) {
    me = data.me || "";
    stations = data.stations && data.stations.length ? data.stations : stations;
    friends = data.friends || [];
    groups = data.groups || [];
    renderStations();
  }

  fetch("/jukebox/meta", { credentials: "same-origin" })
    .then(function (r) { return r.ok ? r.json() : null; })
    .then(function (data) {
      if (data) applyMeta(data);
      applyChrome();
      if (!personalOwner()) connectWs();
    })
    .catch(function () {
      if (!personalOwner()) connectWs();
    });
})();
