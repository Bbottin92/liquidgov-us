// counters: The next-id counters written at the top of the store.
// LGP1 record types: n pn nn gcn gmsn (format: the header comment of file_store.hpp).
// Split out of file_store.cpp 2026-11-12 (code moved, not rewritten). Listed in ../../sources.cmake.
#include "liquidgov/platform/file_store.hpp"

#include "core/codec.hpp"
#include "core/load_pending.hpp"

#include <ostream>
#include <stdexcept>
#include <vector>

namespace liquidgov::platform {

using namespace store_detail;

// The record types this module reads (one branch each, as they were in load()'s chain).
bool FileStore::read_counters(const std::vector<std::string>& f, LoadPending& pending) {
    (void)pending;
    const std::string& tag = f[0];
    if (tag == "n") {
        if (f.size() != 2) throw std::runtime_error("LGP1: bad n record");
        next_id_ = parse_u64(f[1], "next_id");
        if (next_id_ == 0) next_id_ = 1;
        return true;
    }
    if (tag == "pn") {
        if (f.size() != 2) throw std::runtime_error("LGP1: bad pn record");
        next_post_id_ = parse_u64(f[1], "next_post_id");
        if (next_post_id_ == 0) next_post_id_ = 1;
        return true;
    }
    if (tag == "nn") {
        if (f.size() != 2) throw std::runtime_error("LGP1: bad nn record");
        next_note_id_ = parse_u64(f[1], "next_note_id");
        if (next_note_id_ == 0) next_note_id_ = 1;
        return true;
    }
    if (tag == "gcn") {
        if (f.size() != 2) throw std::runtime_error("LGP1: bad gcn record");
        next_conv_id_ = parse_u64(f[1], "next_conversation_id");
        if (next_conv_id_ == 0) next_conv_id_ = 1;
        return true;
    }
    if (tag == "gmsn") {
        if (f.size() != 2) throw std::runtime_error("LGP1: bad gmsn record");
        next_message_id_ = parse_u64(f[1], "next_message_id");
        if (next_message_id_ == 0) next_message_id_ = 1;
        return true;
    }
    return false;
}

// Save: this module's records, at their place in the store (flush() calls the sections in order).
void FileStore::write_counters(std::ostream& body) {
    body << "n\t" << next_id_ << "\n";
    body << "pn\t" << next_post_id_ << "\n";
    body << "nn\t" << next_note_id_ << "\n";
    body << "gcn\t" << next_conv_id_ << "\n";
    body << "gmsn\t" << next_message_id_ << "\n";
}

} // namespace liquidgov::platform
