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
  // A page inside the persistent shell's frame (see the shell section at the end) must not start a second player.
  // The server already leaves Juketube out of framed pages (Sec-Fetch-Dest: iframe); this covers browsers that do
  // not send that header (Safari before 16.4).
  try {
    if (window.top !== window && window.top.document.documentElement.classList.contains("lg-shell")) return;
  } catch (e) {} // err-ok: a cross-origin parent is not our shell; the widget behaves as before

  var root = document.getElementById("juketube-root");
  if (!root) {
    root = document.createElement("div");
    root.id = "juketube-root";
    document.body.appendChild(root);
  }

  var ICON = '<svg viewBox="0 0 32 32"><path fill="#1c1610" stroke="#c4a574" stroke-width="1.5" d="M6 12c0-6 4-9 10-9s10 3 10 9v14c0 1.5-2 2.5-10 2.5S6 27.5 6 26z"/><rect x="9" y="6.5" width="14" height="4" rx="1" fill="#c4a574"/><circle cx="16" cy="19.5" r="6" fill="#0a0806" stroke="#c4a574"/><circle cx="16" cy="19.5" r="1.8" fill="#c4a574"/></svg>';

  root.innerHTML =
    '<button id="jbtoggle" type="button" aria-label="Juketube" aria-expanded="false">' + ICON + "</button>" +
    '<div id="jbhint" class="lg-tip"><span>Juketube — search, stations, history. Tap a track and everyone on that station hears it.</span>' +
    '<button type="button" id="jbhintclose" class="lg-tip-close" aria-label="Hide this tip">×</button></div>' +
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
  // Brandon 2026-11-08: "it should keep playing when the widget is closed". Moving the wrapper between the panel and
  // root re-inserted the YouTube iframe, and a browser reloads an iframe it re-inserts, so every open/close restarted
  // the player. Now the wrapper moves to root once, here, before any player exists, and is only positioned after
  // that: over the panel's slot while open, off-screen (still laid out, so it keeps playing) while closed.
  root.appendChild(wrapEl);
  var dockTimer = null;
  function clearDockBox() {
    wrapEl.style.left = wrapEl.style.top = wrapEl.style.width = wrapEl.style.height = "";
  }
  function trackSlot() {
    var r = wrapSlot.getBoundingClientRect();
    var p = panel.getBoundingClientRect();
    var outside = r.top < p.top - 1 || r.bottom > p.bottom + 1 || r.left < p.left - 1 || r.right > p.right + 1;
    // the slot is hidden (keyboard up) or pushed past the panel's clipped edge (long search results): park, so the
    // player never floats over the page or the results the panel used to clip
    if (r.width < 2 || r.height < 2 || outside) {
      wrapEl.classList.remove("jb-docked");
      wrapEl.classList.add("jb-parked");
      clearDockBox();
      return;
    }
    wrapEl.classList.remove("jb-parked");
    wrapEl.classList.add("jb-docked");
    wrapEl.style.left = r.left + "px";
    wrapEl.style.top = r.top + "px";
    wrapEl.style.width = r.width + "px";
    wrapEl.style.height = r.height + "px";
  }
  function parkPlayer() {
    if (dockTimer) { clearInterval(dockTimer); dockTimer = null; }
    wrapEl.classList.remove("jb-docked");
    wrapEl.classList.add("jb-parked");
    clearDockBox();
  }
  function dockPlayer() {
    trackSlot();
    // the slot moves when search results or the queue change the panel's layout; follow it while the panel is open
    if (!dockTimer) dockTimer = setInterval(trackSlot, 250);
  }
  function placePlayer() {
    if (panel.classList.contains("open") && !panel.classList.contains("kb")) dockPlayer();
    else parkPlayer();
  }

  // The tip used to stay on every page until Juketube was opened, covering controls (Post on /feed) on phones
  // (2026-11-07 iPhone sweep). Now: a real close button, gone after 8 s, shown on at most 3 page views, never
  // inside a framed page. The mechanism and its seen-flag ("lg-jb-hint") are tip.js's (window.LG_TIP), shared with the
  // walker's Drop/Scroll note.
  var jbTip = window.LG_TIP ? LG_TIP.wire(hint, document.getElementById("jbhintclose"), "jb", { maxViews: 3, hideAfterMs: 8000 }) : null;
  function hideHint(forGood) {
    if (jbTip) jbTip.hide(forGood);
    else hint.style.display = "none";
  }
  if (!jbTip) hideHint(false);

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
              // A personal jukebox is its owner's own list: skip it there. On a shared station this is only a vote:
              // the server skips once at least 25% of connected listeners' players have failed (Brandon 2026-11-08).
              if (personalOwner()) {
                nowEl.textContent = "That video cannot play here. Skipping.";
                reportEnded({ force: true });
              } else {
                nowEl.textContent = "That video cannot play here. It is skipped if it fails for a quarter of the listeners.";
                sendUnplayable(lastId);
              }
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
    // Name the track: the server ignores an "ended" for anything but the track it is playing.
    if (ws && ws.readyState === 1) ws.send(JSON.stringify({ ended: endedId }));
    else { pendingEnded = true; connectWs(); }
  }

  var pendingUnplayable = "";
  function sendUnplayable(id) {
    if (!id) return;
    lgBeacon("unplayable", id);
    if (ws && ws.readyState === 1) ws.send(JSON.stringify({ unplayable: id }));
    else { pendingUnplayable = id; connectWs(); }
  }

  // Juketube B (Brandon 2026-11-08): ask before telling the station. ok() runs unless YouTube says the owner blocks
  // other sites or the video is gone; an unknown answer (network, YouTube down) lets it through as before.
  function checkEmbeddable(id, ok, no) {
    fetch("/jukebox/oembed?id=" + encodeURIComponent(id), { credentials: "same-origin" })
      .then(function (r) { return r.ok ? r.json() : null; })
      .then(function (j) {
        if (j && j.title) rememberTitle(id, j.title);
        if (j && j.embeddable === false) no(j);
        else ok();
      })
      .catch(function () { ok(); }); // err-ok: an unknown answer must not block a pick; the station's vote still protects the room
  }
  function refusePick(id, j) {
    lgBeacon("not-embeddable", id);
    localQueue = localQueue.filter(function (x) { return x.id !== id; });
    renderQueue();
    if (holdId === id) holdId = "";
    if (lastId === id) {
      lastId = null;
      lastElapsed = 0;
      if (ytPlayer) { try { ytPlayer.stopVideo(); } catch (e) {} }
    }
    nowEl.textContent = (j && j.missing)
      ? "That video is not available on YouTube anymore. Pick another."
      : "That video's owner lets it play only on YouTube, not on LiquidGov. Pick another.";
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
      // Only when something is loaded: with lastId already cleared this used to call stopVideo every 1.5 s for as long
      // as the station stayed empty (seen in the 2026-11-08 repro).
      if (lastId) {
        emptyTimer = setTimeout(function () {
          emptyTimer = null;
          lastId = null;
          lastElapsed = 0;
          if (ytPlayer) {
            try { ytPlayer.stopVideo(); } catch (e) {}
          }
        }, 1500);
      }
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
        ws.send(JSON.stringify({ ended: lastId }));
      }
      if (pendingUnplayable) {
        ws.send(JSON.stringify({ unplayable: pendingUnplayable }));
        pendingUnplayable = "";
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
      // started here, inside the tap, so iPhone counts it as a user gesture; the station is only told after the check
      loadTrack(id, 0);
    }
    checkEmbeddable(id, function () {
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
    }, function (j) { refusePick(id, j); });
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
    checkEmbeddable(id, function () {
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
    }, function (j) { refusePick(id, j); });
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
    if (ws && ws.readyState === 1 && id) ws.send(JSON.stringify({ ended: id }));
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

  // ---- Persistent shell: music keeps playing between pages (Brandon 2026-11-08) ----
  // A full page load destroys the YouTube player. So while it is playing, a click on a site link keeps this
  // document (its header and this widget) and loads the next page into a full-screen frame below the header.
  // Framed pages render in the server's embed mode (no header, footer or Juketube: the same mode the /map field
  // uses), and links and forms inside them simply navigate the frame. The address bar and title follow the frame
  // with replaceState; each shell-started page is its own history entry, and in-frame clicks are frame entries,
  // so Back works. Anything that must not run in a frame (sign-in, logout, theme, /listen's own player, the /world
  // game, app install pages, other sites) does a normal full load, as does every page while nothing is playing.
  // /map stays in the shell (its own field frame simply nests; checked in a browser 2026-11-08).
  // On any failure the shell falls back to a normal page load, so a broken shell can stop the music but never
  // strand you on a blank page; each fallback is reported with a /log beacon.
  // Rejected: swapping page content by script (inline page scripts declare ~378 top-level let/const names that
  // would collide on a second page), and reload-and-resume (a gap on every page, and iPhone needs a tap to resume).
  var FULL_LOAD = ["/login", "/logout", "/signup", "/forgot", "/reset", "/theme", "/listen", "/world",
    "/install", "/apps"];
  function fullLoadPath(p) {
    // whole path segments only: "/login" matches "/login" and "/login/google", never "/loginx" or "/logs"
    for (var i = 0; i < FULL_LOAD.length; i++) {
      var f = FULL_LOAD[i];
      if (p === f || p.indexOf(f + "/") === 0) return true;
    }
    return false;
  }
  // Night mode while music plays (Brandon 2026-11-08: "just stops when i switch to/from night mode"). /theme/<mode>
  // is a full-load path because the server answers it with a cookie and a redirect, and a reload stops the music.
  // So while the shell is up or something is playing, the toggle is done here instead: the same cookie the server
  // sets (http_server.cpp, GET /theme/dark|light: lg_theme, Path=/, SameSite=Lax, a year, Secure on https), then
  // data-theme on this page and the framed page, then every toggle link flips to point the other way (its glyph as
  // page_wrap draws it: a sun while dark, a moon while light). Nested frames (the /map field) pick it up on their
  // next load. Nothing playing: the link loads normally, as before.
  var THEME_PATH = /^\/theme\/(dark|light)$/;
  function themeInPlace(url) {
    var m = THEME_PATH.exec(url.pathname);
    if (!m || url.origin !== location.origin) return false;
    var mode = m[1], next = mode === "dark" ? "light" : "dark";
    document.cookie = "lg_theme=" + mode + "; Path=/; SameSite=Lax; Max-Age=31536000" +
      (location.protocol === "https:" ? "; Secure" : "");
    var docs = [document];
    try {
      if (shellFrame && shellFrame.contentDocument) docs.push(shellFrame.contentDocument);
    } catch (e) {} // err-ok: an unreadable frame is another origin; it is replaced by a normal load anyway
    docs.forEach(function (d) {
      d.documentElement.setAttribute("data-theme", mode);
      Array.prototype.forEach.call(d.querySelectorAll('a[href="/theme/dark"],a[href="/theme/light"]'), function (a) {
        a.setAttribute("href", "/theme/" + next);
        a.textContent = mode === "dark" ? "☀" : "☽";
      });
    });
    lgBeacon("shell-theme", mode);
    return true;
  }
  var shellFrame = null;
  var shellHomeTitle = "";
  var shellPending = "";
  function playing() {
    if (!playerReady || !ytPlayer) return false;
    try {
      var st = ytPlayer.getPlayerState();
      return st === YT.PlayerState.PLAYING || st === YT.PlayerState.BUFFERING;
    } catch (e) { return false; } // err-ok: an unreadable player counts as not playing, so links load normally
  }
  // Juketube ripples (Brandon 2026-11-15: "the juketube should make ripples too when its playing and not muted"): while
  // a track is playing and the sound is on, rings pulse off the Juketube button, stronger at a higher volume. YouTube's
  // player does not let a page read its sound, so the rings follow the volume slider, not the music itself. The same
  // look as the sound ripples off your drop (walker.js, .lg-ripple). Not with reduced motion.
  var JB_RIPPLE = { everyMs: 640, ms: 1100, reach: 1.25, spread: 1.0 };
  function jbRipple() {
    if (!toggle || !toggle.animate || document.hidden) return;
    if (window.matchMedia && matchMedia("(prefers-reduced-motion: reduce)").matches) return;
    var on = false;
    try { on = playerReady && ytPlayer && ytPlayer.getPlayerState() === YT.PlayerState.PLAYING; } catch (e) { on = false; } // err-ok: an unreadable player makes no ripple
    var level = Math.max(0, Math.min(100, Number(vol.value) || 0)) / 100;
    if (!on || muted || level === 0) return;
    var b = toggle.getBoundingClientRect();
    if (!b.width) return;
    var r = document.createElement("span");
    r.className = "lg-ripple lg-ripple-jb";
    r.setAttribute("aria-hidden", "true");
    r.style.cssText = "position:fixed;z-index:9;left:" + (b.left + b.width / 2) + "px;top:" + (b.top + b.height / 2) + "px;width:" +
      b.width + "px;height:" + b.height + "px";
    document.body.appendChild(r);
    var a = r.animate([
      { transform: "translate(-50%,-50%) scale(1)", opacity: Math.min(0.85, 0.2 + level * 0.6), borderWidth: (1 + level * 2.5).toFixed(1) + "px" },
      { transform: "translate(-50%,-50%) scale(" + (JB_RIPPLE.reach + level * JB_RIPPLE.spread).toFixed(2) + ")", opacity: 0, borderWidth: "0.5px" }
    ], { duration: JB_RIPPLE.ms, easing: "cubic-bezier(.2,.6,.35,1)" });
    a.onfinish = function () { r.remove(); };
  }
  setInterval(jbRipple, JB_RIPPLE.everyMs);
  function shellLeave(href, why) {
    lgBeacon("shell-exit", why);
    location.href = href;
  }
  function cleanHref(href) {
    var u = new URL(href);
    u.searchParams.delete("embed");
    return u.href;
  }
  function sizeShell() {
    if (!shellFrame) return;
    var head = document.querySelector("header.top");
    var top = head ? Math.max(0, Math.round(head.getBoundingClientRect().bottom)) : 0;
    document.documentElement.style.setProperty("--lg-shell-top", top + "px");
  }
  function showShellFrame(on) {
    document.documentElement.classList.toggle("lg-shell-home", !on);
    if (on) sizeShell();
    else document.title = shellHomeTitle;
  }
  function startShell() {
    shellHomeTitle = document.title;
    // the current page becomes the "home" entry, so Back from the first framed page returns to it
    history.replaceState({ lgShellHome: 1 }, "", location.href);
    shellFrame = document.createElement("iframe");
    shellFrame.id = "lg-shell-frame";
    shellFrame.title = "Page";
    shellFrame.addEventListener("load", onShellLoad);
    document.documentElement.classList.add("lg-shell");
    document.body.appendChild(shellFrame);
    var head = document.querySelector("header.top");
    if (head && window.ResizeObserver) new ResizeObserver(sizeShell).observe(head);
    window.addEventListener("resize", sizeShell);
    window.addEventListener("popstate", onShellPop);
  }
  function shellGo(href) {
    if (!shellFrame) startShell();
    history.pushState({ lgShell: 1 }, "", href);
    showShellFrame(true);
    loadInFrame(href);
  }
  function loadInFrame(href) {
    shellPending = href;
    var w = shellFrame.contentWindow, same = false;
    try { same = !!(w && w.location.href && w.location.href !== "about:blank"); } catch (e) {} // err-ok: not ours yet; set src instead
    // replace, not a new frame entry: shellGo already pushed this page's history entry
    if (same) w.location.replace(href);
    else shellFrame.src = href;
  }
  function onShellPop(e) {
    try {
      if (e.state && e.state.lgShell) {
        showShellFrame(true);
        var cur = "";
        try { cur = cleanHref(shellFrame.contentWindow.location.href); } catch (err) {} // err-ok: reloaded below
        if (cur !== location.href) loadInFrame(location.href);
        else document.title = shellFrame.contentDocument.title || shellHomeTitle; // no load event will set it
      } else {
        showShellFrame(false);
      }
    } catch (err) {
      shellLeave(location.href, "pop " + err);
    }
  }
  function onShellLoad() {
    var w = shellFrame.contentWindow, href;
    try {
      href = w.location.href;
    } catch (e) {
      // another origin or the browser's own error page (no network): load the page we were going to normally
      shellLeave(shellPending || location.href, "frame not readable");
      return;
    }
    try {
      if (href === "about:blank") {
        if (shellPending) shellLeave(shellPending, "frame blank");
        return;
      }
      shellPending = "";
      var url = new URL(href);
      if (url.origin !== location.origin || fullLoadPath(url.pathname)) {
        // e.g. a form whose redirect lands on /login because the session ended
        shellLeave(href, "landed on " + url.pathname);
        return;
      }
      var doc = w.document;
      // a browser that sends no Sec-Fetch-Dest gets the full page: hide its header and footer, as embed mode does
      if (doc.querySelector("header.top")) {
        var fit = doc.createElement("style");
        fit.textContent = "header.top,footer.site{display:none!important}main.wrap{padding-top:.75rem}";
        doc.head.appendChild(fit);
      }
      document.title = doc.title || shellHomeTitle;
      var st = history.state && history.state.lgShell ? history.state : { lgShell: 1 };
      history.replaceState(st, "", cleanHref(href));
      showShellFrame(true);
      doc.addEventListener("click", onFrameClick);
      doc.addEventListener("submit", onFrameSubmit, true);
      doc.addEventListener("pointerdown", function () {
        if (panel.classList.contains("open")) setOpen(false);
      });
    } catch (e) {
      shellLeave(href, "load " + e);
    }
  }
  function linkTarget(e, base) {
    if (e.defaultPrevented || e.button !== 0 || e.metaKey || e.ctrlKey || e.shiftKey || e.altKey) return null;
    var a = e.target && e.target.closest && e.target.closest("a[href]");
    if (!a || a.hasAttribute("download")) return null;
    var t = (a.getAttribute("target") || "").toLowerCase();
    if (t && t !== "_self" && t !== "_top" && t !== "_parent") return null; // _blank and named windows: untouched
    return { a: a, t: t, url: new URL(a.getAttribute("href"), base) };
  }
  function onFrameClick(e) {
    try {
      var hit = linkTarget(e, e.target.ownerDocument.baseURI);
      if (!hit) return;
      if (themeInPlace(hit.url)) {
        e.preventDefault();
        return;
      }
      if (hit.url.origin !== location.origin || fullLoadPath(hit.url.pathname)) {
        e.preventDefault();
        shellLeave(hit.url.href, "frame link " + (hit.url.origin !== location.origin ? "other site" : hit.url.pathname));
        return;
      }
      if (hit.t === "_top" || hit.t === "_parent") {
        e.preventDefault();
        shellGo(hit.url.href);
        return;
      }
      shellPending = hit.url.href; // the frame navigates itself
    } catch (err) {
      lgBeacon("shell-error", "frame click " + err); // err-ok: the browser's own navigation still happens
    }
  }
  function onFrameSubmit(e) {
    try {
      var f = e.target;
      var action = new URL(f.getAttribute("action") || "", f.ownerDocument.baseURI);
      if (action.origin !== location.origin || fullLoadPath(action.pathname)) {
        f.target = "_top"; // sign-in and the like leave the shell, as they would without it
        lgBeacon("shell-exit", "form " + action.pathname);
      } else {
        shellPending = action.href;
      }
    } catch (err) {
      lgBeacon("shell-error", "frame submit " + err); // err-ok: the form still submits inside the frame
    }
  }
  document.addEventListener("click", function (e) {
    try {
      if (!shellFrame && !playing()) return; // nothing playing: every page loads exactly as before
      var hit = linkTarget(e, location.href);
      if (!hit || root.contains(hit.a)) return;
      var u = hit.url;
      if (themeInPlace(u)) {
        e.preventDefault();
        return;
      }
      if (u.origin !== location.origin || fullLoadPath(u.pathname)) return;
      if (u.pathname === location.pathname && u.search === location.search && u.hash) return; // in-page anchor
      e.preventDefault();
      shellGo(u.href);
    } catch (err) {
      lgBeacon("shell-error", "click " + err); // err-ok: no preventDefault ran, so the link loads normally
    }
  });
})();
