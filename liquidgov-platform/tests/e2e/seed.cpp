// Seeds a throwaway LiquidGov store for the program-mail end-to-end test:
// Bbottin (the default operator), pal and stranger; Bbottin<->pal and
// pal<->stranger are friends. Prints "name session" per line.
#include "liquidgov/platform/auth.hpp"
#include "liquidgov/platform/file_store.hpp"

#include <iostream>

using namespace liquidgov::platform;

class AlwaysAllow : public RateLimiter {
public:
    bool allow(const std::string&, const std::string&) override { return true; }
    void record_attempt(const std::string&, const std::string&) override {}
};

class NeverBreached : public BreachChecker {
public:
    bool is_breached(const std::string&) const override { return false; }
};

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    FileStore store(argv[1]);
    NeverBreached breach;
    AlwaysAllow limiter;
    const char* names[] = {"Bbottin", "pal", "stranger"};
    const char* phones[] = {"+15550000001", "+15550000002", "+15550000003"};
    for (int i = 0; i < 3; ++i) {
        auto r = create_account(store, breach, names[i], "correct-horse-battery-staple", phones[i]);
        if (r.error != AuthError::None) { std::cerr << "signup failed " << names[i] << " " << int(r.error) << "\n"; return 1; }
        auto l = login(store, limiter, names[i], "correct-horse-battery-staple", "e2e", "127.0.0.1");
        if (l.error != AuthError::None || !l.session) { std::cerr << "login failed " << names[i] << "\n"; return 1; }
        std::cout << names[i] << " " << l.session->id << "\n";
    }
    store.add_friend_request("Bbottin", "pal");
    store.accept_friend_request("Bbottin", "pal");
    store.add_friend_request("pal", "stranger");
    store.accept_friend_request("pal", "stranger");
    return store.are_friends("Bbottin", "pal") && store.are_friends("pal", "stranger") ? 0 : 3;
}
