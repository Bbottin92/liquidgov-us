"""Websocket side of juketube_history.sh: as the only listener on the site-wide station, plays two tracks, so the
first is replaced and goes into the station's history.
Usage: python3 juketube_history_client.py <port> <session>   (prints PLAYED <id1> <id2>)"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import e2e_ws  # noqa: E402  (the one e2e websocket client)

V1, V2 = "dQw4w9WgXcQ", "9bZkp7q19f0"
ws = e2e_ws.connect(sys.argv[1], sys.argv[2], "/community/ws")
e2e_ws.send(ws, {"play": "https://www.youtube.com/watch?v=" + V1})
time.sleep(0.6)
e2e_ws.send(ws, {"play": "https://www.youtube.com/watch?v=" + V2})
time.sleep(0.6)
ws.close()
print("PLAYED %s %s" % (V1, V2))
