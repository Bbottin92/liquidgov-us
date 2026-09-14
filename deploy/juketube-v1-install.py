#!/usr/bin/env python3
"""Wire Juketube V1 files into droplet http_server.cpp. Does not overwrite the live file.
Run from /opt/liquidgov AFTER assets/juketube.js and assets/juketube.css are in place.
"""
from pathlib import Path

p = Path("/opt/liquidgov/src/http_server.cpp")
s = p.read_text()

old_widget = '        std::string widget = global_jukebox_widget(youtube_api_key_from_env(), user);'
new_widget = '''        std::string widget =
            "<link rel=\\"stylesheet\\" href=\\"/assets/juketube.css\\">"
            "<div id=\\"juketube-root\\"></div>"
            "<script src=\\"/assets/juketube.js\\" defer></script>";'''

print("widget_line", s.count(old_widget))
if s.count(old_widget) != 1:
    raise SystemExit("STOP widget_line")
if "/assets/juketube.js" not in s:
    s = s.replace(old_widget, new_widget, 1)

marker = '    if(method=="GET"&&path=="/assets/intro.mp4") {'
serve = '''    if(method=="GET"&&path=="/assets/juketube.js") {
        std::ifstream in("assets/juketube.js", std::ios::binary);
        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","not found");
        std::ostringstream ss; ss << in.rdbuf();
        return http_raw("200 OK","text/javascript; charset=utf-8", ss.str(),
            "Cache-Control: no-store\\r\\n");
    }
    if(method=="GET"&&path=="/assets/juketube.css") {
        std::ifstream in("assets/juketube.css", std::ios::binary);
        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","not found");
        std::ostringstream ss; ss << in.rdbuf();
        return http_raw("200 OK","text/css; charset=utf-8", ss.str(),
            "Cache-Control: no-store\\r\\n");
    }
    if(method=="GET"&&path=="/assets/intro.mp4") {'''

print("intro_route", s.count(marker))
if s.count(marker) != 1:
    raise SystemExit("STOP intro_route")
if 'path=="/assets/juketube.js"' not in s:
    s = s.replace(marker, serve, 1)

p.write_text(s)
t = p.read_text()
print("ok_js_href", t.count("/assets/juketube.js"))
print("ok_route", t.count('path=="/assets/juketube.js"'))
print("ok_old_call", t.count("global_jukebox_widget(youtube_api_key_from_env()"))
