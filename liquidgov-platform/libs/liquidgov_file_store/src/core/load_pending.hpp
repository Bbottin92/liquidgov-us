// LoadPending: rows load() parks until every line is read (a row may come before the record it belongs to).
// Each record module's read_* fills its own fields; its finish_* applies them (file_store.cpp calls them in order).
// Moved out of load() 2026-11-12 (the same declarations, word for word).
#pragma once

#include "liquidgov/platform/file_store.hpp"

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace liquidgov::platform {

struct FileStore::LoadPending {
    std::map<std::uint64_t, std::pair<std::string, std::uint64_t>> pending_reposts;
    std::map<std::uint64_t, std::pair<std::string, std::string>> pending_attachments;
    struct PendingBk {
        int kind = 0;
        std::string unit;
        double min_v = 0;
        double max_v = 0;
        double step = 1;
    };
    std::map<std::uint64_t, PendingBk> pending_bk;
    std::map<std::uint64_t, std::vector<std::pair<int, std::string>>> pending_bo;
    std::set<std::uint64_t> pending_popen;
    std::map<std::uint64_t, std::vector<std::pair<int, std::string>>> pending_pba;
    std::map<std::uint64_t, std::string> pending_poff;
    std::map<std::uint64_t, std::string> pending_pcity;
    std::map<std::uint64_t, std::string> pending_pwho;
    std::map<std::string, std::vector<std::string>> pending_ctag;
    std::map<std::uint64_t, std::vector<std::string>> pending_ptag;
    std::vector<ProposalReply> pending_prply;
    std::map<std::uint64_t, std::map<std::string, std::string>> pending_prx;
    std::map<std::uint64_t, PostAudience> pending_audience;
    std::map<std::uint64_t, std::vector<std::string>> pending_exceptions;
    std::map<std::uint64_t, std::vector<std::string>> pending_friend_allowlist;
    std::map<std::uint64_t, std::vector<std::string>> pending_extra_groups;
    std::map<std::string, std::string> pending_city; // slug -> city (ccity)
    std::map<std::string, std::set<std::string>> pending_clist; // city key -> usernames (clist)
};

} // namespace liquidgov::platform
