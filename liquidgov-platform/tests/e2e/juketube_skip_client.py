"""Websocket side of juketube_skip.sh (Brandon 2026-11-08, "do a and b"): on a shared station, one listener's
player error or a bare/stale/early "ended" must not skip the room's track; the track is skipped when at least 25% of
connected listeners report it unplayable (Brandon's threshold), and a listener who is alone still ends it as before.
Stdlib only. Usage: python3 juketube_skip_client.py <port> <session A> <session B>
Prints PASS/FAIL lines and passes=N fails=N; exit 0 only when every check passed."""
import base64
import json
import os
import socket
import struct
import sys
import time

PORT, SA, SB = int(sys.argv[1]), sys.argv[2], sys.argv[3]
V1, V2, V3 = "dQw4w9WgXcQ", "9bZkp7q19f0", "kJQP7kiw5Fk"
passes = fails = 0


def check(name, ok, detail=""):
    global passes, fails
    if ok:
        passes += 1
        print("PASS " + name)
    else:
        fails += 1
        print("FAIL %s (%s)" % (name, detail))


class Sock:
    def __init__(self, session):
        self.s = socket.create_connection(("127.0.0.1", PORT), timeout=5)
        key = base64.b64encode(os.urandom(16)).decode()
        self.s.sendall(("GET /community/ws HTTP/1.1\r\nHost: 127.0.0.1:%d\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                        "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\nCookie: lg_session=%s\r\n\r\n"
                        % (PORT, key, session)).encode())
        buf = b""
        while b"\r\n\r\n" not in buf:
            chunk = self.s.recv(4096)
            if not chunk:
                raise IOError("closed during handshake")
            buf += chunk
        head, self.buf = buf.split(b"\r\n\r\n", 1)
        if b" 101 " not in head.split(b"\r\n")[0]:
            raise IOError("no websocket upgrade: %r" % head[:120])

    def send(self, obj):
        data = json.dumps(obj).encode()
        mask = os.urandom(4)
        n = len(data)
        hdr = bytes([0x81]) + (bytes([0x80 | n]) if n < 126 else bytes([0x80 | 126]) + struct.pack(">H", n))
        self.s.sendall(hdr + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(data)))

    def _need(self, k):
        while len(self.buf) < k:
            chunk = self.s.recv(65536)
            if not chunk:
                raise IOError("socket closed")
            self.buf += chunk
        out, self.buf = self.buf[:k], self.buf[k:]
        return out

    def frame(self):
        b1, b2 = self._need(2)
        n = b2 & 0x7F
        if n == 126:
            n = struct.unpack(">H", self._need(2))[0]
        elif n == 127:
            n = struct.unpack(">Q", self._need(8))[0]
        return b1 & 0x0F, self._need(n)

    def latest(self, wait=1.3):
        """The newest snapshot after `wait` seconds (the server sends one about every 500 ms)."""
        end, snap = time.time() + wait, None
        while time.time() < end or snap is None:
            op, data = self.frame()
            if op == 1:
                try:
                    snap = json.loads(data)
                except ValueError:
                    pass
            if time.time() > end + 5:
                break
        return snap or {}

    def close(self):
        try:
            self.s.sendall(bytes([0x88, 0x80]) + os.urandom(4))
            self.s.close()
        except OSError:
            pass


def now_id(snap):
    return (snap.get("now") or {}).get("video_id")


# Five connections (the seed has three people; a person with two tabs is two listeners): 1 of 5 = 20% is under the
# 25% Brandon chose, 2 of 5 = 40% is over it.
a = Sock(SA)
b = Sock(SB)
others = [Sock(SB), Sock(SA), Sock(SB)]
s = a.latest()
check("precondition: five listeners on the site station", s.get("listeners") == 5, json.dumps(s)[:200])
a.send({"play": "https://www.youtube.com/watch?v=" + V1})
s = a.latest()
check("A's play starts V1 for the station", now_id(s) == V1, json.dumps(s)[:200])

b.send({"ended": True})
check("an old client's bare ended from B does not skip V1 (not alone)", now_id(a.latest()) == V1)
b.send({"ended": V1})
check("a named ended right after the start does not skip V1", now_id(a.latest()) == V1)
b.send({"ended": V2})
check("an ended naming another video does not skip V1", now_id(a.latest()) == V1)
b.send({"unplayable": V1})
check("1 of 5 listeners (20%) reporting unplayable does not skip V1", now_id(a.latest()) == V1)
b.send({"unplayable": V1})
check("the same listener reporting twice still counts once", now_id(a.latest()) == V1)
others[0].send({"unplayable": V1})
s = a.latest()
check("2 of 5 listeners (40%, over 25%) reporting unplayable skips V1", now_id(s) != V1, json.dumps(s)[:200])

# a listener alone keeps the old behaviour: its own ended (even an old client's bare one) ends the track
for o in [b] + others:
    o.close()
time.sleep(1.5)
a.send({"play": "https://www.youtube.com/watch?v=" + V2})
s = a.latest()
check("precondition: A alone with V2 playing", now_id(s) == V2 and s.get("listeners") == 1, json.dumps(s)[:200])
a.send({"ended": True})
check("alone, an old client's bare ended still ends the track", now_id(a.latest()) != V2)
a.send({"play": "https://www.youtube.com/watch?v=" + V3})
check("precondition: A alone with V3 playing", now_id(a.latest()) == V3)
a.send({"unplayable": V3})
check("alone, one unplayable report skips the track", now_id(a.latest()) != V3)
a.close()

print("passes=%d fails=%d" % (passes, fails))
sys.exit(0 if fails == 0 else 1)
