#!/usr/bin/env python3
from pathlib import Path

js_p = Path("/opt/liquidgov/assets/juketube.js")
s = js_p.read_text()

old = '''  document.addEventListener("click", function (e) {
    if (!panel.classList.contains("open")) return;
    if (panel.contains(e.target) || toggle.contains(e.target)) return;
    setOpen(false);
  });'''
new = '''  document.addEventListener("pointerdown", function (e) {
    if (!panel.classList.contains("open")) return;
    var t = e.target;
    if (!t || !document.documentElement.contains(t)) return;
    if (panel.contains(t) || toggle.contains(t)) return;
    setOpen(false);
  });'''
print("outside", s.count(old))
if s.count(old) != 1:
    raise SystemExit("STOP outside")
s = s.replace(old, new, 1)

old = '''  function playChannel(ch) {
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
  }'''
new = '''  function playChannel(ch) {
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
        items.forEach(function (it) { ytHit({ video_id: vidOf(it), title: it.title || vidOf(it) }); });
        sendPlay(items[0]);
        for (var i = 1; i < items.length; i++) sendNext(items[i]);
      })
      .catch(function () {
        if (my === gen) resultsEl.textContent = "Channel failed.";
      });
  }'''
print("playChannel", s.count(old))
if s.count(old) != 1:
    raise SystemExit("STOP playChannel")
s = s.replace(old, new, 1)

old = '''    if (ch) {
      lab.textContent = title + " · channel";
      row.addEventListener("click", function () { playChannel(ch); });
      row.addEventListener("keydown", function (e) { if (e.key === "Enter") playChannel(ch); });
    } else {'''
new = '''    if (ch) {
      lab.textContent = title + " · channel";
      row.addEventListener("pointerdown", function (e) { e.stopPropagation(); });
      row.addEventListener("click", function (e) {
        e.preventDefault();
        e.stopPropagation();
        setOpen(true);
        playChannel(ch);
      });
      row.addEventListener("keydown", function (e) { if (e.key === "Enter") playChannel(ch); });
    } else {'''
print("ytHit", s.count(old))
if s.count(old) != 1:
    raise SystemExit("STOP ytHit")
s = s.replace(old, new, 1)
js_p.write_text(s)
print("js_ok")

cpp = Path("/opt/liquidgov/src/http_server.cpp")
c = cpp.read_text()
need_build = 0
if 'path=="/jukebox/channel"' in c:
    print("channel_already")
else:
    needle = '    if(method=="GET"&&path=="/jukebox/oembed") {'
    if c.count(needle) != 1:
        print("STOP oembed", c.count(needle))
        raise SystemExit(1)
    chan = r'''    if(method=="GET"&&path=="/jukebox/channel") {
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
        if (FILE* fp = popen(cmd.c_str(), "r")) {
            char buf[4096]; size_t n;
            while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) resp.append(buf, n);
            pclose(fp);
        }
        std::string out = "{\"items\":[";
        bool first = true; std::size_t pos = 0, nvid = 0;
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
            out += "{\"video_id\":\"" + json_escape(vid) + "\"}";
            nvid++;
        }
        out += "]}";
        return http_raw("200 OK", "application/json", out, "Cache-Control: no-store\r\n");
    }
    if(method=="GET"&&path=="/jukebox/oembed") {'''
    c = c.replace(needle, chan, 1)
    cpp.write_text(c)
    need_build = 1
    print("channel_ok")
print("NEED_BUILD", need_build)
print("DONE")
