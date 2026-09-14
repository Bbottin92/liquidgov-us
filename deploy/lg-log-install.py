#!/usr/bin/env python3
import gzip, base64
from pathlib import Path
from datetime import datetime, timezone

JS_B64 = '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'
HDR_B64 = 'H4sIAEw/qGoC/41XW3ObOBR+9684oWMHmgS7SWcvduxOZ7ezfdjuQzcvu02HkUFgEkAYiVy2zX/fcyQwwsYz9QOBc/l0Lp+OlFdlxZKcgShCPppO4SYtnqGsRMilhEwkCa98kCriVQUXK5DPUvE8gjtRVwXLfPiTSQWXsxlkacElpAV8fv8JYlHBHx9uYIoI0ifcvwTEacblORQC1KbiLDLvES+lD7+dnb352R+9SoswqyMO1+GmEoVY2RKMIj2QZOl6T1SlRdITqTTntiDi27onyGvFn2yBRBTO8p6owR0VLOeyZCGHLMHs4NtolBaUPFVpPtdYkNeLnlQvea1fGyCg52IUigIL+lRWxk6m//FAwf1nVMKS6rrYwT+INAKep8rVThBuWPUaQqbOwQgs+AnkMvEwNsBfLQktzER4j5jaylQXrXU7A61baGMDxWqFXUJrrZnPlQioigGubQSFeHQ9z7hoRJWDyr+9GEmSa+vKnWBwE5U3hhQxKPnl6vKr5YkRx2TuKiQEFUDE+OqdgzP+52KcX4yjm/HH+fjTfPz3v44NZ+Wr2dcmZ0SEAWfgwBcH/2CZ6OMr0AfWxiCY+uywKLUgqVkVXXe9XEHi5nWzJP0I3C9ruQnWLLx3aWVL+7hBloOrjSgZ14OV6afXeIoyiLH6ym28Xrpk4hJNVOya/UYVkLcF5kxr+GGAiemiv+w4sRYiA3mflkHJ1MYd4gFXGxENUoRcWo6kMbjGFE6W4ODedWDSemvRxw/vf3cwB65w50PMMskXO1eC8qs4LSLXmTIpuZJTjHvmwXJJz8ZLVfWeE+mdacweUgzQx4cD37/DoUY+JHuaSqyFkr56Us4AviZwJIjCVmyn/qnXrU/qkx5nkNelkJQ46c7gDVwbd9NJzyKMvQf5026dpkno2yB0zKAlT9rFwrx00Q1bfCcxfsxsQBXK47q8vNK6HTz9Bu3eHsUoi+So7q5MfgifGnMMg9p5TPfI16XjeT34gy6ardGnXEd+3CrYPKZqGeDIGCB4hYO67ZkmhCyxTyT1DR3ApgMph9iA8aOKyLBqnFs2NIHNFnaYGoApkbratiOEAeltX3uCSVakisb/Vrq2fGtv0a0J3Lnnz0ss7RB7d2E5GEDEQsUjz1lYCLup9AtuzK2PUZIAP4wRHd0u8XoCIcxpeXILcSdcXXooWgKWrZfx1k5Jn1IspOvDcEu2g7PooFUZ161qE76tcBBazSL1YPJ9v9Pb4vRHvEwmh8dKOz0r9jh4YqTGzlwXIJUurSzrNfV8hnObe9bRkEpYrdqRim870Je9EezzvFTPHcMO4zJjsEPoEUZaI3BLDNaGTUHe2fXQ2mESdYl2a1mZkaeV2pYWsRlsWetFiPnefrbdwdXWWR9J/bQNWcxe1yd8u+E1ZY7eBJoy0wWAjnydRPthLiwiaC4KUlklOdnKrvwa6wxPG3hHfm1l9SXM2ShVNmfz0U0d1XkZ4CUCD/umoj940bBBhBEJ2qq8euC9y8Vr+PWn3s7d3d8mkOH+rfTNo+umoHyyRf+btsnAxBXH0gozbBw+ON5iejPMnlWymzRvZzhqZDtq6Gtw1sh21ixNSDR6m4/qdHj2SB3iC+B/GHsX89H/n31v/NoMAAA='

stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
js = gzip.decompress(base64.b64decode(JS_B64))
hdr = gzip.decompress(base64.b64decode(HDR_B64))

root = Path("/opt/liquidgov")
srcp = root / "src" / "http_server.cpp"
hpp = root / "src" / "lg_log.hpp"
jsp = root / "assets" / "juketube.js"
orig = srcp.read_text()
s = orig
need = 0

def once(label, old, new):
    global s, need
    n = s.count(old)
    print(label, n)
    if n == 0:
        print("SKIP", label)
        return False
    if n != 1:
        print("STOP", label)
        raise SystemExit(1)
    s = s.replace(old, new, 1)
    need = 1
    return True

