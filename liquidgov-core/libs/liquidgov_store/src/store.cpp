#include "liquidgov/store.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <stdexcept>
#include <unistd.h>

namespace liquidgov {
namespace {

void write_u32le(std::ostream& o, std::uint32_t v) {
    unsigned char b[4] = {
        static_cast<unsigned char>(v),
        static_cast<unsigned char>(v >> 8),
        static_cast<unsigned char>(v >> 16),
        static_cast<unsigned char>(v >> 24),
    };
    o.write(reinterpret_cast<char*>(b), 4);
}

bool read_u32le(std::istream& i, std::uint32_t& v) {
    unsigned char b[4];
    i.read(reinterpret_cast<char*>(b), 4);
    if (i.gcount() != 4) return false;
    v = static_cast<std::uint32_t>(b[0]) | (static_cast<std::uint32_t>(b[1]) << 8) |
        (static_cast<std::uint32_t>(b[2]) << 16) | (static_cast<std::uint32_t>(b[3]) << 24);
    return true;
}

} // namespace

LedgerFile::LedgerFile(std::filesystem::path path) : path_(std::move(path)) {}

void LedgerFile::append(const LedgerEvent& event) {
    auto body = encode_for_hash(event);
    body.append(reinterpret_cast<const char*>(event.content_hash.data()), event.content_hash.size());

    const auto n = static_cast<std::uint32_t>(body.size());
    std::string rec(4 + body.size(), '\0');
    rec[0] = static_cast<char>(n);
    rec[1] = static_cast<char>(n >> 8);
    rec[2] = static_cast<char>(n >> 16);
    rec[3] = static_cast<char>(n >> 24);
    std::memcpy(rec.data() + 4, body.data(), body.size());

    std::filesystem::create_directories(path_.parent_path().empty() ? "." : path_.parent_path());
    const int fd = ::open(path_.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd < 0) throw std::runtime_error("ledger open failed: " + path_.string());
    const char* p = rec.data();
    std::size_t left = rec.size();
    while (left > 0) {
        const ssize_t n = ::write(fd, p, left);
        if (n < 0) {
            if (errno == EINTR) continue; // nothing written; same offset
            const int e = errno;
            ::close(fd);
            throw std::runtime_error(std::string("ledger write failed: ") + std::strerror(e));
        }
        if (n == 0) {
            ::close(fd);
            throw std::runtime_error("ledger write returned zero");
        }
        p += n; // short count: only the remainder
        left -= static_cast<std::size_t>(n);
    }
    if (::fsync(fd) != 0) {
        const int e = errno;
        ::close(fd);
        throw std::runtime_error(std::string("ledger fsync failed: ") + std::strerror(e));
    }
    ::close(fd);
}

LedgerFile::Read LedgerFile::read_all() const {
    Read out;
    std::ifstream i(path_, std::ios::binary);
    if (!i) return out;
    std::uint64_t pos = 0;
    for (;;) {
        std::uint32_t n = 0;
        if (!read_u32le(i, n)) break; // incomplete length prefix: crash mid-write
        if (n < 32) {
            throw std::runtime_error("ledger corrupt: invalid record length");
        }
        std::string body(n, '\0');
        i.read(body.data(), static_cast<std::streamsize>(n));
        if (static_cast<std::uint32_t>(i.gcount()) != n) break; // claimed > available
        Hash ch{};
        std::memcpy(ch.data(), body.data() + body.size() - 32, 32);
        body.resize(body.size() - 32);
        auto ev = decode_for_hash(body);
        if (!ev) throw std::runtime_error("ledger decode failed");
        ev->content_hash = ch;
        out.events.push_back(std::move(*ev));
        pos += 4ull + n;
        out.good_bytes = pos;
    }
    return out;
}

void LedgerFile::truncate_to(std::uint64_t good_bytes) {
    if (!std::filesystem::exists(path_)) return;
    const auto size = std::filesystem::file_size(path_);
    if (size > good_bytes) std::filesystem::resize_file(path_, good_bytes);
}

std::optional<ReplayError> replay_into(Polity& polity, const std::vector<LedgerEvent>& events) {
    for (const auto& ev : events) {
        if (ev.prev_hash != polity.tip()) {
            return ReplayError{"prev hash mismatch", ev.id};
        }
        LedgerEvent check = ev;
        check.content_hash = zero_hash();
        if (hash_event(check) != ev.content_hash) {
            return ReplayError{"content hash mismatch", ev.id};
        }
        auto err = polity.apply(ev);
        if (err) return ReplayError{err->message, ev.id};
    }
    return std::nullopt;
}

GovernanceStore::GovernanceStore(LedgerFile file, Polity polity)
    : file_(std::move(file)), polity_(std::move(polity)) {}

GovernanceStore GovernanceStore::load_or_create(std::filesystem::path ledger_path) {
    LedgerFile file(std::move(ledger_path));
    Polity polity;
    auto snap = file.read_all();
    if (auto err = replay_into(polity, snap.events)) {
        throw std::runtime_error("ledger replay failed: " + err->message);
    }
    // Framing-only good_bytes. Never shrink after a hash/apply failure —
    // that would turn fail-closed tamper into a silently shortened log.
    file.truncate_to(snap.good_bytes);
    return GovernanceStore(std::move(file), std::move(polity));
}

std::optional<ApplyError> GovernanceStore::submit(LedgerEvent event) {
    auto err = polity_.apply(event);
    if (err) return err;
    file_.append(polity_.ledger().back());
    return std::nullopt;
}

Replica::Replica(std::filesystem::path path) : path_(std::move(path)) {}

Replica Replica::open(std::filesystem::path ledger_path) {
    Replica r(std::move(ledger_path));
    if (auto err = r.pull()) {
        throw std::runtime_error("replica open failed: " + err->message);
    }
    return r;
}

std::optional<ReplayError> Replica::pull() {
    LedgerFile file(path_);
    LedgerFile::Read snap;
    try {
        snap = file.read_all();
    } catch (const std::exception& e) {
        return ReplayError{std::string(e.what())};
    }
    Polity fresh;
    if (auto err = replay_into(fresh, snap.events)) return err;
    polity_ = std::move(fresh);
    applied_ = snap.events.size();
    return std::nullopt;
}

} // namespace liquidgov
