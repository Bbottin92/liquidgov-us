#include "site/features/auth/auth_routes.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/html.hpp"
#include "site/core/http.hpp"
#include "site/core/net.hpp"
#include "site/core/text.hpp"
#include "site/features/auth/signup.hpp"
#include "site/features/feed/composer.hpp"
#include "site/ui/favicon_cookies.hpp"

namespace liquidgov::site {

std::optional<std::string> auth_routes(Route& r) {
    LG_ROUTE_LOCALS(r)
    if(method=="GET"&&path=="/signup") return http_ok(signup_page(user, unread, "", SignupDraft{}));
    if(method=="GET"&&path=="/login") {
        // Deliberately one generic message for bad-username and
        // bad-password alike (and phone-not-verified folded in too) --
        // splitting them would let a caller enumerate which usernames
        // exist. Rate-limit is the one case safe to name specifically:
        // it doesn't depend on whether the account exists.
        const std::string err = flash_error(qs);
        const std::string next = safe_next_path(qs["next"]);
        return pg("Log in", "<div class=\"card\"><h1>Log in</h1>"
            "<p class=\"muted\">Log in with your username and password.</p>"+err+
            google_signin_button_html()+
            "<form method=\"POST\" action=\"/login\"><label>Username</label>"
            "<input name=\"username\" required autocomplete=\"username\" value=\""+html_escape(qs["username"])+"\">"
            "<label>Password</label><input type=\"password\" name=\"password\" required autocomplete=\"current-password\">"
            +(next.empty()?std::string():"<input type=\"hidden\" name=\"next\" value=\""+html_escape(next)+"\">")+
            "<button>Continue</button></form>"
            "<p class=\"muted\" style=\"margin-top:.7rem\">"
            "<a href=\"/forgot\">Forgot password?</a> &middot; "
            "New here? <a href=\"/signup\">Create an account</a>.</p></div>");
    }
    if(method=="GET"&&path=="/forgot") {
        return pg("Forgot password", "<div class=\"card\"><h1>Forgot your password?</h1>"
            "<p class=\"muted\">Used Google? <a href=\"/login\">Sign in with Google</a> instead "
            "&mdash; no reset needed.</p>"
            +flash_error(qs)+
            "<form method=\"POST\" action=\"/forgot\">"
            "<label>Email</label><input type=\"email\" name=\"email\" required autocomplete=\"email\">"
            "<button>Send reset link</button></form>"
            "<p class=\"muted\" style=\"margin-top:.7rem\"><a href=\"/login\">Back to log in</a></p></div>");
    }
    // GET renders the form regardless of whether t looks valid -- POST
    // does the actual checking (see its own comment), so this route
    // can't be used to probe token validity separately from consuming
    // one.
    if(method=="GET"&&path=="/reset") {
        return pg("Reset password", "<div class=\"card\"><h1>Set a new password</h1>"
            +flash_error(qs)+
            "<form method=\"POST\" action=\"/reset\">"
            "<input type=\"hidden\" name=\"t\" value=\""+html_escape(qs["t"])+"\">"
            "<label>New password</label><input type=\"password\" name=\"password\" required "
            "minlength=\"8\" autocomplete=\"new-password\">"
            "<button>Set password</button></form></div>");
    }
    if(method=="GET"&&path=="/logout") {
        logout(g_store, sid);
        return http_redir("/", clear_cookie());
    }

    if(method=="POST"&&path=="/signup") {
        auto f=parse_form(body);
        SignupDraft d;
        d.username=trim_ws(f["username"]);
        d.phone=trim_ws(f["phone"]);
        d.email=trim_ws(f["email"]);
        d.display_name=trim_ws(f["display_name"]);
        d.hometown=trim_ws(f["hometown"]);
        d.living=trim_ws(f["living"]);
        d.bio=trim_ws(f["bio"]);
        d.interests=trim_ws(f["interests"]);
        d.career=trim_ws(f["career"]);
        d.school=trim_ws(f["school"]);
        auto pass=f["password"];
        auto fail=[&](const std::string& err){ return http_ok(signup_page(user, unread, err, d)); };
        if (!valid_username(d.username) || d.username.size() < 3) {
            std::string err = d.username.empty()
                ? "Need a username: 3–32 letters/numbers (dots, underscores, hyphens ok). No spaces, no @."
                : d.username.size() < 3
                    ? "Username needs at least three characters."
                    : ("“"+d.username+"” isn’t a valid username. Letters, numbers, dots, underscores, hyphens. No spaces.");
            return fail(err);
        }
        if (d.display_name.empty() || d.display_name.size() > 80)
            return fail("Need a name (up to 80 characters).");
        if (pass.size() < 8)
            return fail("Password needs at least eight characters.");
        if (!looks_like_email(d.email))
            return fail("Need a real email address, kept only so you can reset your password if you forget it.");
        if (count_digits(d.phone) < 10)
            return fail("Need a real phone number (at least ten digits). It is a signup binding, not stored as the number itself, and not used to log in.");
        if (pass.empty() || d.phone.empty())
            return fail("Need username, password, email, phone, and a name.");
        // Dots on profile pages: a required choice, nothing pre-selected (Brandon 2026-11-12)
        const std::string here = f["here"];
        if (here != "1" && here != "0")
            return fail("Choose whether to show as a dot on profile pages (Yes or No). You can change it later in Settings.");
        if (g_store.phone_quarantined(fingerprint_phone(d.phone)))
            return fail("Could not create that account.");
        auto r=create_account(g_store,g_breach,d.username,pass,d.phone);
        if(r.error==AuthError::None) {
            if (auto created = g_store.find_by_username(d.username)) {
                g_store.set_phone_fp(created->id, fingerprint_phone(d.phone));
                if (!d.bio.empty()) g_store.set_blurb(created->id, d.bio.substr(0, 500));
                g_store.set_email(d.username, lowercase_ascii(d.email));
                Profile p;
                p.display_name = d.display_name.substr(0, 80);
                p.email_fp = fingerprint_phone(d.email);
                p.hometown = d.hometown.substr(0, 80);
                p.living = d.living.substr(0, 80);
                p.interests = d.interests.substr(0, 200);
                p.career = d.career.substr(0, 80);
                p.school = d.school.substr(0, 80);
                g_store.set_profile(d.username, p);
                g_store.set_map_prefs(d.username, MapPrefs{}); // today's defaults written down: no "now show" note later
                { HintPrefs hp; hp.on = true; g_store.set_hints(d.username, hp); }   // a new account starts with hints on
                g_store.set_here_choice(d.username, here == "1");
            }
            r=login(g_store,g_limiter,d.username,pass,device_label_from(req,ip),ip);
        }
        if(r.error==AuthError::UsernameTaken)
            return fail("That username is taken. Pick another.");
        if(r.error==AuthError::PasswordTooShort)
            return fail("Password needs at least eight characters.");
        if(r.error!=AuthError::None||!r.session)
            return fail("Could not create that account.");
        return http_redir("/", set_cookie(r.session->id));
    }
    if(method=="POST"&&path=="/login") {
        auto f=parse_form(body);
        auto uname = trim_ws(f["username"]);
        auto r=login(g_store,g_limiter,uname,f["password"],device_label_from(req,ip),ip);
        if(r.error!=AuthError::None||!r.session) {
            const char* code = (r.error==AuthError::RateLimited) ? "ratelimit" : "login";
            // No username in the URL: it would sit in server logs and browser history (security gate,
            // Brandon 2026-11-13). The form comes back empty; the person retypes it.
            std::string loc = "/login?err=" + std::string(code);
            auto next = safe_next_path(trim_ws(f["next"]));
            if (!next.empty()) loc += "&next=" + url_encode(next);
            return http_redir(loc);
        }
        auto dest = safe_next_path(trim_ws(f["next"]));
        if (dest.empty()) dest = "/feed";
        return http_redir(dest, set_cookie(r.session->id));
    }
    // Always the same response whether or not the address is on an
    // account -- checked deliberately before ever looking anything up,
    // not just by coincidence of the code path. Never let mail-delivery
    // status (or even the timing of a real lookup+send vs. a no-op)
    // leak back to the caller; that's exactly how account enumeration
    // via a password-reset form usually happens.
    if(method=="POST"&&path=="/forgot") {
        auto f = parse_form(body);
        std::string email = lowercase_ascii(trim_ws(f["email"]));
        if (looks_like_email(email)) {
            if (auto username = g_store.username_by_email(email)) {
                std::string token = generate_session_id(); // same crypto-random 32-byte-hex primitive as a session id
                g_store.set_reset_token(*username, sha256_hex_of(token), now() + 3600);
                send_password_reset_email(email, "https://liquidgov.us/reset?t=" + token);
            }
        }
        return pg("Forgot password", "<div class=\"card\"><h1>Check your email</h1>"
            "<p class=\"muted\">If that address is on an account, a link is on its way. "
            "It works once, for the next hour.</p>"
            "<p class=\"muted\"><a href=\"/login\">Back to log in</a></p></div>");
    }
    if(method=="POST"&&path=="/reset") {
        auto f = parse_form(body);
        std::string t = trim_ws(f["t"]);
        std::string pass = f["password"];
        if (t.empty()) return http_redir("/reset?err=reset");
        if (pass.size() < 8) return http_redir("/reset?t="+url_encode(t)+"&err=short");
        if (g_breach.is_breached(pass)) return http_redir("/reset?t="+url_encode(t)+"&err=password");
        // consume_reset_token erases it either way (found-and-expired or
        // found-and-valid) -- a token is good for exactly one attempt,
        // successful or not, same as any other one-time credential.
        auto username = g_store.consume_reset_token(sha256_hex_of(t));
        if (!username) return http_redir("/reset?err=reset");
        auto acc2 = g_store.find_by_username(*username);
        if (!acc2) return http_redir("/reset?err=reset");
        g_store.set_password_hash(acc2->id, hash_password(pass));
        logout_all_devices(g_store, acc2->id, "");
        auto session = create_session(g_store, acc2->id, device_label_from(req, ip));
        return http_redir("/", set_cookie(session.id));
    }
    // Google Identity Services posts here directly (data-ux_mode="redirect"
    // in google_signin_button_html()) -- no custom JS callback, just a
    // plain form POST carrying the ID token as "credential". Verified
    // against Google's own tokeninfo endpoint (a plain HTTPS GET, no JWT/
    // JWKS library needed) rather than trusting anything the browser says
    // about itself.
    if(method=="POST"&&path=="/login/google") {
        const char* client_id = google_client_id_from_env();
        if (!client_id[0]) return http_redir("/login?err=google");
        auto f = parse_form(body);
        std::string credential = trim_ws(f["credential"]);
        if (credential.empty()) return http_redir("/login?err=google");

        int curl_exit = 0;
        std::string resp = curl_get(
            "https://oauth2.googleapis.com/tokeninfo?id_token=" + url_encode(credential), 8, &curl_exit);
        // nothing came back (no network, timeout): say Google could not be reached, not that the sign-in was bad
        if (curl_exit != 0 && resp.empty()) return http_redir("/login?err=google-down");
        if (resp.empty() || resp[0] != '{') return http_redir("/login?err=google");

        std::string aud = oembed_json_string_field(resp, "aud");
        std::string iss = oembed_json_string_field(resp, "iss");
        std::string sub = oembed_json_string_field(resp, "sub");
        std::string email = lowercase_ascii(oembed_json_string_field(resp, "email"));
        std::string email_verified = oembed_json_string_field(resp, "email_verified");

        if (aud != client_id || sub.empty()) return http_redir("/login?err=google");
        if (iss != "accounts.google.com" && iss != "https://accounts.google.com")
            return http_redir("/login?err=google");
        if (email_verified != "true") return http_redir("/login?err=google");

        const std::string device_label = device_label_from(req, ip);

        // gid hit: this Google identity is already linked to an account.
        if (auto existing = g_store.username_by_google_sub(sub)) {
            auto acc2 = g_store.find_by_username(*existing);
            if (!acc2) return http_redir("/login?err=google");
            auto session = create_session(g_store, acc2->id, device_label);
            return http_redir("/", set_cookie(session.id));
        }
        // gid miss, ml hit: same email as an existing password account --
        // link it (never auto-link on anything looser than a verified
        // email match).
        if (!email.empty()) {
            if (auto linked = g_store.username_by_email(email)) {
                auto acc2 = g_store.find_by_username(*linked);
                if (acc2) {
                    g_store.set_google_id(*linked, sub);
                    auto session = create_session(g_store, acc2->id, device_label);
                    return http_redir("/", set_cookie(session.id));
                }
            }
        }
        // Neither: brand new account, no phone required on this path
        // (bypasses create_account()'s phone-required contract on
        // purpose -- see username_from_email_local_part's own comment --
        // by calling the raw AccountStore::create directly, same as
        // every other FileStore-only account field this file sets right
        // after signup).
        PlatformAccount account;
        account.username = username_from_email_local_part(email.empty() ? sub : email);
        account.password_hash = dummy_hash();
        account.phone_verified = false;
        account.created_at = now();
        AccountId new_id = g_store.create(account);
        if (!email.empty()) g_store.set_email(account.username, email);
        g_store.set_google_id(account.username, sub);
        g_store.set_map_prefs(account.username, MapPrefs{}); // today's defaults written down: no "now show" note later
        auto session = create_session(g_store, new_id, device_label);
        return http_redir("/", set_cookie(session.id));
    }
    return std::nullopt;
}

}  // namespace liquidgov::site