hpp.write_bytes(hdr)
print("wrote", hpp, len(hdr))
jsp.parent.mkdir(parents=True, exist_ok=True)
if jsp.exists():
    jsp.with_name(jsp.name + ".bak-log-" + stamp).write_bytes(jsp.read_bytes())
jsp.write_bytes(js)
print("js", len(js))

if '#include "lg_log.hpp"' not in s:
    old = "#include <vector>\n"
    if old not in s:
        print("STOP include_anchor")
        raise SystemExit(1)
    s = s.replace(old, old + '#include "lg_log.hpp"\n', 1)
    need = 1
    print("include_added")
else:
    print("include_present")

ROUTES = (
    '    if(method=="GET"&&path=="/logs") {\n'
    '        const char* op = std::getenv("LIQUIDGOV_OPERATOR");\n'
    '        if (!op || !op[0] || user != op)\n'
    '            return http_raw("404 Not Found", "text/plain", "not found\\n");\n'
    '        return http_raw("200 OK", "text/plain; charset=utf-8", lglog::dump_text(),\n'
    '            "Cache-Control: no-store\\r\\n");\n'
    "    }\n"
    '    if(method=="POST"&&path=="/log") {\n'
    '        if (!acc) return http_raw("401 Unauthorized", "text/plain", "no\\n");\n'
    "        static std::map<std::string, long long> lg_last_ms;\n"
    "        const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(\n"
    "            std::chrono::system_clock::now().time_since_epoch()).count();\n"
    "        auto& last = lg_last_ms[user];\n"
    "        if (now_ms - last < 400)\n"
    '            return http_raw("204 No Content", "text/plain", "");\n'
    "        last = now_ms;\n"
    '        lglog::emit("client", user + " " + lglog::clip_client(body));\n'
    '        return http_raw("204 No Content", "text/plain", "");\n'
    "    }\n"
    "\n"
)

if 'path=="/logs"' not in s:
    old = '    if(method=="GET"&&path=="/jukebox/search") {'
    n = s.count(old)
    print("search_anchor", n)
    if n != 1:
        print("STOP search_anchor")
        raise SystemExit(1)
    s = s.replace(old, ROUTES + old, 1)
    need = 1
    print("routes_added")
else:
    print("routes_present")

once(
    "access_hook",
    "            resp = handle(req,ipbuf);\n"
    "        } catch (const std::exception& e) {\n"
    '            std::cerr<<"handle error: "<<e.what()<<"\\n";',
    "            resp = handle(req,ipbuf);\n"
    "            lglog::access(req, resp);\n"
    "        } catch (const std::exception& e) {\n"
    '            lglog::emit("error", e.what());\n'
    '            std::cerr<<"handle error: "<<e.what()<<"\\n";',
)

once(
    "boot_log",
    '    std::cerr<<"LiquidGov http://0.0.0.0:"<<kPort<<" store="<<g_store.path()',
    '    lglog::emit("boot", "listen");\n'
    '    std::cerr<<"LiquidGov http://0.0.0.0:"<<kPort<<" store="<<g_store.path()',
)

once(
    "play_log",
    "                        g_community_now_by_station[station] = CommunityNowPlaying{*id, username, 0, now, true};",
    "                        g_community_now_by_station[station] = CommunityNowPlaying{*id, username, 0, now, true};\n"
    '                        lglog::emit("jukebox", "play "+station+" "+*id+" by "+username);',
)

once(
    "search_empty",
    "        if (resp.empty() || resp[0] != '{' || resp.find(\"\\\"items\\\"\") == std::string::npos)\n"
    '            return http_raw("200 OK", "application/json", "{\\"items\\":[]}",\n'
    '                "Cache-Control: no-store\\r\\n");\n'
    '        return http_raw("200 OK", "application/json", resp,',
    "        if (resp.empty() || resp[0] != '{' || resp.find(\"\\\"items\\\"\") == std::string::npos) {\n"
    '            lglog::emit("jukebox", "search-bad qlen="+std::to_string(term.size()));\n'
    '            return http_raw("200 OK", "application/json", "{\\"items\\":[]}",\n'
    '                "Cache-Control: no-store\\r\\n");\n'
    "        }\n"
    '        lglog::emit("jukebox", "search-ok qlen="+std::to_string(term.size())+" bytes="+std::to_string(resp.size()));\n'
    '        return http_raw("200 OK", "application/json", resp,',
)

if "?v=12" in s:
    s = s.replace("?v=12", "?v=13")
    need = 1
    print("cache_bust_13")

if s != orig:
    srcp.with_name(srcp.name + ".bak-log-" + stamp).write_text(orig)
    srcp.write_text(s)
    print("cpp_written", len(s))
else:
    print("cpp_unchanged")

if '#include "lg_log.hpp"' not in srcp.read_text() or 'path=="/logs"' not in srcp.read_text():
    print("STOP final_state")
    raise SystemExit(1)

print("NEED_BUILD", 1)
print("hint", "optional: LIQUIDGOV_OPERATOR=<your-username> in /opt/liquidgov/.env.local then restart to enable GET /logs")
print("DONE")
