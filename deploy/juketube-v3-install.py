#!/usr/bin/env python3
"""Juketube: history, stations stay open, channels, shared chrome, persist mute, autoplay, history radio."""
from pathlib import Path
import shutil
from datetime import datetime, timezone

ROOT = Path("/opt/liquidgov")
stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def backup(p: Path):
    if p.exists():
        b = Path(str(p) + ".bak-v3-" + stamp)
        shutil.copy2(p, b)
        print("backup", b)


def must_replace(s, old, new, name, n=1):
    c = s.count(old)
    print(name, c)
    if c != n:
        print("STOP", name)
        raise SystemExit(1)
    return s.replace(old, new)


# ---------- JS / CSS (served from disk; takes effect after hard refresh) ----------
js_path = ROOT / "assets/juketube.js"
css_path = ROOT / "assets/juketube.css"
if not js_path.exists():
    print("STOP no", js_path)
    raise SystemExit(1)

backup(js_path)
backup(css_path)
js = js_path.read_text()
css = css_path.read_text() if css_path.exists() else ""

if "lg-jb-vol" not in js:
    js = must_replace(
        js,
        """    '<div id="jbtube-wrap"><div id="jbtube-player"></div></div>' +
    '<div class="jb-controls">' +
    '<button type="button" id="jbpause">Pause</button>' +
    '<button type="button" id="jbskip">Skip</button>' +
    '<button type="button" id="jbmute">Mute</button>' +
    '<input id="jbvol" type="range" min="0" max="100" value="80" aria-label="Volume">' +
    "</div>" +""",
        """    '<div id="jbtube-wrap"><div id="jbtube-player"></div><div id="jbtube-block" hidden></div></div>' +
    '<div class="jb-controls">' +
    '<button type="button" id="jbpause">Pause</button>' +
    '<button type="button" id="jbskip">Skip</button>' +
    '<button type="button" id="jbmute">Mute</button>' +
    '<input id="jbvol" type="range" min="0" max="100" value="80" aria-label="Volume">' +
    '<button type="button" id="jbcc">CC</button>' +
    '<button type="button" id="jbfs">Full</button>' +
    '<button type="button" id="jbcopy">Copy URL</button>' +
    '<button type="button" id="jbqual">Quality</button>' +
    "</div>" +""",
        "controls_html",
    )

    js = must_replace(
        js,
        """  var muteBtn = document.getElementById("jbmute");
  var vol = document.getElementById("jbvol");""",
        """  var muteBtn = document.getElementById("jbmute");
  var vol = document.getElementById("jbvol");
  var ccBtn = document.getElementById("jbcc");
  var fsBtn = document.getElementById("jbfs");
  var copyBtn = document.getElementById("jbcopy");
  var qualBtn = document.getElementById("jbqual");
  var blockEl = document.getElementById("jbtube-block");""",
        "control_vars",
    )

    js = must_replace(
        js,
        """      hint.style.display = "none";
      localStorage.setItem("lg-jb-hint", "1");
      ensurePlayer(function () {});
      if (stationId.indexOf("u:") !== 0 && (!ws || ws.readyState > 1)) connectWs();
      syncViewport();""",
        """      hint.style.display = "none";
      localStorage.setItem("lg-jb-hint", "1");
      ensurePlayer(function () {
        applySound();
        try { if (lastId) ytPlayer.playVideo(); } catch (e) {}
      });
      if (stationId.indexOf("u:") !== 0 && (!ws || ws.readyState > 1)) connectWs();
      syncViewport();""",
        "setopen_play",
    )

    js = must_replace(
        js,
        """        playerVars: { autoplay: 1, rel: 0, modestbranding: 1, playsinline: 1, origin: location.origin },
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
          }""",
        """        playerVars: { autoplay: 1, rel: 0, modestbranding: 1, playsinline: 1, origin: location.origin, controls: 0, disablekb: 1, fs: 1, iv_load_policy: 3 },
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
                var tgt = saneStart(lastElapsed);
                var cur = ytPlayer.getCurrentTime() || 0;
                if (tgt > 1 && lastDuration > 1 && tgt < lastDuration - 0.5 && Math.abs(cur - tgt) > 2) {
                  ytPlayer.seekTo(tgt, true);
                }
              } catch (err) {}
            }
            if (e.data === YT.PlayerState.PAUSED) {
              pauseBtn.textContent = "Play";
              if (isShared()) { try { ytPlayer.playVideo(); } catch (err) {} }
            }
            if (e.data === YT.PlayerState.ENDED) reportEnded();
          }""",
        "player_vars",
    )

    js = must_replace(
        js,
        """    if (opts.open) setOpen(true);
    setStationsOpen(false);
    if (personalOwner()) {""",
        """    if (opts.open) setOpen(true);
    applyChrome();
    if (personalOwner()) {""",
        "tune_keep_stations",
    )

    js = must_replace(
        js,
        """      b.addEventListener("click", function () { tune(st.id); });""",
        """      b.addEventListener("click", function (e) { e.stopPropagation(); tune(st.id, { open: true }); });""",
        "station_click",
    )

    js = must_replace(
        js,
        """  function loadHistory() {
    var owner = personalOwner() || me;
    if (!owner) {
      resultsEl.textContent = "No history yet.";
      return;
    }
    var q = search.value.trim().toLowerCase();
    var my = ++gen;
    resultsEl.textContent = "Loading history…";
    fetch("/u/" + encodeURIComponent(owner) + "/jukebox/history", { credentials: "same-origin" })""",
        """  function isShared() { return stationId !== ("u:" + me); }
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
    fetch("/jukebox/history?station=" + encodeURIComponent(stationId), { credentials: "same-origin" })""",
        "history_fn",
    )

    js = must_replace(
        js,
        """          var label = (it.queued_by || "track") + " · " + it.video_id;
          if (q && label.toLowerCase().indexOf(q) === -1 && it.video_id.toLowerCase().indexOf(q) === -1) return;
          ytHit({ video_id: it.video_id, title: label });""",
        """          var label = it.title || ((it.queued_by || "track") + " · " + it.video_id);
          if (q && label.toLowerCase().indexOf(q) === -1 && it.video_id.toLowerCase().indexOf(q) === -1) return;
          ytHit({ video_id: it.video_id, title: label });""",
        "history_label",
    )

    js = must_replace(
        js,
        """  pauseBtn.addEventListener("click", function () {
    if (!ytPlayer) return;""",
        """  pauseBtn.addEventListener("click", function () {
    if (isShared()) return;
    if (!ytPlayer) return;""",
        "pause_guard",
    )

    js = must_replace(
        js,
        """  skipBtn.addEventListener("click", function () {
    ignoreEndedUntil = Date.now() + 1500;""",
        """  skipBtn.addEventListener("click", function () {
    if (isShared()) return;
    ignoreEndedUntil = Date.now() + 1500;""",
        "skip_guard",
    )

    js = must_replace(
        js,
        """  var muted = false;
  muteBtn.addEventListener("click", function () {
    if (!ytPlayer) return;
    muted = !muted;
    try {
      if (muted) ytPlayer.mute();
      else ytPlayer.unMute();
    } catch (e) {}
    muteBtn.textContent = muted ? "Unmute" : "Mute";
  });
  vol.addEventListener("input", function () {
    if (!ytPlayer) return;
    try {
      ytPlayer.setVolume(Number(vol.value) || 0);
      if (muted && Number(vol.value) > 0) {
        muted = false;
        ytPlayer.unMute();
        muteBtn.textContent = "Mute";
      }
    } catch (e) {}
  });""",
        """  var muted = localStorage.getItem("lg-jb-muted") === "1";
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
  var qLevels = ["small", "medium", "large", "hd720", "hd1080", "highres"];
  var qIdx = 2;
  qualBtn.addEventListener("click", function () {
    if (!ytPlayer) return;
    try {
      var avail = ytPlayer.getAvailableQualityLevels && ytPlayer.getAvailableQualityLevels();
      var list = (avail && avail.length) ? avail : qLevels;
      qIdx = (qIdx + 1) % list.length;
      ytPlayer.setPlaybackQuality(list[qIdx]);
      qualBtn.textContent = list[qIdx];
    } catch (e) {}
  });""",
        "sound_persist",
    )

    js = must_replace(
        js,
        """  function ytHit(it) {
    var id = (it.id && it.id.videoId) || it.video_id || "";
    var title = (it.snippet && it.snippet.title) || it.title || "untitled";
    var th = it.snippet && it.snippet.thumbnails && (it.snippet.thumbnails.default || it.snippet.thumbnails.medium);
    var thumb = (th && th.url) || (id ? "https://img.youtube.com/vi/" + id + "/default.jpg" : "");
    if (!id) return;""",
        """  function playChannel(ch) {
    resultsEl.textContent = "Loading channel…";
    fetch("/jukebox/channel?id=" + encodeURIComponent(ch), { credentials: "same-origin" })
      .then(function (r) { return r.ok ? r.json() : { items: [] }; })
      .then(function (data) {
        var items = data.items || [];
        if (!items.length) { resultsEl.textContent = data.error || "No videos on that channel."; return; }
        sendPlay(items[0]);
        for (var i = 1; i < items.length; i++) sendNext(items[i]);
      })
      .catch(function () { resultsEl.textContent = "Channel failed."; });
  }
  function ytHit(it) {
    var ch = (it.id && it.id.channelId) || it.channel_id || "";
    var id = (it.id && it.id.videoId) || it.video_id || "";
    var title = (it.snippet && it.snippet.title) || it.title || "untitled";
    var th = it.snippet && it.snippet.thumbnails && (it.snippet.thumbnails.default || it.snippet.thumbnails.medium);
    var thumb = (th && th.url) || (id ? "https://img.youtube.com/vi/" + id + "/default.jpg" : "");
    if (!id && !ch) return;""",
        "yt_hit_channel",
    )

    js = must_replace(
        js,
        """    nx.addEventListener("click", function (e) {
      e.stopPropagation();
      sendNext(id);
    });
    row.appendChild(nx);
    row.addEventListener("click", function () { sendPlay(id); });
    row.addEventListener("keydown", function (e) {
      if (e.key === "Enter") sendPlay(id);
    });""",
        """    if (ch) {
      lab.textContent = title + " · channel";
      row.addEventListener("click", function () { playChannel(ch); });
      row.addEventListener("keydown", function (e) { if (e.key === "Enter") playChannel(ch); });
    } else {
    nx.addEventListener("click", function (e) {
      e.stopPropagation();
      sendNext(id);
    });
    row.appendChild(nx);
    row.addEventListener("click", function () { sendPlay(id); });
    row.addEventListener("keydown", function (e) {
      if (e.key === "Enter") sendPlay(id);
    });
    }""",
        "yt_hit_click",
    )

    js = must_replace(
        js,
        """      if (data) applyMeta(data);
      if (!personalOwner()) connectWs();""",
        """      if (data) applyMeta(data);
      applyChrome();
      if (!personalOwner()) connectWs();""",
        "meta_chrome",
    )

    js_path.write_text(js)
    print("js_patched", js_path.stat().st_size)
