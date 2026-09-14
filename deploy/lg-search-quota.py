#!/usr/bin/env python3
"""Stop per-keystroke YouTube search. Min 3 chars, 10min cache, 429 copy.
Replaces the live /jukebox/search body by slice, not exact-text match.
"""
import gzip, base64
from pathlib import Path
from datetime import datetime, timezone

JS_B64 = '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'
HDR_B64 = 'H4sIAIdEqGoC/41XW3ObRhR+1684JiMbYhvJdqYXyVImk2aah6YPqV/aOMOsYJGwgEXs4ksT//eeswtikVBjzViGc/n23PfoVVGyZcZA5CEfjEZwk+RPUJQi5FJCKpZLXvogVcTLEs7nIJ+k4lkEd6Iqc5b68AeTCi7HY0iTnEtIcvj87hPEooTfP9zACBGkT7h/CoiTlMszyAWoVclZZJ4jXkgf3p+eXvzsD14leZhWEYfrcFWKXMxtClqR7FHSZLFDKpN82SGpJOM2IeKbqkPIKsUfbYJEFM6yDqnGHeQs47JgIYd0id7Bt8Egycl5itJkorEgq6Ydqj7yWj/WQEDf00EocgzoY1EaOZn8ywMF68/IhBnFdbqFvxdJBDxLlKuVIFyx8jWETJ2BIVjwx5DJpYe2AX4qSWhhKsI1YmopE12U1ukMNG+qhQ0UqxRmCaU1ZzJRIqAoBni2IeTiwfU8o6IRVQYq+/ZsKMtMS5fuMRp3rLJakCwGJb9cXX61NNHimMRdhQVBARAxPnpn4Az/Ph9m58PoZvhxMvw0Gf71j2PDWf7q6mucMyTCgFNw4IuD/zBM9PIV6AVjYxBMfLZY5FqwrFgZXbe5nMPSzar6SPoQuF9UchUsWLh26WSL+7DCKgdXC5Ezrgdzk0+v1hRFEGP0lVtrPbfOxAWKqNg1/UYRkLc5+kxn+GGAjumgP29rYiFECnKdFEHB1MrtqwOuViLqLRFSaWokicE1onA0Awd714HjRluTPn5495uDPnCFnQ8xSyWfblUJyi/jJI9cZ8Sk5EqO0O6xB7MZfddaqqx2lIjvjGJ2n6CBPn458P077HPk/XKHU4qFUNJXj8r5If5dteYL8ThCd9gOTMbyJOZS+Q980Tz/GJBs2gFiRZHyc834H/W2Ji4uKcCaFoqsYCXvCJyjwJn+a83HpnMORlS3bCSoaa1snPgnXmsBsY86XYKdXAhJlhDvFC7gGiwzPKtF7KnDH7fn1GWJujVC2wt05FFzWJgVLqqhP3cSvcDY9bBCeZiXFVeat4WnT6/cm4MYRb48yLsrli/Cp1I8hEEFfIiHBVY4nteB38uiGQbdJmvbHYcDJo+pSgY4JHtausSrqcmZLghZYJ6I6ptyALsciNlXDWg/sqgY5rVyUw21YeOpbaYGYEokrpZtC8KAdAaWPbMltpuiC28jXZu+sYfSxhjurPnTDEPbV71bsxw0IGKh4pHnTC2Ebc/9go2z8dFKIuCLEaJlxaW6PoYQJnQ8qYXYCVeXHpJmgGHreLyxXdL3MgtpYepPyaZ3+u6lKuU6VY3DtyWOfitZxO51vqt3cpufvETLeLJ/kTb3Rckeeu/IxMiZBQkS6dLJslpQzsd4U3HPugwTCfN5c4ng0xb0eefS8XlWqKe2wvbtMoO2RegUjLRG4IYqWAvWAXlrx0Nz+4uodbQ9y/KMNC3XNnSIXcGWtD6EKt/b9ba9qps460u467YpFtPreqdpGl6XzMHdpw4zrTy05GgnmhezoomgXo2kskJytJFt+DXWKd5n8Jb0msjqtdNZKVXU28jBpo6qrAhwbcL1po7oC1crG0QYkqBW5eU976xTr+HXnzqdu91YjyHF/i31rtVmU5A/6bT7Tm3SM3HFIbfCFBOHXxz3ts4Ms2eVbCfNmzGOGtmMGnrrnTWymTUzYxKN3vqlPOmfPVKb+Az4m2rnp8jgPzxdXi/MDQAA'
stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
root = Path("/opt/liquidgov")
srcp = root / "src" / "http_server.cpp"
hpp = root / "src" / "lg_log.hpp"
jsp = root / "assets" / "juketube.js"

