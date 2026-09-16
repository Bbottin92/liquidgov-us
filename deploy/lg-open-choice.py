#!/usr/bin/env python3
"""Open-choice (write-in) proposals. Run on the droplet from /opt/liquidgov."""
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

# --- hpp ---
h = HPP.read_text()
old = """//   pbo    <proposal_id> <index> <label>        // Choice/Scale labels
//   pbv    <proposal_id> <voter> <payload>      // number/date/choice/scale value
"""
new = """//   pbo    <proposal_id> <index> <label>        // Choice/Scale labels
//   pbv    <proposal_id> <voter> <payload>      // number/date/choice/scale value
//   popen  <proposal_id>                        // Choice write-ins on. Absence = closed.
//          Separate tag: pbk is exact 7 fields (loader throws otherwise).
//   pba    <proposal_id> <index> <username>     // who added that pbo row. No row = creator-seeded.
"""
h = sub_once(h, old, new, "hpp records")

old = """    std::vector<std::string> options; // Choice: 2–12 labels; Scale: optional left/right
};
"""
new = """    std::vector<std::string> options; // Choice: 2–12 closed, 1–48 if open_choice; Scale: optional left/right
    bool open_choice = false;         // people may append a choice (non-author: one each)
    std::vector<std::string> option_authors; // parallel to options; empty = creator-seeded
};
"""
h = sub_once(h, old, new, "hpp BallotSpec")

old = """    BallotTally value_tally_of(std::uint64_t proposal_id) const;

    bool create_community(const Community& c);
"""
new = """    BallotTally value_tally_of(std::uint64_t proposal_id) const;
    // Append a Choice option. Author: unlimited (up to 48). Anyone else: at most one
    // on this proposal, and only if open_choice. Not a vote. Duplicate/empty = false.
    bool add_proposal_option(std::uint64_t proposal_id, const std::string& username, const std::string& label);

    bool create_community(const Community& c);
"""
h = sub_once(h, old, new, "hpp method")
HPP.write_text(h)
print("hpp ok")

# --- cpp: spec_ok ---
c = CPP.read_text()
old = """    if (s.kind == BallotKind::Choice) {
        if (s.options.size() < 2 || s.options.size() > 12) return false;
        for (const auto& o : s.options) {
            if (o.empty() || o.size() > 80) return false;
        }
    }
"""
new = """    if (s.kind == BallotKind::Choice) {
        const std::size_t mn = s.open_choice ? 1 : 2;
        const std::size_t mx = s.open_choice ? 48 : 12;
        if (s.options.size() < mn || s.options.size() > mx) return false;
        for (const auto& o : s.options) {
            if (o.empty() || o.size() > 80) return false;
        }
    }
"""
c = sub_once(c, old, new, "spec_ok")

# add_proposal_option after add_proposal
old = """    flush();
    return p.id;
}

std::vector<Proposal> FileStore::all_proposals() const {
"""
new = """    flush();
    return p.id;
}

namespace {
std::string trim_label(std::string s) {
    std::size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\\t' || s[a] == '\\n' || s[a] == '\\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\\t' || s[b - 1] == '\\n' || s[b - 1] == '\\r')) --b;
    return s.substr(a, b - a);
}
}

bool FileStore::add_proposal_option(std::uint64_t proposal_id, const std::string& username, const std::string& label) {
    if (username.empty()) return false;
    const std::string lab = trim_label(label);
    if (lab.empty() || lab.size() > 80) return false;
    Proposal* p = nullptr;
    for (auto& it : proposals_) {
        if (it.id == proposal_id) { p = &it; break; }
    }
    if (!p || p->ballot.kind != BallotKind::Choice) return false;
    if (!p->ballot.open_choice && username != p->author) return false;
    if (p->ballot.options.size() >= 48) return false;
    if (p->ballot.option_authors.size() != p->ballot.options.size())
        p->ballot.option_authors.resize(p->ballot.options.size());
    const std::string want = lower_copy(lab);
    for (const auto& o : p->ballot.options) {
        if (lower_copy(o) == want) return false;
    }
    if (username != p->author) {
        int n = 0;
        for (const auto& a : p->ballot.option_authors) if (a == username) ++n;
        if (n >= 1) return false;
    }
    p->ballot.options.push_back(lab);
    p->ballot.option_authors.push_back(username == p->author ? std::string() : username);
    flush();
    return true;
}

std::vector<Proposal> FileStore::all_proposals() const {
"""
c = sub_once(c, old, new, "add_proposal_option")

