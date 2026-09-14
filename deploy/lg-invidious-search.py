#!/usr/bin/env python3
"""Install quota-free Juketube search (InnerTube + optional Invidious)."""
from pathlib import Path
from datetime import datetime, timezone

ROOT = Path("/opt/liquidgov")
SRC = ROOT / "src" / "http_server.cpp"
SCRIPT_PATH = ROOT / "scripts" / "yt_search.py"

YT_SEARCH = r'''#!/usr/bin/env python3
"""Quota-free search for Juketube.

Public Invidious APIs are currently off (403 / api:false). Default path is
YouTube InnerTube — the website's own search, no Data API quota.
Set INVIDIOUS_HOSTS to try those first when a host comes back.
"""
import json, os, sys, urllib.parse, urllib.request

# Public YouTube WEB client key (ships in youtube.com; not a billed Data API key).
YT_WEB_KEY = "AIzaSyAO_FJ2SlqU8Q4STEHLGCilw_Y9_11qcW8"


def out(obj, code=0):
    sys.stdout.write(json.dumps(obj, ensure_ascii=False))
    sys.stdout.write("\n")
    sys.exit(code)


def shaped(pairs):
    items = []
    for vid, title in pairs[:8]:
        items.append({
            "id": {"videoId": vid},
            "snippet": {
                "title": title or "untitled",
                "thumbnails": {"default": {"url": "https://i.ytimg.com/vi/%s/default.jpg" % vid}},
            },
        })
    return {"items": items}


def title_of(vr):
    t = vr.get("title") or {}
    if isinstance(t, str):
        return t
    if isinstance(t, dict):
        if t.get("simpleText"):
            return t["simpleText"]
        runs = t.get("runs") or []
        return "".join(x.get("text", "") for x in runs if isinstance(x, dict))
    return ""


def from_invidious(host, q):
    url = host.rstrip("/") + "/api/v1/search?type=video&q=" + urllib.parse.quote(q)
    req = urllib.request.Request(url, headers={
        "User-Agent": "LiquidGov/1.0",
        "Accept": "application/json",
    })
    with urllib.request.urlopen(req, timeout=4) as r:
        data = json.loads(r.read().decode("utf-8", "replace"))
    pairs = []
    if not isinstance(data, list):
        return pairs
    for row in data:
        if not isinstance(row, dict):
            continue
        if row.get("type") not in (None, "video"):
            continue
        vid = row.get("videoId") or ""
        if not isinstance(vid, str) or not (8 <= len(vid) <= 15):
            continue
        pairs.append((vid, row.get("title") or ""))
        if len(pairs) >= 8:
            break
    return pairs


def from_innertube(q):
    body = json.dumps({
        "context": {"client": {
            "clientName": "WEB",
            "clientVersion": "2.20260901.00.00",
            "hl": "en",
            "gl": "US",
        }},
        "query": q,
    }).encode("utf-8")
    url = "https://www.youtube.com/youtubei/v1/search?key=" + YT_WEB_KEY + "&prettyPrint=false"
    req = urllib.request.Request(url, data=body, headers={
        "Content-Type": "application/json",
        "User-Agent": "Mozilla/5.0",
    }, method="POST")
    with urllib.request.urlopen(req, timeout=10) as r:
        data = json.loads(r.read().decode("utf-8", "replace"))
    seen = set()
    pairs = []

    def walk(o):
        if len(pairs) >= 8:
            return
        if isinstance(o, dict):
            for k in ("videoRenderer", "compactVideoRenderer"):
                vr = o.get(k)
                if isinstance(vr, dict) and isinstance(vr.get("videoId"), str):
                    vid = vr["videoId"]
                    if 8 <= len(vid) <= 15 and vid not in seen:
                        seen.add(vid)
                        pairs.append((vid, title_of(vr)))
                    return
            for v in o.values():
                walk(v)
                if len(pairs) >= 8:
                    return
        elif isinstance(o, list):
            for v in o:
                walk(v)
                if len(pairs) >= 8:
                    return

    walk(data)
    return pairs


def main():
    q = sys.argv[1] if len(sys.argv) > 1 else ""
    q = " ".join(q.split())
    if len(q) < 3:
        out({"items": []})
    hosts = [h.strip() for h in os.environ.get("INVIDIOUS_HOSTS", "").split(",") if h.strip()]
    for host in hosts:
        try:
            pairs = from_invidious(host, q)
            if pairs:
                out(shaped(pairs))
        except Exception:
            continue
    try:
        pairs = from_innertube(q)
        if pairs:
            out(shaped(pairs))
    except Exception:
        pass
    out({"error": "search unavailable"}, 2)


if __name__ == "__main__":
    main()
'''

OLD = r'''        const char* k=youtube_api_key();
        if(!k||!*k) {
            lglog::emit("jukebox", "search-fail no-key");
            return http_raw("200 OK", "application/json",
                "{\"items\":[],\"error\":\"YouTube search is not configured.\"}",
                "Cache-Control: no-store\r\n");
        }
        std::string enc; enc.reserve(term.size()*3);
        static const char* hex="0123456789ABCDEF";
        for(unsigned char ch : term) {
            if(std::isalnum(ch)||ch=='-'||ch=='_'||ch=='.'||ch=='~') enc.push_back((char)ch);
            else if(ch==' ') enc.push_back('+');
            else { enc.push_back('%'); enc.push_back(hex[ch>>4]); enc.push_back(hex[ch&15]); }
        }
        const std::string url =
            "https://www.googleapis.com/youtube/v3/search?part=snippet&type=video"
            "&maxResults=8&order=relevance&safeSearch=moderate&q="+enc
            +"&key="+k;
        std::string cmd="curl -sS --max-time 8 '"+url+"'";'''

NEW = r'''        std::string cmd="timeout 12 python3 /opt/liquidgov/scripts/yt_search.py '"+term+"' 2>/dev/null";'''

def main():
    SCRIPT_PATH.parent.mkdir(parents=True, exist_ok=True)
    SCRIPT_PATH.write_text(YT_SEARCH)
    SCRIPT_PATH.chmod(0o755)
    print("wrote", SCRIPT_PATH, SCRIPT_PATH.stat().st_size)

    s = SRC.read_text()
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    SRC.with_name(SRC.name + ".bak-invsearch-" + stamp).write_text(s)

    n = s.count(OLD)
    print("old_block", n)
    if n == 1:
        s = s.replace(OLD, NEW, 1)
    else:
        if "scripts/yt_search.py" in s and "googleapis.com/youtube/v3/search" not in s:
            print("already wired")
            print("DONE")
            return
        g = s.find("https://www.googleapis.com/youtube/v3/search")
        print("fallback_g", g)
        if g < 0:
            raise SystemExit("STOP no googleapis search block")
        start = s.rfind("        const char* k=youtube_api_key();", 0, g)
        if start < 0:
            raise SystemExit("STOP no youtube_api_key before googleapis")
        cmd_i = s.find('std::string cmd="curl -sS --max-time 8', g)
        if cmd_i < 0:
            cmd_i = s.find('std::string cmd="curl', g)
        if cmd_i < 0:
            raise SystemExit("STOP no curl cmd")
        end = s.find("\n", cmd_i)
        print("fallback_slice", start, end, "bytes", end - start)
        s = s[:start] + NEW + s[end:]
    if "googleapis.com/youtube/v3/search" in s:
        raise SystemExit("STOP googleapis leftover")
    SRC.write_text(s)
    print("search_cmd", SRC.read_text().count("scripts/yt_search.py"))
    print("NEED_BUILD 1")
    print("DONE")

if __name__ == "__main__":
    main()
