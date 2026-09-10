#include "liquidgov/platform/auth.hpp"

#include "liquidgov/platform/clock.hpp"

namespace liquidgov::platform {

AuthResult create_account(AccountStore& store, const BreachChecker& breach_checker,
                           const std::string& username, const std::string& password,
                           const std::string& verified_phone_e164) {
    if (verified_phone_e164.empty()) {
        return {AuthError::PhoneNotVerified, std::nullopt};
    }
    if (store.username_exists(username)) {
        return {AuthError::UsernameTaken, std::nullopt};
    }
    if (breach_checker.is_breached(password)) {
        return {AuthError::PasswordBreached, std::nullopt};
    }

    PlatformAccount account;
    account.username = username;
    account.password_hash = hash_password(password);
    account.phone_verified = true;
    account.created_at = now();
    account.id = store.create(account);

    return {AuthError::None, std::nullopt};
}

AuthResult login(AccountStore& store, RateLimiter& limiter,
                  const std::string& username, const std::string& password,
                  const std::string& device_label, const std::string& ip) {
    if (!limiter.allow(username, ip)) {
        return {AuthError::RateLimited, std::nullopt};
    }
    limiter.record_attempt(username, ip);

    auto account = store.find_by_username(username);

    bool ok;
    if (account) {
        ok = verify_password(password, account->password_hash);
    } else {
        verify_password(password, dummy_hash());
        ok = false;
    }

    if (!ok) {
        return {AuthError::InvalidCredentials, std::nullopt};
    }

    Session session;
    session.id = generate_session_id();
    session.account = account->id;
    session.created_at = session.last_seen = now();
    session.expires_at = now() + kSessionAbsoluteLifetimeSeconds;
    session.device_label = device_label;
    store.save_session(session);

    return {AuthError::None, session};
}

void logout(AccountStore& store, const SessionId& id) {
    store.revoke_session(id);
}

void logout_all_devices(AccountStore& store, AccountId account, const SessionId& keep_session) {
    for (const auto& session : store.sessions_for(account)) {
        if (!keep_session.empty() && session.id == keep_session) continue;
        store.revoke_session(session.id);
    }
}

} // namespace liquidgov::platform
