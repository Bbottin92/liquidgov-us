// city: Cities: spelling and keys, department/council tags, People-here lists.
// LGP1 record types: clist ctag (format: the header comment of file_store.hpp).
// Split out of file_store.cpp 2026-11-12 (code moved, not rewritten). Listed in ../../sources.cmake.
#include "liquidgov/platform/file_store.hpp"

#include "core/codec.hpp"
#include "core/load_pending.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <ostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace liquidgov::platform {

using namespace store_detail;

bool FileStore::remember_city_tag(const std::string& key, const std::string& raw) {
    const std::string tag = clean_city(raw);
    if (key.empty() || tag.empty()) return false;
    auto& list = city_tags_[key];
    const std::string fold = lower_copy(tag);
    for (const auto& have : list)
        if (lower_copy(have) == fold) return false;
    list.push_back(tag);
    return true;
}

bool FileStore::add_city_tag(const std::string& key, const std::string& actor, const std::string& tag) {
    if (!in_city(key, actor) || clean_city(tag).empty()) return false;
    if (!remember_city_tag(key, tag)) return true;
    flush();
    return true;
}

std::vector<std::string> FileStore::city_tags(const std::string& key) const {
    auto it = city_tags_.find(key);
    if (it == city_tags_.end()) return {};
    return it->second;
}

std::vector<std::pair<std::string, std::string>> FileStore::all_city_tags() const {
    std::vector<std::pair<std::string, std::string>> out;
    for (const auto& [key, tags] : city_tags_)
        for (const auto& tag : tags) out.emplace_back(key, tag);
    return out;
}

std::vector<std::uint64_t> FileStore::proposals_with_city_tag(const std::string& key, const std::string& tag) const {
    std::vector<std::uint64_t> ids;
    const std::string fold = lower_copy(clean_city(tag));
    for (const auto& p : proposals_) {
        if (p.city != key) continue;
        for (const auto& have : p.tags)
            if (lower_copy(have) == fold) { ids.push_back(p.id); break; }
    }
    return ids;
}

bool FileStore::delete_city_tag(const std::string& key, const std::string& tag) {
    auto it = city_tags_.find(key);
    if (it == city_tags_.end()) return false;
    const std::string fold = lower_copy(clean_city(tag));
    if (fold.empty()) return false;
    auto& list = it->second;
    const auto before = list.size();
    list.erase(std::remove_if(list.begin(), list.end(), [&](const std::string& t) { return lower_copy(t) == fold; }),
               list.end());
    if (list.size() == before) return false;
    if (list.empty()) city_tags_.erase(it);
    // proposals in this city drop the tag now (flush() would also prune it; done here so the in-memory state is right
    // before the write, and so a failed flush does not leave a proposal naming a tag its city no longer has)
    for (auto& p : proposals_) {
        if (p.city != key) continue;
        p.tags.erase(std::remove_if(p.tags.begin(), p.tags.end(), [&](const std::string& t) { return lower_copy(t) == fold; }),
                     p.tags.end());
    }
    flush();
    return true;
}

std::string clean_city(const std::string& raw) {
    std::string out;
    bool pending_space = false;
    for (unsigned char ch : raw) {
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v') {
            pending_space = true;
            continue;
        }
        if (ch < 0x20 || ch == 0x7f) continue; // other control characters
        if (pending_space && !out.empty()) out += ' ';
        pending_space = false;
        out += static_cast<char>(ch);
    }
    if (out.size() > 80) {
        std::size_t cut = 80;
        // Step back off UTF-8 continuation bytes so no character is split.
        while (cut > 0 && (static_cast<unsigned char>(out[cut]) & 0xC0) == 0x80) --cut;
        out.resize(cut);
        while (!out.empty() && out.back() == ' ') out.pop_back();
    }
    return out;
}

std::string city_key(const std::string& city) {
    std::string key;
    bool pending_dash = false;
    bool has_word = false;
    for (unsigned char ch : city) {
        const bool ascii_alnum = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
        if (ascii_alnum || ch >= 0x80) {
            if (pending_dash && !key.empty()) key += '-';
            pending_dash = false;
            key += (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch - 'A' + 'a') : static_cast<char>(ch);
            has_word = true;
        } else {
            pending_dash = true;
        }
    }
    return has_word ? key : std::string();
}

std::vector<CityUse> FileStore::cities_in_use() const {
    // key -> spelling -> number of groups using that spelling
    std::map<std::string, std::map<std::string, std::size_t>> spellings;
    for (const auto& [slug, c] : communities_) {
        if (c.city.empty()) continue;
        const std::string key = city_key(c.city);
        if (key.empty()) continue;
        ++spellings[key][c.city];
    }
    std::vector<CityUse> out;
    for (const auto& [key, names] : spellings) {
        CityUse u;
        u.key = key;
        std::size_t best = 0;
        for (const auto& [name, n] : names) {
            u.groups += n;
            if (n > best) { best = n; u.name = name; } // map order: ties keep the first A-Z
        }
        out.push_back(u);
    }
    std::sort(out.begin(), out.end(), [](const CityUse& a, const CityUse& b) {
        return a.name != b.name ? a.name < b.name : a.key < b.key;
    });
    return out;
}

