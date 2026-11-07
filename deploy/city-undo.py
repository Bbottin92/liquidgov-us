#!/usr/bin/env python3
"""Undo the city features (city proposals slices 1-5) in a LiquidGov LGP1 store,
without rewinding the rest of the site.

Why: the version before the city features refuses to open a store that holds the
city records (an unknown tag fails closed), so rolling the binary back needs the
store to lose those records first. This removes ONLY these record types:

    ccity  clist  pcity  pwho  ctag  ptag  prply  prpln  prx  mapp  pain  painn

Everything else (accounts, posts, mail, friends, groups, votes...) is kept byte for
byte, in the same order. What is lost: group cities, People-here opt-ins, proposal
cities and "who could act", department/council tags, proposal comments, proposal
reactions, map privacy settings (mapp), wishes and pain points (pain, painn).

Usage (STOP the site process first; a running server rewrites the file on its next
save and would put the records back):

    python3 city-undo.py <store.lgp>          # dry run: counts only, changes nothing
    python3 city-undo.py <store.lgp> --go     # backup, strip, verify

--go writes <store>.pre-city-undo-<UTC time> first (same bytes, mode 0600), then
writes the stripped store to <store>.city-undo.tmp with the original mode and owner,
fsyncs it and renames it over the store (same atomic replace FileStore uses), then
re-reads it and checks the result. Exit codes: 0 ok, 1 refused or failed, 2 usage.
Running it again on an already stripped store removes nothing and changes nothing.

REINSTALLING LATER: people's map privacy choices (mapp rows: "Show me on the map"
and the rest) are in the undo's backup. Without them, everyone falls back to the
defaults and someone who hid themselves would quietly reappear. So, with the site
STOPPED and BEFORE starting the reinstalled version for the first time:

    python3 city-undo.py --restore-privacy <store>.pre-city-undo-<time> <store>        # dry run
    python3 city-undo.py --restore-privacy <store>.pre-city-undo-<time> <store> --go   # apply

It copies every mapp row from the backup into the store, replacing any mapp row
for the same person, and changes nothing else. It writes its own backup first
(<store>.pre-privacy-restore-<UTC time>) and uses the same atomic replace. Running
it twice changes nothing the second time. Wishes and pain points are NOT restored:
they were private notes the person could no longer review while the feature was out.
"""
import os
import shutil
import sys
import time

CITY_TAGS = ("ccity", "clist", "pcity", "pwho", "ctag", "ptag", "prply", "prpln", "prx", "mapp", "pain", "painn")


def fail(msg):
    print("city-undo: " + msg, file=sys.stderr)
    sys.exit(1)


def split_records(data):
    """LGP1 records are LF-terminated lines; the tag is the text before the first tab.
    Returns (kept_lines, removed_counts). Lines keep their own endings."""
    lines = data.split(b"\n")
    trailing_newline = data.endswith(b"\n")
    if trailing_newline:
        lines = lines[:-1]
    kept = []
    removed = {t: 0 for t in CITY_TAGS}
    for i, line in enumerate(lines):
        if i == 0:
            kept.append(line)
            continue
        tag = line.split(b"\t", 1)[0].rstrip(b"\r").decode("ascii", "replace")
        if tag in removed:
            removed[tag] += 1
        else:
            kept.append(line)
    out = b"\n".join(kept) + (b"\n" if trailing_newline else b"")
    return out, removed


def read_lgp1(path):
    try:
        st = os.stat(path)
        with open(path, "rb") as f:
            data = f.read()
    except OSError as e:
        fail("cannot read %s: %s" % (path, e))
    if data.split(b"\n", 1)[0].rstrip(b"\r") != b"LGP1":
        fail("%s is not an LGP1 store (first line is not LGP1); refusing" % path)
    return st, data


def mapp_rows(data):
    """username (raw, still percent-encoded) -> the whole mapp line, from LGP1 bytes."""
    rows = {}
    for line in data.split(b"\n")[1:]:
        line = line.rstrip(b"\r")
        parts = line.split(b"\t")
        if parts[0] == b"mapp":
            if len(parts) not in (7, 8) or not parts[1] or any(p not in (b"0", b"1") for p in parts[2:]):
                fail("malformed mapp row in the backup: %r; refusing" % line[:80])
            rows[parts[1]] = line
    return rows


