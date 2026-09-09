#include "liquidgov/platform/auth.hpp"

#include <cassert>
#include <iostream>
#include <map>

using namespace liquidgov::platform;

namespace {

// Minimal in-memory AccountStore for testing. Not the real persistence
// layer — that's a separate, real-database implementation of the same
// interface.
class InMemoryStore : public AccountStore {
public:
    bool username_exists(const std::string& username) const override {
        return by_username_.count(username) > 0;
    }

    std::optional<PlatformAccount> find_by_username(const std::string& username) const override {
        auto it = by_username_.find(username);
        if (it == by_username_.end()) return std::nullopt;
        return accounts_.at(it->second);
    }

    AccountId create(const PlatformAccount& account) override {
        AccountId id = next_id_++;
        PlatformAccount stored = account;
        stored.id = id;
        accounts_[id] = stored;
        by_username_[account.username] = id;
        return id;
    }

    void save_session(const Session& session) override {
        sessions_[session.id] = session;
    }

    std::vector<Session> sessions_for(AccountId account) const override {
        std::vector<Session> out;
        for (const auto& [id, s] : sessions_) {
            if (s.account == account) out.push_back(s);
        }
        return out;
    }

    void revoke_session(const SessionId& id) override {
        auto it = sessions_.find(id);
        if (it != sessions_.end()) it->second.revoked = true;
    }

private:
    AccountId next_id_ = 1;
    std::map<std::string, AccountId> by_username_;
    std::map<AccountId, PlatformAccount> accounts_;
    std::map<SessionId, Session> sessions_;
};

class NeverBreached : public BreachChecker {
public:
    bool is_breached(const std::string&) const override { return false; }
};

class AlwaysAllow : public RateLimiter {
public:
    bool allow(const std::string&, const std::string&) override { return true; }
    void record_attempt(const std::string&, const std::string&) override {}
};

class BlockAfterN : public RateLimiter {
public:
    explicit BlockAfterN(int n) : limit_(n) {}
    bool allow(const std::string& key, const std::string& ip) override {
        return counts_[key] < limit_ && counts_[ip] < limit_;
    }
    void record_attempt(const std::string& key, const std::string& ip) override {
        counts_[key]++;
        counts_[ip]++;
    }

private:
    int limit_;
    std::map<std::string, int> counts_;
};

} // namespace

int main() {
    InMemoryStore store;
    NeverBreached breach_checker;
    AlwaysAllow limiter;

    // 1. Signup succeeds
    auto signup = create_account(store, breach_checker, "brandon", "correct-horse-battery-staple", "+15551212");
    assert(signup.error == AuthError::None);
    std::cout << "signup ok\n";

    // 2. Duplicate username rejected
    auto dup = create_account(store, breach_checker, "brandon", "another-password-here", "+15551212");
    assert(dup.error == AuthError::UsernameTaken);
    std::cout << "duplicate username rejected\n";

    // 3. Correct login succeeds, issues a session
    auto good_login = login(store, limiter, "brandon", "correct-horse-battery-staple", "test-device", "127.0.0.1");
    assert(good_login.error == AuthError::None);
    assert(good_login.session.has_value());
    std::cout << "correct login ok, session issued\n";

    // 4. Wrong password on a real account -> InvalidCredentials
    auto wrong_password = login(store, limiter, "brandon", "wrong-password", "test-device", "127.0.0.1");
    assert(wrong_password.error == AuthError::InvalidCredentials);
    std::cout << "wrong password rejected\n";

    // 5. Unknown username -> the SAME error as wrong password, not a
    //    distinct "no such user" — this is the property that matters.
    auto unknown_user = login(store, limiter, "nobody-here", "whatever", "test-device", "127.0.0.1");
    assert(unknown_user.error == AuthError::InvalidCredentials);
    assert(unknown_user.error == wrong_password.error);
    std::cout << "unknown username returns identical error to wrong password\n";

    // 6. Rate limiting blocks after N attempts
    BlockAfterN strict_limiter(2);
    login(store, strict_limiter, "brandon", "wrong", "d", "127.0.0.1");
    login(store, strict_limiter, "brandon", "wrong", "d", "127.0.0.1");
    auto rate_limited = login(store, strict_limiter, "brandon", "wrong", "d", "127.0.0.1");
    assert(rate_limited.error == AuthError::RateLimited);
    std::cout << "rate limiting engages after threshold\n";

    // 7. logout_all_devices keeps the current session
    auto second = login(store, limiter, "brandon", "correct-horse-battery-staple", "device-2", "127.0.0.1");
    assert(second.session.has_value());
    auto before = store.sessions_for(good_login.session->account);
    assert(before.size() >= 2);
    logout_all_devices(store, good_login.session->account, second.session->id);
    bool kept = false;
    for (const auto& s : store.sessions_for(good_login.session->account)) {
        if (s.id == second.session->id) {
            assert(!s.revoked);
            kept = true;
        } else {
            assert(s.revoked);
        }
    }
    assert(kept);
    std::cout << "logout_all_devices keeps the current session\n";

    // 8. signup without a verified phone is rejected
    auto no_phone = create_account(store, breach_checker, "nopho", "correct-horse-battery-staple", "");
    assert(no_phone.error == AuthError::PhoneNotVerified);
    std::cout << "signup requires verified phone\n";

    std::cout << "\nall auth tests passed\n";
    return 0;
}
