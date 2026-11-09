#!/usr/bin/env python3
"""Apply leftover v3 C++ hunks: history ?station=, /jukebox/channel, ended record+refill."""
from pathlib import Path

p = Path("/opt/liquidgov/src/http_server.cpp")
s = p.read_text()

old_hist = """        auto for_owner = trim_ws(qs["for"]);
        auto hist = (!for_owner.empty() && valid_username(for_owner)) ? g_store.jukebox_history_of(for_owner)
                                                                        : std::vector<JukeboxHistoryEntry>{};
        if (hist.empty()) hist = g_store.jukebox_history_of(user);"""
new_hist = """        auto for_owner = trim_ws(qs["for"]);
        auto st = trim_ws(qs["station"]);
        std::string hist_key;
        if (st.rfind("u:", 0)==0 && valid_username(st.substr(2))) hist_key = st.substr(2);
        else if (!st.empty() && st.rfind("u:", 0)!=0) hist_key = std::string("jbs:")+st;
        else if (!for_owner.empty() && valid_username(for_owner)) hist_key = for_owner;
        auto hist = hist_key.empty() ? std::vector<JukeboxHistoryEntry>{} : g_store.jukebox_history_of(hist_key);
        if (hist.empty()) hist = g_store.jukebox_history_of(user);"""
if 'qs["station"]' in s and "hist_key" in s:
    print("hist_already")
else:
    n = s.count(old_hist)
    print("hist", n)
    if n != 1:
        print("STOP hist")
        raise SystemExit(1)
    s = s.replace(old_hist, new_hist, 1)
    print("hist_ok")

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
if 'path=="/jukebox/channel"' in s:
    print("channel_already")
else:
    needle = '    if(method=="GET"&&path=="/jukebox/oembed") {'
    n = s.count(needle)
    print("oembed", n)
    if n != 1:
        print("STOP oembed")
        raise SystemExit(1)
    s = s.replace(needle, chan, 1)
    print("channel_ok")

lines = s.splitlines(keepends=True)
start = None
for i, l in enumerate(lines):
    if 's.find("\\"ended\\"")' in l or 's.find("\"ended\"")' in l:
        start = i
        break
print("ended_line", None if start is None else start + 1)
if start is None:
    print("STOP ended_missing")
    raise SystemExit(1)
end = None
for i in range(start + 1, min(start + 25, len(lines))):
    if lines[i].rstrip("\n") == "                }":
        end = i
        break
print("ended_end", None if end is None else end + 1)
if end is None:
    print("STOP ended_end")
    raise SystemExit(1)

if "record_jukebox_play(hkey" in "".join(lines[start : end + 1]):
    print("ended_already")
else:
    new_body = lines[start] + """                    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    std::string finished_id, finished_by;
                    bool need_fill = false;
                    {
                        std::lock_guard<std::mutex> lock(g_community_mu);
                        auto& np0 = g_community_now_by_station[station];
                        if (np0.active) { finished_id = np0.video_id; finished_by = np0.queued_by; }
                        if (np0.active) community_advance_locked(station, now);
                        const auto& np = g_community_now_by_station[station];
                        if (np.active) g_store.set_jukebox_pos(station, np.video_id, 0, now);
                        g_community_last_flush_by_station[station] = now;
                        need_fill = !g_community_now_by_station[station].active;
                    }
                    if (!finished_id.empty()) {
                        std::string hkey = std::string("jbs:") + station;
                        g_store.record_jukebox_play(hkey, finished_id, finished_by);
                        if (need_fill) {
                            auto hist = g_store.jukebox_history_of(hkey);
                            if (!hist.empty()) {
                                std::size_t hi = static_cast<std::size_t>(now) % hist.size();
                                if (hist.size() > 1 && hist[hi].video_id == finished_id) hi = (hi + 1) % hist.size();
                                std::string pick_id = hist[hi].video_id;
                                std::lock_guard<std::mutex> lock(g_community_mu);
                                auto& np = g_community_now_by_station[station];
                                if (!np.active) {
                                    np.video_id = pick_id;
                                    np.queued_by = "history";
                                    np.active = true;
                                    np.frozen_offset = 0;
                                    np.resumed_at = now;
                                    g_store.set_jukebox_pos(station, pick_id, 0, now);
                                }
                            }
                        }
                    }
                }
"""
    lines[start : end + 1] = [new_body]
    s = "".join(lines)
    print("ended_ok")

p.write_text(s)
print(
    "written",
    p.stat().st_size,
    "channel",
    s.count('path=="/jukebox/channel"'),
    "station",
    s.count('qs["station"]'),
    "record",
    s.count("record_jukebox_play(hkey"),
)
print("DONE")
