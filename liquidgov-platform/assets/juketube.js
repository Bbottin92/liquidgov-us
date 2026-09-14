/* Juketube V1 — one site-wide radio. Tap a result = play now for everyone. */
(function () {
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
    '<div id="jbhint">Juketube — one radio. Search, tap a track, everyone here hears it.</div>' +
    '<div id="jbpanel" role="dialog" aria-label="Juketube">' +
    '<p class="jb-name">Juketube</p>' +
    '<div id="jbnow"><span class="muted">Connecting…</span></div>' +
    '<div id="jbplayer"></div>' +
    '<div class="jb-controls">' +
    '<button type="button" id="jbpause">Pause</button>' +
    '<input id="jbvol" type="range" min="0" max="100" value="80" aria-label="Volume">' +
    "</div>" +
    '<input id="jbsearch" type="search" placeholder="Search YouTube" autocomplete="off">' +
    '<div id="jbresults"></div>' +
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
  var vol = document.getElementById("jbvol");

  if (localStorage.getItem("lg-jb-hint") === "1") hint.style.display = "none";

  function setOpen(open) {
    panel.classList.toggle("open", open);
    panel.style.display = open ? "flex" : "none";
    toggle.setAttribute("aria-expanded", open ? "true" : "false");
    if (open) {
      hint.style.display = "none";
      localStorage.setItem("lg-jb-hint", "1");
      ensurePlayer(function () {});
      if (!ws || ws.readyState > 1) connect();
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
  document.addEventListener("click", function (e) {
    if (!panel.classList.contains("open")) return;
    if (panel.contains(e.target) || toggle.contains(e.target)) return;
    setOpen(false);
  });

  var ytPlayer = null;
  var ytReady = false;
  var lastId = null;
  var lastElapsed = 0;
  var lastDuration = 0;
  var ignoreEndedUntil = 0;
  var loadedAt = 0;
  var ws = null;
  var gen = 0;
  var pendingPlay = null;

  function saneStart(elapsed) {
    elapsed = Number(elapsed);
    if (!isFinite(elapsed) || elapsed < 0) return 0;
    if (elapsed > 3 * 3600) return 0;
    if (lastDuration > 1 && elapsed >= lastDuration - 0.5) return 0;
    return elapsed;
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
      ytPlayer = new YT.Player("jbplayer", {
        width: "100%",
        height: "100%",
        playerVars: { autoplay: 1, rel: 0, modestbranding: 1, playsinline: 1, origin: location.origin },
        events: {
          onReady: function () {
            ytReady = true;
            try {
              ytPlayer.setVolume(Number(vol.value) || 80);
            } catch (e) {}
            cb();
          },
          onStateChange: function (e) {
            if (e.data === YT.PlayerState.PLAYING) {
              pauseBtn.textContent = "Pause";
              try {
                var d = ytPlayer.getDuration();
                if (d > 1) lastDuration = d;
                var tgt = saneStart(lastElapsed);
                var cur = ytPlayer.getCurrentTime() || 0;
                if (tgt > 1 && lastDuration > 1 && tgt < lastDuration - 0.5 && Math.abs(cur - tgt) > 2) {
                  ytPlayer.seekTo(tgt, true);
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
    if (d < 1 || t < Math.max(1, d - 1.5) || Date.now() - loadedAt < 2500) return;
    if (ws && ws.readyState === 1) ws.send(JSON.stringify({ ended: true }));
  }

  function loadTrack(id, elapsed) {
    if (!id) return;
    lastDuration = 0;
    lastId = id;
    lastElapsed = saneStart(elapsed);
    loadedAt = Date.now();
    ignoreEndedUntil = Date.now() + 4000;
    ensurePlayer(function () {
      try {
        ytPlayer.loadVideoById({ videoId: id, startSeconds: lastElapsed && lastDuration > 1 ? lastElapsed : 0 });
      } catch (e) {}
    });
  }

  function applySnapshot(data) {
    if (!data) return;
    if (data.now && data.now.video_id) {
      var who = data.now.queued_by ? " — " + data.now.queued_by : "";
      nowEl.textContent = "Now playing" + who;
      var el = saneStart(data.now.elapsed);
      lastElapsed = el;
      if (data.now.video_id !== lastId) loadTrack(data.now.video_id, el);
    } else {
      nowEl.textContent = "Nothing playing — search to start.";
      lastId = null;
      lastElapsed = 0;
      if (ytPlayer) {
        try {
          ytPlayer.stopVideo();
        } catch (e) {}
      }
    }
    var n = data.upcoming || 0;
    upcomingEl.textContent = n ? n + " queued next" : "";
  }

  function connect() {
    if (ws && (ws.readyState === 0 || ws.readyState === 1)) return;
    var proto = location.protocol === "https:" ? "wss:" : "ws:";
    ws = new WebSocket(proto + "//" + location.host + "/community/ws");
    ws.onopen = function () {
      if (pendingPlay) {
        ws.send(JSON.stringify({ play: pendingPlay }));
        pendingPlay = null;
      }
    };
    ws.onmessage = function (ev) {
      var data;
      try {
        data = JSON.parse(ev.data);
      } catch (e) {
        return;
      }
      applySnapshot(data);
    };
    ws.onclose = function () {
      ws = null;
      setTimeout(connect, 1500);
    };
  }

  function ytUrl(id) {
    return "https://www.youtube.com/watch?v=" + id;
  }

  function sendPlay(id) {
    var url = ytUrl(id);
    loadTrack(id, 0);
    nowEl.textContent = "Starting…";
    if (ws && ws.readyState === 1) ws.send(JSON.stringify({ play: url }));
    else {
      pendingPlay = url;
      connect();
    }
  }

  function sendNext(id) {
    var url = ytUrl(id);
    if (ws && ws.readyState === 1) ws.send(JSON.stringify({ add: url }));
    else {
      connect();
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
  vol.addEventListener("input", function () {
    if (!ytPlayer) return;
    try {
      ytPlayer.setVolume(Number(vol.value) || 0);
    } catch (e) {}
  });

  var t = null;
  search.addEventListener("input", function () {
    var q = search.value.trim();
    clearTimeout(t);
    if (!q) {
      resultsEl.textContent = "";
      return;
    }
    resultsEl.textContent = "Searching…";
    var my = ++gen;
    t = setTimeout(function () {
      fetch("/jukebox/search?q=" + encodeURIComponent(q), { credentials: "same-origin" })
        .then(function (r) {
          if (!r.ok) throw new Error("http " + r.status);
          return r.json();
        })
        .then(function (data) {
          if (my !== gen) return;
          resultsEl.textContent = "";
          var items = data.items || [];
          if (!items.length) {
            resultsEl.textContent = "No matches.";
            return;
          }
          items.forEach(function (it) {
            var id = (it.id && it.id.videoId) || "";
            var title = (it.snippet && it.snippet.title) || "untitled";
            var th = it.snippet && it.snippet.thumbnails && (it.snippet.thumbnails.default || it.snippet.thumbnails.medium);
            var thumb = (th && th.url) || "";
            if (!id) return;
            var row = document.createElement("div");
            row.className = "jb-hit";
            row.tabIndex = 0;
            if (thumb) {
              var img = document.createElement("img");
              img.src = thumb;
              img.alt = "";
              row.appendChild(img);
            }
            var lab = document.createElement("span");
            lab.textContent = title;
            row.appendChild(lab);
            var nx = document.createElement("button");
            nx.type = "button";
            nx.className = "next";
            nx.textContent = "Next";
            nx.addEventListener("click", function (e) {
              e.stopPropagation();
              sendNext(id);
            });
            row.appendChild(nx);
            row.addEventListener("click", function () {
              sendPlay(id);
            });
            row.addEventListener("keydown", function (e) {
              if (e.key === "Enter") sendPlay(id);
            });
            resultsEl.appendChild(row);
          });
        })
        .catch(function () {
          if (my === gen) resultsEl.textContent = "Search failed.";
        });
    }, 280);
  });

  connect();
})();
