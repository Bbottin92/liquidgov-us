#!/usr/bin/env python3
from pathlib import Path
import sys
p = Path(sys.argv[1] if len(sys.argv) > 1 else "/opt/liquidgov/src/http_server.cpp")
s = p.read_text()
old = '''        if(!acc) return need_login();
        if (valid_username(who)) g_store.complete_jukebox_track(who, parse_form(body)["video_id"]);
        return http_ok("ok");'''
new = '''        if(!acc) return need_login();
        auto jf = parse_form(body);
        if (valid_username(who)) {
            if (jf["skip"] == "1") g_store.skip_jukebox_track(who, jf["video_id"]);
            else g_store.complete_jukebox_track(who, jf["video_id"]);
        }
        return http_ok("ok");'''
if "skip_jukebox_track" in s:
    print("already")
    raise SystemExit(0)
if old not in s:
    print("STOP complete handler")
    raise SystemExit(1)
p.write_text(s.replace(old, new, 1))
print("ok skip route")
