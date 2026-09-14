#!/usr/bin/env python3
"""Wire Juketube V1 into droplet http_server.cpp. Run on the box AFTER the two asset files are in /opt/liquidgov/assets/."""
from pathlib import Path

p = Path("/opt/liquidgov/src/http_server.cpp")
s = p.read_text()

old_widget = '        std::string widget = global_jukebox_widget(youtube_api_key_from_env(), user);'
new_widget = (
    '        std::string widget =\n'
    '            "<link rel=\\"stylesheet\\" href=\\"/assets/juketube.css\\">"\n'
    '            "<div id=\\"juketube-root\\"></div>"\n'
    '            "<script src=\\"/assets/juketube.js\\" defer></script>";'
)
print("widget_line", s.count(old_widget))
if s.count(old_widget) != 1:
    raise SystemExit("STOP widget_line")
if "/assets/juketube.js" not in s:
    s = s.replace(old_widget, new_widget, 1)

marker = '    if(method=="GET"&&path=="/assets/intro.mp4") {'
serve = (
    '    if(method=="GET"&&path=="/assets/juketube.js") {\n'
    '        std::ifstream in("assets/juketube.js", std::ios::binary);\n'
    '        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","not found");\n'
    '        std::ostringstream ss; ss << in.rdbuf();\n'
    '        return http_raw("200 OK","text/javascript; charset=utf-8", ss.str(),\n'
    '            "Cache-Control: no-store\\r\\n");\n'
    '    }\n'
    '    if(method=="GET"&&path=="/assets/juketube.css") {\n'
    '        std::ifstream in("assets/juketube.css", std::ios::binary);\n'
    '        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","not found");\n'
    '        std::ostringstream ss; ss << in.rdbuf();\n'
    '        return http_raw("200 OK","text/css; charset=utf-8", ss.str(),\n'
    '            "Cache-Control: no-store\\r\\n");\n'
    '    }\n'
    + marker
)
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