# load tags before unknown
old = """        } else {
            throw std::runtime_error("LGP1: unknown record type '" + tag + "'");
        }
"""
new = """        } else if (tag == "popen") {
            if (f.size() != 2) throw std::runtime_error("LGP1: bad popen record");
            pending_popen.insert(parse_u64(f[1], "popen proposal id"));
        } else if (tag == "pba") {
            if (f.size() != 4) throw std::runtime_error("LGP1: bad pba record");
            const auto pid = parse_u64(f[1], "pba proposal id");
            const auto idx = parse_u64(f[2], "pba index");
            const auto who = pct_decode(f[3]);
            if (!who.empty()) pending_pba[pid].push_back({static_cast<int>(idx), who});
        } else {
            throw std::runtime_error("LGP1: unknown record type '" + tag + "'");
        }
"""
c = sub_once(c, old, new, "load tags")

# pending maps next to pending_bo
old = """    std::map<std::uint64_t, std::vector<std::pair<int, std::string>>> pending_bo;
"""
new = """    std::map<std::uint64_t, std::vector<std::pair<int, std::string>>> pending_bo;
    std::set<std::uint64_t> pending_popen;
    std::map<std::uint64_t, std::vector<std::pair<int, std::string>>> pending_pba;
"""
c = sub_once(c, old, new, "pending maps")

old = """        auto oit = pending_bo.find(p.id);
        if (oit != pending_bo.end()) {
            auto opts = oit->second;
            std::sort(opts.begin(), opts.end());
            p.ballot.options.clear();
            for (const auto& item : opts) p.ballot.options.push_back(item.second);
        }
    }
}
"""
new = """        auto oit = pending_bo.find(p.id);
        if (oit != pending_bo.end()) {
            auto opts = oit->second;
            std::sort(opts.begin(), opts.end());
            p.ballot.options.clear();
            p.ballot.option_authors.clear();
            for (const auto& item : opts) {
                p.ballot.options.push_back(item.second);
                p.ballot.option_authors.push_back("");
            }
        }
        if (pending_popen.count(p.id)) p.ballot.open_choice = true;
        auto ait = pending_pba.find(p.id);
        if (ait != pending_pba.end()) {
            if (p.ballot.option_authors.size() != p.ballot.options.size())
                p.ballot.option_authors.resize(p.ballot.options.size());
            for (const auto& item : ait->second) {
                if (item.first >= 0 && static_cast<std::size_t>(item.first) < p.ballot.option_authors.size())
                    p.ballot.option_authors[static_cast<std::size_t>(item.first)] = item.second;
            }
        }
    }
}
"""
c = sub_once(c, old, new, "apply load")

old = """        for (std::size_t i = 0; i < p.ballot.options.size(); ++i) {
            body << "pbo\\t" << p.id << "\\t" << i << "\\t" << pct_encode(p.ballot.options[i]) << "\\n";
        }
    }
"""
new = """        for (std::size_t i = 0; i < p.ballot.options.size(); ++i) {
            body << "pbo\\t" << p.id << "\\t" << i << "\\t" << pct_encode(p.ballot.options[i]) << "\\n";
        }
        if (p.ballot.open_choice) body << "popen\\t" << p.id << "\\n";
        if (p.ballot.option_authors.size() == p.ballot.options.size()) {
            for (std::size_t i = 0; i < p.ballot.option_authors.size(); ++i) {
                if (p.ballot.option_authors[i].empty()) continue;
                body << "pba\\t" << p.id << "\\t" << i << "\\t" << pct_encode(p.ballot.option_authors[i]) << "\\n";
            }
        }
    }
"""
c = sub_once(c, old, new, "flush")
CPP.write_text(c)
print("cpp ok")

