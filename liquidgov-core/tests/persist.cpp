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
    // encode → decode → encode is byte-identical (replay depends on this).
    {
        Polity p;
        auto err = p.apply(enroll(EventId{1}, 1, CitizenId{1}, "alice"));
        assert(!err);
        const auto& ev = p.ledger().back();
        auto bytes = encode_for_hash(ev);
        auto back = decode_for_hash(bytes);
        assert(back.has_value());
        back->content_hash = ev.content_hash;
        assert(encode_for_hash(*back) == bytes);
        assert(back->id == ev.id);
        assert(back->at == ev.at);
        assert(back->prev_hash == ev.prev_hash);
        assert(std::holds_alternative<EnrollCitizen>(back->act));
        assert(std::get<EnrollCitizen>(back->act).handle == "alice");
    }

    auto path = std::filesystem::temp_directory_path() / "liquidgov-persist.ledger";
    std::filesystem::remove(path);

    {
        auto store = GovernanceStore::load_or_create(path);
        assert(!store.submit(enroll(EventId{1}, 1, CitizenId{1}, "alice")));
        assert(!store.submit(enroll(EventId{2}, 2, CitizenId{2}, "bob")));
        assert(!store.submit(enroll(EventId{3}, 3, CitizenId{3}, "cara")));
        assert(store.polity().active_citizens() == 3);
    }

    const auto full = std::filesystem::file_size(path);
    assert(full > 16);

    // Coherent substitution: record 2 is replaced by a different act whose
    // content_hash is valid for the new payload. Record 3 is untouched.
    // Fail because record 3.prev_hash != hash(new record 2).
    {
        LedgerFile f(path);
        auto evs = f.read_all().events;
        assert(evs.size() == 3);
        LedgerEvent sub = evs[1];
        sub.act = EnrollCitizen{CitizenId{2}, "mallory"};
        sub.content_hash = hash_event(sub);
        assert(sub.content_hash != evs[1].content_hash);
        assert(sub.prev_hash == evs[1].prev_hash);
        std::vector<LedgerEvent> forged{evs[0], sub, evs[2]};
        Polity p;
        auto err = replay_into(p, forged);
        assert(err.has_value());
        assert(err->message.find("prev hash") != std::string::npos);
    }

    // Truncated tail: last record incomplete → dropped, earlier events intact.
    {
        auto cut = path;
        cut += ".trunc";
        std::filesystem::copy_file(path, cut, std::filesystem::copy_options::overwrite_existing);
        std::filesystem::resize_file(cut, full - 8);
        LedgerFile f(cut);
        auto evs = f.read_all().events;
        assert(evs.size() == 2);
        Polity p;
        auto err = replay_into(p, evs);
        assert(!err);
        assert(p.active_citizens() == 2);
        std::filesystem::remove(cut);
    }

    // Tampered complete record: fail-closed, do not boot.
    {
        auto dirty = path;
        dirty += ".bad";
        std::filesystem::copy_file(path, dirty, std::filesystem::copy_options::overwrite_existing);
        {
            std::fstream io(dirty, std::ios::binary | std::ios::in | std::ios::out);
            unsigned char lenb[4];
            io.read(reinterpret_cast<char*>(lenb), 4);
            const auto n = static_cast<std::uint32_t>(lenb[0]) | (static_cast<std::uint32_t>(lenb[1]) << 8) |
                           (static_cast<std::uint32_t>(lenb[2]) << 16) | (static_cast<std::uint32_t>(lenb[3]) << 24);
            // Flip a payload byte of the first complete record, not the length prefix.
            io.seekg(4 + static_cast<std::streamoff>(n / 2));
            io.seekp(4 + static_cast<std::streamoff>(n / 2));
            char c = 0;
            io.read(&c, 1);
            io.seekp(-1, std::ios::cur);
            c = static_cast<char>(c ^ 0x5a);
            io.write(&c, 1);
        }
        const auto before = std::filesystem::file_size(dirty);
        bool refused = false;
        try {
            auto store = GovernanceStore::load_or_create(dirty);
            (void)store;
        } catch (const std::exception&) {
            refused = true;
        }
        assert(refused);
        assert(std::filesystem::file_size(dirty) == before);
        std::filesystem::remove(dirty);
    }

    // Last record only: internally broken but framing-complete. Must throw
    // and leave the file length alone (not trim the bad last record).
    {
        auto last = path;
        last += ".last";
        std::filesystem::copy_file(path, last, std::filesystem::copy_options::overwrite_existing);
        {
            std::fstream io(last, std::ios::binary | std::ios::in | std::ios::out);
            io.seekp(-40, std::ios::end);
            char c = 0;
            io.read(&c, 1);
            io.seekp(-1, std::ios::cur);
            c = static_cast<char>(c ^ 0x5a);
            io.write(&c, 1);
        }
        const auto before = std::filesystem::file_size(last);
        bool refused = false;
        try {
            auto store = GovernanceStore::load_or_create(last);
            (void)store;
        } catch (const std::exception&) {
            refused = true;
        }
        assert(refused);
        assert(std::filesystem::file_size(last) == before);
        std::filesystem::remove(last);
    }

    // Complete length prefix claiming n<32: corruption, not truncation → throw.
    {
        auto broken = path;
        broken += ".len";
        std::filesystem::copy_file(path, broken, std::filesystem::copy_options::overwrite_existing);
        {
            std::fstream io(broken, std::ios::binary | std::ios::in | std::ios::out);
            unsigned char one[4] = {1, 0, 0, 0};
            io.write(reinterpret_cast<char*>(one), 4);
        }
        const auto before = std::filesystem::file_size(broken);
        bool refused = false;
        try {
            auto store = GovernanceStore::load_or_create(broken);
            (void)store;
        } catch (const std::exception&) {
            refused = true;
        }
        assert(refused);
        assert(std::filesystem::file_size(broken) == before);
        std::filesystem::remove(broken);
    }

    // First-write crash: partial length prefix only. Boot empty, do not brick.
    {
        auto baby = std::filesystem::temp_directory_path() / "liquidgov-first-crash.ledger";
        std::filesystem::remove(baby);
        {
            std::ofstream o(baby, std::ios::binary);
            unsigned char two[] = {0x20, 0x00};
            o.write(reinterpret_cast<char*>(two), 2);
        }
        auto store = GovernanceStore::load_or_create(baby);
        assert(store.polity().ledger().empty());
        std::filesystem::remove(baby);
    }

    // First-write crash: valid length prefix, body cut short. Boot empty.
    {
        auto baby = std::filesystem::temp_directory_path() / "liquidgov-first-short.ledger";
        std::filesystem::remove(baby);
        {
            std::ofstream o(baby, std::ios::binary);
            unsigned char len[4] = {80, 0, 0, 0};
            o.write(reinterpret_cast<char*>(len), 4);
            o.write("partial", 7);
        }
        auto store = GovernanceStore::load_or_create(baby);
        assert(store.polity().ledger().empty());
        std::filesystem::remove(baby);
    }

    // Crash tail left on disk, then a new submit after recover must not
    // land behind the garbage. Reload must see the post-crash event.
    {
        auto rec = std::filesystem::temp_directory_path() / "liquidgov-recover.ledger";
        std::filesystem::remove(rec);
        {
            auto store = GovernanceStore::load_or_create(rec);
            assert(!store.submit(enroll(EventId{1}, 1, CitizenId{1}, "alice")));
            assert(!store.submit(enroll(EventId{2}, 2, CitizenId{2}, "bob")));
        }
        {
            std::ofstream o(rec, std::ios::binary | std::ios::app);
            o.write("GARBAGE", 7);
        }
        {
            auto store = GovernanceStore::load_or_create(rec);
            assert(store.polity().ledger().size() == 2);
            assert(!store.submit(enroll(EventId{3}, 3, CitizenId{3}, "cara")));
        }
        {
            auto store = GovernanceStore::load_or_create(rec);
            assert(store.polity().ledger().size() == 3);
            assert(store.polity().active_citizens() == 3);
            assert(std::get<EnrollCitizen>(store.polity().ledger().back().act).handle ==
                   "cara");
        }
        std::filesystem::remove(rec);
    }

    std::filesystem::remove(path);
    std::cout << "persist ok\n";
    return 0;
}
