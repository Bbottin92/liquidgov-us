#!/usr/bin/env python3
import gzip, base64
from pathlib import Path
from datetime import datetime, timezone
stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
js = gzip.decompress(base64.b64decode('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'))
css = gzip.decompress(base64.b64decode('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'))
jp = Path("/opt/liquidgov/assets/juketube.js")
cp = Path("/opt/liquidgov/assets/juketube.css")
jp.with_name(jp.name + ".bak-v8-" + stamp).write_bytes(jp.read_bytes())
if cp.exists():
    cp.with_name(cp.name + ".bak-v8-" + stamp).write_bytes(cp.read_bytes())
jp.write_bytes(js)
cp.write_bytes(css)
print("js", len(js), "css", len(css))

srcp = Path("/opt/liquidgov/src/http_server.cpp")
s = srcp.read_text()
need = 0

OLD_C = (
    'os << ",\\"upcoming\\":" << g_community_upcoming_by_station[station].size() << "}";'
)
NEW_C = (
    'os << ",\\"upcoming\\":" << g_community_upcoming_by_station[station].size();\n'
    '            os << ",\\"queue\\":[";\n'
    '            {\n'
    '                bool qfirst = true;\n'
    '                for (const auto& t : g_community_upcoming_by_station[station]) {\n'
    '                    if (!qfirst) os << ",";\n'
    '                    qfirst = false;\n'
    '                    os << "{\\"video_id\\":\\"" << t.video_id << "\\",\\"queued_by\\":\\"" << t.queued_by << "\\"}";\n'
    '                }\n'
    '            }\n'
    '            os << "]}";'
)
OLD_P = (
    'os << ",\\"upcoming\\":" << (tracks.empty() ? 0 : tracks.size() - 1) << "}";'
)
NEW_P = (
    'os << ",\\"upcoming\\":" << (tracks.empty() ? 0 : tracks.size() - 1);\n'
    '        os << ",\\"queue\\":[";\n'
    '        {\n'
    '            bool qfirst = true;\n'
    '            for (size_t qi = 1; qi < tracks.size(); ++qi) {\n'
    '                if (!qfirst) os << ",";\n'
    '                qfirst = false;\n'
    '                os << "{\\"video_id\\":\\"" << tracks[qi].video_id << "\\",\\"queued_by\\":\\"" << tracks[qi].queued_by << "\\"}";\n'
    '            }\n'
    '        }\n'
    '        os << "]}";'
)

n_c = s.count(OLD_C)
n_p = s.count(OLD_P)
print("community_hits", n_c, "personal_hits", n_p)
if n_c == 1:
    s = s.replace(OLD_C, NEW_C, 1)
    need = 1
elif n_c != 0:
    print("STOP community_hits", n_c)
else:
    print("community already patched or drifted")
if n_p == 1:
    s = s.replace(OLD_P, NEW_P, 1)
    need = 1
elif n_p != 0:
    print("STOP personal_hits", n_p)
else:
    print("personal already patched or drifted")
if need:
    bak = srcp.with_name(srcp.name + ".bak-v8-" + stamp)
    bak.write_text(srcp.read_text())
    srcp.write_text(s)
    print("cpp_patched")
print("NEED_BUILD", need)
print("DONE")
