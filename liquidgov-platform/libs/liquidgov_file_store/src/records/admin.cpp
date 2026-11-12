// admin: Operator tools: account locks, audit log, program grants.
// LGP1 record types: lk alog prgr (format: the header comment of file_store.hpp).
// Split out of file_store.cpp 2026-11-12 (code moved, not rewritten). Listed in ../../sources.cmake.
#include "liquidgov/platform/file_store.hpp"

#include "core/codec.hpp"
#include "core/load_pending.hpp"
#include "liquidgov/platform/clock.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace liquidgov::platform {

using namespace store_detail;

bool FileStore::is_locked(const std::string& username) const {
    return locked_.count(username) > 0;
}

void FileStore::set_locked(const std::string& username, bool locked) {
    if (locked) {
        locked_.insert(username);
        if (auto acc = find_by_username(username)) {
            for (const auto& s : sessions_for(acc->id)) revoke_session(s.id);
        }
    } else {
        locked_.erase(username);
    }
    flush();
}

void FileStore::add_audit_log(const std::string& operator_name, const std::string& action,
                              const std::string& target) {
    AuditLogEntry e;
    e.ts = now();
    e.operator_name = operator_name;
    e.action = action;
    e.target = target;
    audit_log_.push_back(std::move(e));
    if (audit_log_.size() > kAuditLogCap)
        audit_log_.erase(audit_log_.begin(), audit_log_.end() - static_cast<long>(kAuditLogCap));
    flush();
}

std::vector<AuditLogEntry> FileStore::recent_audit_log(std::size_t limit) const {
    std::vector<AuditLogEntry> out;
    for (auto it = audit_log_.rbegin(); it != audit_log_.rend() && out.size() < limit; ++it)
        out.push_back(*it);
    return out;
}

std::optional<ProgramGrant> FileStore::program_grant(const std::string& username) const {
    auto it = program_grants_.find(username);
    if (it == program_grants_.end()) return std::nullopt;
    return it->second;
}

void FileStore::set_program_grant(const ProgramGrant& grant) {
    if (grant.kinds == 0) {
        program_grants_.erase(grant.username);
    } else {
        program_grants_[grant.username] = grant;
    }
    flush();
}

void FileStore::revoke_program_grant(const std::string& username) {
    if (program_grants_.erase(username)) flush();
}

// The record types this module reads (one branch each, as they were in load()'s chain).
bool FileStore::read_admin(const std::vector<std::string>& f, LoadPending& pending) {
    (void)pending;
    const std::string& tag = f[0];
    if (tag == "lk") {
        if (f.size() != 2) throw std::runtime_error("LGP1: bad lk record");
        locked_.insert(pct_decode(f[1]));
        return true;
    }
    if (tag == "alog") {
        if (f.size() != 5) throw std::runtime_error("LGP1: bad alog record");
        AuditLogEntry e;
        e.ts = parse_i64(f[1], "alog ts");
        e.operator_name = pct_decode(f[2]);
        e.action = pct_decode(f[3]);
        e.target = pct_decode(f[4]);
        audit_log_.push_back(std::move(e));
        return true;
    }
    if (tag == "prgr") {
        if (f.size() != 6) throw std::runtime_error("LGP1: bad prgr record");
        ProgramGrant g;
        g.username = pct_decode(f[1]);
        g.kinds = static_cast<std::uint32_t>(parse_u64(f[2], "prgr kinds"));
        g.max_bytes = parse_u64(f[3], "prgr max_bytes");
        g.granted_by = pct_decode(f[4]);
        g.granted_at = parse_i64(f[5], "prgr granted_at");
        if (!g.username.empty() && g.kinds != 0) program_grants_[g.username] = g;
        return true;
    }
    return false;
}

// Save: this module's records, at their place in the store (flush() calls the sections in order).
void FileStore::write_admin(std::ostream& body) {
    for (const auto& name : locked_) {
        if (!by_username_.count(name)) continue;
        body << "lk\t" << pct_encode(name) << "\n";
    }
    {
        std::size_t start = audit_log_.size() > kAuditLogCap ? audit_log_.size() - kAuditLogCap : 0;
        for (std::size_t i = start; i < audit_log_.size(); ++i) {
            const auto& e = audit_log_[i];
            body << "alog\t" << e.ts << "\t" << pct_encode(e.operator_name) << "\t"
                 << pct_encode(e.action) << "\t" << pct_encode(e.target) << "\n";
        }
    }
    for (const auto& [name, g] : program_grants_) {
        if (!by_username_.count(name)) continue;
        body << "prgr\t" << pct_encode(name) << "\t" << g.kinds << "\t" << g.max_bytes << "\t"
             << pct_encode(g.granted_by) << "\t" << g.granted_at << "\n";
    }
}

} // namespace liquidgov::platform