else:
    print("js_already 1")

if css_path.exists() and "#jbtube-block" not in css:
    css += """
#jbtube-block{position:absolute;inset:0;z-index:2;background:transparent}
#jbpanel.jb-shared #jbpause,#jbpanel.jb-shared #jbskip{display:none !important}
.jb-controls button.on{background:var(--accent);color:var(--paper);border-color:var(--accent)}
.jb-controls{flex-wrap:wrap}
.jb-controls button{font-size:.72rem;padding:.25rem .45rem;min-height:32px}
"""
    css_path.write_text(css)
    print("css_patched")
else:
    print("css_ok")


# ---------- C++ store + routes ----------
hpp = ROOT / "libs/liquidgov_file_store/include/liquidgov/platform/file_store.hpp"
cpp = ROOT / "libs/liquidgov_file_store/src/file_store.cpp"
http = ROOT / "src/http_server.cpp"
for p in (hpp, cpp, http):
    if not p.exists():
        print("STOP missing", p)
        raise SystemExit(1)
    backup(p)

h = hpp.read_text()
if "record_jukebox_play" not in h:
    h = must_replace(
        h,
        "    std::vector<JukeboxHistoryEntry> jukebox_history_of(const std::string& owner) const;",
        "    void record_jukebox_play(const std::string& owner, const std::string& video_id, const std::string& queued_by);\n"
        "    std::vector<JukeboxHistoryEntry> jukebox_history_of(const std::string& owner) const;",
        "hpp_record",
    )
    hpp.write_text(h)
