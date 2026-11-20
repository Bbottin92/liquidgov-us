#include "site/features/profile/portrait_history.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/html.hpp"
#include "site/core/http.hpp"

namespace liquidgov::site {

namespace {

// "3" -> 3; anything that is not 1 to 3 digits is not an index.
std::optional<std::size_t> parse_index(const std::string& s) {
    if (s.empty() || s.size() > 3) return std::nullopt;
    std::size_t n = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return std::nullopt;
        n = n * 10 + static_cast<std::size_t>(c - '0');
    }
    return n;
}

std::string not_found() { return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found"); }

// The PNG of history entry `index` of this account (oldest = 0), or a 404.
std::string serve_history_png(AccountId id, std::size_t index) {
    auto list = g_store.portrait_history(id);
    if (index >= list.size()) return not_found();
    auto bytes = g_blobs.get(list[index].blob_id);
    if (!bytes) return not_found();
    std::string png(bytes->begin(), bytes->end());
    return http_raw("200 OK", "image/png", png,
        "Cache-Control: private, max-age=300\r\nContent-Disposition: inline; filename=\"portrait.png\"\r\n");
}

}  // namespace

std::optional<std::string> portrait_history_routes(Route& r) {
    LG_ROUTE_LOCALS(r)
    // The owner's own list: only ever the signed-in person's pictures, so there is no username in the URL to guess.
    if (method == "GET" && path.rfind("/me/portrait/old/", 0) == 0) {
        if (!acc) return need_login();
        auto idx = parse_index(path.substr(17));
        if (!idx) return not_found();
        return serve_history_png(acc->id, *idx);
    }
    // Someone else's list: only when its owner chose to show it, only to people who can see that profile.
    if (method == "GET" && path.rfind("/portrait-history/", 0) == 0) {
        const std::string rest = path.substr(18);
        const auto slash = rest.find('/');
        if (slash == std::string::npos) return not_found();
        const std::string who = rest.substr(0, slash);
        auto idx = parse_index(rest.substr(slash + 1));
        if (!idx || !valid_username(who)) return not_found();
        auto found = g_store.find_by_username(who);
        if (!found) return not_found();
        if (acc && g_store.is_blocked(user, found->username)) return not_found();
        const bool is_owner = acc && acc->username == found->username;
        const bool is_friend = acc && !is_owner && g_store.are_friends(user, found->username);
        if (!is_owner) {
            if (!g_store.portrait_history_public(found->id)) return not_found();
            if (!visible_to(g_store.prefs_of(found->username).profile_visibility, is_owner, acc.has_value(), is_friend))
                return not_found();
        }
        return serve_history_png(found->id, *idx);
    }
    if (method == "POST" && path == "/me/portrait/old/delete") {
        if (!acc) return need_login();
        auto f = parse_form(body);
        auto idx = parse_index(f["n"]);
        if (idx) {
            if (auto blob = g_store.drop_portrait(acc->id, *idx)) g_blobs.release(*blob);
        }
        return http_redir("/me/portrait");
    }
    if (method == "POST" && path == "/me/portrait/old/share") {
        if (!acc) return need_login();
        auto f = parse_form(body);
        g_store.set_portrait_history_public(acc->id, f["on"] == "1");
        return http_redir("/me/portrait");
    }
    return std::nullopt;
}

SaveAvatarResult save_portrait_keeping_history(AccountId owner, const std::vector<std::uint8_t>& bytes) {
    // save_avatar releases the portrait it replaces. Take one more reference to that picture first (put of the same
    // bytes returns the same blob and adds one), so it survives; if the save fails, give the reference back.
    auto old = g_store.get(owner);
    bool held = false;
    if (old) {
        if (auto old_bytes = g_blobs.get(old->blob_id)) {
            g_blobs.put(*old_bytes, "image/png");
            held = true;
        }
    }
    auto res = save_avatar(g_blobs, g_store, g_scanner, owner, bytes);
    if (!held) return res;
    if (res.error != SaveAvatarError::None || !res.avatar || res.avatar->blob_id == old->blob_id) {
        g_blobs.release(old->blob_id);   // nothing replaced it, or the same picture was saved again
        return res;
    }
    for (const auto& dropped : g_store.keep_portrait(owner, old->blob_id, old->created_at))
        g_blobs.release(dropped);
    return res;
}

void remove_portrait_keeping_history(AccountId owner) {
    // clear_avatar releases the portrait. Take one more reference first (put of the same bytes returns the same blob
    // and adds one), clear, then hand that reference to the history, exactly as save_portrait_keeping_history does.
    auto old = g_store.get(owner);
    if (!old) return;
    bool held = false;
    if (auto old_bytes = g_blobs.get(old->blob_id)) {
        g_blobs.put(*old_bytes, "image/png");
        held = true;
    }
    clear_avatar(g_blobs, g_store, owner);
    if (!held) return;
    for (const auto& dropped : g_store.keep_portrait(owner, old->blob_id, old->created_at))
        g_blobs.release(dropped);
}

std::string portrait_history_owner_html(const std::string& username) {
    auto me = g_store.find_by_username(username);
    if (!me) return "";
    auto list = g_store.portrait_history(me->id);
    auto cur = g_store.get(me->id);
    const bool have_current = cur && g_blobs.meta(cur->blob_id).has_value();
    if (list.empty() && !have_current) return "";
    std::string h =
        "<div class=\"card\" id=\"oldportraits\"><h2>Earlier portraits</h2>"
        "<p class=\"muted\">Pick one to load it into the canvas above, change it, then Save as self-portrait. "
        "Saving moves the portrait you have now in here; the last " +
        std::to_string(FileStore::kPortraitHistoryKeep) + " are kept.</p>"
        "<div style=\"display:flex;flex-wrap:wrap;gap:.6rem;align-items:flex-start\">";
    auto thumb = [&](const std::string& src, const std::string& label) {
        return "<button type=\"button\" class=\"ghost oldportrait\" data-src=\"" + html_escape(src) + "\" "
               "title=\"Load into the canvas\" style=\"padding:.25rem;display:flex;flex-direction:column;align-items:center;gap:.2rem\">"
               "<img src=\"" + html_escape(src) + "\" alt=\"\" width=\"72\" height=\"72\" style=\"object-fit:cover;border-radius:var(--rsm)\">"
               "<span class=\"muted\" style=\"font-size:.75rem\">" + html_escape(label) + "</span></button>";
    };
    if (have_current)
        h += "<div>" + thumb("/portrait/" + username + "?v=" + cur->blob_id, "Current") + "</div>";
    for (std::size_t k = list.size(); k-- > 0;) {   // newest first
        const std::string src = "/me/portrait/old/" + std::to_string(k) + "?v=" + list[k].blob_id;
        h += "<div style=\"display:flex;flex-direction:column;align-items:center;gap:.2rem\">" +
             thumb(src, fmt_time(list[k].created_at).substr(0, 10)) +
             "<form class=\"inline\" method=\"POST\" action=\"/me/portrait/old/delete\" "
             "onsubmit=\"return confirm('Delete this earlier portrait for good?');\">"
             "<input type=\"hidden\" name=\"n\" value=\"" + std::to_string(k) + "\">"
             "<button class=\"ghost\" type=\"submit\" style=\"font-size:.75rem\">Delete</button></form></div>";
    }
    h += "</div>";
    if (!list.empty()) {
        h += "<form method=\"POST\" action=\"/me/portrait/old/share\" style=\"margin-top:.8rem\">"
             "<label><input type=\"checkbox\" name=\"on\" value=\"1\"" +
             std::string(g_store.portrait_history_public(me->id) ? " checked" : "") +
             "> Show these on my profile (otherwise only you can see them)</label>"
             "<p class=\"row\" style=\"margin:.4rem 0 0\"><button type=\"submit\">Save this choice</button></p></form>";
    }
    h += "</div>";
    return h;
}

std::string portrait_history_public_html(const std::string& username) {
    auto who = g_store.find_by_username(username);
    if (!who || !g_store.portrait_history_public(who->id)) return "";
    auto list = g_store.portrait_history(who->id);
    if (list.empty()) return "";
    std::string h = "<div class=\"card\"><h2>Earlier portraits</h2>"
                    "<div style=\"display:flex;flex-wrap:wrap;gap:.5rem\">";
    for (std::size_t k = list.size(); k-- > 0;) {
        h += "<img src=\"/portrait-history/" + html_escape(username) + "/" + std::to_string(k) + "?v=" +
             html_escape(list[k].blob_id) + "\" alt=\"\" width=\"72\" height=\"72\" "
             "style=\"object-fit:cover;border-radius:var(--rsm)\">";
    }
    h += "</div></div>";
    return h;
}

}  // namespace liquidgov::site
