#include "liquidgov/store.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace liquidgov;

static LedgerEvent enroll(EventId id, Timestamp at, CitizenId cid, const char* handle) {
    LedgerEvent e;
    e.id = id;
    e.at = at;
    e.act = EnrollCitizen{cid, handle};
    return e;
}

int main() {
    auto path = std::filesystem::temp_directory_path() / "liquidgov-replica.ledger";
    std::filesystem::remove(path);

    {
        auto store = GovernanceStore::load_or_create(path);
        assert(!store.submit(enroll(EventId{1}, 1, CitizenId{1}, "alice")));
        assert(!store.submit(enroll(EventId{2}, 2, CitizenId{2}, "bob")));
    }

    {
        auto r = Replica::open(path);
        assert(r.applied() == 2);
        assert(r.polity().active_citizens() == 2);
        assert(r.polity().tip() != zero_hash());
    }

    {
        auto r = Replica::open(path);
        assert(r.applied() == 2);
        auto store = GovernanceStore::load_or_create(path);
        assert(!store.submit(enroll(EventId{3}, 3, CitizenId{3}, "cara")));
        assert(r.applied() == 2);
        assert(!r.pull());
        assert(r.applied() == 3);
        assert(r.polity().active_citizens() == 3);
    }

    // Tamper after a good pull: next pull fails, last good Polity kept.
    {
        auto r = Replica::open(path);
        const auto good_n = r.applied();
        const auto good_tip = r.polity().tip();
        {
            std::fstream io(path, std::ios::binary | std::ios::in | std::ios::out);
            unsigned char lenb[4];
            io.read(reinterpret_cast<char*>(lenb), 4);
            const auto n = static_cast<std::uint32_t>(lenb[0]) |
                           (static_cast<std::uint32_t>(lenb[1]) << 8) |
                           (static_cast<std::uint32_t>(lenb[2]) << 16) |
                           (static_cast<std::uint32_t>(lenb[3]) << 24);
            io.seekp(4 + static_cast<std::streamoff>(n / 2));
            char c = 0;
            io.read(&c, 1);
            io.seekp(-1, std::ios::cur);
            c = static_cast<char>(c ^ 0x5a);
            io.write(&c, 1);
        }
        auto err = r.pull();
        assert(err.has_value());
        assert(r.applied() == good_n);
        assert(r.polity().tip() == good_tip);
    }

    std::filesystem::remove(path);
    std::cout << "replica ok\n";
    return 0;
}
