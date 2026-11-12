#include "site/features/pages/pages_routes.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/http.hpp"
#include "site/features/map/map_page.hpp"
#include "site/features/pages/genesis.hpp"
#include "site/features/pages/home.hpp"
#include "site/features/pages/static_pages.hpp"
#include "site/ui/wrap.hpp"

namespace liquidgov::site {

std::optional<std::string> pages_routes(Route& r) {
    LG_ROUTE_LOCALS(r)
    // Real recorded narration (Helios) for the Chapter 1 Listen control --
    // replaces the earlier browser speechSynthesis approach entirely.
    // Narration for every Laws chapter (Helios, recorded 2026-09-13; Brandon 2026-11-09 "i want the audio"):
    // /assets/chapter<N>.mp3 for N = 1..number of chapters. Chapter 1 used to be the only route (a whole-file copy
    // held in memory for good); all 14 that way would pin ~18 MB on the 512 MB droplet, so each request reads the
    // file from disk. Byte ranges are honoured (iPhone Safari asks for them before playing audio). Cached a day,
    // not a year: a chapter whose text changes gets re-recorded at the same address.
    if(method=="GET"&&path.rfind("/assets/chapter",0)==0&&path.size()>19&&path.compare(path.size()-4,4,".mp3")==0) {
        const std::string num = path.substr(15, path.size()-15-4);
        size_t n = 0;
        if (num.empty() || num.size() > 2 || num[0] == '0' || !std::all_of(num.begin(), num.end(), ::isdigit))
            return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        n = static_cast<size_t>(std::stoi(num));
        if (n < 1 || n > law_chapters().size()) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        // the name is built, so lgp1_consistency (literal asset names) cannot see it; narration_files_check.py checks
        // that assets/chapter1..N.mp3 exist for every Laws chapter instead
        const std::string file = "assets/chapter" + num + ".mp3";
        std::ifstream in(file, std::ios::binary);
        if (!in) {
            lglog::emit("narration", "missing assets/chapter" + num + ".mp3");
            return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        }
        std::ostringstream ss; ss << in.rdbuf();
        const std::string all = ss.str();
        const std::string cache = "Cache-Control: public, max-age=86400\r\nAccept-Ranges: bytes\r\n";
        const std::string range = header_value(req, "Range");
        unsigned long long a = 0, b = 0;
        if (!range.empty() && !all.empty() && std::sscanf(range.c_str(), "bytes=%llu-%llu", &a, &b) >= 1) {
            if (range.find('-') == range.size() - 1 || b >= all.size()) b = all.size() - 1;
            if (a > b || a >= all.size())
                return http_raw("416 Range Not Satisfiable", "text/plain; charset=utf-8", "",
                    "Content-Range: bytes */" + std::to_string(all.size()) + "\r\n");
            return http_raw("206 Partial Content", "audio/mpeg", all.substr(a, b - a + 1), cache +
                "Content-Range: bytes " + std::to_string(a) + "-" + std::to_string(b) + "/" + std::to_string(all.size()) + "\r\n");
        }
        return http_raw("200 OK", "audio/mpeg", all, cache);
    }

    // The old signed-in landing page (counts, your rooms), still one tap away from the map (2026-11-07).
    if(method=="GET"&&path=="/home") {
        if(!acc) return need_login();
        return http_ok(wrap("Home", user, unread, home_dashboard(user)+
                "<div class=\"card\"><h2>What this is</h2>"
                "<p class=\"muted\">Genesis, not a government. Three rules, and the rest is up to you.</p>"
                "<p class=\"muted\"><a href=\"/laws\">Read the disclosure</a></p></div>"));
    }
    if(method=="GET"&&path=="/") {
        if(acc) {
            // A returning member's "landing page" is here, not the
            // logged-out Genesis page below -- so that's where the opted-in
            // replay actually has to happen for the Settings toggle to mean
            // anything for someone who can reach Settings at all. Bypasses
            // pg() (which never shows the intro) specifically for this;
            // show_intro is otherwise only ever true for the Genesis path.
            const bool replay_intro = g_store.prefs_of(user).intro_every_visit;
            return http_ok(wrap("MAINstream", user, unread, map_page_body(user, ""), replay_intro));
        }
        std::string extra =
            "<div class=\"card\"><p class=\"row\"><a class=\"btn\" href=\"/signup\">Create an account</a> "
            "<a class=\"btn ghost\" href=\"/login\">Log in</a></p></div>";
        return http_ok(wrap("Home", user, unread, genesis_body()+extra, true));
    }
    if(method=="GET"&&path=="/laws") return pg("Laws", laws_body());
    if(method=="GET"&&path.rfind("/laws/",0)==0) {
        size_t n = 0;
        try { n = std::stoull(path.substr(6)); } catch (...) {}
        const auto& chapters = law_chapters();
        if (n < 1 || n > chapters.size())
            return http_code("404 Not Found", pg("Not found", "<div class=\"card\"><p>No such chapter.</p></div>"));
        return pg("Laws · " + chapters[n-1].title, laws_chapter_body(n));
    }
    if(method=="GET"&&path=="/control") return pg("Who holds control", control_body());
    if(method=="GET"&&path=="/privacy") return pg("Privacy", privacy_body());
    if(method=="GET"&&path=="/terms") return pg("How this association works", terms_body());
    if(method=="GET"&&path=="/about") return http_redir("/control");
    if(method=="GET"&&path=="/profile") return http_redir(user.empty() ? login_url("/me") : "/me");
    return std::nullopt;
}

}  // namespace liquidgov::site
