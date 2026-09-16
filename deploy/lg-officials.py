#!/usr/bin/env python3
"""Account-vote Officials races. Run on the droplet from /opt/liquidgov.

An Officials race is a Choice proposal flagged with LGP1 tag:
  poff <proposal_id> <jurisdiction>

Eligibility: any signed-in account (same as proposals). Not a verified
civic election. Identity verification stays a later layer.
"""
from pathlib import Path
import sys

ROOT = Path("/opt/liquidgov")
HPP = ROOT / "libs/liquidgov_file_store/include/liquidgov/platform/file_store.hpp"
CPP = ROOT / "libs/liquidgov_file_store/src/file_store.cpp"
TEST = ROOT / "tests/file_store_test.cpp"
HTTP = ROOT / "src/http_server.cpp"


def must_count(s, needle, n, label):
    c = s.count(needle)
    print(f"  {label}: {c} (want {n})")
    if c != n:
        print(f"STOP {label}")
        sys.exit(1)


def sub_once(s, old, new, label):
    must_count(s, old, 1, label)
    return s.replace(old, new, 1)


h = HPP.read_text()
old = """//   pba    <proposal_id> <index> <username>     // who added that pbo row. No row = creator-seeded.\n"""
new = """//   pba    <proposal_id> <index> <username>     // who added that pbo row. No row = creator-seeded.\n//   poff   <proposal_id> <jurisdiction>         // Officials race. Absence = ordinary proposal.\n"""
if old not in h:
    old = """//   pbv    <proposal_id> <voter> <payload>      // number/date/choice/scale value\n"""
    new = """//   pbv    <proposal_id> <voter> <payload>      // number/date/choice/scale value\n//   poff   <proposal_id> <jurisdiction>         // Officials race. Absence = ordinary proposal.\n"""
h = sub_once(h, old, new, "hpp records")

old = """    std::string title;\n    std::string body;\n    BallotSpec ballot;\n"""
new = """    std::string title;\n    std::string body;\n    BallotSpec ballot;\n    bool is_office = false;\n    std::string jurisdiction;\n"""
h = sub_once(h, old, new, "hpp Proposal")

old = """    bool add_proposal_option(std::uint64_t proposal_id, const std::string& username, const std::string& label);\n"""
new = """    bool add_proposal_option(std::uint64_t proposal_id, const std::string& username, const std::string& label);\n    bool mark_proposal_office(std::uint64_t proposal_id, const std::string& jurisdiction);\n"""
if old not in h:
    old = """    BallotTally value_tally_of(std::uint64_t proposal_id) const;\n"""
    new = """    BallotTally value_tally_of(std::uint64_t proposal_id) const;\n    bool mark_proposal_office(std::uint64_t proposal_id, const std::string& jurisdiction);\n"""
h = sub_once(h, old, new, "hpp method")
HPP.write_text(h)
print("hpp ok")

c = CPP.read_text()
old = """        } else {\n            throw std::runtime_error(\"LGP1: unknown record type '\" + tag + \"'\");\n        }\n"""
new = """        } else if (tag == \"poff\") {\n            if (f.size() != 3) throw std::runtime_error(\"LGP1: bad poff record\");\n            pending_poff[parse_u64(f[1], \"poff proposal id\")] = pct_decode(f[2]);\n        } else {\n            throw std::runtime_error(\"LGP1: unknown record type '\" + tag + \"'\");\n        }\n"""
c = sub_once(c, old, new, "load tags")

old = """    std::map<std::uint64_t, std::vector<std::pair<int, std::string>>> pending_pba;\n"""
new = """    std::map<std::uint64_t, std::vector<std::pair<int, std::string>>> pending_pba;\n    std::map<std::uint64_t, std::string> pending_poff;\n"""
if old not in c:
    old = """    std::map<std::uint64_t, std::vector<std::pair<int, std::string>>> pending_bo;\n"""
    new = """    std::map<std::uint64_t, std::vector<std::pair<int, std::string>>> pending_bo;\n    std::map<std::uint64_t, std::string> pending_poff;\n"""
c = sub_once(c, old, new, "pending maps")

