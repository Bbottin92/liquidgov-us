#!/opt/lg-webpush-venv/bin/python
import json, os, sys, tempfile
from pathlib import Path
ROOT = Path("/opt/liquidgov")
SUBS, PUB, PRIV = ROOT/"push_subs.txt", ROOT/"deploy/vapid.public", ROOT/"deploy/vapid.private"
MAILTO = "mailto:webmaster@liquidgov.us"
CAP = 8
def atomic_write(path, text, mode=0o600):
    fd, tmp = tempfile.mkstemp(dir=str(path.parent), prefix=".tmp-")
    try:
        os.write(fd, text.encode()); os.fsync(fd); os.close(fd)
        os.chmod(tmp, mode); os.replace(tmp, path)
    except Exception:
        try: os.close(fd)
        except OSError: pass
        try: os.unlink(tmp)
        except OSError: pass
        raise
def init_keys():
    from cryptography.hazmat.primitives.asymmetric import ec
    from cryptography.hazmat.primitives import serialization
    import base64
    if PUB.exists() and PRIV.exists():
        print("vapid exists"); return
    priv = ec.generate_private_key(ec.SECP256R1())
    pem = priv.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption())
    raw = priv.public_key().public_bytes(serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint)
    atomic_write(PRIV, pem.decode(), 0o600)
    atomic_write(PUB, base64.urlsafe_b64encode(raw).decode().rstrip("=")+"\n", 0o644)
    print("vapid generated")
def load():
    if not SUBS.exists(): return []
    return [tuple(p) for line in SUBS.read_text().splitlines() if len((p:=line.split("\t")))==4]
def save(rows):
    atomic_write(SUBS, "".join("%s\t%s\t%s\t%s\n"%r for r in rows), 0o600)
def subscribe(user, endpoint, p256dh, auth):
    if not user or not endpoint.startswith("https://") or not p256dh or not auth:
        sys.exit(2)
    rows=[r for r in load() if r[1]!=endpoint]
    mine=[r for r in rows if r[0]==user]; others=[r for r in rows if r[0]!=user]
    mine.append((user,endpoint,p256dh,auth)); save(others+mine[-CAP:])
    print("ok", user, "devices", min(len(mine), CAP))
def send(user, title, body, url, tag="lg"):
    from pywebpush import webpush, WebPushException
    from py_vapid import Vapid
    from cryptography.hazmat.primitives.serialization import load_pem_private_key
    vapid=Vapid(); vapid.private_key=load_pem_private_key(PRIV.read_bytes(), password=None)
    payload=json.dumps({"title":title,"body":body,"url":url or "/","tag":tag})
    keep=[]; sent=0
    for u,endpoint,p256dh,auth in load():
        if u!=user:
            keep.append((u,endpoint,p256dh,auth)); continue
        try:
            webpush(subscription_info={"endpoint":endpoint,"keys":{"p256dh":p256dh,"auth":auth}},
                    data=payload, vapid_private_key=vapid, vapid_claims={"sub":MAILTO})
            keep.append((u,endpoint,p256dh,auth)); sent+=1
        except WebPushException as e:
            code=getattr(getattr(e,"response",None),"status_code",None)
            if code in (404,410):
                print("drop", code); continue
            print("send fail", code or e, file=sys.stderr); keep.append((u,endpoint,p256dh,auth))
        except Exception as e:
            print("send fail", e, file=sys.stderr); keep.append((u,endpoint,p256dh,auth))
    save(keep); print("sent", sent)
cmd=sys.argv[1] if len(sys.argv)>1 else ""
if cmd=="init": init_keys()
elif cmd=="public":
    if not PUB.exists(): init_keys()
    sys.stdout.write(PUB.read_text())
elif cmd=="list":
    for r in load(): print(r[0], r[1][:48])
elif cmd=="subscribe" and len(sys.argv)==6: subscribe(*sys.argv[2:6])
elif cmd=="send" and len(sys.argv)>=6: send(*sys.argv[2:6], sys.argv[6] if len(sys.argv)>6 else "lg")
else:
    print("usage"); sys.exit(1)
