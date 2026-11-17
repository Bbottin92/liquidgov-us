// Earlier self-portraits' store records (avh, avhs; Brandon 2026-11-16: "save and load prior self portraits").
// A new file because committed tests stay frozen (hard rules #6847b, #1bcfd). Covers: oldest-first order, the keep-10
// cap (the dropped blob ids come back so the caller can release them), removing one by index, the public flag (default
// off, only 1 is stored), save/load round trip, rows of a deleted account not written, and malformed rows refused.
#include "liquidgov/platform/auth.hpp"
#include "liquidgov/platform/file_store.hpp"

#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

using namespace liquidgov::platform;

namespace {

class NeverBreached : public BreachChecker {
public:
    bool is_breached(const std::string&) const override { return false; }
};

std::string tmp_path() {
    char buf[] = "/tmp/lgp-portraits-XXXXXX";
    int fd = mkstemp(buf);
    assert(fd >= 0);
    close(fd);
    unlink(buf);
    return std::string(buf) + ".lgp";
}

std::string slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void append(const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::app);
    out << text;
}

bool loads(const std::string& path) {
    try { FileStore s(path); return true; } catch (const std::exception&) { return false; }
}

} // namespace

int main() {
    NeverBreached breach;
    const std::string path = tmp_path();
    AccountId ann = 0, bob = 0;
    {
        FileStore store(path);
        assert(create_account(store, breach, "ann", "correct-horse-battery-staple", "+15559320001").error == AuthError::None);
        assert(create_account(store, breach, "bob", "correct-horse-battery-staple", "+15559320002").error == AuthError::None);
        ann = store.find_by_username("ann")->id;
        bob = store.find_by_username("bob")->id;
        assert(ann && bob && ann != bob);
        assert(store.portrait_history(ann).empty());
        assert(!store.portrait_history_public(ann));
        assert(!store.drop_portrait(ann, 0));
        std::cout << "empty start ok\n";

        // oldest first
        assert(store.keep_portrait(ann, "blob-1", 100).empty());
        assert(store.keep_portrait(ann, "blob-2", 200).empty());
        auto h = store.portrait_history(ann);
        assert(h.size() == 2 && h[0].blob_id == "blob-1" && h[1].blob_id == "blob-2" && h[1].created_at == 200);
        assert(store.portrait_history(bob).empty()); // per person
        std::cout << "order ok\n";

        // the cap: keeps the newest kPortraitHistoryKeep, hands back what it dropped, oldest first
        assert(FileStore::kPortraitHistoryKeep == 10);
        std::vector<std::string> dropped;
        for (int i = 3; i <= 13; ++i) {
            auto d = store.keep_portrait(ann, "blob-" + std::to_string(i), i * 100);
            dropped.insert(dropped.end(), d.begin(), d.end());
        }
        h = store.portrait_history(ann);
        assert(h.size() == 10 && h.front().blob_id == "blob-4" && h.back().blob_id == "blob-13");
        assert(dropped.size() == 3 && dropped[0] == "blob-1" && dropped[1] == "blob-2" && dropped[2] == "blob-3");
        std::cout << "cap ok\n";

        // remove one by index
        assert(!store.drop_portrait(ann, 10));
        auto gone = store.drop_portrait(ann, 0);
        assert(gone && *gone == "blob-4");
        assert(store.portrait_history(ann).size() == 9 && store.portrait_history(ann).front().blob_id == "blob-5");
        std::cout << "drop ok\n";

        // the public flag: default off; off removes the row
        assert(!store.portrait_history_public(ann));
        store.set_portrait_history_public(ann, true);
        store.set_portrait_history_public(bob, true);
        assert(store.portrait_history_public(ann) && store.portrait_history_public(bob));
        store.set_portrait_history_public(bob, false);
        assert(!store.portrait_history_public(bob));
        assert(store.all_portrait_history_blob_ids().size() == 9);
        std::cout << "public flag ok\n";
    }
    {
        // everything survives a save + load, in order; only the "on" choice is written
        const std::string disk = slurp(path);
        assert(disk.find("\navh\t" + std::to_string(ann) + "\tblob-5\t500\n") != std::string::npos);
        assert(disk.find("\navhs\t" + std::to_string(ann) + "\t1\n") != std::string::npos);
        assert(disk.find("\navhs\t" + std::to_string(bob)) == std::string::npos);
        assert(disk.find("blob-4") == std::string::npos && disk.find("blob-1\t") == std::string::npos);
        FileStore store(path);
        auto h = store.portrait_history(ann);
        assert(h.size() == 9 && h.front().blob_id == "blob-5" && h.back().blob_id == "blob-13");
        assert(store.portrait_history_public(ann) && !store.portrait_history_public(bob));
        // a person who is deleted takes the list and the choice with them
        store.delete_account(ann);
        assert(store.portrait_history(ann).empty() && !store.portrait_history_public(ann));
        std::cout << "round trip + delete ok\n";
    }
    {
        // after the delete, nothing of ann's is written back
        const std::string disk = slurp(path);
        assert(disk.find("\navh\t") == std::string::npos && disk.find("\navhs\t") == std::string::npos);
        // malformed rows fail the load (closed), not silently
        const std::string bad = tmp_path();
        { std::ofstream out(bad, std::ios::binary); out << slurp(path); }
        assert(loads(bad));
        append(bad, "avh\t1\tonly-two-fields\n");
        assert(!loads(bad));
        { std::ofstream out(bad, std::ios::binary); out << slurp(path); }
        append(bad, "avhs\t1\n");
        assert(!loads(bad));
        { std::ofstream out(bad, std::ios::binary); out << slurp(path); }
        append(bad, "avh\t0\tblob\t5\n");
        assert(!loads(bad));
        unlink(bad.c_str());
        std::cout << "malformed rows refused ok\n";
    }
    unlink(path.c_str());
    std::cout << "portrait_history_store_test: ok\n";
    return 0;
}
