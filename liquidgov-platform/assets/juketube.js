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

  var ICON = '<svg viewBox="0 0 32 32"><path fill="#1c1610" stroke="#c4a574" stroke-width="1.5" d="M6 12c0-6 4-9 10-9s10 3 10 9v14c0 1.5-2 2.5-10 2.5S6 27.5 6 26z"/><rect x="9" y="6.5" width="14" height="4" rx="1" fill="#c4a574"/><circle cx="16" cy="19.5" r="6" fill="#0a0806" stroke="#c4a574"/><circle cx="16" cy="19.5" r="1.8" fill="#c4a574"/></svg>';

  root.innerHTML =
    '<button id="jbtoggle" type="button" aria-label="Juketube" aria-expanded="false">' + ICON + "</button>" +
    '<div id="jbhint"><span>Juketube — search, stations, history. Tap a track and everyone on that station hears it.</span>' +
    '<button type="button" id="jbhintclose" aria-label="Hide this tip">×</button></div>' +
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
    '<button type="button" id="jbclear" aria-label="Clear search">×</button>' +
    '<div id="jbresults"></div>' +
    "</div>" +
    '<div id="jbnow"><span class="muted">Connecting…</span></div>' +
    '<div id="jbtube-wrap"><div id="jbtube-player"></div><div id="jbtube-block" hidden></div></div>' +
    '<div class="jb-controls">' +
    '<button type="button" id="jbpause">Play</button>' +
    '<button type="button" id="jbskip">Skip</button>' +
    '<button type="button" id="jbmute">Mute</button>' +
    '<button type="button" id="jbcc">CC</button>' +
    '<button type="button" id="jbfs">Full</button>' +
    '<button type="button" id="jbcopy">Copy URL</button>' +
    '<input id="jbvol" type="range" min="0" max="100" value="80" aria-label="Volume">' +
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
  var clearBtn = document.getElementById("jbclear");
  function syncClear() {
    if (!clearBtn) return;
    clearBtn.style.display = search.value ? "flex" : "none";
  }
  if (clearBtn) {
    clearBtn.addEventListener("pointerdown", function (e) {
      e.preventDefault();
      e.stopPropagation();
      search.value = "";
      resultsEl.textContent = "";
      panel.classList.remove("has-results", "kb");
      gen++;
      syncClear();
      search.focus();
    });
  }
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
  var wrapEl = document.getElementById("jbtube-wrap");
  var wrapSlot = document.createElement("div");
  wrapSlot.id = "jbtube-slot";
  wrapEl.parentNode.insertBefore(wrapSlot, wrapEl);

  // The panel is display:none while it is closed. A YouTube iframe inside
  // that panel never reaches ENDED, so a shared station sits on one track.
  // Park the player off-screen, still laid out, for as long as the panel
  // is closed or the keyboard has covered it.
  function parkPlayer() {
    wrapEl.classList.add("jb-parked");
    if (wrapEl.parentNode !== root) root.appendChild(wrapEl);
  }
  function dockPlayer() {
    wrapEl.classList.remove("jb-parked");
    if (wrapEl.parentNode !== wrapSlot) wrapSlot.appendChild(wrapEl);
  }
  function placePlayer() {
    if (panel.classList.contains("open") && !panel.classList.contains("kb")) dockPlayer();
    else parkPlayer();
  }

  // The tip used to stay on every page until Juketube was opened, covering controls (Post on /feed) on phones
  // (2026-11-07 iPhone sweep). Now: a real close button, gone after 8 s, shown on at most 3 page views, never
  // inside a framed page; localStorage is guarded (private mode throws, which used to stop the whole widget).
  function hideHint(forGood) {
    hint.style.display = "none";
    if (forGood) { try { localStorage.setItem("lg-jb-hint", "1"); } catch (e) {} } // err-ok: private mode; hidden for this page anyway
  }
  (function () {
    var framed = true, seen = "1", views = 0;
    try { framed = window.top !== window; } catch (e) {} // err-ok: cross-origin parent: treat as framed (no tip)
    try { seen = localStorage.getItem("lg-jb-hint"); views = +(localStorage.getItem("lg-jb-hint-views") || 0); } catch (e) {} // err-ok: private mode: seen stays "1", no tip
    if (framed || seen === "1") { hideHint(false); return; }
    try { localStorage.setItem("lg-jb-hint-views", String(views + 1)); } catch (e) {} // err-ok: private mode: the tip just keeps its 8 s per page
    if (views + 1 >= 3) { try { localStorage.setItem("lg-jb-hint", "1"); } catch (e) {} } // err-ok: as above
    setTimeout(function () { hideHint(false); }, 8000);
  })();
  document.getElementById("jbhintclose").addEventListener("click", function (e) {
    e.preventDefault();
    e.stopPropagation(); // the tip itself opens Juketube; the x must not
    hideHint(true);
  });

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
    placePlayer();
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
    placePlayer();
    if (open) {
      hideHint(true);
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
  // ytPlayer goes non-null the instant `new YT.Player(...)` returns, but
  // its real methods (loadVideoById etc.) aren't safe to call until the
  // async onReady event fires. A second ensurePlayer() call landing in
  // that gap (very plausible right on page load: setOpen's own
  // ensurePlayer call and the WS's first snapshot -> loadTrack ->
  // ensurePlayer call can both fire within milliseconds of each other)
  // used to see ytPlayer truthy and call cb() immediately, invoking
  // loadVideoById on a not-yet-ready player -- silently swallowed by
  // loadTrack's own try/catch, leaving the iframe stuck on its empty
  // no-video shell forever (reproduced live: a second account's track
  // pick left the first account's player on a blank gray box, embed src
  // with no video id, zero further postMessage traffic). playerReady/
  // pendingPlayerCbs queue behind onReady instead of racing it -- same
  // fix the old inline widget once needed; this file's rewrite had
  // dropped it.
  var playerReady = false;
  var pendingPlayerCbs = [];
  var lastId = null;
  var lastElapsed = 0;
  var lastDuration = 0;
  var ignoreEndedUntil = 0;
  var loadedAt = 0;
  var seekedOnce = false;
  var endedSentFor = "";
  var retriedFor = "";
  var pendingEnded = false;
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
  var stationListeners = 0;

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
    if (isFinite(duration) && duration > 1 && elapsed >= duration - 1) return elapsed;
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
    if (playerReady) {
      cb();
      return;
    }
    pendingPlayerCbs.push(cb);
    if (ytPlayer) return; // construction already in flight -- onReady below drains the queue
    ensureYT(function () {
      if (ytPlayer) return; // guards a second ensureYT completion racing this one
      ytPlayer = new YT.Player("jbtube-player", {
        width: "100%",
        height: "100%",
        playerVars: {
          autoplay: 1, rel: 0, modestbranding: 1, playsinline: 1,
          origin: location.origin, controls: 0, disablekb: 1, fs: 1, iv_load_policy: 3
        },
        events: {
          onReady: function () {
            playerReady = true;
            applySound();
            var cbs = pendingPlayerCbs;
            pendingPlayerCbs = [];
            cbs.forEach(function (fn) { fn(); });
          },
          onStateChange: function (e) {
            if (e.data === YT.PlayerState.PLAYING) {
              pauseBtn.textContent = "Pause";
              applySound();
              try {
                var d = ytPlayer.getDuration();
                if (d > 1) lastDuration = d;
                if (d > 1 && lastElapsed >= d - 1) {
                  seekedOnce = true;
                  reportEnded({ force: true });
                } else if (!seekedOnce && lastElapsed > 2) {
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
          },
          onError: function (e) {
            var code = e && e.data;
            lgBeacon("yt-error", code == null ? "" : String(code));
            if (code === 5 && retriedFor !== lastId) {
              retriedFor = lastId;
              try { ytPlayer.loadVideoById({ videoId: lastId, startSeconds: 0 }); } catch (err) {}
              return;
            }
            if (code === 2 || code === 5 || code === 100 || code === 101 || code === 150) {
              nowEl.textContent = "That video cannot play here. Skipping.";
              reportEnded({ force: true });
              return;
            }
            if (code === 153) nowEl.textContent = "The player was blocked. Tap Play.";
          }
        }
      });
    });
  }

  function reportEnded(opts) {
    opts = opts || {};
    if (!lastId || endedSentFor === lastId) return;
    var d = 0, t = 0;
    try {
      d = ytPlayer.getDuration() || 0;
      t = ytPlayer.getCurrentTime() || 0;
    } catch (e) {}
    if (d > 1) lastDuration = d;
    var atEnd = d > 1 && t >= d - 1.5;
    if (!opts.force && !atEnd) {
      if (Date.now() < ignoreEndedUntil) return;
      if (d < 1 || t < Math.max(2, d - 1.5) || Date.now() - loadedAt < 4000) return;
    }
    var endedId = lastId;
    var owner = personalOwner();
    endedSentFor = lastId;
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
    else { pendingEnded = true; connectWs(); }
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
    if (typeof data.listeners === "number") stationListeners = data.listeners;
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
      if (pendingEnded && endedSentFor === lastId) {
        pendingEnded = false;
        ws.send(JSON.stringify({ ended: true }));
      }
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
    if (!ytPlayer || typeof ytPlayer.setVolume !== "function") return;
    var level = Math.max(0, Math.min(100, Math.round(Number(vol.value) || 0)));
    try {
      // unMute puts back the volume from the last mute. Set the level after
      // it, or a slider move is thrown away and the sound never changes.
      if (muted || level === 0) {
        ytPlayer.setVolume(level);
        ytPlayer.mute();
        return;
      }
      ytPlayer.unMute();
      ytPlayer.setVolume(level);
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
    // Bug fix: a personal station ("u:<name>") has its own durable history keyed by that
    // username (/u/<name>/jukebox/complete writes it) -- querying it by `station=` instead
    // of `for=` hit a bucket that route never writes to and came back empty. A real shared
    // station (not "u:"-prefixed, "" for site-wide) has no owner at all, so `station=` is
    // the only query that means anything for it.
    var owner = personalOwner();
    var q_param = owner ? ("for=" + encodeURIComponent(owner)) : ("station=" + encodeURIComponent(stationId));
    fetch("/jukebox/history?" + q_param, { credentials: "same-origin" })
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
    resultsEl.textContent = "";
    gen++;
    if (t) clearTimeout(t);
    try { search.blur(); } catch (e) {}
    lgBeacon("play", id);
    var owner = personalOwner();
    var queueOnly = !owner && stationListeners > 1 && lastId && lastId !== id;
    if (queueOnly) {
      localQueue.push({ id: id, title: labelOf(id) });
      renderQueue();
      fillTitle(id);
    } else {
      nowTitle = labelOf(id);
      localQueue = [];
      holdId = id;
      holdUntil = Date.now() + 4000;
      renderNow();
      renderQueue();
      fillTitle(id);
      loadTrack(id, 0);
    }
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
      if (q.length < 3) {
        resultsEl.textContent = "Type at least 3 letters.";
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
            if (!data.error) lgBeacon("search-empty", q.slice(0, 40));
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
    t = setTimeout(runSearch, 450);
  }
  new MutationObserver(function () {
    panel.classList.toggle("has-results", !!resultsEl.firstChild);
  }).observe(resultsEl, { childList: true, subtree: true });
  search.addEventListener("input", function () { syncClear(); scheduleSearch(); });
  syncClear();
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
