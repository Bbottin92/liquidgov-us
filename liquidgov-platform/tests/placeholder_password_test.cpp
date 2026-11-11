// The "no password" placeholder must never sign anyone in (found 2026-11-11, live since 2026-09-15).
// Google-only accounts store dummy_hash() as their password: a real argon2 hash of a fixed phrase written in
// password.cpp. login() only checked the typed password against the stored hash, so typing that phrase signed in to
// any Google-only account. dummy_hash() has a fresh random salt in each process, so an account created before the
// last restart holds a DIFFERENT string than today's dummy_hash(): this test covers both.
// Checks use explicit failure counts (not assert) so they still run in a release build.
#include "liquidgov/platform/auth.hpp"
#include "liquidgov/platform/password.hpp"

#include <iostream>
#include <map>

using namespace liquidgov::platform;

namespace {

const char* kPhrase = "liquidgov-dummy-hash-never-a-real-account-password"; // the placeholder's plaintext

class InMemoryStore : public AccountStore {
public:
    bool username_exists(const std::string& u) const override { return by_username_.count(u) > 0; }
    std::optional<PlatformAccount> find_by_username(const std::string& u) const override {
        auto it = by_username_.find(u);
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
    void save_session(const Session& s) override { sessions_[s.id] = s; }
    std::vector<Session> sessions_for(AccountId a) const override {
        std::vector<Session> out;
        for (const auto& [id, s] : sessions_) if (s.account == a) out.push_back(s);
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

class AlwaysAllow : public RateLimiter {
public:
    bool allow(const std::string&, const std::string&) override { return true; }
    void record_attempt(const std::string&, const std::string&) override {}
};

int fails = 0;
void check(const char* name, bool ok) {
    std::cout << (ok ? "PASS " : "FAIL ") << name << "\n";
    if (!ok) ++fails;
}

} // namespace

int main() {
    InMemoryStore store;
    AlwaysAllow limiter;

    PlatformAccount today;          // a Google-only account created since the last restart
    today.username = "googletoday";
    today.password_hash = dummy_hash();
    store.create(today);
    PlatformAccount older;          // one created before a restart: a different salt, same phrase
    older.username = "googleolder";
    older.password_hash = hash_password(kPhrase);
    store.create(older);
    PlatformAccount real;           // an ordinary password account
    real.username = "realperson";
    real.password_hash = hash_password("correct-horse-battery-staple");
    store.create(real);

    check("precondition: the older account's hash differs from today's dummy_hash()",
          older.password_hash != dummy_hash());

    auto a = login(store, limiter, "googletoday", kPhrase, "t", "127.0.0.1");
    check("the placeholder phrase does not sign in to a Google-only account (today's hash)",
          a.error == AuthError::InvalidCredentials && !a.session);
    auto b = login(store, limiter, "googleolder", kPhrase, "t", "127.0.0.1");
    check("the placeholder phrase does not sign in to a Google-only account (hash from before a restart)",
          b.error == AuthError::InvalidCredentials && !b.session);
    check("verify_password refuses the phrase against any hash, so every caller is covered",
          !verify_password(kPhrase, older.password_hash) && !verify_password(kPhrase, dummy_hash()));

    auto c = login(store, limiter, "realperson", "correct-horse-battery-staple", "t", "127.0.0.1");
    check("an ordinary password account still signs in", c.error == AuthError::None && c.session.has_value());
    auto d = login(store, limiter, "realperson", "wrong-password-entirely", "t", "127.0.0.1");
    check("... and a wrong password is still refused", d.error == AuthError::InvalidCredentials);

    std::cout << "placeholder_password_test: " << fails << " failure(s)\n";
    return fails ? 1 : 0;
}