std::vector<Community> FileStore::communities_in_city(const std::string& key) const {
    std::vector<Community> out;
    if (key.empty()) return out;
    for (const auto& [slug, c] : communities_) {
        if (!c.city.empty() && city_key(c.city) == key) out.push_back(c);
    }
    std::sort(out.begin(), out.end(), [](const Community& a, const Community& b) {
        return a.name != b.name ? a.name < b.name : a.slug < b.slug;
    });
    return out;
}

bool FileStore::in_city(const std::string& key, const std::string& username) const {
    if (key.empty() || username.empty()) return false;
    for (const auto& [slug, c] : communities_) {
        if (city_key(c.city) == key && is_member(slug, username)) return true;
    }
    return false;
}

bool FileStore::set_city_listed(const std::string& key, const std::string& username, bool on) {
    if (key.empty() || !valid_username(username)) return false;
    if (on && !in_city(key, username)) return false;
    auto& names = city_listed_[key];
    bool changed = false;
    if (on) {
        changed = names.insert(username).second;
    } else {
        changed = names.erase(username) > 0;
        if (names.empty()) city_listed_.erase(key);
    }
    if (changed) flush();
    return true;
}

bool FileStore::is_city_listed(const std::string& key, const std::string& username) const {
    auto it = city_listed_.find(key);
    if (it == city_listed_.end() || !it->second.count(username)) return false;
    return in_city(key, username);
}

std::vector<std::string> FileStore::people_listed_in_city(const std::string& key) const {
    std::vector<std::string> out;
    auto it = city_listed_.find(key);
    if (it == city_listed_.end()) return out;
    for (const auto& name : it->second)
        if (in_city(key, name)) out.push_back(name);
    return out;
}

bool FileStore::set_community_city(const std::string& slug, const std::string& actor, const std::string& city) {
    auto it = communities_.find(slug);
    if (it == communities_.end()) return false;
    if (actor.empty() || it->second.owner != actor) return false;
    std::string cleaned = clean_city(city);
    if (city_key(cleaned).empty()) cleaned.clear();
    it->second.city = cleaned;
    flush();
    return true;
}

// The record types this module reads (one branch each, as they were in load()'s chain).
bool FileStore::read_city(const std::vector<std::string>& f, LoadPending& pending) {
    auto& pending_clist = pending.pending_clist;
    auto& pending_ctag = pending.pending_ctag;
    const std::string& tag = f[0];
    if (tag == "clist") {
        if (f.size() != 3) throw std::runtime_error("LGP1: bad clist record");
        pending_clist[pct_decode(f[1])].insert(pct_decode(f[2]));
        return true;
    }
    if (tag == "ctag") {
        if (f.size() != 3) throw std::runtime_error("LGP1: bad ctag record");
        pending_ctag[pct_decode(f[1])].push_back(pct_decode(f[2]));
        return true;
    }
    return false;
}

// After every line is read (load() calls the finishers in a fixed order).
void FileStore::finish_city_lists(LoadPending& pending) {
    auto& pending_clist = pending.pending_clist;
    for (const auto& [key, names] : pending_clist) {
        if (key.empty()) continue;
        for (const auto& name : names)
            if (!name.empty() && in_city(key, name)) city_listed_[key].insert(name);
    }
}

// After every line is read (load() calls the finishers in a fixed order).
void FileStore::finish_city_tags(LoadPending& pending) {
    auto& pending_ctag = pending.pending_ctag;
    // Kept even while no group names the city (Brandon 2026-11-06, "keep it"):
    // the last group leaving or re-spelling its city must not erase them.
    for (const auto& [key, tags] : pending_ctag) {
        if (key.empty()) continue;
        for (const auto& tag : tags) remember_city_tag(key, tag);
    }
}

// Save: this module's records, at their place in the store (flush() calls the sections in order).
void FileStore::write_city_lists(std::ostream& body) {
    for (auto it = city_listed_.begin(); it != city_listed_.end(); ) {
        for (auto name = it->second.begin(); name != it->second.end(); ) {
            if (!in_city(it->first, *name)) name = it->second.erase(name);
            else ++name;
        }
        if (it->second.empty()) it = city_listed_.erase(it);
        else ++it;
    }
    for (const auto& [key, names] : city_listed_) {
        for (const auto& name : names)
            body << "clist\t" << pct_encode(key) << "\t" << pct_encode(name) << "\n";
    }
}

// Save: this module's records, at their place in the store (flush() calls the sections in order).
void FileStore::write_city_tags(std::ostream& body) {
    for (const auto& [key, tags] : city_tags_) {
        for (const auto& tag : tags)
            body << "ctag\t" << pct_encode(key) << "\t" << pct_encode(tag) << "\n";
    }
}

} // namespace liquidgov::platform
