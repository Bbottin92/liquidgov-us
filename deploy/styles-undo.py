#!/usr/bin/env python3
"""Undo the profile style library (profile CSS slice 2, Brandon 2026-11-12) in a LiquidGov LGP1 store, without
rewinding anything else.

Why: the version before the style library refuses to open a store that holds its records (an unknown tag fails
closed), so rolling the binary back needs the store to lose those records first. This removes ONLY these record types:

    sty  styn  stu  str

Everything else (accounts, Profile CSS, posts, mail, city records, map privacy...) is kept byte for byte, in the same
order. What is lost: the published styles, who used which ("based on" lines and "most used" counts) and the reports.
NOT lost: anyone's own Profile CSS, including CSS they copied from the library (copies live in the ac record, not
here), and the moderation archive's copies of taken-down styles.

Usage (STOP the site process first; a running server rewrites the file on its next save and would put the records
back):

    python3 styles-undo.py <store.lgp>          # dry run: counts only, changes nothing
    python3 styles-undo.py <store.lgp> --go     # backup, strip, verify

--go writes <store>.pre-styles-undo-<UTC time> first (same bytes, mode 0600), then writes the stripped store to
<store>.styles-undo.tmp with the original mode and owner, fsyncs it and renames it over the store (same atomic replace
FileStore uses), then re-reads it and checks the result. Exit codes: 0 ok, 1 refused or failed, 2 usage. Running it
again on an already stripped store removes nothing and changes nothing.

REINSTALLING LATER: the library comes back empty. To bring the styles back, with the site STOPPED and BEFORE starting
the reinstalled version, copy the sty/styn/stu/str lines from the backup to the end of the store (a style whose author
is gone by then is dropped on load). Tests: liquidgov-platform/tests/rollback/styles_undo.sh (by hand: it needs the
previous build), commons-daemon/tests/styles_undo_lists_check.py.
"""
import os
import sys
import time

STYLE_TAGS = ("sty", "styn", "stu", "str")


def fail(msg):
    print("styles-undo: " + msg, file=sys.stderr)
    sys.exit(1)


def split_records(data):
    """LGP1 records are LF-terminated lines; the tag is the text before the first tab.
    Returns (kept bytes, removed counts). Lines keep their own endings."""
    lines = data.split(b"\n")
    trailing_newline = data.endswith(b"\n")
    if trailing_newline:
        lines = lines[:-1]
    kept = []
    removed = {t: 0 for t in STYLE_TAGS}
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


def main(argv):
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
    for t in STYLE_TAGS:
        print("  %-5s %d" % (t, removed[t]))
    print("style records to remove: %d" % total)
    if not go:
        print("dry run: nothing changed. Stop the site, then rerun with --go.")
        return 0
    if total == 0:
        print("nothing to remove; store left as it is.")
        return 0

    stamp = time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())
    backup = "%s.pre-styles-undo-%s" % (path, stamp)
    tmp = path + ".styles-undo.tmp"
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
    print("done: removed %d style records; %d other records kept unchanged." % (total, after_lines - 1))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
