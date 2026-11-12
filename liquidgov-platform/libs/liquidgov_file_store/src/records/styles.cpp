// styles: the profile style library (ProfileStyle, StyleReport).
// LGP1 record types: sty styn stu str (format: the header comment of file_store.hpp).
// Moved out of file_store.cpp 2026-11-13 (code moved, not rewritten). Listed in ../../sources.cmake.
#include "liquidgov/platform/file_store.hpp"

#include "core/codec.hpp"
#include "core/load_pending.hpp"

#include "liquidgov/platform/clock.hpp"

#include <algorithm>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace liquidgov::platform {

using namespace store_detail;

// ---- profile style library (sty/styn/stu/str; Brandon 2026-11-12, profile CSS slice 2)

std::optional<std::uint64_t> FileStore::publish_style(const std::string& author, const std::string& name,
                                                      const std::string& css) {
    if (author.empty() || name.empty() || css.empty() || !by_username_.count(author)) return std::nullopt;
    ProfileStyle s;
    s.id = next_style_id_++;
    s.author = author;
    s.created_at = now();
    s.name = name;
    s.css = css;
    styles_[s.id] = s;
    flush();
    return s.id;
}

std::vector<ProfileStyle> FileStore::styles() const {
    std::vector<ProfileStyle> out;
    for (auto it = styles_.rbegin(); it != styles_.rend(); ++it) out.push_back(it->second);  // ids only grow
    return out;
}

std::optional<ProfileStyle> FileStore::style(std::uint64_t id) const {
    auto it = styles_.find(id);
    if (it == styles_.end()) return std::nullopt;
    return it->second;
}

bool FileStore::remove_style(std::uint64_t id) {
    if (!styles_.erase(id)) return false;
    style_uses_.erase(id);
    style_reports_.erase(id);
    flush();
    return true;
}

bool FileStore::use_style(std::uint64_t id, const std::string& username) {
    if (!styles_.count(id) || !by_username_.count(username)) return false;
    // strictly newer than any earlier use by this person, so "based on" follows the last copy even within a second
    std::int64_t ts = now();
    for (const auto& [sid, people] : style_uses_) {
        auto u = people.find(username);
        if (u != people.end() && u->second >= ts) ts = u->second + 1;
    }
    style_uses_[id][username] = ts;
    flush();
    return true;
}

std::size_t FileStore::style_use_count(std::uint64_t id) const {
    auto s = styles_.find(id);
    auto it = style_uses_.find(id);
    if (s == styles_.end() || it == style_uses_.end()) return 0;
    std::size_t n = 0;
    for (const auto& [who, ts] : it->second) if (who != s->second.author) ++n;
    return n;
}

std::optional<ProfileStyle> FileStore::style_based_on(const std::string& username) const {
    std::optional<ProfileStyle> best;
    std::int64_t best_ts = 0;
    for (const auto& [id, people] : style_uses_) {
        auto u = people.find(username);
        auto s = styles_.find(id);
        if (u == people.end() || s == styles_.end()) continue;
        if (!best || u->second > best_ts || (u->second == best_ts && id > best->id)) {
            best = s->second;
            best_ts = u->second;
        }
    }
    return best;
}

bool FileStore::report_style(std::uint64_t id, const std::string& reporter, const std::string& reason) {
    if (!styles_.count(id) || !by_username_.count(reporter)) return false;
    StyleReport r;
    r.style_id = id;
    r.reporter = reporter;
    r.at = now();
    r.reason = reason;
    style_reports_[id][reporter] = r;
    flush();
    return true;
}

std::vector<StyleReport> FileStore::style_reports(std::uint64_t id) const {
    std::vector<StyleReport> out;
    auto it = style_reports_.find(id);
    if (it == style_reports_.end()) return out;
    for (const auto& [who, r] : it->second) out.push_back(r);
    std::stable_sort(out.begin(), out.end(), [](const StyleReport& a, const StyleReport& b) { return a.at < b.at; });
    return out;
}

bool FileStore::dismiss_style_reports(std::uint64_t id) {
    if (!styles_.count(id)) return false;
    style_reports_.erase(id);
    flush();
    return true;
}

