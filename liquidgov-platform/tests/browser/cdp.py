"""Headless Edge over the DevTools protocol, stdlib only (Windows). Used by phone_check.py.

Moved into the repo 2026-11-09 from the mobile sweep's scratch helper (scratch/2026-11-07-claude-mobile-sweep/
mobile_sweep.py) so the phone checks no longer depend on a scratch folder. Lessons kept from that helper:
  - a new DevTools port per run, and refuse one that already answers (2026-11-07: nine leftover Edges on a fixed port
    made later runs attach to an old browser with old storage);
  - stop_edge() kills the launcher's tree AND every msedge.exe with this run's profile folder (a tree kill alone left
    47 processes behind: Edge's real browser process is not the launcher's child).
"""
import base64
import json
import os
import socket
import struct
import subprocess
import sys
import tempfile
import time
import urllib.request

EDGE = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
UA = ("Mozilla/5.0 (iPhone; CPU iPhone OS 17_5 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) "
      "Version/17.5 Mobile/15E148 Safari/604.1")


class WS:
    """Minimal websocket client for one DevTools page target."""

    def __init__(self, url):
        host, rest = url[len("ws://"):].split("/", 1)
        h, p = host.split(":")
        self.s = socket.create_connection((h, int(p)), timeout=60)
        key = base64.b64encode(os.urandom(16)).decode()
        self.s.sendall(("GET /%s HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                        "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n" % (rest, host, key)).encode())
        buf = b""
        while b"\r\n\r\n" not in buf:
            chunk = self.s.recv(4096)
            if not chunk:
                raise IOError("devtools socket closed during the handshake")
            buf += chunk
        if b" 101 " not in buf.split(b"\r\n", 1)[0]:
            raise IOError("devtools handshake refused: %r" % buf.split(b"\r\n", 1)[0])
        self.extra = buf.split(b"\r\n\r\n", 1)[1]
        self.n, self.events = 0, []
        self.profile, self.browser = None, "?"

    def _recv(self, k):
        while len(self.extra) < k:
            chunk = self.s.recv(1 << 20)
            if not chunk:
                raise IOError("devtools socket closed")
            self.extra += chunk
        out, self.extra = self.extra[:k], self.extra[k:]
        return out

    def send(self, obj):
        data = json.dumps(obj).encode()
        n = len(data)
        if n < 126:
            hdr = bytes([0x81, 0x80 | n])
        elif n < 65536:
            hdr = bytes([0x81, 0x80 | 126]) + struct.pack(">H", n)
        else:
            hdr = bytes([0x81, 0x80 | 127]) + struct.pack(">Q", n)
        mask = os.urandom(4)
        self.s.sendall(hdr + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(data)))

    def recv(self):
        msg = b""
        while True:
            b1, b2 = self._recv(2)
            n = b2 & 0x7F
            if n == 126:
                n = struct.unpack(">H", self._recv(2))[0]
            elif n == 127:
                n = struct.unpack(">Q", self._recv(8))[0]
            msg += self._recv(n)
            if b1 & 0x80:
                return json.loads(msg)

    def call(self, method, **params):
        self.n += 1
        my = self.n
        self.send({"id": my, "method": method, "params": params})
        while True:
            m = self.recv()
            if m.get("id") == my:
                if "error" in m:
                    raise RuntimeError("%s: %s" % (method, m["error"]))
                return m.get("result", {})
            self.events.append(m)


def launch_edge(prefix="lg-phone-edge-"):
    """Start a fresh headless Edge on a free port with a new profile folder; return (proc, ws)."""
    if not os.path.exists(EDGE):
        raise RuntimeError("Edge not found at %s" % EDGE)
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    try:
        urllib.request.urlopen("http://127.0.0.1:%d/json" % port, timeout=1)
        raise RuntimeError("port %d already answers: refusing to attach to a browser this run did not start" % port)
    except OSError:
        pass  # err-ok: nothing listening is the precondition we want
    prof = tempfile.mkdtemp(prefix=prefix)
    proc = subprocess.Popen([EDGE, "--headless=new", "--disable-gpu", "--no-first-run", "--remote-debugging-port=%d" % port,
                             "--user-data-dir=" + prof, "about:blank"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    tabs = []
    for _ in range(60):
        try:
            tabs = json.load(urllib.request.urlopen("http://127.0.0.1:%d/json" % port, timeout=2))
            if any(t.get("type") == "page" for t in tabs):
                break
        except OSError:  # err-ok: not listening yet; retried for 30 s, then the check below raises
            pass
        time.sleep(0.5)
    page = next((t for t in tabs if t.get("type") == "page"), None)
    if page is None:
        stop_edge(proc, prof)
        raise RuntimeError("Edge started but showed no page target on port %d within 30 s" % port)
    ws = WS(page["webSocketDebuggerUrl"])
    ver = json.load(urllib.request.urlopen("http://127.0.0.1:%d/json/version" % port, timeout=5))
    ws.profile = prof
    ws.browser = ver.get("Browser", "?")
    return proc, ws


def stop_edge(proc, profile=None):
    """Kill the launcher's tree and every msedge.exe started with this run's profile folder; say if any are left."""
    try:
        subprocess.run(["taskkill", "/PID", str(proc.pid), "/T", "/F"], capture_output=True, timeout=30)
    except (OSError, subprocess.SubprocessError) as e:
        print("stop_edge: could not kill Edge tree %d: %s" % (proc.pid, e), file=sys.stderr)
    if not profile:
        return
    name = os.path.basename(profile.rstrip("\\/"))
    ps = ("$n=0; Get-CimInstance Win32_Process -Filter \"Name='msedge.exe'\" | Where-Object { $_.CommandLine -like '*%s*' } | "
          "ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue; $n++ }; "
          "$left=(Get-CimInstance Win32_Process -Filter \"Name='msedge.exe'\" | Where-Object { $_.CommandLine -like '*%s*' }).Count; "
          "Write-Output \"$n $left\"" % (name, name))
    try:
        out = subprocess.run(["powershell", "-NoProfile", "-Command", ps], capture_output=True, text=True, timeout=60).stdout.split()
        if len(out) == 2 and out[1] != "0":
            print("stop_edge: %s Edge process(es) with profile %s still running" % (out[1], name), file=sys.stderr)
    except (OSError, subprocess.SubprocessError) as e:
        print("stop_edge: could not sweep profile %s: %s" % (name, e), file=sys.stderr)


def js(ws, expr):
    """Evaluate expr in the page (promises awaited); a page exception comes back as {"__error": ...}."""
    r = ws.call("Runtime.evaluate", expression=expr, returnByValue=True, awaitPromise=True)
    if r.get("exceptionDetails"):
        d = r["exceptionDetails"]
        return {"__error": d.get("text", "") + " " + str(d.get("exception", {}).get("description", ""))[:300]}
    return r.get("result", {}).get("value")
