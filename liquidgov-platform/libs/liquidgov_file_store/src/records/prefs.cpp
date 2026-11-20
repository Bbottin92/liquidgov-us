// prefs: Account settings, map privacy, findability and name search.
// LGP1 record types: pref mapp (format: the header comment of file_store.hpp).
// Split out of file_store.cpp 2026-11-12 (code moved, not rewritten). Listed in ../../sources.cmake.
#include "liquidgov/platform/file_store.hpp"

#include "core/codec.hpp"
#include "core/load_pending.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <map>
#include <ostream>
#include <stdexcept>
#include <vector>

namespace liquidgov::platform {

using namespace store_detail;

// fmix values: a rate is 0, 3, 5 or 10; a tab name is 1-20 lowercase letters (the feed's ?filter= names).
static bool feed_mix_every_ok(const std::string& s) { return s == "0" || s == "3" || s == "5" || s == "10"; }
static bool feed_mix_view_ok(const std::string& s) {
    if (s.empty() || s.size() > 20) return false;
    for (unsigned char ch : s) if (ch < 'a' || ch > 'z') return false;
    return true;
}


namespace {

bool starts_with_ci(const std::string& s, const std::string& prefix) {
    if (prefix.size() > s.size()) return false;
    for (std::size_t i = 0; i < prefix.size(); ++i) {
        char a = s[i], b = prefix[i];
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
        if (a != b) return false;
    }
    return true;
}

// Brandon 2026-11-16: "i want to be able to type in someone's name in search and find them. right now i have to know
// someones handle." A display name matches when it starts with what was typed, or when every typed word starts one of
// its words ("smi jo" finds "Jo Smith"). Same findability rules as the handle: this only widens WHAT is matched.
std::vector<std::string> words_ci(const std::string& s) {
    std::vector<std::string> w;
    std::string cur;
    for (unsigned char c : s) {
        if (c == ' ' || c == '\t' || c == '-' || c == '.' || c == ',') {
            if (!cur.empty()) { w.push_back(cur); cur.clear(); }
        } else {
            cur += static_cast<char>(c);
        }
    }
    if (!cur.empty()) w.push_back(cur);
    return w;
}

bool name_matches(const std::string& display, const std::string& typed) {
    if (display.empty()) return false;
    if (starts_with_ci(display, typed)) return true;
    const auto have = words_ci(display), want = words_ci(typed);
    if (want.empty()) return false;
    for (const auto& q : want) {
        bool hit = false;
        for (const auto& h : have) if (starts_with_ci(h, q)) { hit = true; break; }
        if (!hit) return false;
    }
    return true;
}

} // namespace

AccountPrefs FileStore::prefs_of(const std::string& username) const {
    auto it = prefs_.find(username);
    if (it == prefs_.end()) return AccountPrefs{};
    return it->second;
}

void FileStore::set_prefs(const std::string& username, const AccountPrefs& p) {
    prefs_[username] = p;
    flush();
}

MapPrefs FileStore::map_prefs_of(const std::string& username) const {
    auto it = map_prefs_.find(username);
    return it == map_prefs_.end() ? MapPrefs{} : it->second;
}

bool FileStore::has_map_prefs(const std::string& username) const {
    return map_prefs_.count(username) > 0;
}

std::vector<std::string> FileStore::all_usernames() const {
    std::vector<std::string> out;
    out.reserve(by_username_.size());
    for (const auto& [name, id] : by_username_) {
        (void)id;
        out.push_back(name);
    }
    return out; // by_username_ is a std::map, so already sorted
}

void FileStore::set_map_prefs(const std::string& username, const MapPrefs& p) {
    if (username.empty()) return;
    // Settings forms only know today's fields: keep a newer version's extras.
    auto old = map_prefs_.find(username);
    MapPrefs merged = p;
    if (old != map_prefs_.end() && merged.extra_fields.empty()) merged.extra_fields = old->second.extra_fields;
    map_prefs_[username] = merged;
    if (!p.pains) // off deletes everything it collected
        pains_.erase(std::remove_if(pains_.begin(), pains_.end(),
                                    [&](const PainPoint& x) { return x.owner == username; }), pains_.end());
    flush();
}

bool FileStore::is_findable_exact(const std::string& requester, const std::string& target) const {
    if (requester.empty() || target.empty()) return false;
    if (!username_exists(target)) return false;
    if (requester == target) return true;
    if (is_blocked(requester, target)) return false;
    if (are_friends(requester, target)) return true;
    auto p = prefs_of(target);
    return p.findability != Findability::FriendsOnly;
}

std::vector<std::string> FileStore::typeahead(const std::string& requester, const std::string& prefix) const {
    std::vector<std::string> out;
    if (prefix.size() < 3) return out;
    for (const auto& [name, id] : by_username_) {
        if (!starts_with_ci(name, prefix)) {
            auto pr = profiles_.find(name);
            if (pr == profiles_.end() || !name_matches(pr->second.display_name, prefix)) continue;
        }
        if (is_blocked(requester, name)) continue;
        if (requester == name) {
            out.push_back(name);
            continue;
        }
        if (are_friends(requester, name)) {
            out.push_back(name);
            continue;
        }
        auto p = prefs_of(name);
        if (p.findability == Findability::Typeahead) out.push_back(name);
    }
    std::sort(out.begin(), out.end(), [&](const std::string& a, const std::string& b) {
        const bool ea = lower_copy(a) == lower_copy(prefix);
        const bool eb = lower_copy(b) == lower_copy(prefix);
        if (ea != eb) return ea;
        const bool fa = are_friends(requester, a);
        const bool fb = are_friends(requester, b);
        if (fa != fb) return fa;
        return a < b;
    });
    if (out.size() > 20) out.resize(20);
    return out;
}

// The record types this module reads (one branch each, as they were in load()'s chain).
bool FileStore::read_prefs(const std::vector<std::string>& f, LoadPending& pending) {
    (void)pending;
    const std::string& tag = f[0];
    if (tag == "pref") {
        if (f.size() != 9 && f.size() != 12 && f.size() != 13 && f.size() != 14) throw std::runtime_error("LGP1: bad pref record");
        AccountPrefs p;
        const auto fv = parse_u64(f[2], "findability");
        if (fv > 2) throw std::runtime_error("LGP1: bad findability");
        p.findability = static_cast<Findability>(fv);
        p.friends_only_dm = parse_bit(f[3], "friends_only_dm");
        p.group_invite_confirm = parse_bit(f[4], "group_invite_confirm");
        p.presence_hide = parse_bit(f[5], "presence_hide");
        p.notify_friend = parse_bit(f[6], "n_friend");
        p.notify_message = parse_bit(f[7], "n_message");
        p.notify_group = parse_bit(f[8], "n_group");
        if (f.size() == 12 || f.size() == 13) {
            const auto jp = parse_u64(f[9], "jukebox_policy");
            if (jp > 2) throw std::runtime_error("LGP1: bad jukebox_policy");
            p.jukebox_policy = static_cast<JukeboxPolicy>(jp);
            const auto pv = parse_u64(f[10], "profile_visibility");
            const auto ov = parse_u64(f[11], "posts_visibility");
            if (pv > 3 || ov > 3) throw std::runtime_error("LGP1: bad visibility");
            p.profile_visibility = static_cast<Visibility>(pv);
            p.posts_visibility = static_cast<Visibility>(ov);
            if (f.size() == 13) p.intro_every_visit = parse_bit(f[12], "intro_every_visit");
        } else if (f.size() == 14) {
            // Superseded shape from this release's own earlier dev build:
            // jukebox_policy plus four independent hide_from_friends/others
            // bools. Translated to the closest point on the Visibility
            // ladder -- lossy only for the one combination that ladder
            // can't express (hidden from friends but not from strangers),
            // which nothing but this codebase's own throwaway test
            // accounts ever produced.
            const auto jp = parse_u64(f[9], "jukebox_policy");
            if (jp > 2) throw std::runtime_error("LGP1: bad jukebox_policy");
            p.jukebox_policy = static_cast<JukeboxPolicy>(jp);
            const bool hide_profile_friends = parse_bit(f[10], "hide_profile_from_friends");
            const bool hide_profile_others = parse_bit(f[11], "hide_profile_from_others");
            const bool hide_posts_friends = parse_bit(f[12], "hide_posts_from_friends");
            const bool hide_posts_others = parse_bit(f[13], "hide_posts_from_others");
            p.profile_visibility = hide_profile_friends ? Visibility::OnlyMe
                : hide_profile_others ? Visibility::Friends : Visibility::Public;
            p.posts_visibility = hide_posts_friends ? Visibility::OnlyMe
                : hide_posts_others ? Visibility::Friends : Visibility::Public;
        }
        prefs_[pct_decode(f[1])] = p;
        return true;
    }
    if (tag == "hints") {
        if (f.size() < 3) throw std::runtime_error("LGP1: bad hints record");
        const std::string who = pct_decode(f[1]);
        if (f[2] != "0" && f[2] != "1") throw std::runtime_error("LGP1: bad hints flag");
        HintPrefs hp;
        hp.on = f[2] == "1";
        if (f.size() >= 4 && !f[3].empty()) {
            std::string cur;
            for (char ch : f[3] + ",") {
                if (ch == ',') { if (!cur.empty()) hp.dismissed.push_back(cur); cur.clear(); } else cur += ch;
            }
        }
        for (std::size_t i = 4; i < f.size(); ++i) hp.extra_fields.push_back(f[i]);
        if (!who.empty()) hints_[who] = hp;
        return true;
    }
    if (tag == "fmix") {
        // 3 or 4 fields, or more from a newer version (kept verbatim in extra_fields)
        if (f.size() < 3) throw std::runtime_error("LGP1: bad fmix record");
        const std::string who = pct_decode(f[1]);
        FeedMix m;
        if (!feed_mix_every_ok(f[2])) throw std::runtime_error("LGP1: bad fmix rate");
        m.every = std::stoi(f[2]);
        if (f.size() >= 4 && !f[3].empty()) {
            std::string cur;
            for (char ch : f[3] + ",") {
                if (ch != ',') { cur += ch; continue; }
                const auto colon = cur.find(':');
                if (colon == std::string::npos || !feed_mix_view_ok(cur.substr(0, colon)) ||
                    !feed_mix_every_ok(cur.substr(colon + 1)))
                    throw std::runtime_error("LGP1: bad fmix tab");
                m.per_view[cur.substr(0, colon)] = std::stoi(cur.substr(colon + 1));
                cur.clear();
            }
        }
        for (std::size_t i = 4; i < f.size(); ++i) m.extra_fields.push_back(f[i]);
        if (!who.empty()) feed_mix_[who] = m;
        return true;
    }
    if (tag == "mapp") {
        // 7 or 8 fields, or more from a newer version (kept verbatim in extra_fields).
        if (f.size() < 7) throw std::runtime_error("LGP1: bad mapp record");
        const std::string who = pct_decode(f[1]);
        auto bit = [&](const std::string& v) {
            if (v != "0" && v != "1") throw std::runtime_error("LGP1: bad mapp flag");
            return v == "1";
        };
        MapPrefs mp;
        mp.show = bit(f[2]); mp.groups = bit(f[3]); mp.posts = bit(f[4]);
        mp.comments = bit(f[5]); mp.friends = bit(f[6]);
        if (f.size() >= 8) mp.pains = bit(f[7]); // 7 fields: written before pains existed, default on
        for (std::size_t i = 8; i < f.size(); ++i) mp.extra_fields.push_back(f[i]); // still percent-encoded
        if (!who.empty()) map_prefs_[who] = mp;
        return true;
    }
    return false;
}

// Hints are plain ids (letters, digits, - and _), so nothing in them needs escaping; anything else is dropped on save.
static bool hint_id_ok(const std::string& s) {
    if (s.empty() || s.size() > 40) return false;
    for (unsigned char ch : s) if (!(std::isalnum(ch) || ch == '-' || ch == '_')) return false;
    return true;
}

FeedMix FileStore::feed_mix_of(const std::string& username) const {
    auto it = feed_mix_.find(username);
    return it == feed_mix_.end() ? FeedMix{} : it->second;
}

// Only the known rates and plain tab names are kept; anything else is dropped on save (never written, so never refused
// on the next load).
void FileStore::set_feed_mix(const std::string& username, const FeedMix& m) {
    if (username.empty()) return;
    FeedMix c;
    c.every = feed_mix_every_ok(std::to_string(m.every)) ? m.every : 0;
    for (const auto& [view, every] : m.per_view)
        if (feed_mix_view_ok(view) && feed_mix_every_ok(std::to_string(every))) c.per_view[view] = every;
    c.extra_fields = m.extra_fields;
    auto old = feed_mix_.find(username);
    if (old != feed_mix_.end() && c.extra_fields.empty()) c.extra_fields = old->second.extra_fields;
    feed_mix_[username] = c;
    flush();
}

void FileStore::write_feed_mix(std::ostream& body) {
    for (const auto& [who, m] : feed_mix_) {
        body << "fmix\t" << pct_encode(who) << "\t" << m.every << "\t";
        bool first = true;
        for (const auto& [view, every] : m.per_view) { body << (first ? "" : ",") << view << ":" << every; first = false; }
        for (const auto& x : m.extra_fields) body << "\t" << x;
        body << "\n";
    }
}

HintPrefs FileStore::hints_of(const std::string& username) const {
    auto it = hints_.find(username);
    return it == hints_.end() ? HintPrefs{} : it->second;
}

void FileStore::set_hints(const std::string& username, const HintPrefs& h) {
    if (username.empty()) return;
    HintPrefs c;
    c.on = h.on;
    for (const auto& d : h.dismissed) {
        if (!hint_id_ok(d)) continue;
        if (std::find(c.dismissed.begin(), c.dismissed.end(), d) != c.dismissed.end()) continue;
        c.dismissed.push_back(d);
    }
    auto old = hints_.find(username);
    c.extra_fields = h.extra_fields;
    if (old != hints_.end() && c.extra_fields.empty()) c.extra_fields = old->second.extra_fields;
    hints_[username] = c;
    flush();
}

void FileStore::write_hints(std::ostream& body) {
    for (const auto& [who, hp] : hints_) {
        body << "hints\t" << pct_encode(who) << "\t" << (hp.on ? 1 : 0) << "\t";
        for (std::size_t i = 0; i < hp.dismissed.size(); ++i) body << (i ? "," : "") << hp.dismissed[i];
        for (const auto& x : hp.extra_fields) body << "\t" << x;
        body << "\n";
    }
}

// Save: this module's records, at their place in the store (flush() calls the sections in order).
void FileStore::write_map_prefs(std::ostream& body) {
    for (const auto& [who, mp] : map_prefs_) {
        body << "mapp\t" << pct_encode(who) << "\t" << (mp.show ? 1 : 0) << "\t" << (mp.groups ? 1 : 0) << "\t"
             << (mp.posts ? 1 : 0) << "\t" << (mp.comments ? 1 : 0) << "\t" << (mp.friends ? 1 : 0) << "\t"
             << (mp.pains ? 1 : 0);
        for (const auto& x : mp.extra_fields) body << "\t" << x; // a newer version's fields, untouched
        body << "\n";
    }
}

// Save: this module's records, at their place in the store (flush() calls the sections in order).
void FileStore::write_prefs(std::ostream& body) {
    for (const auto& [name, p] : prefs_) {
        body << "pref\t" << pct_encode(name) << "\t" << static_cast<int>(p.findability) << "\t"
             << (p.friends_only_dm ? 1 : 0) << "\t" << (p.group_invite_confirm ? 1 : 0) << "\t"
             << (p.presence_hide ? 1 : 0) << "\t" << (p.notify_friend ? 1 : 0) << "\t"
             << (p.notify_message ? 1 : 0) << "\t" << (p.notify_group ? 1 : 0) << "\t"
             << static_cast<int>(p.jukebox_policy) << "\t" << static_cast<int>(p.profile_visibility) << "\t"
             << static_cast<int>(p.posts_visibility) << "\t" << (p.intro_every_visit ? 1 : 0) << "\n";
    }
}

} // namespace liquidgov::platform
