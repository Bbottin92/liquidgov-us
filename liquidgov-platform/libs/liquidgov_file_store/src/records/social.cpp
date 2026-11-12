// social: Friends, friend requests, blocks, inner circle.
// LGP1 record types: f r blk inner (format: the header comment of file_store.hpp).
// Split out of file_store.cpp 2026-11-12 (code moved, not rewritten). Listed in ../../sources.cmake.
#include "liquidgov/platform/file_store.hpp"

#include "core/codec.hpp"
#include "core/load_pending.hpp"
#include "liquidgov/platform/clock.hpp"

#include <algorithm>
#include <ostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace liquidgov::platform {

using namespace store_detail;

bool FileStore::are_friends(const std::string& a, const std::string& b) const {
    return friends_.count({a, b}) > 0 || friends_.count({b, a}) > 0;
}

std::vector<std::string> FileStore::friends_of(const std::string& user) const {
    std::vector<std::string> out;
    for (const auto& p : friends_) {
        if (p.first == user) out.push_back(p.second);
        else if (p.second == user) out.push_back(p.first);
    }
    return out;
}

void FileStore::add_friend_request(const std::string& from, const std::string& to) {
    if (from.empty() || to.empty() || from == to) return;
    if (are_friends(from, to)) return;
    if (is_blocked(from, to) || is_blocked(to, from)) return;
    for (const auto& r : requests_) {
        if (r.first == from && r.second == to) return;
    }
    requests_.push_back({from, to});
    auto p = prefs_of(to);
    if (p.notify_friend) add_notice(to, "friend_request", from, "");
    else flush();
}

bool FileStore::accept_friend_request(const std::string& from, const std::string& to) {
    bool found = false;
    std::vector<std::pair<std::string, std::string>> keep;
    keep.reserve(requests_.size());
    for (const auto& r : requests_) {
        if (r.first == from && r.second == to) {
            found = true;
        } else {
            keep.push_back(r);
        }
    }
    if (!found) return false;
    requests_.swap(keep);
    friends_.insert({to, from});
    auto p = prefs_of(from);
    if (p.notify_friend) add_notice(from, "friend_accept", to, "");
    else flush();
    return true;
}

bool FileStore::decline_friend_request(const std::string& from, const std::string& to) {
    const auto before = requests_.size();
    requests_.erase(std::remove_if(requests_.begin(), requests_.end(),
                                   [&](const auto& r) { return r.first == from && r.second == to; }),
                    requests_.end());
    if (requests_.size() == before) return false;
    flush();
    return true;
}

void FileStore::drop_requests_between(const std::string& a, const std::string& b) {
    std::vector<std::pair<std::string, std::string>> keep;
    for (const auto& r : requests_) {
        if ((r.first == a && r.second == b) || (r.first == b && r.second == a)) continue;
        keep.push_back(r);
    }
    requests_.swap(keep);
}

void FileStore::strip_inner_both(const std::string& a, const std::string& b) {
    if (inner_.count(a)) inner_[a].erase(b);
    if (inner_.count(b)) inner_[b].erase(a);
}

void FileStore::unfriend(const std::string& actor, const std::string& other) {
    if (actor.empty() || other.empty() || actor == other) return;
    friends_.erase({actor, other});
    friends_.erase({other, actor});
    drop_requests_between(actor, other);
    strip_inner_both(actor, other);
    // Age-blind, uniform: only the actor leaves shared group conversations
    // (see messenger.hpp on_relationship_ended). block() calls this same
    // function, so a block gets identical conversation-leaving behavior —
    // no separate code path to keep in sync.
    auto actor_id = by_username_.find(actor);
    auto other_id = by_username_.find(other);
    if (actor_id != by_username_.end() && other_id != by_username_.end()) {
        on_relationship_ended(*this, actor_id->second, other_id->second);
    }
    flush();
}

std::vector<std::string> FileStore::incoming_requests(const std::string& user) const {
    std::vector<std::string> out;
    for (const auto& r : requests_) {
        if (r.second == user) out.push_back(r.first);
    }
    return out;
}

std::vector<std::string> FileStore::outgoing_requests(const std::string& user) const {
    std::vector<std::string> out;
    for (const auto& r : requests_) {
        if (r.first == user) out.push_back(r.second);
    }
    return out;
}

