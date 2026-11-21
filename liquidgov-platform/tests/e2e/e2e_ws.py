"""The one WebSocket client for the e2e suites (standard library only; the site's own server, loopback, no TLS).
2026-11-21: here_ws_client.py and juketube_skip_client.py each carry their own copy of this (queued to move here);
new clients import this one.
    ws = connect(port, session, "/community/ws");  send(ws, {"play": url});  ws.close()"""
import base64
import json
import os
import socket
import struct


def connect(port, session, path):
    """An upgraded socket to path with the lg_session cookie; raises IOError when the server does not upgrade."""
    s = socket.create_connection(("127.0.0.1", int(port)), timeout=5)
    key = base64.b64encode(os.urandom(16)).decode()
    s.sendall(("GET %s HTTP/1.1\r\nHost: 127.0.0.1:%s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
               "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\nCookie: lg_session=%s\r\n\r\n"
               % (path, port, key, session)).encode())
    buf = b""
    while b"\r\n\r\n" not in buf:
        chunk = s.recv(4096)
        if not chunk:
            raise IOError("closed during handshake")
        buf += chunk
    if b" 101 " not in buf.split(b"\r\n", 1)[0]:
        raise IOError("no websocket upgrade: %r" % buf[:120])
    return s


def send(s, obj):
    """One masked text frame holding obj as JSON."""
    data = json.dumps(obj).encode()
    mask = os.urandom(4)
    n = len(data)
    hdr = bytes([0x81]) + (bytes([0x80 | n]) if n < 126 else bytes([0x80 | 126]) + struct.pack(">H", n))
    s.sendall(hdr + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(data)))