# --- tests ---
t = TEST.read_text()
old = """    unlink(path.c_str());
    unlink((path + ".tmp").c_str());
    std::cout << "\\nall file_store tests passed\\n";
"""
new = """    {
        FileStore store(path);
        auto a = create_account(store, breach, "oc-a", "correct-horse-battery-staple", "+15559200000");
        auto b = create_account(store, breach, "oc-b", "correct-horse-battery-staple", "+15559200001");
        assert(a.error == AuthError::None && b.error == AuthError::None);

        BallotSpec closed;
        closed.kind = BallotKind::Choice;
        closed.options = {"Red", "Blue"};
        auto cid = store.add_proposal("oc-a", "Closed colors", "No write-ins.", closed);
        assert(cid != 0);
        assert(!store.add_proposal_option(cid, "oc-b", "Green"));
        assert(store.add_proposal_option(cid, "oc-a", "Green"));
        assert(store.proposal_by_id(cid)->ballot.options.size() == 3);

        BallotSpec open;
        open.kind = BallotKind::Choice;
        open.open_choice = true;
        open.options = {"Alpha"};
        auto oid = store.add_proposal("oc-a", "Name it", "Add yours.", open);
        assert(oid != 0);
        assert(store.add_proposal_option(oid, "oc-b", "Beta"));
        assert(!store.add_proposal_option(oid, "oc-b", "Gamma")); // second add blocked
        assert(store.add_proposal_option(oid, "oc-a", "Gamma"));  // author unlimited
        assert(!store.add_proposal_option(oid, "oc-b", "beta"));  // duplicate, different case
        auto got = store.proposal_by_id(oid);
        assert(got && got->ballot.open_choice);
        assert(got->ballot.options.size() == 3);
        assert(got->ballot.options[1] == "Beta");
        assert(got->ballot.option_authors[1] == "oc-b");
        assert(got->ballot.option_authors[2].empty());
        const std::string disk = slurp(path);
        assert(disk.find("\\npopen\\t") != std::string::npos);
        assert(disk.find("\\npba\\t") != std::string::npos);
        std::cout << "open-choice add/cap/duplicate ok\\n";
    }
    {
        FileStore store(path);
        bool found = false;
        for (const auto& p : store.all_proposals()) {
            if (p.title != "Name it") continue;
            found = true;
            assert(p.ballot.open_choice);
            assert(p.ballot.options.size() == 3);
            assert(p.ballot.option_authors.size() == 3);
            assert(p.ballot.option_authors[1] == "oc-b");
        }
        assert(found);
        std::cout << "open-choice popen/pba survive reload\\n";
    }

    unlink(path.c_str());
    unlink((path + ".tmp").c_str());
    std::cout << "\\nall file_store tests passed\\n";
"""
t = sub_once(t, old, new, "tests")
TEST.write_text(t)
print("tests ok")

# --- http_server UI ---
hs = HTTP.read_text()

old = '''        case BallotKind::Choice: kindEyebrow = "Choice"; break;'''
new = '''        case BallotKind::Choice: kindEyebrow = p.ballot.open_choice ? "Open choice" : "Choice"; break;'''
hs = sub_once(hs, old, new, "eyebrow")

old = '''        list += "</div>";
        body = bar + "<p class=\\"muted\\">"+std::to_string(total)+" responses</p>" + list;
    } else {'''