void FileStore::block(const std::string& actor, const std::string& other, const std::string& reason) {
    if (actor.empty() || other.empty() || actor == other) return;
    unfriend(actor, other);
    blocks_.insert({actor, other});
    block_meta_[{actor, other}] = BlockEntry{actor, other, now(), reason};
    flush();
}

bool FileStore::is_blocked(const std::string& blocker, const std::string& blocked) const {
    return blocks_.count({blocker, blocked}) > 0;
}

std::vector<std::string> FileStore::blocked_by(const std::string& actor) const {
    std::vector<std::string> out;
    for (const auto& p : blocks_) {
        if (p.first == actor) out.push_back(p.second);
    }
    return out;
}

std::vector<BlockEntry> FileStore::block_entries_of(const std::string& actor) const {
    std::vector<BlockEntry> out;
    for (const auto& p : blocks_) {
        if (p.first != actor) continue;
        auto it = block_meta_.find(p);
        out.push_back(it != block_meta_.end() ? it->second : BlockEntry{p.first, p.second, 0, ""});
    }
    return out;
}

void FileStore::unblock(const std::string& actor, const std::string& other) {
    blocks_.erase({actor, other});
    block_meta_.erase({actor, other});
    flush();
}

bool FileStore::add_inner(const std::string& owner, const std::string& friend_name) {
    if (!are_friends(owner, friend_name)) return false;
    auto& set = inner_[owner];
    if (set.count(friend_name)) return true;
    if (static_cast<int>(set.size()) >= kInnerCircleCap) return false;
    set.insert(friend_name);
    flush();
    return true;
}

void FileStore::remove_inner(const std::string& owner, const std::string& friend_name) {
    if (inner_.count(owner)) inner_[owner].erase(friend_name);
    flush();
}

std::vector<std::string> FileStore::inner_of(const std::string& owner) const {
    std::vector<std::string> out;
    auto it = inner_.find(owner);
    if (it == inner_.end()) return out;
    for (const auto& n : it->second) out.push_back(n);
    return out;
}

// The record types this module reads (one branch each, as they were in load()'s chain).
bool FileStore::read_social(const std::vector<std::string>& f, LoadPending& pending) {
    (void)pending;
    const std::string& tag = f[0];
    if (tag == "f") {
        if (f.size() != 3) throw std::runtime_error("LGP1: bad f record");
        friends_.insert({pct_decode(f[1]), pct_decode(f[2])});
        return true;
    }
    if (tag == "r") {
        if (f.size() != 3) throw std::runtime_error("LGP1: bad r record");
        requests_.push_back({pct_decode(f[1]), pct_decode(f[2])});
        return true;
    }
    if (tag == "blk") {
        if (f.size() != 3 && f.size() != 5) throw std::runtime_error("LGP1: bad blk record");
        BlockEntry b;
        b.actor = pct_decode(f[1]);
        b.other = pct_decode(f[2]);
        if (f.size() == 5) {
            b.at = parse_i64(f[3], "blk at");
            b.reason = pct_decode(f[4]);
        }
        blocks_.insert({b.actor, b.other});
        block_meta_[{b.actor, b.other}] = b;
        return true;
    }
    if (tag == "inner") {
        if (f.size() != 3) throw std::runtime_error("LGP1: bad inner record");
        inner_[pct_decode(f[1])].insert(pct_decode(f[2]));
        return true;
    }
    return false;
}

// Save: this module's records, at their place in the store (flush() calls the sections in order).
void FileStore::write_friend_links(std::ostream& body) {
    for (const auto& fr : friends_) {
        body << "f\t" << pct_encode(fr.first) << "\t" << pct_encode(fr.second) << "\n";
    }
    for (const auto& r : requests_) {
        body << "r\t" << pct_encode(r.first) << "\t" << pct_encode(r.second) << "\n";
    }
}

// Save: this module's records, at their place in the store (flush() calls the sections in order).
void FileStore::write_blocks(std::ostream& body) {
    for (const auto& b : blocks_) {
        auto mit = block_meta_.find(b);
        const BlockEntry meta = mit != block_meta_.end() ? mit->second : BlockEntry{b.first, b.second, 0, ""};
        body << "blk\t" << pct_encode(b.first) << "\t" << pct_encode(b.second) << "\t"
             << meta.at << "\t" << pct_encode(meta.reason) << "\n";
    }
    for (const auto& [owner, set] : inner_) {
        for (const auto& u : set) {
            body << "inner\t" << pct_encode(owner) << "\t" << pct_encode(u) << "\n";
        }
    }
}

} // namespace liquidgov::platform
