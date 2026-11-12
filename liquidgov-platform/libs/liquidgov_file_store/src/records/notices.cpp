// notices: Notices (the bell).
// LGP1 record types: note (format: the header comment of file_store.hpp).
// Split out of file_store.cpp 2026-11-12 (code moved, not rewritten). Listed in ../../sources.cmake.
#include "liquidgov/platform/file_store.hpp"

#include "core/codec.hpp"
#include "core/load_pending.hpp"
#include "liquidgov/platform/clock.hpp"

#include <cstddef>
#include <ostream>
#include <stdexcept>
#include <vector>

namespace liquidgov::platform {

using namespace store_detail;

void FileStore::add_notice(const std::string& to, const std::string& type, const std::string& from,
                           const std::string& extra) {
    Notice n;
    n.id = next_note_id_++;
    n.to = to;
    n.type = type;
    n.from = from;
    n.ts = now();
    n.read = false;
    n.extra = extra;
    notices_.push_back(n);
    flush();
}

std::vector<Notice> FileStore::notices_for(const std::string& user, std::size_t limit) const {
    std::vector<Notice> out;
    for (auto it = notices_.rbegin(); it != notices_.rend(); ++it) {
        if (it->to == user) {
            out.push_back(*it);
            if (out.size() >= limit) break;
        }
    }
    return out;
}

int FileStore::unread_count(const std::string& user) const {
    int n = 0;
    for (const auto& note : notices_) {
        if (note.to == user && !note.read) ++n;
    }
    return n;
}

int FileStore::unread_message_count(const std::string& user) const {
    int n = 0;
    for (const auto& note : notices_) {
        if (note.to == user && !note.read && note.type == "message") ++n;
    }
    return n;
}

void FileStore::mark_notices_read(const std::string& user) {
    bool any = false;
    for (auto& note : notices_) {
        if (note.to == user && !note.read) {
            note.read = true;
            any = true;
        }
    }
    if (any) flush();
}

void FileStore::mark_notices_read_of_type(const std::string& user, const std::string& type) {
    bool any = false;
    for (auto& note : notices_) {
        if (note.to == user && !note.read && note.type == type) {
            note.read = true;
            any = true;
        }
    }
    if (any) flush();
}

void FileStore::mark_notices_read_except_type(const std::string& user, const std::string& except_type) {
    bool any = false;
    for (auto& note : notices_) {
        if (note.to == user && !note.read && note.type != except_type) {
            note.read = true;
            any = true;
        }
    }
    if (any) flush();
}

// The record types this module reads (one branch each, as they were in load()'s chain).
bool FileStore::read_notices(const std::vector<std::string>& f, LoadPending& pending) {
    (void)pending;
    const std::string& tag = f[0];
    if (tag == "note") {
        if (f.size() != 8) throw std::runtime_error("LGP1: bad note record");
        Notice n;
        n.id = parse_u64(f[1], "note id");
        n.to = pct_decode(f[2]);
        n.type = pct_decode(f[3]);
        n.from = pct_decode(f[4]);
        n.ts = parse_i64(f[5], "note ts");
        n.read = parse_bit(f[6], "note read");
        n.extra = pct_decode(f[7]);
        notices_.push_back(n);
        if (n.id >= next_note_id_) next_note_id_ = n.id + 1;
        return true;
    }
    return false;
}

// Save: this module's records, at their place in the store (flush() calls the sections in order).
void FileStore::write_notices(std::ostream& body) {
    for (const auto& n : notices_) {
        body << "note\t" << n.id << "\t" << pct_encode(n.to) << "\t" << pct_encode(n.type) << "\t"
             << pct_encode(n.from) << "\t" << n.ts << "\t" << (n.read ? 1 : 0) << "\t"
             << pct_encode(n.extra) << "\n";
    }
}

} // namespace liquidgov::platform