new = '''        list += "</div>";
        if (!viewer.empty() && p.ballot.open_choice) {
            bool already = false;
            for (const auto& a : p.ballot.option_authors) if (a == viewer) { already = true; break; }
            const bool can_add = (viewer == p.author) || !already;
            if (can_add) {
                list += "<form method=\\"POST\\" action=\\"/proposals/option\\" style=\\"margin-top:.7rem\\">"
                        "<input type=\\"hidden\\" name=\\"id\\" value=\\""+std::to_string(p.id)+"\\">"
                        "<label>Add a choice"
                        + std::string(viewer == p.author ? "" : " <span class=\\"muted\\">(one per person)</span>")
                        + "</label>"
                        "<div class=\\"row\\"><input name=\\"label\\" maxlength=\\"80\\" required placeholder=\\"Your option\\">"
                        "<button type=\\"submit\\">Add</button></div></form>";
            } else {
                list += "<p class=\\"muted\\">You already added a choice on this one.</p>";
            }
        }
        body = bar + "<p class=\\"muted\\">"+std::to_string(total)+" responses</p>" + list;
    } else {'''
hs = sub_once(hs, old, new, "writein form")

old = '''                "<div id=\\"bchoice\\" hidden>"
                "<label>Options (one per line, 2–12)</label>"
                "<textarea name=\\"options\\" maxlength=\\"800\\" placeholder=\\"One option per line\\"></textarea>"
                "</div>"'''
new = '''                "<div id=\\"bchoice\\" hidden>"
                "<label>Options (one per line)</label>"
                "<textarea name=\\"options\\" maxlength=\\"800\\" placeholder=\\"One option per line\\"></textarea>"
                "<label class=\\"row\\" style=\\"gap:.4rem;align-items:center;margin-top:.4rem\\">"
                "<input type=\\"checkbox\\" name=\\"open\\" value=\\"1\\"> "
                "Allow people to add a choice (one each, besides you)</label>"
                "</div>"'''
hs = sub_once(hs, old, new, "create checkbox")

old = '''        } else if (spec.kind == BallotKind::Choice) {
            std::istringstream in(f["options"]);
            std::string line;
            while (std::getline(in, line)) {
                auto o = trim_ws(line);
                if (!o.empty()) spec.options.push_back(o);
            }
            if (spec.options.size() < 2 || spec.options.size() > 12) return http_redir("/proposals?err=options");
        } else if (spec.kind == BallotKind::Scale) {'''
new = '''        } else if (spec.kind == BallotKind::Choice) {
            spec.open_choice = !trim_ws(f["open"]).empty();
            std::istringstream in(f["options"]);
            std::string line;
            while (std::getline(in, line)) {
                auto o = trim_ws(line);
                if (!o.empty()) spec.options.push_back(o);
            }
            const std::size_t mn = spec.open_choice ? 1 : 2;
            if (spec.options.size() < mn || spec.options.size() > 12) return http_redir("/proposals?err=options");
        } else if (spec.kind == BallotKind::Scale) {'''
hs = sub_once(hs, old, new, "create parser")

old = '''    if(method=="POST"&&path=="/proposals/vote") {'''
new = '''    if(method=="POST"&&path=="/proposals/option") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        std::uint64_t id = 0;
        try { id = std::stoull(f["id"]); } catch (...) { return http_redir("/proposals"); }
        if (!g_store.add_proposal_option(id, user, trim_ws(f["label"])))
            return http_redir("/proposals?err=option");
        return http_redir("/proposals");
    }
    if(method=="POST"&&path=="/proposals/vote") {'''
hs = sub_once(hs, old, new, "option POST")

old = '''    if (e=="nogroup") return "<p class=\\"warn\\">Pick at least one group, or turn Groups off.</p>";'''
new = '''    if (e=="nogroup") return "<p class=\\"warn\\">Pick at least one group, or turn Groups off.</p>";
    if (e=="options") return "<p class=\\"warn\\">Need 2–12 options, or 1–12 if people can add their own.</p>";
    if (e=="option") return "<p class=\\"warn\\">Could not add that choice. Duplicate, empty, or you already added one.</p>";'''
hs = sub_once(hs, old, new, "flash")

HTTP.write_text(hs)
print("http ok")
print("ALL_PATCHES_OK")
