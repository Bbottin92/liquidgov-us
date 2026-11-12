// communities: Groups (communities), their cities and members.
// LGP1 record types: comm cm ccity (format: the header comment of file_store.hpp).
// Split out of file_store.cpp 2026-11-12 (code moved, not rewritten). Listed in ../../sources.cmake.
#include "liquidgov/platform/file_store.hpp"

#include "core/codec.hpp"
#include "core/load_pending.hpp"

#include <optional>
#include <ostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace liquidgov::platform {

using namespace store_detail;

bool FileStore::create_community(const Community& c) {
    if (!valid_username(c.slug)) return false;
    if (communities_.count(c.slug)) return false;
    Community stored = c;
    stored.city = clean_city(c.city);
    if (city_key(stored.city).empty()) stored.city.clear();
    communities_[c.slug] = stored;
    members_[c.slug].insert(c.owner);
    flush();
    return true;
}

std::optional<Community> FileStore::community(const std::string& slug) const {
    auto it = communities_.find(slug);
    if (it == communities_.end()) return std::nullopt;
    return it->second;
}

std::vector<Community> FileStore::all_communities() const {
    std::vector<Community> out;
    for (const auto& [slug, c] : communities_) out.push_back(c);
    return out;
}

std::vector<std::string> FileStore::subcategories_of(CommunityCategory category) const {
    std::set<std::string> seen;
    for (const auto& [slug, c] : communities_) {
        if (c.category == category && !c.subcategory.empty()) seen.insert(c.subcategory);
    }
    return std::vector<std::string>(seen.begin(), seen.end());
}

bool FileStore::is_member(const std::string& slug, const std::string& username) const {
    auto it = members_.find(slug);
    if (it == members_.end()) return false;
    return it->second.count(username) > 0;
}

bool FileStore::join_community(const std::string& slug, const std::string& username) {
    if (!communities_.count(slug)) return false;
    members_[slug].insert(username);
    flush();
    return true;
}

bool FileStore::invite_community(const std::string& slug, const std::string& actor,
                                 const std::string& target) {
    if (!communities_.count(slug)) return false;
    if (!is_member(slug, actor)) return false;
    if (!username_exists(target)) return false;
    if (is_blocked(actor, target) || is_blocked(target, actor)) return false;
    auto p = prefs_of(target);
    if (p.group_invite_confirm) return false;
    members_[slug].insert(target);
    if (p.notify_group) add_notice(target, "group_invite", actor, slug);
    else flush();
    return true;
}

void FileStore::leave_community(const std::string& slug, const std::string& username) {
    auto it = communities_.find(slug);
    if (it == communities_.end()) return;
    if (it->second.owner == username) return; // owner stays until delete
    if (members_.count(slug)) members_[slug].erase(username);
    flush();
}

bool FileStore::delete_community(const std::string& slug, const std::string& actor) {
    auto it = communities_.find(slug);
    if (it == communities_.end()) return false;
    if (it->second.owner != actor) return false;
    communities_.erase(it);
    members_.erase(slug);
    // Posts made into this room are left exactly as they are -- see this
    // method's own header comment. Closing a room is not a reason to erase
    // what anyone said in it.
    flush();
    return true;
}

std::vector<std::string> FileStore::members_of(const std::string& slug) const {
    std::vector<std::string> out;
    auto it = members_.find(slug);
    if (it == members_.end()) return out;
    for (const auto& n : it->second) out.push_back(n);
    return out;
}

std::vector<Community> FileStore::communities_of(const std::string& username) const {
    std::vector<Community> out;
    for (const auto& [slug, set] : members_) {
        if (set.count(username) && communities_.count(slug)) out.push_back(communities_.at(slug));
    }
    return out;
}

// The record types this module reads (one branch each, as they were in load()'s chain).
bool FileStore::read_communities(const std::vector<std::string>& f, LoadPending& pending) {
    auto& pending_city = pending.pending_city;
    const std::string& tag = f[0];
    if (tag == "comm") {
        if (f.size() != 6 && f.size() != 8) throw std::runtime_error("LGP1: bad comm record");
        Community c;
        c.slug = pct_decode(f[1]);
        c.name = pct_decode(f[2]);
        c.owner = pct_decode(f[3]);
        c.about = pct_decode(f[4]);
        c.created_at = parse_i64(f[5], "comm created");
        if (f.size() == 8) {
            int cat = 10;
            try { cat = std::stoi(f[6]); } catch (...) { cat = 10; }
            if (cat < 0 || cat > 10) cat = 10;
            c.category = static_cast<CommunityCategory>(cat);
            c.subcategory = pct_decode(f[7]);
        }
        communities_[c.slug] = c;
        return true;
    }
    if (tag == "cm") {
        if (f.size() != 3) throw std::runtime_error("LGP1: bad cm record");
        members_[pct_decode(f[1])].insert(pct_decode(f[2]));
        return true;
    }
    if (tag == "ccity") {
        if (f.size() != 3) throw std::runtime_error("LGP1: bad ccity record");
        pending_city[pct_decode(f[1])] = pct_decode(f[2]);
        return true;
    }
    return false;
}

// After every line is read (load() calls the finishers in a fixed order).
void FileStore::finish_communities(LoadPending& pending) {
    auto& pending_city = pending.pending_city;
    for (const auto& [slug, city] : pending_city) {
        auto cit = communities_.find(slug);
        if (cit != communities_.end()) cit->second.city = city; // orphan ccity rows are dropped
    }
}

// Save: this module's records, at their place in the store (flush() calls the sections in order).
void FileStore::write_communities(std::ostream& body) {
    for (const auto& [slug, c] : communities_) {
        body << "comm\t" << pct_encode(c.slug) << "\t" << pct_encode(c.name) << "\t"
             << pct_encode(c.owner) << "\t" << pct_encode(c.about) << "\t" << c.created_at << "\t"
             << static_cast<int>(c.category) << "\t" << pct_encode(c.subcategory) << "\n";
        if (!c.city.empty()) body << "ccity\t" << pct_encode(c.slug) << "\t" << pct_encode(c.city) << "\n";
    }
}

// Save: this module's records, at their place in the store (flush() calls the sections in order).
void FileStore::write_members(std::ostream& body) {
    for (const auto& [slug, set] : members_) {
        for (const auto& u : set) {
            body << "cm\t" << pct_encode(slug) << "\t" << pct_encode(u) << "\n";
        }
    }
}

} // namespace liquidgov::platform