else:
    print("hpp_already 1")

c = cpp.read_text()
if "FileStore::record_jukebox_play" not in c:
    c = must_replace(
        c,
        """    for (const auto& [owner, list] : jukebox_history_) {
        if (!by_username_.count(owner)) continue;""",
        """    for (const auto& [owner, list] : jukebox_history_) {
        if (!by_username_.count(owner) && owner.rfind("jbs:", 0) != 0) continue;""",
        "flush_jbs",
    )
    c = must_replace(
        c,
        "std::vector<JukeboxHistoryEntry> FileStore::jukebox_history_of(const std::string& owner) const {",
        """void FileStore::record_jukebox_play(const std::string& owner, const std::string& video_id,
                                    const std::string& queued_by) {
    if (owner.empty() || !valid_youtube_id(video_id)) return;
    auto& hist = jukebox_history_[owner];
    hist.push_back(JukeboxHistoryEntry{owner, video_id, queued_by, now()});
    if (hist.size() > kJukeboxHistoryCap) {
        hist.erase(hist.begin(), hist.begin() + static_cast<std::ptrdiff_t>(hist.size() - kJukeboxHistoryCap));
    }
    flush();
}

std::vector<JukeboxHistoryEntry> FileStore::jukebox_history_of(const std::string& owner) const {""",
        "cpp_record",
    )
    cpp.write_text(c)
