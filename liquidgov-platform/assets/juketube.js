/* Juketube — shared radio + stations/follow/scopes/history. IDs are unique
   so the profile-page embed cannot steal the player. */
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
    '<div id="jbhint">Juketube — search, stations, history. Tap a track and everyone on that station hears it.</div>' +
    '<div id="jbpanel" role="dialog" aria-label="Juketube">' +
    '<p class="jb-name">Juketube</p>' +
    '<div class="jb-tabs">' +
    '<button type="button" id="jbtab-radio" class="on">Radio</button>' +
    '<button type="button" id="jbtab-stations">Stations</button>' +
    "</div>" +
    '<div id="jbradio">' +
    '<div id="jbnow"><span class="muted">Connecting…</span></div>' +
    '<div id="jbtube-player"></div>' +
    '<div class="jb-controls">' +
    '<button type="button" id="jbpause">Pause</button>' +
    '<button type="button" id="jbskip">Skip</button>' +
    '<input id="jbvol" type="range" min="0" max="100" value="80" aria-label="Volume">' +
    "</div>" +
    '<div class="jb-bar">' +
    '<select id="jbscope" aria-label="Search in">' +
    '<option value="all">All</option>' +
    '<option value="groups">Groups</option>' +
    '<option value="friends">Friends</option>' +
    '<option value="others">Others</option>' +
    '<option value="self">Self</option>' +
    "</select>" +
    '<select id="jbhist" aria-label="History"><option value="">History</option></select>' +
    "</div>" +
    '<input id="jbsearch" type="search" placeholder="Search" autocomplete="off">' +
    '<div id="jbresults"></div>' +
    '<div id="jbupcoming"></div>' +
    "</div>" +
    '<div id="jbstations" hidden>' +
    '<p class="muted jb-label">Tap to tune in</p>' +
    '<div id="jbstationlist"></div>' +
    '<div class="jb-follow">' +
    '<input id="jbfollow" placeholder="Follow a username" autocomplete="off">' +
    '<button type="button" id="jbfollowbtn">Follow</button>' +
    "</div>" +
    '<p class="muted" id="jbfollowerr" hidden></p>' +
    "</div>" +
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
  var vol = document.getElementById("jbvol");
  var scopeEl = document.getElementById("jbscope");
  var histEl = document.getElementById("jbhist");
  var tabRadio = document.getElementById("jbtab-radio");
  var tabStations = document.getElementById("jbtab-stations");
  var radioUi = document.getElementById("jbradio");
  var stationsUi = document.getElementById("jbstations");
  var stationList = document.getElementById("jbstationlist");
  var followInput = document.getElementById("jbfollow");
  var followBtn = document.getElementById("jbfollowbtn");
  var followErr = document.getElementById("jbfollowerr");

  if (localStorage.getItem("lg-jb-hint") === "1") hint.style.display = "none";

  function setOpen(open) {
    panel.classList.toggle("open", open);
    panel.style.display = open ? "flex" : "none";
    toggle.setAttribute("aria-expanded", open ? "true" : "false");
    if (open) {
      hint.style.display = "none";
      localStorage.setItem("lg-jb-hint", "1");
      ensurePlayer(function () {});
      if (stationId.indexOf("u:") !== 0 && (!ws || ws.readyState > 1)) connectWs();
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

  function showTab(which) {
    var radio = which === "radio";
    radioUi.hidden = !radio;
    stationsUi.hidden = radio;
    tabRadio.classList.toggle("on", radio);
    tabStations.classList.toggle("on", !radio);
  }
  tabRadio.addEventListener("click", function () { showTab("radio"); });
  tabStations.addEventListener("click", function () { showTab("stations"); });

  var ytPlayer = null;
  var lastId = null;
  var lastElapsed = 0;
  var lastDuration = 0;
  var ignoreEndedUntil = 0;
  var loadedAt = 0;
  var ws = null;
  var pollTimer = null;
  var gen = 0;
  var pendingPlay = null;
  var me = "";
  var stations = [{ id: "", name: "Site-wide", kind: "site" }];
  var friends = [];
  var groups = [];
  var stationId = "";

  function personalOwner() {
    return stationId.indexOf("u:") === 0 ? stationId.slice(2) : null;
  }
  function ytUrl(id) {
    return "https://www.youtube.com/watch?v=" + id;
  }
  function saneStart(elapsed) {
    elapsed = Number(elapsed);
    if (!isFinite(elapsed) || elapsed < 0) return 0;
    if (elapsed > 3 * 3600) return 0;
    if (lastDuration > 1 && elapsed >= lastDuration - 0.5) return 0;
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
        playerVars: { autoplay: 1, rel: 0, modestbranding: 1, playsinline: 1, origin: location.origin },
        events: {
          onReady: function () {
            try { ytPlayer.setVolume(Number(vol.value) || 80); } catch (e) {}
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
    var owner = personalOwner();
    if (owner && lastId) {
      fetch("/u/" + encodeURIComponent(owner) + "/jukebox/complete", {
        method: "POST",
        credentials: "same-origin",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: "video_id=" + encodeURIComponent(lastId)
      });
      return;
    }
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
      nowEl.textContent = stationName(stationId) + who;
      var el = saneStart(data.now.elapsed || 0);
      lastElapsed = el;
      if (data.now.video_id !== lastId) loadTrack(data.now.video_id, el);
    } else {
      nowEl.textContent = stationName(stationId) + " — nothing playing.";
      lastId = null;
      lastElapsed = 0;
      if (ytPlayer) {
        try { ytPlayer.stopVideo(); } catch (e) {}
      }
    }
    var n = data.upcoming || 0;
    upcomingEl.textContent = n ? n + " queued next" : "";
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
    };
    ws.onmessage = function (ev) {
      var data;
      try { data = JSON.parse(ev.data); } catch (e) { return; }
      applySnapshot(data);
    };
    ws.onclose = function () {
      ws = null;
      if (personalOwner()) return;
      setTimeout(function () {
        if (!personalOwner()) connectWs();
      }, 1500);
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

  function loadHistory() {
    histEl.innerHTML = '<option value="">History</option>';
    var owner = personalOwner() || me;
    if (!owner) return;
    fetch("/u/" + encodeURIComponent(owner) + "/jukebox/history", { credentials: "same-origin" })
      .then(function (r) { return r.ok ? r.json() : { items: [] }; })
      .then(function (data) {
        var items = data.items || [];
        histEl.innerHTML = '<option value="">History</option>';
        items.forEach(function (it) {
          if (!it.video_id) return;
          var o = document.createElement("option");
          o.value = it.video_id;
          o.textContent = (it.queued_by || "track") + " · " + it.video_id;
          histEl.appendChild(o);
        });
      })
      .catch(function () {});
  }

  function tune(id, opts) {
    opts = opts || {};
    if (id === stationId && !opts.force) {
      if (opts.open) setOpen(true);
      return;
    }
    stationId = id == null ? "" : id;
    lastId = null;
    teardownWs();
    teardownPoll();
    renderStations();
    loadHistory();
    if (opts.open) setOpen(true);
    showTab("radio");
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

  function sendPlay(id) {
    var url = ytUrl(id);
    loadTrack(id, 0);
    nowEl.textContent = "Starting…";
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

  function sendNext(id) {
    var url = ytUrl(id);
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
    else connectWs();
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
    ignoreEndedUntil = 0;
    loadedAt = 0;
    reportEnded();
  });
  vol.addEventListener("input", function () {
    if (!ytPlayer) return;
    try { ytPlayer.setVolume(Number(vol.value) || 0); } catch (e) {}
  });
  histEl.addEventListener("change", function () {
    if (histEl.value) sendPlay(histEl.value);
    histEl.selectedIndex = 0;
  });

  function ytHit(it) {
    var id = (it.id && it.id.videoId) || it.video_id || "";
    var title = (it.snippet && it.snippet.title) || it.title || "untitled";
    var th = it.snippet && it.snippet.thumbnails && (it.snippet.thumbnails.default || it.snippet.thumbnails.medium);
    var thumb = (th && th.url) || (id ? "https://img.youtube.com/vi/" + id + "/default.jpg" : "");
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
    row.addEventListener("click", function () { sendPlay(id); });
    row.addEventListener("keydown", function (e) {
      if (e.key === "Enter") sendPlay(id);
    });
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
    if (scope === "all" || scope === "self") {
      if (!q) {
        if (scope === "self") {
          resultsEl.textContent = "Type to add to your Juketube.";
        }
        return;
      }
      resultsEl.textContent = "Searching…";
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
          if (scope === "self" && me) tune("u:" + me);
          items.forEach(ytHit);
        })
        .catch(function () {
          if (my === gen) resultsEl.textContent = "Search failed.";
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
            resultsEl.textContent = "No match.";
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
  search.addEventListener("input", function () {
    clearTimeout(t);
    t = setTimeout(runSearch, 180);
  });
  scopeEl.addEventListener("change", function () {
    search.placeholder =
      scopeEl.value === "all" || scopeEl.value === "self" ? "Search YouTube" :
      scopeEl.value === "groups" ? "Search groups" :
      "Search people";
    runSearch();
  });

  function renderStations() {
    stationList.textContent = "";
    stations.forEach(function (s) {
      var row = document.createElement("button");
      row.type = "button";
      row.className = "jb-station" + (s.id === stationId ? " on" : "");
      row.textContent = s.name;
      row.addEventListener("click", function () { tune(s.id, { open: true }); });
      stationList.appendChild(row);
    });
  }

  function followSomeone() {
    var name = followInput.value.trim();
    followErr.hidden = true;
    if (!name) return;
    fetch("/u/" + encodeURIComponent(name) + "/jukebox/subscribe", {
      method: "POST",
      credentials: "same-origin"
    }).then(function (r) {
      if (!r.ok) throw new Error("no");
      return fetchMeta();
    }).then(function () {
      tune("u:" + name, { open: true });
      followInput.value = "";
    }).catch(function () {
      followErr.hidden = false;
      followErr.textContent = "Could not follow " + name + ".";
    });
  }
  followBtn.addEventListener("click", followSomeone);
  followInput.addEventListener("keydown", function (e) {
    if (e.key === "Enter") {
      e.preventDefault();
      followSomeone();
    }
  });

  function fetchMeta() {
    return fetch("/jukebox/meta", { credentials: "same-origin" })
      .then(function (r) { return r.ok ? r.json() : {}; })
      .then(function (data) {
        me = data.me || "";
        stations = data.stations && data.stations.length ? data.stations : [{ id: "", name: "Site-wide", kind: "site" }];
        friends = data.friends || [];
        groups = data.groups || [];
        renderStations();
      })
      .catch(function () { renderStations(); });
  }

  fetchMeta().then(function () {
    var m = location.pathname.match(/^\/u\/([A-Za-z0-9_]+)\/?$/);
    if (m) tune("u:" + m[1], { open: true });
    else tune("", { force: true });
  });
})();