// The record types this module reads (one branch each, as they were in load()'s chain).
bool FileStore::read_styles(const std::vector<std::string>& f, LoadPending& pending) {
    (void)pending;
    const std::string& tag = f[0];
    if (tag == "sty") {
        // lower bounds (not exact counts): a later release may add trailing fields without breaking a rollback
        if (f.size() < 6) throw std::runtime_error("LGP1: bad sty record");
        ProfileStyle s;
        s.id = parse_u64(f[1], "style id");
        s.author = pct_decode(f[2]);
        s.created_at = parse_i64(f[3], "style created_at");
        s.name = pct_decode(f[4]);
        s.css = pct_decode(f[5]);
        if (s.id == 0 || s.author.empty()) throw std::runtime_error("LGP1: empty sty record");
        if (s.id >= next_style_id_) next_style_id_ = s.id + 1;
        styles_[s.id] = s;
        return true;
    }
    if (tag == "styn") {
        if (f.size() < 2) throw std::runtime_error("LGP1: bad styn record");
        const auto n = parse_u64(f[1], "next_style_id");
        if (n > next_style_id_) next_style_id_ = n;
        return true;
    }
    if (tag == "stu") {
        if (f.size() < 4) throw std::runtime_error("LGP1: bad stu record");
        const auto sid = parse_u64(f[1], "style use id");
        const std::string who = pct_decode(f[2]);
        if (!who.empty()) style_uses_[sid][who] = parse_i64(f[3], "style use ts");
        return true;
    }
    if (tag == "str") {
        if (f.size() < 5) throw std::runtime_error("LGP1: bad str record");
        StyleReport r;
        r.style_id = parse_u64(f[1], "style report id");
        r.reporter = pct_decode(f[2]);
        r.at = parse_i64(f[3], "style report ts");
        r.reason = pct_decode(f[4]);
        if (!r.reporter.empty()) style_reports_[r.style_id][r.reporter] = r;
        return true;
    }
    return false;
}

// After every line is read: style library orphans (as load() did, right after the attachment-name prune).
void FileStore::finish_styles() {
    for (auto it = styles_.begin(); it != styles_.end(); )
        it = by_username_.count(it->second.author) ? std::next(it) : styles_.erase(it);
    for (auto* rows : {&style_uses_}) {
        for (auto it = rows->begin(); it != rows->end(); ) {
            if (!styles_.count(it->first)) { it = rows->erase(it); continue; }
            for (auto u = it->second.begin(); u != it->second.end(); )
                u = by_username_.count(u->first) ? std::next(u) : it->second.erase(u);
            it = it->second.empty() ? rows->erase(it) : std::next(it);
        }
    }
    for (auto it = style_reports_.begin(); it != style_reports_.end(); ) {
        if (!styles_.count(it->first)) { it = style_reports_.erase(it); continue; }
        for (auto r = it->second.begin(); r != it->second.end(); )
            r = by_username_.count(r->first) ? std::next(r) : it->second.erase(r);
        it = it->second.empty() ? style_reports_.erase(it) : std::next(it);
    }
}

// Save: this module's records, at their place in the store (flush() calls the sections in order).
void FileStore::write_styles(std::ostream& body) {
    // only once a style was ever published: until then the store stays readable by the version before the library
    if (next_style_id_ > 1) body << "styn\t" << next_style_id_ << "\n";
    for (const auto& [id, s] : styles_) {
        body << "sty\t" << id << "\t" << pct_encode(s.author) << "\t" << s.created_at << "\t" << pct_encode(s.name)
             << "\t" << pct_encode(s.css) << "\n";
    }
    for (const auto& [id, people] : style_uses_) {
        if (!styles_.count(id)) continue;
        for (const auto& [who, ts] : people) body << "stu\t" << id << "\t" << pct_encode(who) << "\t" << ts << "\n";
    }
    for (const auto& [id, reps] : style_reports_) {
        if (!styles_.count(id)) continue;
        for (const auto& [who, r] : reps)
            body << "str\t" << id << "\t" << pct_encode(who) << "\t" << r.at << "\t" << pct_encode(r.reason) << "\n";
    }
}

// Account deletion: the person's styles (with their uses and reports), their uses and their reports.
void FileStore::forget_styles_of(const std::string& name) {
    for (auto it = styles_.begin(); it != styles_.end(); ) {
        if (it->second.author == name) {
            style_uses_.erase(it->first);
            style_reports_.erase(it->first);
            it = styles_.erase(it);
        } else {
            ++it;
        }
    }
    for (auto& [sid, people] : style_uses_) people.erase(name);
    for (auto& [sid, reps] : style_reports_) reps.erase(name);
}

} // namespace liquidgov::platform
