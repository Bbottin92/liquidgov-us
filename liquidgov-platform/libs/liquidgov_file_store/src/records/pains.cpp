// pains: Wishes and pain points found in a person's own writing.
// LGP1 record types: pain painn (format: the header comment of file_store.hpp).
// Split out of file_store.cpp 2026-11-12 (code moved, not rewritten). Listed in ../../sources.cmake.
#include "liquidgov/platform/file_store.hpp"

#include "core/codec.hpp"
#include "core/load_pending.hpp"
#include "liquidgov/platform/clock.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <ostream>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace liquidgov::platform {

using namespace store_detail;

// ---- Wishes and pain points ----------------------------------------------

namespace {

std::string pain_lower(const std::string& s) {
    std::string o = s;
    for (char& c : o) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return o;
}
bool pain_alnum(unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c >= 0x80; }
// Curly quotes/apostrophes to plain ones, so "I’m tired of" matches.
std::string pain_normalize(const std::string& s) {
    std::string o;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (i + 2 < s.size() && static_cast<unsigned char>(s[i]) == 0xE2 && static_cast<unsigned char>(s[i + 1]) == 0x80) {
            const unsigned char c = static_cast<unsigned char>(s[i + 2]);
            if (c == 0x98 || c == 0x99) { o += '\''; i += 2; continue; }
            if (c == 0x9C || c == 0x9D) { o += '"'; i += 2; continue; }
        }
        o += s[i];
    }
    return o;
}
std::string pain_stem(std::string w) {
    const auto ends = [&](const char* suf) {
        const std::size_t n = std::strlen(suf);
        return w.size() > n && w.compare(w.size() - n, n, suf) == 0;
    };
    if (w.size() > 4 && (ends("ses") || ends("xes") || ends("ches") || ends("shes"))) w.resize(w.size() - 2);
    else if (w.size() > 3 && ends("s") && !ends("ss")) w.resize(w.size() - 1);
    return w;
}
std::vector<std::string> pain_words(const std::string& text) {
    std::vector<std::string> out;
    std::string cur;
    for (unsigned char c : pain_lower(text)) {
        if (pain_alnum(c) || c == '\'') { if (c != '\'') cur += static_cast<char>(c); }
        else if (!cur.empty()) { out.push_back(cur); cur.clear(); }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

} // namespace

std::string pain_subject_key(const std::string& phrase) {
    std::string o;
    bool sp = false;
    for (unsigned char c : phrase) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!o.empty()) sp = true;
            continue;
        }
        if (sp) { o.push_back(' '); sp = false; }
        if (c >= 'A' && c <= 'Z') c = static_cast<unsigned char>(c - 'A' + 'a');
        o.push_back(static_cast<char>(c));
    }
    return o;
}

std::vector<std::string> pain_keywords(const std::string& phrase) {
    static const std::set<std::string> kFiller = {
        "that", "this", "with", "when", "what", "were", "they", "them", "then", "than", "have", "just", "like",
        "really", "would", "could", "should", "there", "their", "about", "always", "never", "every", "because",
        "from", "into", "more", "much", "some", "still", "very", "been", "being", "make", "makes", "made", "feel",
        "want", "wish", "hate", "tired", "only", "people", "thing", "things", "stuff", "time", "times", "also",
        "even", "dont", "doesnt", "didnt", "isnt", "arent", "wasnt", "cant", "wont", "your", "mine", "yours",
        "ours", "here", "where", "which", "while", "other", "again", "going", "gets", "getting", "happen",
        "happens", "actually", "someone", "something", "anyone", "everyone", "nobody", "everything", "anything",
        "kind", "sort", "lot", "lots", "yeah", "okay", "gonna", "wanna", "great", "good", "better", "best", "worse"};
    std::vector<std::string> out;
    for (const auto& w : pain_words(phrase)) {
        if (w.size() < 4 || kFiller.count(w)) continue;
        if (std::find(out.begin(), out.end(), w) == out.end()) out.push_back(w);
    }
    return out;
}

bool pain_words_overlap(const std::vector<std::string>& keywords, const std::string& text) {
    if (keywords.empty()) return false;
    std::set<std::string> have;
    for (const auto& w : pain_words(text)) have.insert(pain_stem(w));
    for (const auto& k : keywords) if (have.count(pain_stem(k))) return true;
    return false;
}

