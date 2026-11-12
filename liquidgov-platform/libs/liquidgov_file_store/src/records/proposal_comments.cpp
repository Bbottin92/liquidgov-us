// proposal_comments: Comments and reactions on proposals.
// LGP1 record types: prply prpln prx (format: the header comment of file_store.hpp).
// Split out of file_store.cpp 2026-11-12 (code moved, not rewritten). Listed in ../../sources.cmake.
#include "liquidgov/platform/file_store.hpp"

#include "core/codec.hpp"
#include "core/load_pending.hpp"
#include "liquidgov/platform/clock.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace liquidgov::platform {

using namespace store_detail;

namespace {

bool proposal_reaction_kind(const std::string& kind) {
    return kind == "support" || kind == "oppose" || kind == "question" || kind == "noted";
}

// A proposal comment: keep newlines, drop other controls, trim, cap at 1000
// bytes without splitting a UTF-8 character. Empty when nothing is left.
std::string clean_comment(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (unsigned char ch : raw) {
        if (ch == '\t' || ch == '\n' || ch == '\r') {
            out += static_cast<char>(ch);
            continue;
        }
        if (ch < 0x20 || ch == 0x7f) continue;
        out += static_cast<char>(ch);
    }
    auto is_ws = [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; };
    std::size_t a = 0;
    std::size_t b = out.size();
    while (a < b && is_ws(out[a])) ++a;
    while (b > a && is_ws(out[b - 1])) --b;
    out = out.substr(a, b - a);
    if (out.size() > 1000) {
        std::size_t cut = 1000;
        while (cut > 0 && (static_cast<unsigned char>(out[cut]) & 0xC0) == 0x80) --cut;
        out.resize(cut);
        while (!out.empty() && is_ws(out.back())) out.pop_back();
    }
    return out;
}

} // namespace

std::vector<ProposalReply> FileStore::recent_proposal_replies(std::size_t limit) const {
    std::vector<ProposalReply> out;
    for (auto it = proposal_replies_.rbegin(); it != proposal_replies_.rend() && out.size() < limit; ++it) out.push_back(*it);
    return out;
}

std::optional<ProposalReply> FileStore::proposal_reply_by_id(std::uint64_t id) const {
    for (const auto& r : proposal_replies_) if (r.id == id) return r;
    return std::nullopt;
}

bool FileStore::delete_proposal_reply(std::uint64_t reply_id) {
    auto it = std::find_if(proposal_replies_.begin(), proposal_replies_.end(),
                           [&](const ProposalReply& r) { return r.id == reply_id; });
    if (it == proposal_replies_.end()) return false;
    proposal_replies_.erase(it);
    prune_pains();
    flush();
    return true;
}

std::uint64_t FileStore::add_proposal_reply(std::uint64_t proposal_id, const std::string& author,
                                            const std::string& text) {
    if (author.empty() || !proposal_by_id(proposal_id)) return 0;
    const std::string cleaned = clean_comment(text);
    if (cleaned.empty()) return 0;
    ProposalReply r;
    r.id = next_proposal_reply_id_++;
    r.proposal_id = proposal_id;
    r.author = author;
    r.ts = now();
    r.text = cleaned;
    proposal_replies_.push_back(r);
    flush();
    return r.id;
}

std::vector<ProposalReply> FileStore::proposal_replies_of(std::uint64_t proposal_id) const {
    std::vector<ProposalReply> out;
    for (const auto& r : proposal_replies_)
        if (r.proposal_id == proposal_id) out.push_back(r);
    return out;
}

bool FileStore::set_proposal_reaction(std::uint64_t proposal_id, const std::string& user,
                                      const std::string& kind) {
    if (user.empty() || !proposal_by_id(proposal_id)) return false;
    if (!kind.empty() && !proposal_reaction_kind(kind)) return false;
    auto& mine = proposal_reactions_[proposal_id];
    if (kind.empty()) mine.erase(user);
    else mine[user] = kind;
    if (mine.empty()) proposal_reactions_.erase(proposal_id);
    flush();
    return true;
}

std::optional<std::string> FileStore::proposal_reaction_of(std::uint64_t proposal_id,
                                                           const std::string& user) const {
    auto it = proposal_reactions_.find(proposal_id);
    if (it == proposal_reactions_.end()) return std::nullopt;
    auto u = it->second.find(user);
    if (u == it->second.end()) return std::nullopt;
    return u->second;
}

