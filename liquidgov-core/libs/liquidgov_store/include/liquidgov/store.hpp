#pragma once

#include "liquidgov/state.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace liquidgov {

// Length-prefixed append log. One record = encode_for_hash bytes + content_hash.
// Polity is not aware of this type.
class LedgerFile {
public:
    explicit LedgerFile(std::filesystem::path path);

    void append(const LedgerEvent& event);

    struct Read {
        std::vector<LedgerEvent> events;
        std::uint64_t good_bytes = 0; // offset after last complete record
    };
    Read read_all() const;
    void truncate_to(std::uint64_t good_bytes);
    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

struct ReplayError {
    std::string message;
    EventId at = EventId::None;
};

// Fail-closed: first hash or apply mismatch refuses the whole rebuild.
std::optional<ReplayError> replay_into(Polity& polity, const std::vector<LedgerEvent>& events);

class GovernanceStore {
public:
    static GovernanceStore load_or_create(std::filesystem::path ledger_path);

    // Apply first, then append the committed (hash-filled) event and flush.
    // A crash between the two may drop the last submit; it will not boot a
    // chain that memory accepted and disk never saw. Poisoning the log with
    // an un-applicable record is the worse failure.
    std::optional<ApplyError> submit(LedgerEvent event);

    Polity& polity() { return polity_; }
    const Polity& polity() const { return polity_; }

private:
    GovernanceStore(LedgerFile file, Polity polity);
    LedgerFile file_;
    Polity polity_;
};

// Single-writer follower. Reads only complete flushed records from a LedgerFile
// and applies them through replay_into. Does not submit. Does not read Polity RAM.
class Replica {
public:
    static Replica open(std::filesystem::path ledger_path);

    // Rebuild from the flushed file. Fail-closed on hash/apply mismatch;
    // on failure the previous successful Polity is kept.
    std::optional<ReplayError> pull();

    const Polity& polity() const { return polity_; }
    std::size_t applied() const { return applied_; }

private:
    explicit Replica(std::filesystem::path path);
    std::filesystem::path path_;
    Polity polity_;
    std::size_t applied_ = 0;
};

} // namespace liquidgov