PainScan find_pain_points(const std::string& raw) {
    PainScan scan;
    const std::string norm = pain_normalize(raw);
    const std::string low_all = pain_lower(norm);
    // Crisis words anywhere (quotes included): never store, never connect.
    static const char* kCrisis[] = {
        "wasn't here", "wasnt here", "weren't here", "werent here", "not be here anymore", "not wake up",
        "kill myself", "killing myself", "end my life", "end it all", "want to die", "wanna die",
        "wish i was dead", "wish i were dead", "suicid", "hurt myself", "harm myself", "self harm", "self-harm",
        "no reason to live", "better off without me", "better off dead", "disappear forever"};
    for (const char* c : kCrisis)
        if (low_all.find(c) != std::string::npos) { scan.crisis = true; return scan; }

    // Blank out quoted text and quoted lines ('>'), keeping byte positions.
    std::string text = norm;
    bool in_quote = false, line_start = true, quoted_line = false;
    for (char& ch : text) {
        if (line_start) { quoted_line = ch == '>'; line_start = false; }
        if (ch == '\n') { line_start = true; quoted_line = false; in_quote = false; continue; }
        if (ch == '"') { in_quote = !in_quote; ch = ' '; continue; }
        if (in_quote || quoted_line) ch = ' ';
    }
    const std::string low = pain_lower(text);
    struct Trig { std::string words; const char* kind; };
    // "I wish", and the same with one softener: "I really wish", "I'm so tired of".
    static const std::vector<Trig> kTrig = [] {
        std::vector<Trig> t = {{"it would be great if", "wish"}, {"it'd be great if", "wish"}, {"if only", "wish"}};
        const char* soft[] = {"", "really ", "just ", "so ", "truly ", "honestly ", "totally ", "really really "};
        for (const char* s : soft) {
            const std::string m = s;
            t.push_back({"i " + m + "wish", "wish"});
            t.push_back({"i " + m + "hate it when", "pain"});
            t.push_back({"i " + m + "hate when", "pain"});
            t.push_back({"i " + m + "hate that", "pain"});
            for (const char* who : {"i'm ", "im ", "i am "}) t.push_back({std::string(who) + m + "tired of", "pain"});
        }
        return t;
    }();
    std::vector<std::pair<std::size_t, const Trig*>> found;
    for (const auto& t : kTrig) {
        const std::size_t n = t.words.size();
        for (std::size_t at = low.find(t.words); at != std::string::npos; at = low.find(t.words, at + 1)) {
            const bool left_ok = at == 0 || (!pain_alnum(static_cast<unsigned char>(low[at - 1])) && low[at - 1] != '\'');
            const bool right_ok = at + n >= low.size() || !pain_alnum(static_cast<unsigned char>(low[at + n]));
            if (left_ok && right_ok) found.push_back({at, &t});
        }
    }
    std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    std::size_t covered = 0;
    for (const auto& [at, t] : found) {
        if (scan.hits.size() >= 3) break;
        if (at < covered) continue; // one hit per clause
        std::size_t i = at + t->words.size();
        std::size_t end = i;
        while (end < text.size() && std::strchr(".!?;\n", text[end]) == nullptr) ++end;
        covered = end;
        std::string clause = text.substr(i, end - i);
        // At most 12 words.
        std::size_t words = 0, k = 0;
        bool in_word = false;
        for (; k < clause.size(); ++k) {
            const bool sp = clause[k] == ' ' || clause[k] == '\t' || clause[k] == '\r';
            if (!sp && !in_word) { if (++words > 12) break; }
            in_word = !sp;
        }
        clause = clause.substr(0, k);
        clause = clean_city(clause); // whitespace tidy + 80-byte UTF-8-safe cut
        while (!clause.empty() && std::strchr(",:-( ", clause.back())) clause.pop_back();
        while (!clause.empty() && std::strchr(",:- ", clause.front())) clause.erase(0, 1);
        const std::string first = pain_words(clause).empty() ? "" : pain_words(clause).front();
        if (std::string(t->kind) == "wish" && (first == "you" || first == "u" || first == "everyone" || first == "all"))
            continue; // "I wish you a happy birthday"
        if (pain_keywords(clause).empty()) continue; // no subject after the phrase
        bool dup = false;
        for (const auto& h : scan.hits) if (pain_lower(h.phrase) == pain_lower(clause)) dup = true;
        if (!dup) scan.hits.push_back({t->kind, clause});
    }
    return scan;
}

