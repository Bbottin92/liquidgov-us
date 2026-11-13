#include "site/features/styles/styles_routes.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/html.hpp"
#include "site/core/http.hpp"
#include "site/features/profile/profile_css.hpp"
#include "site/features/styles/styles.hpp"

namespace liquidgov::site {

namespace {

std::uint64_t form_id(const std::map<std::string, std::string>& f) {
    auto it = f.find("id");
    if (it == f.end() || it->second.empty() || it->second.size() > 19) return 0;
    for (char c : it->second) if (c < '0' || c > '9') return 0;
    return std::stoull(it->second);
}

}  // namespace

std::optional<std::string> styles_routes(Route& r) {
    LG_ROUTE_LOCALS(r)
    if (method == "GET" && path == "/styles") {
        std::size_t page = 1;
        auto p = qs.find("page");
        if (p != qs.end() && !p->second.empty() && p->second.size() < 6 &&
            std::all_of(p->second.begin(), p->second.end(), [](char c) { return c >= '0' && c <= '9'; }))
            page = static_cast<std::size_t>(std::stoul(p->second));
        return pg("Profile styles", styles_page(acc ? user : "", qs["sort"], page, qs));
    }
    // /styles/<id>/preview: the document a sandboxed iframe on /styles shows. Its own CSP repeats the sandbox (no
    // scripts, opaque origin) for anyone who opens it directly, and lets nothing load but the site's stylesheet.
    if (method == "GET" && path.rfind("/styles/", 0) == 0 && path.size() > 16 &&
        path.compare(path.size() - 8, 8, "/preview") == 0) {
        const std::string num = path.substr(8, path.size() - 16);
        std::optional<ProfileStyle> s;
        if (!num.empty() && num.size() < 20 && std::all_of(num.begin(), num.end(), [](char c) { return c >= '0' && c <= '9'; }))
            s = g_store.style(std::stoull(num));
        if (!s || !style_visible_to(*s, acc ? user : ""))
            return http_code("404 Not Found", "<!doctype html><meta charset=\"utf-8\"><p>No such style.</p>");
        return http_raw("200 OK", "text/html; charset=utf-8", style_preview_document(*s),
                        "Content-Security-Policy: sandbox; default-src 'none'; style-src 'self' 'unsafe-inline'; "
                        "frame-ancestors 'self'\r\nReferrer-Policy: no-referrer\r\nCache-Control: no-store\r\n");
    }
    if (method == "POST" && path == "/styles/publish") {
        if (!acc) return need_login();
        auto f = parse_form(body);
        const std::string name = tidy_style_name(f["name"]);
        if (name.empty() || utf8_chars(name) > kStyleNameMaxChars) return http_redir("/me?err=style&reason=name#profile-css");
        const std::string css = g_store.author_css(acc->id);
        if (css.empty()) return http_redir("/me?err=style&reason=empty#profile-css");
        // checked again now (Brandon: "it must pass sanitize_author_css at publish time"): CSS saved under older
        // rules can fail today's
        auto check = sanitize_author_css(css);
        if (!check.ok) return http_redir("/me?err=style&reason=" + check.code + "#profile-css");
        std::size_t mine = 0;
        for (const auto& s : g_store.styles()) if (s.author == user) ++mine;
        if (mine >= kStylesPerAuthor) return http_redir("/me?err=style&reason=limit#profile-css");
        auto id = g_store.publish_style(user, name, css);
        if (!id) return http_redir("/me?err=style&reason=name#profile-css");
        return http_redir("/styles?ok=published#style-" + std::to_string(*id));
    }
    if (method == "POST" && path == "/styles/use") {
        if (!acc) return need_login();
        auto s = g_store.style(form_id(parse_form(body)));
        if (!s || !style_visible_to(*s, user)) return http_redir("/styles?err=gone");
        // a copy, never a link: the author's later changes (or removal) never change it
        g_store.set_author_css(acc->id, s->css);
        g_store.use_style(s->id, user);
        return http_redir("/me?ok=style-used#profile-css");
    }
    if (method == "POST" && path == "/styles/report") {
        if (!acc) return need_login();
        auto f = parse_form(body);
        auto s = g_store.style(form_id(f));
        if (!s || !style_visible_to(*s, user)) return http_redir("/styles?err=gone");
        if (s->author != user) g_store.report_style(s->id, user, utf8_prefix(trim_ws(f["reason"]), kStyleReportMaxChars));
        return http_redir("/styles?ok=reported#style-" + std::to_string(s->id));
    }
    if (method == "POST" && path == "/styles/remove") {
        if (!acc) return need_login();
        auto s = g_store.style(form_id(parse_form(body)));
        if (!s) return http_redir("/styles?err=gone");
        if (s->author != user) return http_redir("/styles#style-" + std::to_string(s->id));
        g_store.remove_style(s->id);
        return http_redir("/styles?ok=removed");
    }
    return std::nullopt;
}

}  // namespace liquidgov::site