std::vector<std::pair<std::string, int>> FileStore::proposal_reaction_counts(
        std::uint64_t proposal_id) const {
    static const char* kinds[] = {"support", "oppose", "question", "noted"};
    std::vector<std::pair<std::string, int>> out;
    auto it = proposal_reactions_.find(proposal_id);
    for (const char* kind : kinds) {
        int n = 0;
        if (it != proposal_reactions_.end())
            for (const auto& [user, k] : it->second) if (k == kind) ++n;
        out.emplace_back(kind, n);
    }
    return out;
}

// The record types this module reads (one branch each, as they were in load()'s chain).
bool FileStore::read_proposal_comments(const std::vector<std::string>& f, LoadPending& pending) {
    auto& pending_prply = pending.pending_prply;
    auto& pending_prx = pending.pending_prx;
    const std::string& tag = f[0];
    if (tag == "prply") {
        if (f.size() != 6) throw std::runtime_error("LGP1: bad prply record");
        ProposalReply r;
        r.id = parse_u64(f[1], "prply id");
        r.proposal_id = parse_u64(f[2], "prply proposal id");
        r.author = pct_decode(f[3]);
        r.ts = parse_i64(f[4], "prply ts");
        r.text = pct_decode(f[5]);
        pending_prply.push_back(std::move(r));
        return true;
    }
    if (tag == "prpln") {
        if (f.size() != 2) throw std::runtime_error("LGP1: bad prpln record");
        next_proposal_reply_id_ = parse_u64(f[1], "next_proposal_reply_id");
        if (next_proposal_reply_id_ == 0) next_proposal_reply_id_ = 1;
        return true;
    }
    if (tag == "prx") {
        if (f.size() != 4) throw std::runtime_error("LGP1: bad prx record");
        const auto pid = parse_u64(f[1], "prx proposal id");
        const auto who = pct_decode(f[2]);
        const auto kind = pct_decode(f[3]);
        if (!who.empty() && proposal_reaction_kind(kind)) pending_prx[pid][who] = kind;
        return true;
    }
    return false;
}

// After every line is read (load() calls the finishers in a fixed order).
void FileStore::finish_proposal_comments(LoadPending& pending) {
    auto& pending_prply = pending.pending_prply;
    auto& pending_prx = pending.pending_prx;
    std::set<std::uint64_t> proposal_ids;
    for (const auto& p : proposals_) proposal_ids.insert(p.id);
    for (const auto& r : pending_prply) {
        if (!proposal_ids.count(r.proposal_id) || r.author.empty() || r.text.empty()) continue;
        proposal_replies_.push_back(r);
        if (r.id >= next_proposal_reply_id_) next_proposal_reply_id_ = r.id + 1;
    }
    for (const auto& [pid, reactions] : pending_prx) {
        if (!proposal_ids.count(pid)) continue;
        proposal_reactions_[pid] = reactions;
    }
}

// Save: this module's records, at their place in the store (flush() calls the sections in order).
void FileStore::write_proposal_comments(std::ostream& body) {
    {
        std::set<std::uint64_t> live;
        for (const auto& p : proposals_) live.insert(p.id);
        proposal_replies_.erase(std::remove_if(proposal_replies_.begin(), proposal_replies_.end(),
            [&](const ProposalReply& r) {
                return !live.count(r.proposal_id) || r.author.empty() || r.text.empty();
            }), proposal_replies_.end());
        for (auto it = proposal_reactions_.begin(); it != proposal_reactions_.end(); ) {
            if (!live.count(it->first)) { it = proposal_reactions_.erase(it); continue; }
            for (auto u = it->second.begin(); u != it->second.end(); ) {
                if (u->first.empty() || !proposal_reaction_kind(u->second)) u = it->second.erase(u);
                else ++u;
            }
            if (it->second.empty()) it = proposal_reactions_.erase(it);
            else ++it;
        }
    }
    body << "prpln\t" << next_proposal_reply_id_ << "\n";
    for (const auto& r : proposal_replies_) {
        body << "prply\t" << r.id << "\t" << r.proposal_id << "\t" << pct_encode(r.author) << "\t"
             << r.ts << "\t" << pct_encode(r.text) << "\n";
    }
    for (const auto& [pid, reactions] : proposal_reactions_) {
        for (const auto& [who, kind] : reactions) {
            body << "prx\t" << pid << "\t" << pct_encode(who) << "\t" << pct_encode(kind) << "\n";
        }
    }
}

} // namespace liquidgov::platform