NEW_SEARCH = r'''        auto term = trim_ws(qs["q"]);
        const char* key = youtube_api_key_from_env();
        if (term.size() < 3 || !key[0])
            return http_raw("200 OK", "application/json", "{\"items\":[]}",
                "Cache-Control: no-store\r\n");
        std::string cache_key = term;
        for (char& c : cache_key) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
        const auto now_unix = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        {
            std::lock_guard<std::mutex> lock(g_yt_mu);
            auto it = g_yt_cache.find(cache_key);
            if (it != g_yt_cache.end() && it->second.exp > now_unix) {
                lglog::emit("jukebox", "search-cache qlen="+std::to_string(term.size()));
                return http_raw("200 OK", "application/json", it->second.body,
                    "Cache-Control: no-store\r\n");
            }
        }
        std::string url = "https://www.googleapis.com/youtube/v3/search"
            "?part=snippet&type=video&videoEmbeddable=true&maxResults=8&order=relevance"
            "&q=" + url_encode(term) + "&key=" + url_encode(key);
        std::string cmd = "curl -sS --max-time 8 '" + url + "'";
        std::string resp;
        if (FILE* p = popen(cmd.c_str(), "r")) {
            char buf[4096];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), p)) > 0) resp.append(buf, n);
            pclose(p);
        }
        if (resp.find("rateLimitExceeded") != std::string::npos
            || resp.find("quotaExceeded") != std::string::npos
            || resp.find("\"code\": 429") != std::string::npos) {
            lglog::emit("jukebox", "search-quota qlen="+std::to_string(term.size()));
            return http_raw("200 OK", "application/json",
                "{\"items\":[],\"error\":\"Daily YouTube search limit hit. Resets after midnight Pacific.\"}",
                "Cache-Control: no-store\r\n");
        }
        if (resp.empty() || resp[0] != '{' || resp.find("\"items\"") == std::string::npos) {
            lglog::emit("jukebox", "search-bad qlen="+std::to_string(term.size()));
            return http_raw("200 OK", "application/json", "{\"items\":[]}",
                "Cache-Control: no-store\r\n");
        }
        {
            std::lock_guard<std::mutex> lock(g_yt_mu);
            g_yt_cache[cache_key] = YtCacheEntry{resp, now_unix + 600};
            if (g_yt_cache.size() > 64) g_yt_cache.erase(g_yt_cache.begin());
        }
        lglog::emit("jukebox", "search-ok qlen="+std::to_string(term.size())+" bytes="+std::to_string(resp.size()));
        return http_raw("200 OK", "application/json", resp,
            "Cache-Control: no-store\r\n");
'''

CACHE_OLD = "std::mutex g_community_mu;\n"
CACHE_NEW = (
    "struct YtCacheEntry { std::string body; std::int64_t exp = 0; };\n"
    "std::mutex g_yt_mu;\n"
    "std::map<std::string, YtCacheEntry> g_yt_cache;\n"
    "std::mutex g_community_mu;\n"
)

def main():
    js = gzip.decompress(base64.b64decode(JS_B64))
    hdr = gzip.decompress(base64.b64decode(HDR_B64))
    orig = srcp.read_text()
    s = orig
    hpp.write_bytes(hdr)
    print("wrote", hpp, len(hdr))
    jsp.parent.mkdir(parents=True, exist_ok=True)
    jsp.write_bytes(js)
    print("js", len(js))

    if "g_yt_cache" not in s:
        n = s.count(CACHE_OLD)
        print("yt_cache", n)
        if n != 1:
            print("STOP yt_cache")
            raise SystemExit(1)
        s = s.replace(CACHE_OLD, CACHE_NEW, 1)
    else:
        print("yt_cache present")

    idx = s.find('if(method=="GET"&&path=="/jukebox/search")')
    print("search_idx", idx)
    if idx < 0:
        print("STOP no_search_route")
        raise SystemExit(1)
    term = s.find("        auto term = trim_ws(qs[\"q\"]);", idx)
    print("term_idx", term)
    if term < 0:
        print("STOP no_term")
        print(repr(s[idx:idx+400]))
        raise SystemExit(1)
    nxt = s.find("\n    if(method==", term)
    print("next_idx", nxt, "next", repr(s[nxt:nxt+70]) if nxt >= 0 else None)
    if nxt < 0:
        print("STOP no_next_route")
        raise SystemExit(1)
    old = s[term:nxt]
    print("old_search_bytes", len(old), "has_quota", "search-quota" in old, "has_bad", "search-bad" in old)
    if "search-quota" not in old:
        s = s[:term] + NEW_SEARCH.rstrip("\n") + s[nxt:]
        print("search_handler replaced")
    else:
        print("search_handler present")

    for oldv, newv in (("?v=13", "?v=14"), ("?v=12", "?v=14"), ("?v=11", "?v=14")):
        if oldv in s:
            s = s.replace(oldv, newv)
            print("cache_bust", newv)
            break
    else:
        print("cache_bust none")

    if s != orig:
        srcp.with_name(srcp.name + ".bak-quota-" + stamp).write_text(orig)
        srcp.write_text(s)
        print("cpp_written", len(s))
    else:
        print("cpp_unchanged")
    t = srcp.read_text()
    if "g_yt_cache" not in t or "search-quota" not in t:
        print("STOP final_state")
        raise SystemExit(1)
    print("NEED_BUILD", 1)
    print("DONE")

if __name__ == "__main__":
    main()