if \"pending_popen.count\" in c:
    old = """        if (pending_popen.count(p.id)) p.ballot.open_choice = true;\n"""
    new = """        if (pending_popen.count(p.id)) p.ballot.open_choice = true;\n        auto off = pending_poff.find(p.id);\n        if (off != pending_poff.end()) {\n            p.is_office = true;\n            p.jurisdiction = off->second;\n        }\n"""
    c = sub_once(c, old, new, \"apply load\")
elif \"pending_poff.find\" not in c:
    old = """            for (const auto& item : opts) p.ballot.options.push_back(item.second);\n        }\n    }\n}\n"""
    new = """            for (const auto& item : opts) p.ballot.options.push_back(item.second);\n        }\n        auto off = pending_poff.find(p.id);\n        if (off != pending_poff.end()) {\n            p.is_office = true;\n            p.jurisdiction = off->second;\n        }\n    }\n}\n"""
    c = sub_once(c, old, new, \"apply load fallback\")

old = """        if (p.ballot.open_choice) body << \"popen\\t\" << p.id << \"\\n\";\n"""
new = """        if (p.ballot.open_choice) body << \"popen\\t\" << p.id << \"\\n\";\n        if (p.is_office) body << \"poff\\t\" << p.id << \"\\t\" << pct_encode(p.jurisdiction) << \"\\n\";\n"""
if old not in c:
    old = """        for (std::size_t i = 0; i < p.ballot.options.size(); ++i) {\n            body << \"pbo\\t\" << p.id << \"\\t\" << i << \"\\t\" << pct_encode(p.ballot.options[i]) << \"\\n\";\n        }\n    }\n"""
    new = """        for (std::size_t i = 0; i < p.ballot.options.size(); ++i) {\n            body << \"pbo\\t\" << p.id << \"\\t\" << i << \"\\t\" << pct_encode(p.ballot.options[i]) << \"\\n\";\n        }\n        if (p.is_office) body << \"poff\\t\" << p.id << \"\\t\" << pct_encode(p.jurisdiction) << \"\\n\";\n    }\n"""
c = sub_once(c, old, new, \"flush\")

anchor = """std::vector<Proposal> FileStore::all_proposals() const {\n"""
method = """bool FileStore::mark_proposal_office(std::uint64_t proposal_id, const std::string& jurisdiction) {\n    if (jurisdiction.empty() || jurisdiction.size() > 80) return false;\n    for (auto& p : proposals_) {\n        if (p.id != proposal_id) continue;\n        p.is_office = true;\n        p.jurisdiction = jurisdiction;\n        flush();\n        return true;\n    }\n    return false;\n}\n\nstd::vector<Proposal> FileStore::all_proposals() const {\n"""
c = sub_once(c, anchor, method, \"mark method\")
CPP.write_text(c)
print(\"cpp ok\")

t = TEST.read_text()
old = """    unlink(path.c_str());\n    unlink((path + \".tmp\").c_str());\n    std::cout << \"\\nall file_store tests passed\\n\";\n"""
new = """    {\n        FileStore store(path);\n        auto a = create_account(store, breach, \"off-a\", \"correct-horse-battery-staple\", \"+15559300000\");\n        assert(a.error == AuthError::None);\n        BallotSpec spec;\n        spec.kind = BallotKind::Choice;\n        spec.open_choice = true;\n        spec.options = {\"Ada\", \"Bea\"};\n        auto id = store.add_proposal(\"off-a\", \"Mayor\", \"Who should hold the office.\", spec);\n        assert(id != 0);\n        assert(store.mark_proposal_office(id, \"Fresno, CA\"));\n        auto got = store.proposal_by_id(id);\n        assert(got && got->is_office && got->jurisdiction == \"Fresno, CA\");\n        const std::string disk = slurp(path);\n        assert(disk.find(\"\\npoff\\t\") != std::string::npos);\n        std::cout << \"officials mark/poff ok\\n\";\n    }\n    {\n        FileStore store(path);\n        bool found = false;\n        for (const auto& p : store.all_proposals()) {\n            if (p.title != \"Mayor\") continue;\n            found = true;\n            assert(p.is_office);\n            assert(p.jurisdiction == \"Fresno, CA\");\n        }\n        assert(found);\n        std::cout << \"officials poff survives reload\\n\";\n    }\n\n    unlink(path.c_str());\n    unlink((path + \".tmp\").c_str());\n    std::cout << \"\\nall file_store tests passed\\n\";\n"""
t = sub_once(t, old, new, \"tests\")
TEST.write_text(t)
print(\"tests ok\")
print(\"HTTP_PATCH_SEPARATE\")
print(\"ALL_STORE_OK\")
