#!/usr/bin/env python3
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
