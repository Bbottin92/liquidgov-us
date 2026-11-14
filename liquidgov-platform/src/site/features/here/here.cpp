#include "site/features/here/here.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/html.hpp"
#include "site/core/http.hpp"
#include "site/core/state.hpp"
#include "site/core/text.hpp"
#include "site/core/ws_limits.hpp"
#include "site/ui/assets.hpp"

namespace liquidgov::site {

// Dots on profile pages (Brandon 2026-11-12: "the normal pages with little dots for people that you would see moving
// around"). One room per profile page. A position is a fraction of the page (x of its width, y of its height, 0..1),
// so a phone and a desktop put a dot in roughly the same place. Rooms live in memory only: leaving removes your dot
// and nothing about it is ever stored. Who may be seen: people who chose Yes (a required choice, the "here" record),
// never across a block in either direction.
//
// Spatial step 1: HereDot now also carries instanceId (defaults to the room/page). No behavior change; old clients
// ignore the extra field. Positions remain in-memory only.

namespace {

struct HereDot {
    std::string username;
    double x = 0.5, y = 0.1;
    std::string instanceId;  // spatial step 1: defaults to room; unused by current logic
    bool mic = false, cam = false;        // spatial step 11: opted in to share a microphone / camera (off until they say so)
    std::vector<std::string> inbox;       // spatial step 11: signaling frames waiting for this person's session thread to send
};

// Spatial step 11: proximity voice and video. The media itself flows between the two browsers; the server only relays the
// setup messages (offer, answer, ICE) and only between two dots that are in the same instance, both opted in, within
// this distance of each other (the room's 0..1 page coordinates) and not blocked either way. The client uses the same
// number (assets/here_media.js RANGE.aware) as the edge past which it hangs up.
constexpr double kMediaAware = 0.5;

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

// "<key>":true / false -> its value; nullopt when absent
std::optional<bool> json_bool(const std::string& s, const char* key) {
    auto p = s.find(key);
    if (p == std::string::npos) return std::nullopt;
    p = s.find(':', p);
    if (p == std::string::npos) return std::nullopt;
    p = s.find_first_not_of(' ', p + 1);
    if (p == std::string::npos) return std::nullopt;
    if (s.compare(p, 4, "true") == 0) return true;
    if (s.compare(p, 5, "false") == 0) return false;
    return std::nullopt;
}

// The text between the quotes of "<key>":"...", exactly as sent (escapes untouched, ends at the first unescaped quote);
// "" when absent or malformed. Relayed as it is: it stays inside its own string literal in what the receiver gets.
std::string json_raw_string(const std::string& s, const char* key) {
    auto p = s.find(key);
    if (p == std::string::npos) return "";
    p = s.find(':', p);
    if (p == std::string::npos) return "";
    p = s.find_first_not_of(' ', p + 1);
    if (p == std::string::npos || s[p] != '"') return "";
    for (std::size_t i = p + 1; i < s.size(); ++i) {
        if (s[i] == '\\') { ++i; continue; }
        if (s[i] == '"') return s.substr(p + 1, i - p - 1);
    }
    return "";
}

double clamp01(double v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

// Spatial step 5: the instance a client says it is in ("instanceId":"<a map box id or mainstream>" in its move
// message). "" when absent, longer than 64, or anything but letters, digits and : _ - . / (never echoed otherwise).
std::string json_instance(const std::string& s) {
    const std::string key = "\"instanceId\"";
    auto p = s.find(key);
    if (p == std::string::npos) return "";
    p = s.find(':', p + key.size());
    if (p == std::string::npos) return "";
    p = s.find('"', p);
    if (p == std::string::npos) return "";
    const auto e = s.find('"', p + 1);
    if (e == std::string::npos) return "";
    const std::string v = s.substr(p + 1, e - p - 1);
    if (v.empty() || v.size() > 64) return "";
    for (char c : v) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                        c == ':' || c == '_' || c == '-' || c == '.' || c == '/';
        if (!ok) return "";
    }
    return v;
}

// A start point that differs per person, so two newcomers don't sit on top of each other.
double start_x(const std::string& u) {
    unsigned h = 2166136261u;
    for (unsigned char c : u) h = (h ^ c) * 16777619u;
    return 0.1 + (h % 800) / 1000.0;
}

// Spatial step 9: eject rules. An instance can carry a hazard region (in the room's 0..1 page coordinates); a dot that is
// in that instance and inside the region is put back in its parent, the room's own instance. Admin-defined, read once
// from LIQUIDGOV_HERE_EJECT = "<instance>:<x0>,<y0>,<x1>,<y1>;<instance>:..." (no endpoint, nothing stored). The server
// applies it after every move, so a client cannot stay inside a hazard by keeping quiet or by claiming an instance.
struct EjectRule { std::string instance; double x0, y0, x1, y1; };
const std::vector<EjectRule>& eject_rules() {
    static const std::vector<EjectRule> rules = [] {
        std::vector<EjectRule> out;
        const char* env = std::getenv("LIQUIDGOV_HERE_EJECT");
        if (!env) return out;
        std::istringstream all(env);
        std::string one;
        while (std::getline(all, one, ';')) {
            const auto colon = one.rfind(':');
            if (colon == std::string::npos || colon == 0) continue;
            EjectRule r{one.substr(0, colon), 0, 0, 0, 0};
            if (std::sscanf(one.c_str() + colon + 1, "%lf,%lf,%lf,%lf", &r.x0, &r.y0, &r.x1, &r.y1) == 4) out.push_back(r);
        }
        return out;
    }();
    return rules;
}
void apply_eject(HereDot& d, const std::string& room) {
    for (const auto& r : eject_rules())
        if (d.instanceId == r.instance && d.x >= r.x0 && d.x <= r.x1 && d.y >= r.y0 && d.y <= r.y1) { d.instanceId = room; return; }
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

// A signaling message {"to":"<user>","sig":"<escaped JSON text>"} from `me`: queued for <user> only if the server's own
// state says they may talk (same instance, both opted in, in range, not blocked). Anything else is dropped without a word.
void relay_signal(int fd, const std::string& me, const std::string& room, const std::string& payload) {
    const std::string to = json_raw_string(payload, "\"to\"");
    const std::string raw = json_raw_string(payload, "\"sig\"");
    if (to.empty() || to == me || !valid_username(to) || raw.empty() || raw.size() > 7000) return;
    int target = -1;
    {
        std::lock_guard<std::mutex> lock(here_mu());
        auto rit = here_rooms().find(room);
        if (rit == here_rooms().end()) return;
        auto sit = rit->second.find(fd);
        if (sit == rit->second.end() || !(sit->second.mic || sit->second.cam)) return;
        for (const auto& [ofd, d] : rit->second) {
            if (d.username != to) continue;
            if (d.instanceId == sit->second.instanceId && (d.mic || d.cam) &&
                std::hypot(d.x - sit->second.x, d.y - sit->second.y) <= kMediaAware) target = ofd;
            break;
        }
    }
    if (target < 0) return;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        if (g_store.is_blocked(me, to) || g_store.is_blocked(to, me)) return;
    }
    std::lock_guard<std::mutex> lock(here_mu());
    auto rit = here_rooms().find(room);
    if (rit == here_rooms().end()) return;
    auto tit = rit->second.find(target);
    if (tit == rit->second.end() || tit->second.username != to || tit->second.inbox.size() >= 32) return;
    tit->second.inbox.push_back("{\"sig\":{\"from\":\"" + json_escape(me) + "\",\"data\":\"" + raw + "\"}}");
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
            const std::string& pl = result.frame.payload;
            const bool is_sig = pl.size() <= 8192 && pl.find("\"sig\"") != std::string::npos;   // a setup message, not a move
            if (result.frame.opcode == WsOpcode::Text && is_sig && ++moves <= 20) relay_signal(fd, me, room, pl);
            else if (result.frame.opcode == WsOpcode::Text && result.frame.payload.size() <= 200 && ++moves <= 20) {
                const auto mic = json_bool(result.frame.payload, "\"mic\""), cam = json_bool(result.frame.payload, "\"cam\"");
                auto nx = json_number(result.frame.payload, "\"x\"");
                auto ny = json_number(result.frame.payload, "\"y\"");
                const std::string inst = json_instance(result.frame.payload);
                if ((nx && ny) || !inst.empty() || mic || cam) {
                    std::lock_guard<std::mutex> lock(here_mu());
                    auto& r = here_rooms()[room];
                    auto it = r.find(fd);
                    if (it != r.end() && nx && ny) { it->second.x = clamp01(*nx); it->second.y = clamp01(*ny); }
                    if (it != r.end() && !inst.empty()) it->second.instanceId = inst;
                    if (it != r.end() && mic) it->second.mic = *mic;
                    if (it != r.end() && cam) it->second.cam = *cam;
                    if (it != r.end()) apply_eject(it->second, room);   // spatial step 9: the server's rule, after every move
                }
            }
        }
        // copy the room, then check blocks under the store lock (never both locks at once)
        std::vector<HereDot> dots;
        std::vector<std::string> setup;   // spatial step 11: setup messages queued for me
        {
            std::lock_guard<std::mutex> lock(here_mu());
            auto rit = here_rooms().find(room);
            if (rit == here_rooms().end() || !rit->second.count(fd)) break;
            // spatial step 5: only the dots in the same instance as this person (everyone starts in the room's own)
            const std::string mine = rit->second.at(fd).instanceId;
            setup.swap(rit->second.at(fd).inbox);
            for (const auto& [ofd, d] : rit->second) if (d.instanceId == mine) dots.push_back(d);
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
        bool wrote = true;
        for (const auto& m : setup) wrote = wrote && ws_write_frame(fd, WsOpcode::Text, m);
        if (!wrote) break;
        std::ostringstream os;
        os << "{\"people\":[";
        bool first = true;
        for (const auto& d : dots) {
            os << (first ? "" : ",") << "{\"u\":\"" << json_escape(d.username) << "\",\"x\":" << d.x << ",\"y\":" << d.y
               << ",\"instanceId\":\"" << json_escape(d.instanceId.empty() ? room : d.instanceId) << "\""
               << ",\"mic\":" << (d.mic ? "true" : "false") << ",\"cam\":" << (d.cam ? "true" : "false") << "}";
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

// The map walker's room (2026-11-14): one room for /map; each walker names the box it stands on as its instanceId,
// so a snapshot only holds the walkers on the same box (spatial step 5). Not a page with dots: here_room_for and
// here_page_bits do not know it.
static const char* const kHereMapRoom = "/map";

std::string here_room_for(const std::string& page) {
    if (page.rfind("/u/", 0) != 0) return "";
    const std::string who = page.substr(3);
    if (!valid_username(who) || !g_store.find_by_username(who)) return "";
    return page;
}

bool here_may_join(const std::string& viewer, const std::string& room) {
    if (viewer.empty()) return false;
    auto choice = g_store.here_choice(viewer);
    if (!choice || !*choice) return false;
    if (room == kHereMapRoom) return true;   // the map's one room: anyone who chose Yes (blocks filter each snapshot)
    if (room.rfind("/u/", 0) != 0) return false;
    const std::string owner = room.substr(3);
    auto found = g_store.find_by_username(owner);
    if (!found) return false;
    if (g_store.is_blocked(viewer, owner) || g_store.is_blocked(owner, viewer)) return false;
    const bool is_owner = viewer == owner;
    const bool is_friend = !is_owner && g_store.are_friends(viewer, owner);
    return visible_to(g_store.prefs_of(owner).profile_visibility, is_owner, true, is_friend);
}

bool here_ws_upgrade(int fd, const std::string& req, const std::string& method, const std::string& rawpath,
                     const std::string& peer) {
    if (method != "GET" || rawpath.rfind("/here/ws", 0) != 0 || (rawpath.size() > 8 && rawpath[8] != '?') ||
        header_value(req, "Upgrade").find("websocket") == std::string::npos)
        return false;
    std::string me, room;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        auto acc = account_from_req(req);
        if (acc) {
            const std::string page = query_param(rawpath, "page");
            room = page == kHereMapRoom ? page : here_room_for(page);   // the map walker's room (lgmap_walk.js), else a profile page
            if (!room.empty() && here_may_join(acc->username, room)) me = acc->username;
        }
    }
    auto key = header_value(req, "Sec-WebSocket-Key");
    std::unique_ptr<WsSlot> slot;
    if (!me.empty() && !key.empty()) {
        slot = ws_take_slot(ws_client_ip(req, peer));
        if (!slot) { ws_refuse(fd, "/here/ws"); return true; }
    }
    if (!me.empty() && !key.empty()) {
        std::lock_guard<std::mutex> lock(here_mu());
        auto& r = here_rooms()[room];
        if (r.size() >= kHereRoomCap) me.clear();   // full: refused like any other refusal
        else r[fd] = HereDot{me, start_x(me), 0.08, room};  // instanceId defaults to room
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
    std::thread([fd, me, room, slot = std::move(slot)] { run_here_session(fd, me, room); }).detach();
    return true;
}

// The map box of a page with dots (the map walker's portal arrives there): a profile /u/<name> is "user:<name>",
// as the map names people (map_rules.cpp). Any other page: "" (the walker arrives at you).
static std::string here_box_id(const std::string& page) {
    return page.rfind("/u/", 0) == 0 && page.size() > 3 ? "user:" + page.substr(3) : std::string();
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
           "\",\"box\":\"" + json_escape(here_box_id(page)) +
           "\"};</script><script src=\"" + asset_url("walker.js") + "\" defer></script>"
           "<script src=\"" + asset_url("here.js") + "\" defer></script>"
           "<script src=\"" + asset_url("here_media.js") + "\" defer></script>";
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
