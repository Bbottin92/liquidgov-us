// here: dots on profile pages, the stored Yes/No choice.
// LGP1 record types: here (format: the header comment of file_store.hpp).
// Moved out of file_store.cpp 2026-11-13 (code moved, not rewritten). Listed in ../../sources.cmake.
#include "liquidgov/platform/file_store.hpp"

#include "core/codec.hpp"
#include "core/load_pending.hpp"

#include <algorithm>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace liquidgov::platform {

using namespace store_detail;

std::optional<bool> FileStore::here_choice(const std::string& username) const {
    auto it = here_choice_.find(username);
    if (it == here_choice_.end()) return std::nullopt;
    return it->second;
}

void FileStore::set_here_choice(const std::string& username, bool on) {
    if (!by_username_.count(username)) return;
    here_choice_[username] = on;
    flush();
}

// The record types this module reads (one branch each, as they were in load()'s chain).
bool FileStore::read_here(const std::vector<std::string>& f, LoadPending& pending) {
    (void)pending;
    const std::string& tag = f[0];
    if (tag == "here") {
        // a lower bound, not an exact count: a later release may add trailing fields without breaking a rollback
        if (f.size() < 3 || (f[2] != "0" && f[2] != "1")) throw std::runtime_error("LGP1: bad here record");
        const std::string who = pct_decode(f[1]);
        if (who.empty()) throw std::runtime_error("LGP1: empty here record");
        here_choice_[who] = f[2] == "1";
        return true;
    }
    return false;
}

// After every line is read: a choice whose person is gone (as load() did).
void FileStore::finish_here() {
    for (auto it = here_choice_.begin(); it != here_choice_.end(); )
        it = by_username_.count(it->first) ? std::next(it) : here_choice_.erase(it);
}

// Save: this module's records, at their place in the store (flush() calls the sections in order).
void FileStore::write_here(std::ostream& body) {
    for (const auto& [who, on] : here_choice_) {
        if (!by_username_.count(who)) continue;
        body << "here\t" << pct_encode(who) << "\t" << (on ? 1 : 0) << "\n";
    }
}

} // namespace liquidgov::platform