std::uint64_t FileStore::add_pain_suggestion(const std::string& owner, const PainHit& hit, const std::string& source) {
    if (owner.empty() || hit.phrase.empty() || source.empty()) return 0;
    if (hit.kind != "wish" && hit.kind != "pain") return 0;
    for (const auto& p : pains_)
        if (p.owner == owner && p.source == source && pain_lower(p.phrase) == pain_lower(hit.phrase)) return 0;
    PainPoint p;
    p.id = next_pain_id_++;
    p.owner = owner;
    p.kind = hit.kind;
    p.phrase = hit.phrase;
    p.source = source;
    p.ts = now();
    pains_.push_back(p);
    flush();
    return p.id;
}

std::vector<PainPoint> FileStore::pains_of(const std::string& owner) const {
    std::vector<PainPoint> out;
    for (auto it = pains_.rbegin(); it != pains_.rend(); ++it) if (it->owner == owner) out.push_back(*it);
    return out;
}

std::optional<PainPoint> FileStore::pain_by_id(std::uint64_t id) const {
    for (const auto& p : pains_) if (p.id == id) return p;
    return std::nullopt;
}

std::vector<PainPoint> FileStore::flowed_with_subject(const std::string& phrase) const {
    std::vector<PainPoint> out;
    const std::string key = pain_subject_key(phrase);
    if (key.empty()) return out;
    for (auto it = pains_.rbegin(); it != pains_.rend(); ++it) {
        if (!it->kept || !it->flowed) continue;
        if (pain_subject_key(it->phrase) == key) out.push_back(*it);
    }
    return out;
}

bool FileStore::keep_pain(const std::string& owner, std::uint64_t id) {
    for (auto& p : pains_) {
        if (p.id != id) continue;
        if (p.owner != owner) return false;
        if (!p.kept) { p.kept = true; flush(); }
        return true;
    }
    return false;
}

bool FileStore::flow_pain(const std::string& owner, std::uint64_t id, bool on) {
    for (auto& p : pains_) {
        if (p.id != id) continue;
        if (p.owner != owner || !p.kept) return false;
        if (p.flowed == on) return true;
        p.flowed = on;
        flush();
        return true;
    }
    return false;
}

bool FileStore::forget_pain(const std::string& owner, std::uint64_t id) {
    for (auto it = pains_.begin(); it != pains_.end(); ++it) {
        if (it->id != id) continue;
        if (it->owner != owner) return false;
        pains_.erase(it);
        flush();
        return true;
    }
    return false;
}

void FileStore::clear_pains(const std::string& owner) {
    const auto before = pains_.size();
    pains_.erase(std::remove_if(pains_.begin(), pains_.end(), [&](const PainPoint& p) { return p.owner == owner; }),
                 pains_.end());
    if (pains_.size() != before) flush();
}

