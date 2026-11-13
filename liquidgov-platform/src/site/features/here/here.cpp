#include "site/features/here/here.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/html.hpp"
#include "site/core/http.hpp"
#include "site/core/state.hpp"
#include "site/core/text.hpp"
#include "site/ui/assets.hpp"

namespace liquidgov::site {

// Dots on profile pages (Brandon 2026-11-12: "the normal pages with little dots for people that you would see moving
// around"). One room per profile page. A position is a fraction of the page (x of its width, y of its height, 0..1),
// so a phone and a desktop put a dot in roughly the same place. Rooms live in memory only: leaving removes your dot
// and nothing about it is ever stored. Who may be seen: people who chose Yes (a required choice, the "here" record),
// never across a block in either direction.

namespace {

struct HereDot {
    std::string username;
    double x = 0.5, y = 0.1;
};

// Function-local statics (not namespace-scope globals): MODULES.md's rule against dynamic initializers outside the
// shared-state modules.
std::mutex& here_mu() { static std::mutex m; return m; }
std::map<std::string, std::map<int, HereDot>>& here_rooms() { static std::map<std::string, std::map<int, HereDot>> r; return r; }

std::optional<double> json_number(const std::string& s, const char* key) {
    auto p = s.find(key);
    if (p == std::string::npos) return std::nullopt;
    p = s.find(':', p);
    if (p == std::string::npos) return std::nullopt;
    try {
        double v = std::stod(s.substr(p + 1));
        if (!std::isfinite(v)) return std::nullopt;
        return v;
    } catch (...) {
        return std::nullopt;
    }
}

double clamp01(double v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

// A start point that differs per person, so two newcomers don't sit on top of each other.
double start_x(const std::string& u) {
    unsigned h = 2166136261u;
    for (unsigned char c : u) h = (h ^ c) * 16777619u;
    return 0.1 + (h % 800) / 1000.0;
}

std::string query_param(const std::string& rawpath, const std::string& key) {
    auto q = rawpath.find('?');
    if (q == std::string::npos) return "";
    std::size_t i = q + 1;
    while (i <= rawpath.size()) {
        auto amp = rawpath.find('&', i);
        std::string kv = rawpath.substr(i, amp == std::string::npos ? std::string::npos : amp - i);
        auto eq = kv.find('=');
        if (eq != std::string::npos && kv.substr(0, eq) == key) return url_decode(kv.substr(eq + 1));
        if (amp == std::string::npos) break;
        i = amp + 1;
    }
    return "";
}

void run_here_session(int fd, std::string me, std::string room) {
    timeval tv{}; tv.tv_sec = 0; tv.tv_usec = 200000;  // a snapshot about five times a second
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    int moves = 0;
    auto window = std::chrono::steady_clock::now();
    for (;;) {
        auto result = ws_read_frame(fd);
        if (result.status == WsReadStatus::Closed) break;
        if (result.status == WsReadStatus::Ok) {
            if (result.frame.opcode == WsOpcode::Close) break;
            const auto now = std::chrono::steady_clock::now();
            if (now - window > std::chrono::seconds(1)) { window = now; moves = 0; }
            // at most 20 moves a second, small frames only: anything else is dropped, never echoed
            if (result.frame.opcode == WsOpcode::Text && result.frame.payload.size() <= 200 && ++moves <= 20) {
                auto nx = json_number(result.frame.payload, "\"x\"");
                auto ny = json_number(result.frame.payload, "\"y\"");
                if (nx && ny) {
                    std::lock_guard<std::mutex> lock(here_mu());
                    auto& r = here_rooms()[room];
                    auto it = r.find(fd);
                    if (it != r.end()) { it->second.x = clamp01(*nx); it->second.y = clamp01(*ny); }
                }
            }
        }
        // copy the room, then check blocks under the store lock (never both locks at once)
        std::vector<HereDot> dots;
        {
            std::lock_guard<std::mutex> lock(here_mu());
            auto rit = here_rooms().find(room);
            if (rit == here_rooms().end() || !rit->second.count(fd)) break;
            for (const auto& [ofd, d] : rit->second) dots.push_back(d);
        }
        bool still_ok = true;
        {
            std::lock_guard<std::mutex> lock(g_mu);
            // a choice changed to No, a new block with the owner, or a profile made private ends the visit
            still_ok = here_may_join(me, room);
            dots.erase(std::remove_if(dots.begin(), dots.end(), [&](const HereDot& d) {
                return d.username != me && (g_store.is_blocked(me, d.username) || g_store.is_blocked(d.username, me));
            }), dots.end());
        }
        if (!still_ok) break;
        std::ostringstream os;
        os << "{\"people\":[";
        bool first = true;
        for (const auto& d : dots) {
            os << (first ? "" : ",") << "{\"u\":\"" << json_escape(d.username) << "\",\"x\":" << d.x << ",\"y\":" << d.y << "}";
            first = false;
        }
        os << "]}";
        if (!ws_write_frame(fd, WsOpcode::Text, os.str())) break;
    }
    {
        std::lock_guard<std::mutex> lock(here_mu());
        auto rit = here_rooms().find(room);
        if (rit != here_rooms().end()) {
            rit->second.erase(fd);
            if (rit->second.empty()) here_rooms().erase(rit);
        }
    }
    close(fd);
}

}  // namespace

std::string here_room_for(const std::string& page) {
    if (page.rfind("/u/", 0) != 0) return "";
    const std::string who = page.substr(3);
    if (!valid_username(who) || !g_store.find_by_username(who)) return "";
    return page;
}

bool here_may_join(const std::string& viewer, const std::string& room) {
    if (viewer.empty() || room.rfind("/u/", 0) != 0) return false;
    auto choice = g_store.here_choice(viewer);
    if (!choice || !*choice) return false;
    const std::string owner = room.substr(3);
    auto found = g_store.find_by_username(owner);
    if (!found) return false;
    if (g_store.is_blocked(viewer, owner) || g_store.is_blocked(owner, viewer)) return false;
    const bool is_owner = viewer == owner;
    const bool is_friend = !is_owner && g_store.are_friends(viewer, owner);
    return visible_to(g_store.prefs_of(owner).profile_visibility, is_owner, true, is_friend);
}

bool here_ws_upgrade(int fd, const std::string& req, const std::string& method, const std::string& rawpath) {
    if (method != "GET" || rawpath.rfind("/here/ws", 0) != 0 || (rawpath.size() > 8 && rawpath[8] != '?') ||
        header_value(req, "Upgrade").find("websocket") == std::string::npos)
        return false;
    std::string me, room;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        auto acc = account_from_req(req);
        if (acc) {
            room = here_room_for(query_param(rawpath, "page"));
            if (!room.empty() && here_may_join(acc->username, room)) me = acc->username;
        }
    }
    auto key = header_value(req, "Sec-WebSocket-Key");
    if (!me.empty() && !key.empty()) {
        std::lock_guard<std::mutex> lock(here_mu());
        auto& r = here_rooms()[room];
        if (r.size() >= kHereRoomCap) me.clear();   // full: refused like any other refusal
        else r[fd] = HereDot{me, start_x(me), 0.08};
        if (r.empty()) here_rooms().erase(room);
    }
    if (me.empty() || key.empty()) {
        const std::string no = "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        send(fd, no.data(), no.size(), MSG_NOSIGNAL);
        close(fd);
        return true;
    }
    std::ostringstream resp101;
    resp101 << "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
               "Sec-WebSocket-Accept: " << websocket_accept_value(key) << "\r\n\r\n";
    const std::string r101 = resp101.str();
    if (send(fd, r101.data(), r101.size(), MSG_NOSIGNAL) <= 0) {
        std::lock_guard<std::mutex> lock(here_mu());
        here_rooms()[room].erase(fd);
        if (here_rooms()[room].empty()) here_rooms().erase(room);
        close(fd);
        return true;
    }
    std::thread(run_here_session, fd, me, room).detach();
    return true;
}

std::string here_page_bits(const std::string& viewer, const std::string& page) {
    if (viewer.empty() || here_room_for(page).empty()) return "";
    auto choice = g_store.here_choice(viewer);
    if (!choice) {
        auto btn = [&](const char* v, const char* label, const char* cls) {
            return std::string("<form class=\"inline\" method=\"POST\" action=\"/here/choice\">"
                   "<input type=\"hidden\" name=\"choice\" value=\"") + v + "\"><input type=\"hidden\" name=\"back\" value=\"" +
                   html_escape(page) + "\"><button class=\"" + cls + "\" type=\"submit\">" + label + "</button></form>";
        };
        return "<div class=\"card lg-here-ask\" id=\"here-ask\"><h2>Show yourself on profile pages?</h2>"
               "<p>People on a profile page can show up as small round pictures with their handle, moving around the "
               "page. You'd see others who said yes, and they'd see you. Your spot on the page is never saved. You can "
               "change this any time in <a href=\"/settings#here\">Settings</a>.</p>"
               "<div class=\"row\">" + btn("1", "Yes, show me", "") + btn("0", "No, keep me hidden", "ghost") + "</div></div>";
    }
    if (!*choice) return "";
    // usernames and /u/ paths are [A-Za-z0-9._-] only (valid_username), so JSON-escaped text can't end the script
    return "<script>window.LG_HERE={\"me\":\"" + json_escape(viewer) + "\",\"page\":\"" + json_escape(page) +
           "\"};</script><script src=\"" + asset_url("here.js") + "\" defer></script>";
}

std::string here_settings_card(const std::string& user) {
    auto choice = g_store.here_choice(user);
    auto radio = [&](const char* v, bool on, const char* label) {
        return std::string("<label class=\"row\" style=\"gap:.5rem;margin:.45rem 0\"><input type=\"radio\" name=\"here\" value=\"") +
               v + "\" required" + (on ? " checked" : "") + "> <span>" + label + "</span></label>";
    };
    return "<div class=\"card\" id=\"here\"><h2>Dots on profile pages</h2>"
           "<p class=\"muted\">Show yourself as a small round picture with your handle on profile pages you visit, "
           "moving around the page. You see others who chose yes; they see you. Your spot is never saved. People "
           "you blocked, or who blocked you, never see you.</p>"
           + std::string(choice ? "" : "<p class=\"warn\">You haven't chosen yet, so you're hidden.</p>") +
           "<form method=\"POST\" action=\"/here/choice\"><input type=\"hidden\" name=\"back\" value=\"/settings#here\">"
           + radio("1", choice && *choice, "Yes, show me") + radio("0", choice && !*choice, "No, keep me hidden") +
           "<button type=\"submit\">Save</button></form></div>";
}

std::string here_signup_fields() {
    return "<fieldset class=\"lg-here-signup\" style=\"border:1px solid var(--rule);border-radius:var(--rmd);padding:.6rem .9rem;margin:1rem 0 0\">"
           "<legend style=\"padding:0 .3rem;font-weight:600\">Show me on profile pages</legend>"
           "<p class=\"muted\" style=\"margin:.2rem 0\">People on a profile page can show up as small round pictures "
           "with their handle, moving around. Choose one; you can change it later in Settings.</p>"
           "<label class=\"row\" style=\"gap:.5rem\"><input type=\"radio\" name=\"here\" value=\"1\" required> <span>Yes, show me</span></label>"
           "<label class=\"row\" style=\"gap:.5rem\"><input type=\"radio\" name=\"here\" value=\"0\" required> <span>No, keep me hidden</span></label>"
           "</fieldset>";
}

}  // namespace liquidgov::site
