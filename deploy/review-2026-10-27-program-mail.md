# Programs in mail: fresh reviews and answers (2026-10-27)

Three fresh-session Claude reviews (read-only, scratch\advisor-round.ps1), each on the latest diff. Verdicts:
round 1 "don't deploy", round 2 "don't deploy", round 3 "approve with fixes" (its one fix is done below).
Tests at the end: 7 unit-test programs pass (Debug, asserts on) and tests/e2e/program_mail.sh 48/48 against the
real server binary; the Release build compiles without warnings. nginx itself was not run (building a local nginx
was blocked), so the nginx block is checked at deploy time (program-mail.md step 9).

## Round 1 (on 56041e8)

| # | Finding (reviewer's words, short) | Answer | Reviewer agrees? |
|---|---|---|---|
| 1 | Anyone can still send programs: a zip with an empty [Content_Types].xml, an exe with "ftyp" at byte 4, an archive behind a PDF prefix | Fixed (7729de2, 09a9ba4): program magic first; archives inside PDF/text/video found by end record + directory signature, Zip64 locator; every Office entry checked by content (first 4 KB, any size, unreadable = refused) and by name. Not done: requiring a Word/Excel/PowerPoint main part (committed fixtures lack one; an empty one is as easy to add). Tests: program_messaging_test bypass block | Yes (round 2: "fixed for practical purposes"; round 3: fixed after Zip64) |
| 2 | nginx lets anyone upload 102 MB to /mail | Fixed: /mail 26m; big bodies only via /mail/big behind auth_request (cookie + length checked before the body is read), limit_req / limit_conn; blob writes checked (temp + rename). e2e: upload-ok checks | Yes ("fixed in design, untested") |
| 3 | The app's 413 reaches users as a 502 behind nginx | Fixed: send, half-close, drain (bounded). e2e: 413 without Expect | Yes ("fixed in code, untested through nginx") |
| 4 | Nothing scans programs for malware | Fixed as documentation: the admin form, the code and program-mail.md say programs are not virus-checked; grants are the gate | Yes |
| 5 | Memory: uploads 3x, downloads 4x | Fixed: headers and body kept apart, body released after the file is taken out (~2x); every download through nginx; without it >26 MB gets 503. e2e: 503 check | Yes ("acceptable") |
| 6 | nginx handoff unverified (folder, permissions) | Checked read-only on the server: default folder, www-data can read; no server-level add_header on the live config, so nothing (HSTS) is dropped | Partly ("couldn't confirm"): the check output is in this session's log, not the diff |
| 7 | Linux programs keep any name | Fixed: .bin forced. e2e: invoice.pdf -> invoice.pdf.bin | Yes |
| 8 | Store loose ends (names never pruned, orphan, rename across sends) | Fixed: per-message names written after the message exists, pruned at load and on account deletion; failed group send releases the blob. Left: names of mail deleted while running stay until the next start; two sends of the same bytes share the download link's name (cards show each) | Yes (rest listed under Known limits) |

## Round 2 (on 7729de2)

| # | Finding | Answer | Reviewer agrees? |
|---|---|---|---|
| 1 | A grant of 24 MB or less blocks all mail | Fixed (09a9ba4): forms post to /mail; switch to /mail/big only for a file over the ordinary limit; upload-ok allows any program sender within max(ordinary, their limit). e2e | Yes |
| 2 | Granted users' chat rate-limited, refused messages vanish | Fixed: chat never uses /mail/big; nginx 429 + error pages; pop-up shows non-2xx | Yes |
| 3 | Zip behind a PDF with one byte after | Fixed (end record anywhere + directory signature; Zip64 locator after round 3). Tests | Yes (after Zip64) |
| 4 | Charts' embedded workbooks refused | Won't fix: not a regression, the embeddings/ rule refused them before this branch | Yes ("answer accepted") |
| 5 | Unreadable entries, name gaps | Fixed | Yes |
| 6 | Chunked uploads skip the size check | Fixed: no length = no | Yes |
| 7 | App listens on 0.0.0.0:8080 | Won't fix now: ufw allows only SSH/80/443 (checked read-only); binding 127.0.0.1 is a later change | Yes ("answer accepted") |
| 8 | Old 4-field name records refused | Fixed: skipped | Yes |

## Round 3 (on 09a9ba4)

| # | Finding | Answer | Reviewer agrees? |
|---|---|---|---|
| 1 | A Zip64 archive behind a PDF still gets through | Fixed: the Zip64 locator before an end record marks the file a zip. Test (hand-built tail: miniz won't write a Zip64 end for a small archive) | Not re-reviewed (one condition + a test) |
| 2 | The pop-up calls every non-2xx "too large" | Fixed: 413 / 429 / anything else get their own words. e2e | Not re-reviewed |