void FileStore::prune_pains() {
    const std::int64_t cutoff = now() - 30LL * 24 * 3600;
    // A prefix not named here is deleted on this flush. post/prop/prply are the
    // public ones. fpost/gpost/mail/gmsg are this owner's own writing and stay
    // only while that writing still exists. mail is mail:<ts>:<recipient>, so
    // it cannot share the numeric-id check.
    auto digits = [](const std::string& num, std::uint64_t& id) {
        if (num.empty() || num.size() > 19 || num.find_first_not_of("0123456789") != std::string::npos) return false;
        id = std::stoull(num);
        return true;
    };
    auto source_alive = [&](const PainPoint& pain) {
        const auto colon = pain.source.find(':');
        if (colon == std::string::npos || colon == 0) return false;
        const std::string type = pain.source.substr(0, colon);
        const std::string rest = pain.source.substr(colon + 1);
        if (type == "mail") {
            const auto split = rest.find(':');
            if (split == std::string::npos || split == 0) return false;
            std::uint64_t ts_u = 0;
            if (!digits(rest.substr(0, split), ts_u)) return false;
            const std::string to = rest.substr(split + 1);
            if (to.empty()) return false;
            const auto ts = static_cast<std::int64_t>(ts_u);
            for (const auto& m : mail_) {
                if (m.from == pain.owner && m.to == to && m.ts == ts) return true;
            }
            return false;
        }
        std::uint64_t id = 0;
        if (!digits(rest, id)) return false;
        if (type == "post" || type == "fpost" || type == "gpost") {
            for (const auto& p : posts_) if (p.id == id) return true;
            return false;
        }
        if (type == "prop") {
            for (const auto& p : proposals_) if (p.id == id) return true;
            return false;
        }
        if (type == "prply") {
            for (const auto& r : proposal_replies_) if (r.id == id) return true;
            return false;
        }
        if (type == "gmsg") {
            auto it = messages_.find(id);
            if (it == messages_.end() || it->second.deletion != DeletionState::NotDeleted) return false;
            auto owner = find_by_username(pain.owner);
            return owner && owner->id == it->second.sender;
        }
        return false;
    };
    pains_.erase(std::remove_if(pains_.begin(), pains_.end(), [&](const PainPoint& p) {
        return (!p.kept && p.ts < cutoff) || !source_alive(p) || !find_by_username(p.owner);
    }), pains_.end());
}

// The record types this module reads (one branch each, as they were in load()'s chain).
bool FileStore::read_pains(const std::vector<std::string>& f, LoadPending& pending) {
    (void)pending;
    const std::string& tag = f[0];
    if (tag == "pain") {
        // 8 or 9 fields, or more from a newer version (kept verbatim in extra_fields).
        if (f.size() < 8) throw std::runtime_error("LGP1: bad pain record");
        PainPoint p;
        p.id = parse_u64(f[1], "pain id");
        p.owner = pct_decode(f[2]);
        p.kind = pct_decode(f[3]);
        if (f[4] != "0" && f[4] != "1") throw std::runtime_error("LGP1: bad pain kept flag");
        p.kept = f[4] == "1";
        p.ts = parse_i64(f[5], "pain ts");
        p.source = pct_decode(f[6]);
        p.phrase = pct_decode(f[7]);
        if (f.size() >= 9) {
            if (f[8] != "0" && f[8] != "1") throw std::runtime_error("LGP1: bad pain flowed flag");
            p.flowed = f[8] == "1";
        }
        for (std::size_t i = 9; i < f.size(); ++i) p.extra_fields.push_back(f[i]); // still percent-encoded
        if (!p.kept) p.flowed = false; // a suggestion is never public
        if (!p.owner.empty() && !p.phrase.empty() && (p.kind == "wish" || p.kind == "pain")) {
            pains_.push_back(p);
            if (p.id >= next_pain_id_) next_pain_id_ = p.id + 1;
        }
        return true;
    }
    if (tag == "painn") {
        if (f.size() != 2) throw std::runtime_error("LGP1: bad painn record");
        const auto n = parse_u64(f[1], "next_pain_id");
        if (n > next_pain_id_) next_pain_id_ = n;
        return true;
    }
    return false;
}

// Save: this module's records, at their place in the store (flush() calls the sections in order).
void FileStore::write_pains(std::ostream& body) {
    prune_pains();
    body << "painn\t" << next_pain_id_ << "\n";
    for (const auto& p : pains_) {
        body << "pain\t" << p.id << "\t" << pct_encode(p.owner) << "\t" << pct_encode(p.kind) << "\t" << (p.kept ? 1 : 0)
             << "\t" << p.ts << "\t" << pct_encode(p.source) << "\t" << pct_encode(p.phrase);
        // 9th field only when Flowed. A Still store stays 8 fields, which the
        // binary that first shipped pain records can still open.
        if ((p.kept && p.flowed) || !p.extra_fields.empty()) body << "\t" << (p.kept && p.flowed ? 1 : 0);
        for (const auto& x : p.extra_fields) body << "\t" << x; // a newer version's fields, untouched
        body << "\n";
    }
}

} // namespace liquidgov::platform