def atomic_replace(path, st, old, new, backup_name):
    """Back up `old`, write `new` with the store's mode/owner, fsync, rename, verify."""
    stamp = time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())
    backup = "%s.%s-%s" % (path, backup_name, stamp)
    tmp = path + ".city-undo.tmp"
    try:
        fd = os.open(backup, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(fd, "wb") as f:
            f.write(old)
            f.flush()
            os.fsync(f.fileno())
        fd = os.open(tmp, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, st.st_mode & 0o777)
        with os.fdopen(fd, "wb") as f:
            f.write(new)
            f.flush()
            os.fsync(f.fileno())
        os.chmod(tmp, st.st_mode & 0o777)
        if hasattr(os, "chown"):
            try:
                os.chown(tmp, st.st_uid, st.st_gid)
            except PermissionError:
                print("warning: could not keep the owner (not root); check ownership before starting the site")
        with open(path, "rb") as f:
            if f.read() != old:
                os.unlink(tmp)
                fail("the store changed while working; is the site still running? nothing changed (backup kept)")
        os.replace(tmp, path)
    except OSError as e:
        if os.path.exists(tmp):
            os.unlink(tmp)
        fail("failed: %s (backup: %s)" % (e, backup))
    with open(path, "rb") as f:
        if f.read() != new:
            fail("verify failed: the store does not match what was written. Restore %s by hand." % backup)
    return backup


def restore_privacy(backup_path, store_path, go):
    _, bdata = read_lgp1(backup_path)
    st, data = read_lgp1(store_path)
    want = mapp_rows(bdata)
    have = mapp_rows(data)
    changes = {u: l for u, l in want.items() if have.get(u) != l}
    print("backup: %s has %d map privacy rows" % (backup_path, len(want)))
    print("store:  %s has %d; %d to restore" % (store_path, len(have), len(changes)))
    if not go:
        print("dry run: nothing changed. With the site stopped, rerun with --go.")
        return 0
    if not changes:
        print("nothing to restore; store left as it is.")
        return 0
    trailing = data.endswith(b"\n")
    lines = data.split(b"\n")
    if trailing:
        lines = lines[:-1]
    kept = [lines[0]] + [l for l in lines[1:]
                         if not (l.split(b"\t")[0] == b"mapp" and l.split(b"\t")[1:2] and l.split(b"\t")[1] in changes)]
    new = b"\n".join(kept + [changes[u] for u in sorted(changes)]) + b"\n"
    backup = atomic_replace(store_path, st, data, new, "pre-privacy-restore")
    _, after = read_lgp1(store_path)
    got = mapp_rows(after)
    if any(got.get(u) != l for u, l in want.items()):
        fail("verify failed: a privacy row did not come back. Restore %s by hand." % backup)
    print("backup: " + backup)
    print("done: restored %d people's map privacy choices." % len(changes))
    return 0


def main(argv):
    if "--restore-privacy" in argv[1:]:
        rest = [a for a in argv[1:] if a not in ("--restore-privacy", "--go")]
        if len(rest) != 2 or any(a.startswith("-") for a in rest):
            print(__doc__, file=sys.stderr)
            return 2
        return restore_privacy(rest[0], rest[1], "--go" in argv[1:])
    args = [a for a in argv[1:] if a != "--go"]
    go = "--go" in argv[1:]
    if len(args) != 1 or any(a.startswith("-") for a in args):
        print(__doc__, file=sys.stderr)
        return 2
    path = args[0]
    try:
        st = os.stat(path)
        with open(path, "rb") as f:
            data = f.read()
    except OSError as e:
        fail("cannot read %s: %s" % (path, e))
    if data.split(b"\n", 1)[0].rstrip(b"\r") != b"LGP1":
        fail("%s is not an LGP1 store (first line is not LGP1); refusing" % path)

    stripped, removed = split_records(data)
    total = sum(removed.values())
    print("store: %s (%d bytes)" % (path, len(data)))
    for t in CITY_TAGS:
        print("  %-6s %d" % (t, removed[t]))
    print("city records to remove: %d" % total)
    if not go:
        print("dry run: nothing changed. Stop the site, then rerun with --go.")
        return 0
    if total == 0:
        print("nothing to remove; store left as it is.")
        return 0

    stamp = time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())
    backup = "%s.pre-city-undo-%s" % (path, stamp)
    tmp = path + ".city-undo.tmp"
    try:
        fd = os.open(backup, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(fd, "wb") as f:
            f.write(data)
            f.flush()
            os.fsync(f.fileno())
        with open(backup, "rb") as f:
            if f.read() != data:
                fail("backup %s does not match the store; nothing changed" % backup)
        print("backup: " + backup)

        fd = os.open(tmp, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, st.st_mode & 0o777)
        with os.fdopen(fd, "wb") as f:
            f.write(stripped)
            f.flush()
            os.fsync(f.fileno())
        os.chmod(tmp, st.st_mode & 0o777)
        if hasattr(os, "chown"):
            try:
                os.chown(tmp, st.st_uid, st.st_gid)
            except PermissionError:
                print("warning: could not keep the owner (not root); check ownership before starting the site")
        # The store must not have changed while we worked (a running server).
        with open(path, "rb") as f:
            if f.read() != data:
                os.unlink(tmp)
                fail("the store changed while stripping; is the site still running? nothing changed (backup kept)")
        os.replace(tmp, path)
        dfd = os.open(os.path.dirname(os.path.abspath(path)), os.O_RDONLY)
        try:
            os.fsync(dfd)
        finally:
            os.close(dfd)
    except OSError as e:
        if os.path.exists(tmp):
            os.unlink(tmp)
        fail("failed: %s (the store is unchanged unless the rename happened; backup: %s)" % (e, backup))

    with open(path, "rb") as f:
        now = f.read()
    _, left = split_records(now)
    if now != stripped or sum(left.values()) != 0:
        fail("verify failed: the store does not match the stripped copy. Restore %s by hand." % backup)
    before_lines = data.count(b"\n")
    after_lines = now.count(b"\n")
    if before_lines - after_lines != total:
        fail("verify failed: %d lines removed, expected %d. Restore %s by hand." % (before_lines - after_lines, total, backup))
    print("done: removed %d city records; %d other records kept unchanged." % (total, after_lines - 1))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
