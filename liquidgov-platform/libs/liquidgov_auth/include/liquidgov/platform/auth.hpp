#pragma once

#include <optional>
#include <string>
#include <vector>

#include "liquidgov/platform/account.hpp"
#include "liquidgov/platform/password.hpp"
#include "liquidgov/platform/session.hpp"

namespace liquidgov::platform {

inline constexpr std::int64_t kSessionAbsoluteLifetimeSeconds = 30 * 24 * 60 * 60;

enum class AuthError {
    None,
    UsernameTaken,
    PasswordBreached,
    InvalidCredentials,
    RateLimited,
    PhoneNotVerified,
};

struct AuthResult {
    AuthError error = AuthError::None;
    std::optional<Session> session;
};

class AccountStore {
public:
    virtual ~AccountStore() = default;
    virtual bool username_exists(const std::string& username) const = 0;
    virtual std::optional<PlatformAccount> find_by_username(const std::string& username) const = 0;
    virtual AccountId create(const PlatformAccount& account) = 0;
    virtual void save_session(const Session& session) = 0;
    virtual std::vector<Session> sessions_for(AccountId account) const = 0;
    virtual void revoke_session(const SessionId& id) = 0;
};

class RateLimiter {
public:
    virtual ~RateLimiter() = default;
    virtual bool allow(const std::string& identifier_key, const std::string& ip_key) = 0;
    virtual void record_attempt(const std::string& identifier_key, const std::string& ip_key) = 0;
};

AuthResult create_account(AccountStore& store, const BreachChecker& breach_checker,
                           const std::string& username, const std::string& password,
                           const std::string& verified_phone_e164);

AuthResult login(AccountStore& store, RateLimiter& limiter,
                  const std::string& username, const std::string& password,
                  const std::string& device_label, const std::string& ip);

void logout(AccountStore& store, const SessionId& id);
void logout_all_devices(AccountStore& store, AccountId account, const SessionId& keep_session);

} // namespace liquidgov::platform