else:
    print("cpp_already 1")

s = http.read_text()

if "type=video,channel" not in s:
    old = '?part=snippet&type=video&videoEmbeddable=true&maxResults=8&order=relevance'
    new = '?part=snippet&type=video,channel&maxResults=8&order=relevance'
    if s.count(old) != 1:
        print("search_type_skip", s.count("type=video"))
    else:
        s = s.replace(old, new, 1)
        print("search_type 1")
else:
    print("search_type_already 1")

if 'path=="/jukebox/history"' not in s:
    needle = '    if(method=="GET"&&path=="/jukebox/search") {'
    insert = r'''    if(method=="GET"&&path=="/jukebox/history") {
        if(!acc) return http_raw("401 Unauthorized", "application/json", "{\"items\":[]}");
        auto jesc = [](const std::string& in) {
            std::string o = "\"";
            for (unsigned char c : in) {
                if (c=='"' || c=='\\') { o += '\\'; o += char(c); }
                else if (c>=32) o += char(c);
            }
            o += "\"";
            return o;
        };
        std::string st = qs["station"];
        std::string key = st.rfind("u:", 0)==0 ? st.substr(2) : std::string("jbs:")+st;
        auto hist = g_store.jukebox_history_of(key);
        std::reverse(hist.begin(), hist.end());
        std::ostringstream os;
        os << "{\"items\":[";
        bool first = true;
        std::size_t shown = 0;
        for (const auto& h : hist) {
            if (shown++ >= 20) break;
            if (!first) os << ",";
            first = false;
            os << "{\"video_id\":" << jesc(h.video_id)
               << ",\"queued_by\":" << jesc(h.queued_by)
               << ",\"played_at\":" << h.played_at << "}";
        }
        os << "]}";
        return http_raw("200 OK", "application/json", os.str(), "Cache-Control: no-store\r\n");
    }
    if(method=="GET"&&path=="/jukebox/channel") {
        if(!acc) return http_raw("401 Unauthorized", "application/json", "{\"items\":[]}");
        auto cid = trim_ws(qs["id"]);
        const char* key = youtube_api_key_from_env();
        if (cid.size() < 10 || !key[0])
            return http_raw("200 OK", "application/json", "{\"items\":[]}", "Cache-Control: no-store\r\n");
        for (char ch : cid) {
            if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch=='_' || ch=='-'))
                return http_raw("200 OK", "application/json", "{\"items\":[]}", "Cache-Control: no-store\r\n");
        }
        std::string uploads = cid;
        if (uploads.size() >= 2 && uploads[0]=='U' && uploads[1]=='C') uploads[1] = 'U';
        std::string url = "https://www.googleapis.com/youtube/v3/playlistItems?part=contentDetails&maxResults=8&playlistId="
            + url_encode(uploads) + "&key=" + url_encode(key);
        std::string cmd = "curl -sS --max-time 8 '" + url + "'";
        std::string resp;
        if (FILE* p = popen(cmd.c_str(), "r")) {
            char buf[4096];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), p)) > 0) resp.append(buf, n);
            pclose(p);
        }
        std::string out = "{\"items\":[";
        bool first = true;
        std::size_t pos = 0, nvid = 0;
        while (nvid < 8) {
            auto at = resp.find("\"videoId\"", pos);
            if (at == std::string::npos) break;
            auto q1 = resp.find('"', at + 9);
            if (q1 == std::string::npos) break;
            auto q2 = resp.find('"', q1 + 1);
            if (q2 == std::string::npos) break;
            std::string vid = resp.substr(q1 + 1, q2 - q1 - 1);
            pos = q2 + 1;
            if (vid.size() != 11) continue;
            if (!first) out += ",";
            first = false;
            out += "\"" + vid + "\"";
            nvid++;
        }
        out += "]}";
        return http_raw("200 OK", "application/json", out, "Cache-Control: no-store\r\n");
    }
    if(method=="GET"&&path=="/jukebox/search") {'''
    n = s.count(needle)
    print("hist_route_needle", n)
    if n != 1:
        print("STOP hist_route")
        raise SystemExit(1)
    s = s.replace(needle, insert, 1)
    print("ok_hist_route 1")
