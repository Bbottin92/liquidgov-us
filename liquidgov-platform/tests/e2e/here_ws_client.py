"""A tiny WebSocket client for profile_here_ws.sh (standard library only; the site's own server, loopback, no TLS).
    python3 here_ws_client.py <port> <session> <page> <seconds> [x y]
Connects to /here/ws?page=<page> with the lg_session cookie, sends {"x":x,"y":y} (if given) right away and again
every 0.3 s, and after <seconds> prints the LAST snapshot it received, one line: JSON, or REFUSED <status line> when
the server did not upgrade, or CLOSED when it upgraded and then closed with no snapshot."""
import base64
import json
import os
import socket
import struct
import sys
import time
import urllib.parse


def frame(text):
    data = text.encode()
    mask = os.urandom(4)
    head = bytes([0x81])
    n = len(data)
    head += bytes([0x80 | n]) if n < 126 else bytes([0x80 | 126]) + struct.pack(">H", n)
    return head + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(data))


def read_frames(buf):
    """(complete text payloads, leftover bytes) from server frames (never masked)."""
    out = []
    while len(buf) >= 2:
        op, n = buf[0] & 0x0F, buf[1] & 0x7F
        i = 2
        if n == 126:
            if len(buf) < 4:
                break
            n, i = struct.unpack(">H", buf[2:4])[0], 4
        elif n == 127:
            if len(buf) < 10:
                break
            n, i = struct.unpack(">Q", buf[2:10])[0], 10
        if len(buf) < i + n:
            break
        if op == 1:
            out.append(buf[i:i + n].decode("utf-8", "replace"))
        elif op == 8:
            out.append(None)
        buf = buf[i + n:]
    return out, buf


def main():
    port, session, page, secs = sys.argv[1], sys.argv[2], sys.argv[3], float(sys.argv[4])
    pos = (float(sys.argv[5]), float(sys.argv[6])) if len(sys.argv) > 6 else None
    s = socket.create_connection(("127.0.0.1", int(port)), timeout=5)
    key = base64.b64encode(os.urandom(16)).decode()
    path = "/here/ws?page=" + urllib.parse.quote(page, safe="/&=")
    cookie = "Cookie: lg_session=%s\r\n" % session if session != "-" else ""
    s.sendall(("GET %s HTTP/1.1\r\nHost: 127.0.0.1:%s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
               "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n%s\r\n" % (path, port, key, cookie)).encode())
    head = b""
    try:
        while b"\r\n\r\n" not in head:
            chunk = s.recv(4096)
            if not chunk:
                break
            head += chunk
    except socket.timeout:
        pass
    status = head.split(b"\r\n", 1)[0].decode("latin-1", "replace")
    if " 101 " not in status + " ":
        print("REFUSED " + (status or "(no answer)"))
        return
    buf = head.split(b"\r\n\r\n", 1)[1]
    last, closed = None, False
    s.settimeout(0.1)
    end = time.time() + secs
    next_send = 0.0
    while time.time() < end and not closed:
        if pos and time.time() >= next_send:
            try:
                s.sendall(frame(json.dumps({"x": pos[0], "y": pos[1]})))
            except OSError:
                closed = True
                break
            next_send = time.time() + 0.3
        try:
            chunk = s.recv(65536)
            if not chunk:
                closed = True
                break
            buf += chunk
        except socket.timeout:
            pass
        msgs, buf = read_frames(buf)
        for m in msgs:
            if m is None:
                closed = True
            else:
                last = m
    try:
        s.close()
    except OSError:
        pass
    print(last if last is not None else "CLOSED")


if __name__ == "__main__":
    main()
