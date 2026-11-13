// Dots on profile pages: the stored choice (here <username> <0|1>; Brandon 2026-11-12). A new file because committed
// tests stay frozen (hard rules #6847b, #1bcfd). Absent = not chosen (hidden). Covers set/get, save/load, account
// deletion, a row whose person is gone dropped on load, an extra trailing field accepted (forward compatible), a short
// or bad row failing closed, and a store that never had a choice writing no here rows (still opens on the old binary).
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
    char buf[] = "/tmp/lgp-here-XXXXXX";
    int fd = mkstemp(buf);
    assert(fd >= 0);
    close(fd);
    unlink(buf);
    return std::string(buf) + ".lgp";
}
std::string slurp(const std::string& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}
void append(const std::string& p, const std::string& t) {
    std::ofstream out(p, std::ios::binary | std::ios::app);
    out << t;
}
bool opens(const std::string& p) {
    try { FileStore s(p); return true; } catch (const std::exception&) { return false; }
}
}  // namespace

int main() {
    NeverBreached breach;
    const std::string path = tmp_path();
    {
        FileStore store(path);
        assert(create_account(store, breach, "ann", "correct-horse-battery-staple", "+15559400000").error == AuthError::None);
        assert(create_account(store, breach, "bob", "correct-horse-battery-staple", "+15559400001").error == AuthError::None);
        assert(!store.here_choice("ann"));          // not chosen
        assert(!store.here_choice("nobody"));
    }
    assert(slurp(path).find("\nhere\t") == std::string::npos); // nobody chose: no rows at all
    {
        FileStore store(path);
        store.set_here_choice("ann", true);
        store.set_here_choice("bob", false);
        store.set_here_choice("nobody", true);      // no such person: ignored
        assert(store.here_choice("ann") && *store.here_choice("ann"));
        assert(store.here_choice("bob") && !*store.here_choice("bob"));
        assert(!store.here_choice("nobody"));
        store.set_here_choice("ann", false);
        assert(!*store.here_choice("ann"));
        store.set_here_choice("ann", true);
        std::cout << "set/get ok\n";
    }
    {
        const std::string disk = slurp(path);
        assert(disk.find("\nhere\tann\t1\n") != std::string::npos);
        assert(disk.find("\nhere\tbob\t0\n") != std::string::npos);
        FileStore store(path);
        assert(*store.here_choice("ann") && !*store.here_choice("bob"));
        auto bob = store.find_by_username("bob");
        assert(bob && store.delete_account(bob->id));
        assert(!store.here_choice("bob"));
        std::cout << "save/load + delete ok\n";
    }
    {
        append(path, "here\tghost\t1\nhere\tann\t0\tfuture-field\n");
        FileStore store(path);
        assert(!store.here_choice("ghost"));
        assert(store.here_choice("ann") && !*store.here_choice("ann")); // later row wins, extra field accepted
        std::cout << "load rules ok\n";
    }
    {
        const std::string p2 = tmp_path();
        { FileStore s(p2); }
        append(p2, "here\tann\n");
        assert(!opens(p2));
        const std::string p3 = tmp_path();
        { FileStore s(p3); }
        append(p3, "here\tann\t2\n");
        assert(!opens(p3));
        std::cout << "bad rows fail closed ok\n";
    }
    std::cout << "here_store_test: all ok\n";
    return 0;
}