else:
    print("hist_route_already 1")

ENDED = '''                } else if (s.find("\\"ended\\"") != std::string::npos) {
                    std::lock_guard<std::mutex> lock(g_community_mu);
                    if (g_community_now_by_station[station].active) {
                        community_advance_locked(station, std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count());
                    }
                }'''
ENDED_NEW = r'''                } else if (s.find("\"ended\"") != std::string::npos) {
                    std::string finished_id, finished_by;
                    bool need_fill = false;
                    std::int64_t now_s = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    {
                        std::lock_guard<std::mutex> lock(g_community_mu);
                        auto& np = g_community_now_by_station[station];
                        if (np.active) { finished_id = np.video_id; finished_by = np.queued_by; }
                        if (np.active) community_advance_locked(station, now_s);
                        need_fill = !g_community_now_by_station[station].active;
                    }
                    if (!finished_id.empty()) {
                        std::string hkey = std::string("jbs:") + station;
                        std::string pick_id;
                        {
                            std::lock_guard<std::mutex> lock(g_mu);
                            g_store.record_jukebox_play(hkey, finished_id, finished_by);
                            if (need_fill) {
                                auto hist = g_store.jukebox_history_of(hkey);
                                if (!hist.empty()) {
                                    std::size_t i = static_cast<std::size_t>(now_s) % hist.size();
                                    if (hist.size() > 1 && hist[i].video_id == finished_id) i = (i + 1) % hist.size();
                                    pick_id = hist[i].video_id;
                                }
                            }
                        }
                        if (!pick_id.empty()) {
                            std::lock_guard<std::mutex> lock(g_community_mu);
                            if (!g_community_now_by_station[station].active) {
                                g_community_now_by_station[station] = CommunityNowPlaying{pick_id, std::string("history"), now_s, true};
                            }
                        }
                    }
                }'''
if "record_jukebox_play(hkey" in s:
    print("ended_already 1")
else:
    # python source uses real quotes; match live file
    live_old = """                } else if (s.find(\"\\\"ended\\\"\") != std::string::npos) {
                    std::lock_guard<std::mutex> lock(g_community_mu);
                    if (g_community_now_by_station[station].active) {
                        community_advance_locked(station, std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count());
                    }
                }"""
    n = s.count('s.find("\\"ended\\"")') + s.count("s.find(\"\\\"ended\\\"\")")
    # simpler: unique snippet
    n2 = s.count("} else if (s.find(\"\\\"ended\\\"\") != std::string::npos) {")
    n3 = s.count('} else if (s.find("\\"ended\\"") != std::string::npos) {')
    print("ended_markers", n2, n3)
    target = None
    old1 = '                } else if (s.find("\\"ended\\"") != std::string::npos) {\n                    std::lock_guard<std::mutex> lock(g_community_mu);\n                    if (g_community_now_by_station[station].active) {\n                        community_advance_locked(station, std::chrono::duration_cast<std::chrono::seconds>(\n                            std::chrono::system_clock::now().time_since_epoch()).count());\n                    }\n                }'
    if s.count(old1) == 1:
        s = s.replace(old1, ENDED_NEW, 1)
        print("ok_ended 1")
    else:
        print("SKIP ended — paste grep -n ended src/http_server.cpp | head")

NOW_OLD = '''        auto tracks = g_store.jukebox_of(who);
        std::ostringstream os;
        os << "{";
        if (!tracks.empty()) {'''
NOW_NEW = '''        auto tracks = g_store.jukebox_of(who);
        if (tracks.empty()) {
            auto hist = g_store.jukebox_history_of(who);
            if (!hist.empty()) {
                auto pick = hist[static_cast<std::size_t>(std::time(nullptr)) % hist.size()];
                g_store.add_jukebox_track(who, pick.video_id, "history");
                tracks = g_store.jukebox_of(who);
            }
        }
        std::ostringstream os;
        os << "{";
        if (!tracks.empty()) {'''
if 'add_jukebox_track(who, pick.video_id, "history")' in s:
    print("now_fill_already 1")
else:
    n = s.count(NOW_OLD)
    print("now_fill_needle", n)
    if n == 1:
        s = s.replace(NOW_OLD, NOW_NEW, 1)
        print("ok_now_fill 1")
    else:
        print("SKIP now_fill")

http.write_text(s)
print("http_written", http.stat().st_size)
print("DONE")
