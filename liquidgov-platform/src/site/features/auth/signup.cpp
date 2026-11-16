#include "site/features/auth/signup.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/html.hpp"
#include "site/ui/wrap.hpp"
#include "site/features/here/here.hpp"

namespace liquidgov::site {


bool looks_like_email(const std::string& s) {
    auto at = s.find('@');
    if (at == std::string::npos || at == 0 || at + 1 >= s.size()) return false;
    auto dot = s.find('.', at + 1);
    if (dot == std::string::npos || dot + 1 >= s.size()) return false;
    if (s.find(' ') != std::string::npos) return false;
    return s.size() <= 200;
}

// Google Identity Services' own button, wired to POST straight to
// /login/google as a plain form submit (data-ux_mode="redirect") -- no
// custom JS callback needed, GIS handles the credential POST itself.
// Renders nothing at all if no client ID is configured yet, rather than
// a button that would just fail every click.
std::string google_signin_button_html() {
    const char* cid = google_client_id_from_env();
    if (!cid[0]) return "";
    return "<div style=\"margin:.8rem 0\">"
           "<script src=\"https://accounts.google.com/gsi/client\" async defer></script>"
           "<div id=\"g_id_onload\" data-client_id=\""+html_escape(cid)+"\" "
           "data-login_uri=\"https://liquidgov.us/login/google\" data-ux_mode=\"redirect\"></div>"
           "<div class=\"g_id_signin\" data-type=\"standard\" data-shape=\"pill\" data-width=\"300\"></div>"
           "</div>";
}

// First-time Google sign-in with no prior LiquidGov account: derives a
// username from the email's local-part rather than asking for one (the
// whole point of one-click). Strips anything valid_username() wouldn't
// accept, pads if too short, then appends a numeric suffix until free.
std::string username_from_email_local_part(const std::string& email) {
    auto at = email.find('@');
    std::string local = at == std::string::npos ? email : email.substr(0, at);
    std::string base;
    for (unsigned char c : local) {
        if ((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='_'||c=='-')
            base += static_cast<char>(c);
    }
    if (base.size() > 28) base.resize(28); // leaves room for a numeric suffix under the 32 cap
    if (base.size() < 3) base = "user" + base;
    std::string candidate = base;
    int suffix = 2;
    while (g_store.username_exists(candidate)) {
        candidate = base + std::to_string(suffix);
        ++suffix;
    }
    return candidate;
}

std::string signup_page(const std::string& navuser, int unread, const std::string& err,
                        const SignupDraft& d) {
    std::string e;
    if (!err.empty()) e = "<p class=\"warn\">"+html_escape(err)+"</p>";
    return wrap("Sign up", navuser, unread,
        "<div class=\"card\"><h1>Create an account</h1>"
        "<p class=\"muted\">Username is 3–32 characters: letters, numbers, dots, underscores, hyphens. "
        "No spaces, no @. Password at least 8 characters. Phone is a signup binding "
        "(not used to log in, the number itself is not stored). Email is kept so you can "
        "reset your password if you forget it &mdash; it is not used to log in either. "
        "This is a platform account, not a citizen identity.</p>"+e+
        google_signin_button_html()+
        "<form method=\"POST\" action=\"/signup\">"
        "<label>Username</label><input name=\"username\" required minlength=\"3\" maxlength=\"32\" "
        "pattern=\"[A-Za-z0-9._-]{3,32}\" title=\"3–32 letters, numbers, dots, underscores, hyphens. No spaces.\" "
        "autocomplete=\"username\" placeholder=\"alice\" value=\""+html_escape(d.username)+"\">"
        "<label>Name</label><input name=\"display_name\" required maxlength=\"80\" "
        "autocomplete=\"name\" placeholder=\"Your name, as people know you (they can search for it)\" value=\""+html_escape(d.display_name)+"\">"
        "<label>Password</label><input type=\"password\" name=\"password\" required minlength=\"8\" maxlength=\"200\" autocomplete=\"new-password\">"
        "<label>Email</label><input name=\"email\" type=\"email\" required maxlength=\"200\" autocomplete=\"email\" "
        "placeholder=\"you@example.com\" value=\""+html_escape(d.email)+"\">"
        "<label>Phone</label><input name=\"phone\" type=\"tel\" required autocomplete=\"tel\" inputmode=\"tel\" "
        "placeholder=\"10+ digits\" value=\""+html_escape(d.phone)+"\">"
        + here_signup_fields() +
        "<p class=\"eyebrow\" style=\"margin-top:1.2rem\">Optional</p>"
        "<label>Hometown</label><input name=\"hometown\" maxlength=\"80\" value=\""+html_escape(d.hometown)+"\">"
        "<label>Where you live now</label><input name=\"living\" maxlength=\"80\" value=\""+html_escape(d.living)+"\">"
        "<label>Bio</label><textarea name=\"bio\" maxlength=\"500\">"+html_escape(d.bio)+"</textarea>"
        "<label>Interests</label><input name=\"interests\" maxlength=\"200\" placeholder=\"comma-separated\" value=\""+html_escape(d.interests)+"\">"
        "<label>Career</label><input name=\"career\" maxlength=\"80\" value=\""+html_escape(d.career)+"\">"
        "<label>School</label><input name=\"school\" maxlength=\"80\" value=\""+html_escape(d.school)+"\">"
        "<button>Continue</button></form></div>");
}

}  // namespace liquidgov::site
