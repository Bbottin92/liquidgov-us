// LiquidGov.US site — C++ HTTP + platform auth.
// Persistence: FileStore (LGP1 snapshot). Not liquidgov-core's ledger.
#include "liquidgov/platform/auth.hpp"
#include "liquidgov/platform/avatar.hpp"
#include "liquidgov/platform/blob.hpp"
#include "liquidgov/platform/clock.hpp"
#include "liquidgov/platform/file_store.hpp"
#include "liquidgov/platform/file_transfer.hpp"
#include "liquidgov/platform/file_type.hpp"
#include "liquidgov/platform/messenger.hpp"
#include "liquidgov/platform/websocket.hpp"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
using namespace liquidgov::platform;
namespace {
constexpr int kPort = 8080;
// Mail attachments run through file_transfer's default caps (image 10 MiB,
// document/PDF/OOXML 25 MiB — see SizeCaps in file_transfer.hpp). The read
// loop's ceiling has to cover the largest of those plus multipart overhead.
constexpr std::size_t kMaxBody = 25ull * 1024 * 1024 + 64 * 1024;
std::mutex g_mu;

// --- World: one shared room, connected users see each other move. ---
// Deliberately separate from g_mu / g_store: this is ephemeral in-memory
// position state, never persisted, touched only by world session threads
// (one per connected /world/ws client) — the rest of the server stays on
// its existing single-threaded accept loop, untouched by this feature.
struct WorldPlayer {
    double x = 128, y = 128;
    std::string username;
    std::string color;
};
std::mutex g_world_mu;
std::unordered_map<int, WorldPlayer> g_world_players; // keyed by client socket fd

std::string world_color_for(const std::string& username) {
    static const char* kPalette[] = {"#a23b3b", "#3b6ea2", "#3ba25e", "#a2823b",
                                      "#6e3ba2", "#3ba297", "#a23b8f", "#5f7a2e"};
    std::size_t h = std::hash<std::string>{}(username);
    return kPalette[h % 8];
}

// --- Live jukebox "stations": one shared, synced-elapsed-position queue
// per station that everyone connected to that station's socket hears
// "live" (new joiners seek to the current track's elapsed position).
// Stations are keyed by slug -- "" is the one original site-wide station
// (still /community/ws, unchanged for existing callers), any other slug
// is that community's own jukebox (/c/<slug>/jukebox/ws), gated to
// members of that community since "everyone in the group can hear" means
// the group, not the whole site the way the site-wide one is. One mutex,
// one pair of maps, same one-thread-per-connection pattern as /world/ws.
struct CommunityTrack {
    std::string video_id;
    std::string queued_by;
};
struct CommunityNowPlaying {
    std::string video_id;
    std::string queued_by;
    std::int64_t started_at = 0;
    bool active = false;
};
std::mutex g_community_mu;
std::map<std::string, CommunityNowPlaying> g_community_now_by_station;
std::map<std::string, std::deque<CommunityTrack>> g_community_upcoming_by_station;

// Caller must hold g_community_mu.
void community_advance_locked(const std::string& station, std::int64_t now) {
    auto& upcoming = g_community_upcoming_by_station[station];
    if (upcoming.empty()) {
        g_community_now_by_station[station] = CommunityNowPlaying{};
        return;
    }
    auto next = upcoming.front();
    upcoming.pop_front();
    g_community_now_by_station[station] = CommunityNowPlaying{next.video_id, next.queued_by, now, true};
}

class NeverBreached : public BreachChecker {
public:
    bool is_breached(const std::string&) const override { return false; }
};
class AllowLimiter : public RateLimiter {
public:
    bool allow(const std::string&, const std::string&) override { return true; }
    void record_attempt(const std::string&, const std::string&) override {}
};
// Not a malware engine. Normalized avatars are re-encoded PNG; this
// rejects anything that is not a PNG signature. Always-true would be a lie.
class PngMagicScanner : public MalwareScanner {
public:
    bool is_clean(const std::vector<std::uint8_t>& bytes) const override {
        static const unsigned char sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
        if (bytes.size() < 8) return false;
        return std::memcmp(bytes.data(), sig, 8) == 0;
    }
};
// Same category of placeholder as PngMagicScanner, generalized to every
// type ingest_file allows. Not a malware engine — a real deployment plugs
// one in here. This re-checks that the bytes actually being stored still
// match a known-good signature; always-true would be a lie.
class MailAttachmentScanner : public MalwareScanner {
public:
    bool is_clean(const std::vector<std::uint8_t>& bytes) const override {
        return detect_type(bytes) != DetectedType::Unknown;
    }
};

// Adapter over FileStore's username-keyed friend graph, mapping through
// AccountId as messenger.hpp requires. is_minor has no AgeAssurance
// backing yet — returns false for everyone rather than fail-safe-true,
// per the closed pin: treating everyone as a minor would brick the site.
// The compensating control is that 1:1 mail stays friends-only regardless
// of this predicate (unchanged, enforced separately in the /mail routes
// below) — this predicate only ever matters for the group minor
// invariant, which is consequently unpaid until AgeAssurance lands.
class SiteSocialGraph : public SocialGraph {
public:
    bool are_friends(AccountId a, AccountId b) const override;
    bool is_minor(AccountId) const override { return false; }
};

const char* store_path_from_env() {
    const char* p = std::getenv("LIQUIDGOV_STORE");
    return p && p[0] ? p : "liquidgov_accounts.lgp";
}
const char* blob_path_from_env() {
    const char* p = std::getenv("LIQUIDGOV_BLOBS");
    return p && p[0] ? p : "liquidgov_blobs";
}
// Client-side YouTube search (Data API v3 search.list) needs a key. Read-only
// key, meant to be HTTP-referrer-restricted in Google Cloud Console to this
// site's origin — same trust level as any client-embedded map/analytics key.
// Empty means "not configured"; the search box degrades to a message instead
// of failing silently.
const char* youtube_api_key_from_env() {
    const char* p = std::getenv("YOUTUBE_API_KEY");
    return p && p[0] ? p : "";
}

FileStore g_store{store_path_from_env()};
FilesystemBlobStore g_blobs{blob_path_from_env()};
PngMagicScanner g_scanner;
MailAttachmentScanner g_mail_scanner;
SizeCaps g_mail_caps; // defaults from file_transfer.hpp
NeverBreached g_breach;
AllowLimiter g_limiter;
SiteSocialGraph g_social_graph;

bool SiteSocialGraph::are_friends(AccountId a, AccountId b) const {
    auto ua = g_store.find_by_id(a);
    auto ub = g_store.find_by_id(b);
    if (!ua || !ub) return false;
    return g_store.are_friends(ua->username, ub->username);
}

std::string html_escape(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': o += '&'; o += "amp;"; break;
            case '<': o += '&'; o += "lt;"; break;
            case '>': o += '&'; o += "gt;"; break;
            case '"': o += '&'; o += "quot;"; break;
            default: o += c; break;
        }
    }
    return o;
}

// Quotes a server-controlled (not user-controlled) string as a single-quoted
// JS string literal embedded in generated HTML, e.g. the YouTube API key.
std::string js_string_literal(const std::string& s) {
    std::string o = "'";
    for (char c : s) {
        switch (c) {
            case '\'': o += "\\'"; break;
            case '\\': o += "\\\\"; break;
            case '<': o += "\\x3c"; break;
            default: o += c; break;
        }
    }
    o += "'";
    return o;
}

// Gates a Visibility-tiered surface (profile, posts) for one viewer.
// is_owner always sees their own; otherwise Public admits anyone (including
// an anonymous visitor), Members requires a signed-in account, Friends
// requires the relationship, and OnlyMe admits nobody else at all.
bool visible_to(Visibility v, bool is_owner, bool viewer_signed_in, bool is_friend) {
    if (is_owner) return true;
    switch (v) {
        case Visibility::Public: return true;
        case Visibility::Members: return viewer_signed_in;
        case Visibility::Friends: return is_friend;
        case Visibility::OnlyMe: default: return false;
    }
}

// Author CSS for /u/<username>/theme. This is a deliberately narrow v1 of
// the "Author CSS isolation" design in LIQUIDGOV_CONTEXT_HANDOFF.md, not
// the full spec (no CSSOM-walking parser, no compute budget, no gallery) --
// see that doc's "Still open on this layer" note. What this v1 keeps from
// the spec, because skipping it would be a real hole, not a shortcut:
//   - Reject outright rather than best-effort strip. A rejected save with
//     a clear reason beats a silently mangled stylesheet.
//   - Deny @import, @font-face, @namespace, @layer, @scope, url(),
//     expression(), -moz-binding, behavior:, javascript: -- the constructs
//     that let CSS alone exfiltrate data, load arbitrary resources, or run
//     script. url() is denied outright (no "already in the upload
//     pipeline" allowance yet -- that needs the blob pipeline wired in,
//     not just a text filter).
//   - The theme document itself renders in a sandboxed iframe with no
//     allow-same-origin and no allow-scripts (see /u/<username>/theme and
//     its embedding below), so even a gap in this text filter can't reach
//     cookies, storage, or script execution -- belt and suspenders, not
//     the only defense.
// Not defended against in this v1 (named, not hidden): :has()/sibling
// combinators aimed at platform-injected nodes inside the themed document.
// Lower severity here specifically because the sandboxed iframe has no
// platform chrome/badges inside it to target in the first place.
constexpr std::size_t kAuthorCssMaxBytes = 20000;
// Caps how many snapshots one drawing session can capture -- keeps the
// timelapse @keyframes CSS (one background-image per frame) and blob
// storage bounded regardless of how long someone leaves the canvas open.
constexpr std::size_t kCanvasMaxFrames = 24;

struct CssCheckResult {
    bool ok = true;
    std::string code; // short token, safe to carry in a redirect query string as-is
};

// code -> human-readable reason, rendered server-side on the settings page.
// Codes only ever come from sanitize_author_css below, never user input, so
// this lookup is a fixed, closed set (a missing code just falls back to a
// generic message).
std::string css_reject_message(const std::string& code) {
    static const std::map<std::string, std::string> kMessages = {
        {"size", "That's over the " + std::to_string(kAuthorCssMaxBytes / 1000) + "KB limit."},
        {"import", "Not allowed in profile CSS: @import."},
        {"font-face", "Not allowed in profile CSS: @font-face."},
        {"namespace", "Not allowed in profile CSS: @namespace."},
        {"layer", "Not allowed in profile CSS: @layer."},
        {"scope", "Not allowed in profile CSS: @scope."},
        {"url", "Not allowed in profile CSS: url()."},
        {"expression", "Not allowed in profile CSS: expression()."},
        {"moz-binding", "Not allowed in profile CSS: -moz-binding."},
        {"behavior", "Not allowed in profile CSS: behavior:."},
        {"javascript", "Not allowed in profile CSS: javascript:."},
        {"style-tag", "Not allowed in profile CSS: a style tag."},
        {"script-tag", "Not allowed in profile CSS: a script tag."},
    };
    auto it = kMessages.find(code);
    return it == kMessages.end() ? "That wasn't accepted." : it->second;
}

CssCheckResult sanitize_author_css(const std::string& css_in) {
    if (css_in.size() > kAuthorCssMaxBytes) return {false, "size"};
    std::string lower = css_in;
    for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    static const std::pair<const char*, const char*> kDenied[] = {
        {"@import", "import"},
        {"@font-face", "font-face"},
        {"@namespace", "namespace"},
        {"@layer", "layer"},
        {"@scope", "scope"},
        {"url(", "url"},
        {"expression(", "expression"},
        {"-moz-binding", "moz-binding"},
        {"behavior:", "behavior"},
        {"javascript:", "javascript"},
        {"</style", "style-tag"},
        {"<style", "style-tag"},
        {"<script", "script-tag"},
    };
    for (const auto& [needle, code] : kDenied) {
        if (lower.find(needle) != std::string::npos) return {false, code};
    }
    return {true, ""};
}

// Server-generated (never sanitize_author_css'd -- it isn't user text)
// background for the theme document's .lg-theme, from that account's
// saved canvas frames. Static: the last frame, once. Timelapse: a
// steps(1) @keyframes cycling background-image through every frame in
// order -- CSS animating a discrete stop-motion sequence, which is the
// only way to "replay" a drawing inside a document with zero script
// execution allowed. Empty when there are no frames yet, so it never
// clobbers the default look.
std::string theme_background_css(const std::string& username, std::size_t frame_count, bool timelapse) {
    if (frame_count == 0) return "";
    auto frame_url = [&](std::size_t i) {
        return "/canvas-frame/" + username + "/" + std::to_string(i);
    };
    std::ostringstream os;
    os << ".lg-theme{background-repeat:no-repeat;background-size:cover;background-position:center}";
    if (!timelapse || frame_count == 1) {
        os << ".lg-theme{background-image:url(" << frame_url(frame_count - 1) << ")}";
        return os.str();
    }
    const double step_seconds = 0.6;
    os << "@keyframes lg-canvas-loop{";
    for (std::size_t i = 0; i < frame_count; ++i) {
        double pct = (static_cast<double>(i) / static_cast<double>(frame_count)) * 100.0;
        os << pct << "%{background-image:url(" << frame_url(i) << ")}";
    }
    os << "100%{background-image:url(" << frame_url(0) << ")}";
    os << "}";
    os << ".lg-theme{animation:lg-canvas-loop " << (step_seconds * static_cast<double>(frame_count))
       << "s steps(1) infinite}";
    return os.str();
}

std::string css() {
    return
        ":root{--paper:#f7f1e4;--parchment:#efe8d8;--panel:#f3ecdd;--ink:#1a1814;"
        "--accent:#8a6620;--stone:#6b6458;--rule:rgba(26,24,20,.14);--danger:#6b2424;"
        "--warn:#7a4a12;--ok:#1e3d32;--rsm:6px;--rmd:8px;--rlg:10px;"
        "--display:\"Playfair Display\",\"Iowan Old Style\",Palatino,\"Palatino Linotype\",Georgia,serif;"
        "--body:\"Segoe UI\",system-ui,-apple-system,sans-serif}"
        // Same tokens, ink warmed rather than pure black -- system dark
        // mode applies automatically unless an explicit choice (data-theme,
        // set by the header toggle, see page_wrap) overrides it either way.
        // --accent was --navy (a cool blue) until this project's real,
        // separately-deployed liquidgov.us site turned up in the mobile
        // design's own reference uploads -- same near-black/parchment
        // structure this theme already had, but a warm brass/gold accent
        // instead of blue. Recolored to match rather than invented fresh.
        "@media (prefers-color-scheme:dark){:root:not([data-theme=\"light\"]){"
        "--paper:#14120f;--parchment:#241f18;--panel:#1b1813;--ink:#f2ead8;"
        "--accent:#d1a85c;--stone:#a89c88;--rule:rgba(242,234,216,.14);"
        "--danger:#c9776a;--warn:#d9a866;--ok:#7fa891}}"
        ":root[data-theme=\"dark\"]{"
        "--paper:#14120f;--parchment:#241f18;--panel:#1b1813;--ink:#f2ead8;"
        "--accent:#d1a85c;--stone:#a89c88;--rule:rgba(242,234,216,.14);"
        "--danger:#c9776a;--warn:#d9a866;--ok:#7fa891}"
        "*{box-sizing:border-box}html{-webkit-font-smoothing:antialiased}"
        "body{margin:0;background:var(--paper);color:var(--ink);font:16px/1.5 var(--body);"
        "transition:background .15s ease,color .15s ease}"
        "a{color:var(--accent)}a:hover{color:var(--ink)}"
        "h1,h3{font-family:var(--display);font-weight:600;letter-spacing:-.02em;"
        "text-wrap:balance;line-height:1.2}h1{font-size:clamp(1.6rem,1.2rem + 1.6vw,2.2rem);margin:.2rem 0 .8rem}"
        "h3{font-size:1.15rem;margin:0 0 .3rem}"
        // A register, not app chrome: every card/section title is a small
        // tracked-out label naming what the block below it is, not a
        // headline competing with the page's own h1.
        "h2{font-family:var(--display);font-weight:400;font-size:.75rem;letter-spacing:.12em;"
        "text-transform:uppercase;color:var(--stone);margin:1.5rem 0 .7rem;line-height:1.3}"
        "p{text-wrap:pretty}"
        ".wrap{max-width:58rem;margin:0 auto;padding:0 1.1rem 3.5rem}"
        "header.top{border-bottom:1px solid var(--rule);background:var(--parchment);"
        "position:sticky;top:0;z-index:5}"
        ".topinner{max-width:58rem;margin:0 auto;padding:.7rem 1.1rem;display:flex;"
        "align-items:center;flex-wrap:wrap;gap:.45rem 1rem}"
        ".brand{display:flex;align-items:center;gap:.65rem;text-decoration:none;color:var(--ink);margin-right:auto}"
        ".mark{flex-shrink:0;display:block}"
        ".brand strong{font-family:var(--display);font-weight:600;font-size:1.35rem;letter-spacing:-.03em}"
        ".brand .tld{color:var(--stone);font-size:.72rem;letter-spacing:.12em;text-transform:uppercase;margin-left:.15rem}"
        ".brand .tagline{color:var(--stone);font-size:.62rem;letter-spacing:.14em;text-transform:uppercase;"
        "margin-top:.1rem}"
        "#navc{position:absolute;left:-999px}"
        ".navbtn{display:none;min-height:44px;min-width:44px;align-items:center;justify-content:center;"
        "border:1px solid var(--rule);background:var(--paper);border-radius:var(--rsm);color:var(--ink);"
        "font:600 .85rem var(--body);cursor:pointer}"
        "nav{display:flex;flex-wrap:wrap;gap:.1rem .85rem;align-items:center;flex:1 1 16rem}"
        ".themebtn{min-height:36px;min-width:36px;display:inline-flex;align-items:center;justify-content:center;"
        "border:1px solid var(--rule);border-radius:50%;color:var(--ink);text-decoration:none;"
        "font-size:1rem;margin-left:.4rem;flex-shrink:0}"
        "nav a{color:var(--accent);text-decoration:none;font-size:.92rem;padding:.35rem 0;min-height:44px;"
        "display:inline-flex;align-items:center}nav a:hover{text-decoration:underline}"
        ".badge{display:inline-flex;align-items:center;justify-content:center;min-width:1.3rem;height:1.3rem;"
        "padding:0 .35rem;border-radius:999px;background:var(--accent);color:var(--paper);font-size:.7rem;"
        "margin-left:.25rem;font-variant-numeric:tabular-nums}"
        ".card{background:var(--panel);border:1px solid var(--rule);border-radius:var(--rlg);padding:1.15rem 1.25rem;margin:1rem 0}"
        ".card .card{border-radius:var(--rmd)}"
        ".lede{font-family:var(--display);font-size:1.15rem;line-height:1.4;color:var(--accent)}"
        ".muted{color:var(--stone);font-size:.9rem}.warn{color:var(--warn)}.ok{color:var(--ok)}"
        ".laws{display:grid;gap:.7rem;margin:1rem 0}"
        "@media(min-width:640px){.laws{grid-template-columns:1fr 1fr 1fr}}"
        ".law{background:var(--panel);border:1px solid var(--rule);border-radius:var(--rlg);padding:1rem 1.05rem}"
        ".law strong{display:block;font-family:var(--display);font-size:1.05rem;margin-bottom:.3rem}"
        "label{display:block;font-size:.82rem;color:var(--stone);margin:.4rem 0 .15rem;font-weight:500}"
        "input,textarea,select{width:100%;padding:.65rem .7rem;border-radius:var(--rsm);border:1px solid var(--rule);"
        "background:var(--paper);color:var(--ink);font:1rem var(--body);min-height:44px}"
        // Checkboxes and radios are natively tiny; the width:100%/min-height:44px
        // above (meant for text fields) stretches them into giant boxes if not
        // overridden -- this bit us once already on input[type=color] (fixed
        // with an inline style there), so fix it globally here instead of
        // per-control at every future callsite.
        "input[type=checkbox],input[type=radio]{width:1.1rem;min-height:0;height:1.1rem;padding:0;flex:0 0 auto}"
        "textarea{min-height:6rem;resize:vertical}button,.btn{display:inline-flex;align-items:center;justify-content:center;"
        "gap:.35rem;background:var(--accent);color:var(--paper);border:0;padding:.65rem 1.05rem;border-radius:var(--rsm);"
        "cursor:pointer;font:600 .95rem var(--body);min-height:44px;text-decoration:none}"
        "button:hover,.btn:hover{filter:brightness(1.08)}button:active,.btn:active{transform:scale(.98)}"
        "button.ghost,.btn.ghost{background:transparent;color:var(--accent);border:1px solid var(--rule)}"
        "button.danger,.btn.danger{background:var(--danger);color:#f7f1e4}"
        ".row{display:flex;gap:.5rem;flex-wrap:wrap;align-items:center}"
        ".toolgroup-sep{width:1px;align-self:stretch;background:var(--rule);margin:0 .15rem}"
        // Below this width .canvas-frame's three children (tool row,
        // the .cprops wrapper, and the canvas+layers row) just stack in
        // that order -- exactly what rendered before this rule existed.
        // At or above it, the tool icons wrap vertically down the
        // canvas's left edge (a real toolbox column, full height); the
        // .cprops wrapper -- itself holding the colour/size/opacity row,
        // the glow/mirror/fill row, and the hidden text row, stacked by
        // normal block flow -- wraps horizontally across the top edge (a
        // real options bar). That's deliberately one wrapping element
        // grid-placed once, not three separate items placed at the same
        // grid-row/grid-column: CSS Grid overlaps same-cell items rather
        // than stacking them, which is exactly what broke the first
        // version of this layout live. Together this forms an L-shaped
        // frame around two adjacent sides of the canvas+layers area at
        // bottom-right, the same corner arrangement Photoshop/Procreate/
        // every real paint app uses, without moving any of the underlying
        // markup (so DOM/tab/reading order is identical at every width).
        "@media(min-width:900px){.canvas-frame{display:grid;"
        "grid-template-columns:auto 1fr;grid-template-rows:auto auto;gap:.6rem;align-items:stretch}"
        // A single-file column, one icon wide -- not a 2-up grid, which
        // read as a block of icons rather than a strip wrapping the
        // canvas's edge.
        ".canvas-frame>:nth-child(1){grid-column:1;grid-row:1/3;display:flex;"
        "flex-direction:column;flex-wrap:nowrap;justify-content:space-between}"
        ".canvas-frame>:nth-child(1) .toolgroup-sep{width:100%;height:1px;margin:.2rem 0}"
        ".canvas-frame>:nth-child(2){grid-column:2;grid-row:1}"
        ".canvas-frame>:nth-child(3){grid-column:2;grid-row:2}"
        "}"
        ".person,.sess,.msg,.post{border-top:1px solid var(--rule);padding:.7rem 0;display:flex;"
        "justify-content:space-between;gap:.8rem;flex-wrap:wrap;align-items:flex-start}"
        ".person:first-of-type,.sess:first-of-type,.msg:first-of-type,.post:first-of-type{border-top:0}"
        "table.register{width:100%;border-collapse:collapse;margin:.6rem 0 1rem}"
        "table.register th{text-align:left;font:600 .7rem var(--body);letter-spacing:.08em;"
        "text-transform:uppercase;color:var(--stone);padding:.4rem .3rem;border-bottom:1px solid var(--rule)}"
        "table.register td{padding:.5rem .3rem;border-bottom:1px solid var(--rule)}"
        "table.register select{width:auto;min-height:auto;padding:.3rem .5rem}"
        ".eyebrow{font-family:var(--display);font-weight:400;font-size:.75rem;letter-spacing:.12em;"
        "text-transform:uppercase;color:var(--stone);margin:0 0 .3rem}"
        ".chip{display:inline-block;border:1px solid var(--rule);background:var(--parchment);"
        "border-radius:999px;padding:.25rem .7rem;font-size:.78rem;color:var(--ink)}"
        "table.record{border-collapse:collapse;margin:.6rem 0}"
        "table.record td{padding:.2rem .6rem .2rem 0;border:0;font-size:.9rem}"
        "table.record td:first-child{color:var(--stone);font-size:.72rem;letter-spacing:.08em;"
        "text-transform:uppercase;white-space:nowrap}"
        ".portrait{width:96px;height:96px;border-radius:50%;object-fit:cover;border:1px solid var(--rule);"
        "background:var(--parchment);flex-shrink:0;display:inline-block;vertical-align:middle}"
        ".portrait.sm{width:36px;height:36px}"
        ".portrait.ph{box-sizing:border-box}"
        // The info div next to the avatar is a flex item with no
        // flex-grow, so by default it only ever sizes to fit its own
        // text (a short "name · timestamp" line) -- fine until a post's
        // width:100% image/video landed inside it and sized itself
        // against that narrow shrink-wrapped box instead of the actual
        // card. flex:1 makes it fill the row like the rest of the
        // layout already assumes; min-width:0 is required alongside it
        // so a flex item is actually allowed to be narrower than its
        // content's intrinsic width instead of blowing out the row.
        ".who{display:flex;gap:.75rem;align-items:flex-start}"
        ".who>div{flex:1;min-width:0}"
        ".post p{margin:.25rem 0 0;white-space:pre-wrap}"
        "ul.plain{list-style:none;padding:0;margin:0}ul.plain li{border-top:1px solid var(--rule);padding:.65rem 0}"
        "footer.site{max-width:58rem;margin:0 auto;padding:1.2rem 1.1rem 2.5rem;color:var(--stone);font-size:.82rem;"
        "border-top:1px solid var(--rule)}"
        "footer.site a{color:var(--stone)}"
        ".empty{color:var(--stone);padding:.4rem 0}"
        "form.inline{display:inline}form.stack{margin:0}"
        ".clocks{display:grid;gap:.7rem}@media(min-width:640px){.clocks{grid-template-columns:1fr 1fr}}"
        ".jukebox iframe{width:100%;max-width:480px;aspect-ratio:16/9;border:0;border-radius:var(--rsm);display:block}"
        ".jbstation-row{display:flex;align-items:center;gap:.4rem;background:var(--paper);"
        "border:1px solid var(--rule);border-radius:var(--rsm);padding:.35rem .5rem;touch-action:none}"
        ".jbstation-row.active{border-color:var(--accent);background:var(--parchment)}"
        ".jbdraghandle{cursor:grab;color:var(--stone);font-size:1rem;line-height:1;user-select:none;padding:0 .2rem}"
        ".jbstation-name{flex:1;font-size:.85rem;cursor:pointer;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
        ".jbstation-remove{background:none;border:0;color:var(--stone);cursor:pointer;font-size:.9rem;padding:0 .2rem}"
        ".jukebox .row{margin-top:.5rem}"
        ".stattiles{display:grid;grid-template-columns:repeat(3,1fr);gap:.6rem;margin:.7rem 0}"
        ".stattile{background:var(--paper);border:1px solid var(--rule);border-radius:var(--rmd);"
        "padding:.7rem .5rem;text-align:center}"
        ".stattile strong{display:block;font-family:var(--display);font-size:1.3rem;line-height:1.2}"
        ".stattile span{display:block;font-size:.65rem;letter-spacing:.08em;text-transform:uppercase;"
        "color:var(--stone);margin-top:.2rem}"
        ".roomrow{display:flex;align-items:center;gap:.7rem;padding:.6rem 0;border-top:1px solid var(--rule);"
        "text-decoration:none;color:var(--ink)}.roomrow:first-of-type{border-top:0}"
        ".roomrow:hover .roomname{text-decoration:underline}"
        ".roomini{width:34px;height:34px;border-radius:50%;background:var(--parchment);border:1px solid var(--rule);"
        "display:inline-flex;align-items:center;justify-content:center;font-family:var(--display);"
        "font-weight:600;flex-shrink:0}"
        ".roomname{flex:1;font-weight:600;min-width:0}.roomname .muted{display:block;font-weight:400;font-size:.78rem}"
        ".roomlive{color:var(--stone);font-size:.8rem;white-space:nowrap;flex-shrink:0}"
        ".ticker-wrap{margin-top:.6rem;height:12rem;overflow:hidden;"
        "-webkit-mask-image:linear-gradient(180deg,transparent,#000 20%,#000 80%,transparent);"
        "mask-image:linear-gradient(180deg,transparent,#000 20%,#000 80%,transparent)}"
        ".ticker-roll{animation:lg-roll 26s linear infinite}"
        ".ticker-roll div{font-family:var(--display);font-size:1rem;line-height:1.9;color:var(--ink)}"
        "@keyframes lg-roll{0%{transform:translateY(0)}100%{transform:translateY(-50%)}}"
        "@media(prefers-reduced-motion:reduce){.ticker-roll{animation:none}}"
        ".quote.centered{text-align:center;background:var(--parchment)}"
        ".quote p:not(.attrib):not(.stars){font-family:var(--display);font-size:1.15rem;line-height:1.5}"
        ".quote .stars{color:var(--accent);letter-spacing:.12em;margin:.3rem 0}"
        ".quote .attrib{font-size:.68rem;letter-spacing:.16em;text-transform:uppercase;color:var(--accent);margin:.5rem 0 0}"
        ".quote .attrib.muted{color:var(--stone)}"
        ".lawstrip{display:flex}"
        ".lawstrip>div{flex:1;text-align:center;padding:0 .5rem;border-left:1px solid var(--rule)}"
        ".lawstrip>div:first-child{border-left:0}"
        ".lawstrip .n{margin:0;font-size:.7rem;letter-spacing:.1em;text-transform:uppercase;color:var(--accent)}"
        ".lawstrip .t{margin:.4rem 0 0;font-size:.85rem;line-height:1.3}"
        ".quadcard{display:block;text-decoration:none;color:var(--ink);"
        "background:linear-gradient(160deg,var(--parchment),var(--panel))}"
        ".quadcard:hover{filter:brightness(1.05)}"
        ".quadcard .quadhead{font-family:var(--display);font-size:1.3rem;line-height:1.2;margin:.5rem 0 0;color:var(--ink)}"
        // nav used to expand in normal document flow, inside the sticky
        // header -- fine until the link list itself got taller than a
        // small viewport, at which point the whole (now very tall)
        // header was still sticky, so it stayed pinned over the page as
        // you scrolled past it, covering everything underneath instead
        // of scrolling away. position:absolute here keeps header.top's
        // own box always just the slim top bar; the expanded menu floats
        // over the content below as a real dropdown instead of dragging
        // the sticky container's height along with it.
        "@media(max-width:720px){.navbtn{display:inline-flex}"
        "nav{display:none;position:absolute;top:100%;left:0;right:0;flex-direction:column;"
        "align-items:stretch;background:var(--parchment);border-bottom:1px solid var(--rule);"
        "padding:0 1.1rem;max-height:calc(100vh - 60px);overflow-y:auto;z-index:6}"
        "nav a{min-height:44px}#navc:checked~nav{display:flex}}"
        "@media(prefers-reduced-motion:reduce){*{transition:none!important;animation:none!important}}";
}

std::string favicon_svg() {
    // Same ring-and-droplet mark as the header .mark, in literal colors
    // -- a standalone favicon document has no page CSS to read
    // var(--accent) from.
    return "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 48 48\">"
           "<rect width=\"48\" height=\"48\" fill=\"#14120f\"/>"
           "<circle cx=\"24\" cy=\"24\" r=\"21\" fill=\"none\" stroke=\"#6b6458\" stroke-width=\"1.5\"/>"
           "<circle cx=\"24\" cy=\"24\" r=\"17.5\" fill=\"none\" stroke=\"#d1a85c\" stroke-width=\"2\"/>"
           "<path d=\"M24 9c0 0 9.5 12.2 9.5 17.2a9.5 9.5 0 0 1-19 0C14.5 21.2 24 9 24 9z\" fill=\"#d1a85c\"/></svg>";
}

// "dark", "light", or "" (no explicit choice -- prefers-color-scheme
// decides). Set once per request in handle() (the whole site's single-
// threaded synchronous entry point), read here at render time.
thread_local std::string g_theme_mode;

// A full-bleed, muted, autoplaying clip shown the first time a browser
// ever lands on the site -- gated entirely client-side (localStorage),
// since "have I seen this before" is per-browser state with no reason to
// touch the account store or work for a logged-out visitor differently
// from a member. Skippable; ends by fading out and removing itself,
// revealing the page underneath (already rendered, not reloaded). Storage
// being blocked (private mode, some browsers) fails safe by just never
// showing it, rather than showing it on every single load.
std::string intro_overlay_html() {
    return "<div id=\"lgintro\" style=\"position:fixed;inset:0;z-index:999;background:#14120f;"
           "display:flex;align-items:center;justify-content:center;overflow:hidden\">"
           "<video id=\"lgintrovid\" autoplay muted playsinline "
           "style=\"width:100%;height:100%;object-fit:cover\" src=\"/assets/intro.mp4\"></video>"
           "<button id=\"lgintroskip\" type=\"button\" style=\"position:absolute;bottom:1.25rem;right:1.25rem;"
           "background:rgba(20,18,15,.78);color:#f2ead8;border:1px solid rgba(242,234,216,.3);"
           "border-radius:999px;padding:.5rem 1rem;font:600 .75rem \"Segoe UI\",system-ui,sans-serif;"
           "letter-spacing:.08em;text-transform:uppercase;cursor:pointer\">Skip &middot; shown once</button>"
           "</div>"
           "<script>(function(){"
           "var el=document.getElementById('lgintro');"
           "try{if(localStorage.getItem('liquidgov.intro.seen')){el.remove();return;}}"
           "catch(e){el.remove();return;}"
           "var vid=document.getElementById('lgintrovid'),skip=document.getElementById('lgintroskip');"
           "function finish(){"
           "try{localStorage.setItem('liquidgov.intro.seen','1');}catch(e){}"
           "el.style.transition='opacity .7s ease';el.style.opacity='0';"
           "setTimeout(function(){el.remove();},720);"
           "}"
           "vid.addEventListener('ended',finish);"
           "skip.addEventListener('click',finish);"
           "})();</script>";
}

std::string page_wrap(const std::string& title, const std::string& navhtml, const std::string& body) {
    const std::string theme_attr = g_theme_mode.empty() ? "" : " data-theme=\""+g_theme_mode+"\"";
    const std::string next_theme = g_theme_mode == "dark" ? "light" : "dark";
    const std::string toggle = "<a class=\"themebtn\" href=\"/theme/"+next_theme+
        "\" aria-label=\"Switch to "+next_theme+"\" title=\"Switch to "+next_theme+"\">"+
        (g_theme_mode == "dark" ? "&#9728;" : "&#9789;")+"</a>";
    return "<!doctype html><html lang=\"en\""+theme_attr+"><head><meta charset=\"utf-8\">"
           "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
           "<title>"+html_escape(title)+" — LiquidGov.US</title>"
           "<link rel=\"icon\" href=\"/favicon.svg\" type=\"image/svg+xml\">"
           "<link rel=\"preconnect\" href=\"https://fonts.googleapis.com\">"
           "<link rel=\"preconnect\" href=\"https://fonts.gstatic.com\" crossorigin>"
           "<link rel=\"stylesheet\" href=\"https://fonts.googleapis.com/css2?family=Playfair+Display:wght@400;700&display=swap\">"
           "<style>"+css()+"</style></head><body>"
           +intro_overlay_html()+
           "<header class=\"top\"><div class=\"topinner\">"
           "<a class=\"brand\" href=\"/\">"
           "<svg class=\"mark\" viewBox=\"0 0 48 48\" width=\"28\" height=\"28\" aria-hidden=\"true\">"
           "<circle cx=\"24\" cy=\"24\" r=\"22.5\" fill=\"none\" style=\"stroke:var(--stone)\" stroke-width=\"1\"/>"
           "<circle cx=\"24\" cy=\"24\" r=\"19\" fill=\"none\" style=\"stroke:var(--accent)\" stroke-width=\"1.2\"/>"
           "<path d=\"M24 9c0 0 9.5 12.2 9.5 17.2a9.5 9.5 0 0 1-19 0C14.5 21.2 24 9 24 9z\" style=\"fill:var(--accent)\"/>"
           "</svg>"
           "<div><div><strong>LiquidGov</strong><span class=\"tld\">.US</span></div>"
           "<div class=\"tagline\">Civic association &middot; Beta</div></div></a>"
           +navhtml+toggle+"</div></header>"
           "<main class=\"wrap\">"+body+"</main>"
           "<footer class=\"site\"><a href=\"/\">Genesis</a> · <a href=\"/laws\">Laws</a> · "
           "<a href=\"/control\">Who holds control</a><br>"
           "Not a government. A platform account is not a CitizenId. "
           "LiquidGov.US is in Beta — some of what's here is still being built.</footer></body></html>";
}

std::string nav(const std::string& user, int unread) {
    std::string burger = "<label class=\"navbtn\" for=\"navc\">Menu</label><input id=\"navc\" type=\"checkbox\">";
    if (user.empty()) {
        return burger+"<nav><a href=\"/\">Genesis</a><a href=\"/laws\">Laws</a>"
               "<a href=\"/control\">Who holds control</a>"
               "<a href=\"/signup\">Sign up</a><a href=\"/login\">Log in</a></nav>";
    }
    std::string n = burger+"<nav><a href=\"/feed\">Feed</a>"
                    "<a href=\"/friends\">People</a><a href=\"/mail\">Mail</a>"
                    "<a href=\"/communities\">Groups</a>"
                    "<a href=\"/proposals\">Proposals</a>"
                    "<a href=\"/world\">World</a>"
                    "<a href=\"/notices\">Notices";
    if (unread > 0) n += "<span class=\"badge\">"+std::to_string(unread)+"</span>";
    n += "</a><a href=\"/me\">"+html_escape(user)+"</a>"
         "<a href=\"/settings\">Settings</a><a href=\"/logout\">Log out</a></nav>";
    return n;
}

std::string url_decode(const std::string& s) {
    std::string o;
    for (size_t i=0;i<s.size();++i) {
        if (s[i]=='+') o+=' ';
        else if (s[i]=='%' && i+2<s.size()) { int v=0; std::stringstream ss; ss<<std::hex<<s.substr(i+1,2); ss>>v; o+=char(v); i+=2; }
        else o+=s[i];
    }
    return o;
}
// Percent-encodes for safe embedding in a query string. Needed specifically
// for values that didn't come through our own validation (e.g. a raw login
// username attempt, which -- unlike a stored account's username -- isn't
// guaranteed to be in valid_username's restricted charset).
std::string url_encode(const std::string& s) {
    static const char* kHex = "0123456789ABCDEF";
    std::string o;
    for (unsigned char c : s) {
        if ((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~') o += char(c);
        else { o += '%'; o += kHex[c >> 4]; o += kHex[c & 0xf]; }
    }
    return o;
}
std::map<std::string,std::string> parse_form(const std::string& body) {
    std::map<std::string,std::string> m; std::stringstream ss(body); std::string part;
    while (std::getline(ss,part,'&')) { auto eq=part.find('='); if(eq==std::string::npos) continue;
        m[url_decode(part.substr(0,eq))]=url_decode(part.substr(eq+1)); }
    return m;
}
std::map<std::string,std::string> parse_qs(const std::string& path) {
    std::map<std::string,std::string> m;
    auto q = path.find('?');
    if (q==std::string::npos) return m;
    return parse_form(path.substr(q+1));
}
std::string cookie_of(const std::string& headers) {
    auto pos=headers.find("Cookie:"); if(pos==std::string::npos) pos=headers.find("cookie:");
    if(pos==std::string::npos) return {};
    auto end=headers.find("\r\n",pos); std::string line=headers.substr(pos,end-pos);
    auto k=line.find("lg_session="); if(k==std::string::npos) return {};
    k+=11; auto sc=line.find(';',k); return line.substr(k, sc==std::string::npos?std::string::npos:sc-k);
}
// "dark", "light", or "" -- "" means no explicit choice was ever saved, so
// the page stamps no data-theme attribute at all and prefers-color-scheme
// alone decides (see css()'s dark block). Set once per request (handle()
// is the whole site's single-threaded synchronous entry point, so a
// thread_local read at render time is exactly one request's worth of
// state, never shared or stale across requests).
std::string theme_of(const std::string& headers) {
    auto pos=headers.find("Cookie:"); if(pos==std::string::npos) pos=headers.find("cookie:");
    if(pos==std::string::npos) return {};
    auto end=headers.find("\r\n",pos); std::string line=headers.substr(pos,end-pos);
    auto k=line.find("lg_theme="); if(k==std::string::npos) return {};
    k+=9; auto sc=line.find(';',k);
    std::string v = line.substr(k, sc==std::string::npos?std::string::npos:sc-k);
    return (v=="dark"||v=="light") ? v : std::string{};
}
std::string header_value(const std::string& headers, const std::string& name) {
    std::string key = name + ":";
    auto pos = headers.find(key);
    if (pos == std::string::npos) {
        key = name;
        for (char& c : key) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        pos = headers.find(key + ":");
        if (pos == std::string::npos) return {};
        key += ":";
    }
    pos += key.size();
    while (pos < headers.size() && headers[pos]==' ') ++pos;
    auto end = headers.find("\r\n", pos);
    if (end == std::string::npos) return headers.substr(pos);
    return headers.substr(pos, end-pos);
}
std::string device_label_from(const std::string& headers, const std::string& ip) {
    auto ua = header_value(headers, "User-Agent");
    std::string label;
    if (ua.find("Firefox") != std::string::npos) label = "Firefox";
    else if (ua.find("Edg/") != std::string::npos) label = "Edge";
    else if (ua.find("Chrome") != std::string::npos) label = "Chrome";
    else if (ua.find("Safari") != std::string::npos) label = "Safari";
    else if (ua.find("curl") != std::string::npos) label = "curl";
    else if (!ua.empty()) label = "web";
    else label = "web";
    if (!ip.empty()) label += " · " + ip;
    if (label.size() > 80) label.resize(80);
    return label;
}
std::string fmt_time(std::int64_t t) {
    if (t <= 0) return "";
    std::time_t tt = static_cast<std::time_t>(t);
    std::tm tm{};
    gmtime_r(&tt, &tm);
    char buf[40];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M UTC", &tm);
    return buf;
}

std::optional<PlatformAccount> account_from_req(const std::string& headers) {
    auto sid = cookie_of(headers);
    auto sess=g_store.get_session(sid);
    if(!sess) return std::nullopt;
    g_store.touch_session(sid);
    return g_store.find_by_id(sess->account);
}
std::string http_ok(const std::string& body, const std::string& extra={}) {
    std::ostringstream o;
    o<<"HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\nContent-Length: "<<body.size()<<"\r\n"<<extra<<"\r\n"<<body;
    return o.str();
}
std::string http_raw(const char* status, const char* ctype, const std::string& body, const std::string& extra={}) {
    std::ostringstream o;
    o<<"HTTP/1.1 "<<status<<"\r\nContent-Type: "<<ctype<<"\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\nContent-Length: "<<body.size()<<"\r\n"<<extra<<"\r\n"<<body;
    return o.str();
}
std::string http_code(const char* status, const std::string& body) {
    return http_raw(status, "text/html; charset=utf-8", body);
}
std::string http_redir(const std::string& loc, const std::string& extra={}) {
    std::ostringstream o;
    o<<"HTTP/1.1 303 See Other\r\nLocation: "<<loc<<"\r\nContent-Length: 0\r\nConnection: close\r\n"<<extra<<"\r\n";
    return o.str();
}
std::string set_cookie(const std::string& sid) {
    return "Set-Cookie: lg_session="+sid+"; HttpOnly; Path=/; SameSite=Lax; Max-Age=2592000\r\n";
}
std::string clear_cookie() {
    return "Set-Cookie: lg_session=; HttpOnly; Path=/; SameSite=Lax; Max-Age=0\r\n";
}

std::string trim_ws(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && (s[a]==' '||s[a]=='\t'||s[a]=='\r'||s[a]=='\n')) ++a;
    while (b > a && (s[b-1]==' '||s[b-1]=='\t'||s[b-1]=='\r'||s[b-1]=='\n')) --b;
    return s.substr(a, b-a);
}

std::string header_line_value(const std::string& headers, const char* name) {
    const std::size_t nlen = std::strlen(name);
    std::size_t i = 0;
    while (i < headers.size()) {
        std::size_t e = headers.find('\n', i);
        if (e == std::string::npos) e = headers.size();
        if (e - i >= nlen) {
            bool match = true;
            for (std::size_t k = 0; k < nlen; ++k) {
                char a = headers[i+k], b = name[k];
                if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
                if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
                if (a != b) { match = false; break; }
            }
            if (match && i + nlen < headers.size() && headers[i+nlen] == ':') {
                std::size_t v = i + nlen + 1;
                while (v < e && (headers[v]==' '||headers[v]=='\t')) ++v;
                std::size_t end = e;
                if (end > v && headers[end-1]=='\r') --end;
                return headers.substr(v, end-v);
            }
        }
        i = e + 1;
    }
    return {};
}

bool chunked_complete(const std::string& body) {
    std::size_t i = 0;
    while (i < body.size()) {
        auto nl = body.find("\r\n", i);
        if (nl == std::string::npos) return false;
        std::string sz = body.substr(i, nl-i);
        auto semi = sz.find(';');
        if (semi != std::string::npos) sz = sz.substr(0, semi);
        std::size_t n = 0;
        try { n = std::stoul(sz, nullptr, 16); } catch (...) { return false; }
        i = nl + 2;
        if (n == 0) return true;
        if (i + n + 2 > body.size()) return false;
        i += n + 2;
    }
    return false;
}

std::string decode_chunked(const std::string& body) {
    std::string out;
    std::size_t i = 0;
    while (i < body.size()) {
        auto nl = body.find("\r\n", i);
        if (nl == std::string::npos) break;
        std::string sz = body.substr(i, nl-i);
        auto semi = sz.find(';');
        if (semi != std::string::npos) sz = sz.substr(0, semi);
        std::size_t n = 0;
        try { n = std::stoul(sz, nullptr, 16); } catch (...) { break; }
        i = nl + 2;
        if (n == 0) break;
        if (i + n > body.size()) break;
        out.append(body, i, n);
        i += n;
        if (i + 2 <= body.size()) i += 2;
    }
    return out;
}

bool read_http_request(int fd, std::string& req) {
    req.clear();
    char buf[8192];
    auto more = [&]() -> ssize_t {
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n > 0) req.append(buf, static_cast<std::size_t>(n));
        return n;
    };
    while (req.find("\r\n\r\n") == std::string::npos && req.find("\n\n") == std::string::npos) {
        if (more() <= 0) return !req.empty();
        if (req.size() > 1024 * 1024) return false;
    }
    if (req.find("\r\n\r\n") == std::string::npos) {
        std::string n;
        n.reserve(req.size() + 16);
        for (std::size_t i = 0; i < req.size(); ++i) {
            if (req[i] == '\n' && (i == 0 || req[i-1] != '\r')) n += "\r\n";
            else n += req[i];
        }
        req.swap(n);
    }
    auto sep = req.find("\r\n\r\n");
    if (sep == std::string::npos) return false;
    std::string headers = req.substr(0, sep);
    std::string body = req.substr(sep + 4);

    std::string expect = header_line_value(headers, "Expect");
    std::string te = header_line_value(headers, "Transfer-Encoding");
    std::string cl = header_line_value(headers, "Content-Length");
    for (char& c : te) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    bool chunked = te.find("chunked") != std::string::npos;

    if (!expect.empty() && body.empty()) {
        std::string low = expect;
        for (char& c : low) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        if (low.find("100-continue") != std::string::npos) {
            const char msg[] = "HTTP/1.1 100 Continue\r\n\r\n";
            send(fd, msg, sizeof(msg) - 1, 0);
        }
    }

    if (chunked) {
        while (!chunked_complete(body)) {
            ssize_t n = recv(fd, buf, sizeof(buf), 0);
            if (n <= 0) break;
            body.append(buf, static_cast<std::size_t>(n));
            if (body.size() > kMaxBody) return false;
        }
        body = decode_chunked(body);
        if (body.size() > kMaxBody) return false;
    } else if (!cl.empty()) {
        std::size_t need = 0;
        try { need = static_cast<std::size_t>(std::stoull(cl)); } catch (...) { need = 0; }
        if (need > kMaxBody) return false;
        while (body.size() < need) {
            ssize_t n = recv(fd, buf, sizeof(buf), 0);
            if (n <= 0) break;
            body.append(buf, static_cast<std::size_t>(n));
            if (body.size() > kMaxBody) return false;
        }
        if (body.size() > need) body.resize(need);
    }
    req = headers + "\r\n\r\n" + body;
    return true;
}

std::string global_jukebox_widget(const std::string& yt_key, const std::string& user); // defined below, needed here for wrap()

std::string wrap(const std::string& title, const std::string& user, int unread, const std::string& body) {
    std::string page = page_wrap(title, nav(user, unread), body);
    // Widget goes *after* </main>, not inside it: soft-navigation (see
    // jukebox_script's nav-interception) only ever replaces <main>'s
    // contents, so a sibling widget's live WebSocket/YT player survive a
    // page-to-page click untouched instead of being torn down and rebuilt.
    if (!user.empty()) {
        std::string widget = global_jukebox_widget(youtube_api_key_from_env(), user);
        auto pos = page.find("</main>");
        if (pos != std::string::npos) page.insert(pos + 7, widget);
    }
    return page;
}

std::string trim_copy(std::string s) {
    while (!s.empty() && (s.back()==' '||s.back()=='\t'||s.back()=='"'||s.back()=='\'')) s.pop_back();
    std::size_t i = 0;
    while (i < s.size() && (s[i]==' '||s[i]=='\t'||s[i]=='"'||s[i]=='\'')) ++i;
    return s.substr(i);
}

std::optional<std::vector<std::uint8_t>> multipart_file(const std::string& content_type,
                                                        const std::string& body,
                                                        const char* field) {
    auto bpos = content_type.find("boundary=");
    if (bpos == std::string::npos) return std::nullopt;
    std::string boundary = trim_copy(content_type.substr(bpos + 9));
    auto semi = boundary.find(';');
    if (semi != std::string::npos) boundary.resize(semi);
    boundary = trim_copy(boundary);
    if (boundary.empty()) return std::nullopt;
    const std::string delim = "--" + boundary;
    const std::string needle = std::string("name=\"") + field + "\"";
    std::size_t i = 0;
    while (i < body.size()) {
        auto start = body.find(delim, i);
        if (start == std::string::npos) break;
        start += delim.size();
        if (start + 1 < body.size() && body[start] == '-' && body[start + 1] == '-') break;
        if (start + 1 < body.size() && body[start] == '\r' && body[start + 1] == '\n') start += 2;
        else if (start < body.size() && body[start] == '\n') start += 1;
        auto next = body.find(delim, start);
        if (next == std::string::npos) break;
        std::string part = body.substr(start, next - start);
        while (part.size() >= 2 && part[part.size() - 2] == '\r' && part.back() == '\n')
            part.resize(part.size() - 2);
        auto hdr_end = part.find("\r\n\r\n");
        std::size_t skip = 4;
        if (hdr_end == std::string::npos) { hdr_end = part.find("\n\n"); skip = 2; }
        if (hdr_end == std::string::npos) { i = next; continue; }
        std::string hdrs = part.substr(0, hdr_end);
        if (hdrs.find(needle) == std::string::npos) { i = next; continue; }
        std::string file = part.substr(hdr_end + skip);
        return std::vector<std::uint8_t>(file.begin(), file.end());
    }
    return std::nullopt;
}

// A multipart text field (not a file) is the same wire shape as a file
// part — a header block, blank line, then the value — so multipart_file
// already extracts it correctly; this just gives that a string-typed name
// at call sites that want a form field, not bytes.
std::string multipart_field(const std::string& content_type, const std::string& body,
                             const char* field) {
    auto v = multipart_file(content_type, body, field);
    if (!v) return {};
    return std::string(v->begin(), v->end());
}

// Group rosters and messages key on AccountId, which can outlive the
// account itself (delete_account() does not prune conversations_/
// messages_ — see file_store.cpp). Same "relabel, don't erase" choice
// already used for mail_/posts_.
std::string display_name_for(AccountId id) {
    auto acc = g_store.find_by_id(id);
    if (!acc) return "[deleted account]";
    return acc->username;
}

// No conversation title in the messenger.hpp/conversation.hpp spec — the
// list label is everyone else in the room, so the thread is identifiable
// without inventing a naming feature that isn't there.
std::string group_label(const Conversation& conv, AccountId viewer) {
    std::string label;
    for (const auto& m : conv.members) {
        if (m.account == viewer) continue;
        if (!label.empty()) label += ", ";
        label += display_name_for(m.account);
    }
    return label.empty() ? "(just you)" : label;
}

// Suggested download name only (the browser lets the user rename); no
// original filename is stored, so this is derived from the mime that
// ingest_file actually assigned after content sniffing.
std::string attachment_filename_for_mime(const std::string& mime) {
    if (mime == "image/png") return "attachment.png";
    if (mime == "application/pdf") return "attachment.pdf";
    if (mime.rfind("text/plain", 0) == 0) return "attachment.txt";
    return "attachment";
}

bool has_portrait(AccountId id) {
    auto av = g_store.get(id);
    if (!av) return false;
    return g_blobs.meta(av->blob_id).has_value();
}

std::string portrait_tag(const std::string& username, const char* cls) {
    auto acc = g_store.find_by_username(username);
    if (!acc || !has_portrait(acc->id))
        return std::string("<span class=\"portrait ")+cls+" ph\" aria-hidden=\"true\"></span>";
    return std::string("<img class=\"portrait ")+cls+"\" src=\"/portrait/"+html_escape(username)+
           "\" alt=\"\" width=\"96\" height=\"96\">";
}

// Shared drawing-tool engine: freehand brushes (pen, marker, spray, eraser,
// smudge), shape tools (line, rectangle, circle, with an optional filled
// variant), a paint-bucket flood fill, a rectangular select tool (copy/cut/
// paste, plus crop and resize against the selection), a text tool, up to
// three independently-undoable layers, image/video/gif import, and paste
// of an external image straight from the OS clipboard. Powers three pages
// that share every tool but differ in what "Save" does:
//   portrait=false, post_target empty -- /me/canvas, the profile-background
//     tool. Captures a PNG after every completed action (capped, see
//     kCanvasMaxFrames) by flattening the layers and uploading through
//     /me/canvas/frame -- the exact same normalize/scan pipeline as an
//     avatar upload, just appending to an ordered list instead of
//     replacing a single slot. Save turns that list into the profile
//     theme's background: last frame as a static image, or all of them as
//     a CSS-only looping animation (see theme_background_css) -- no JS
//     runs inside the sandboxed theme document, so the "replay" has to be
//     a real image sequence.
//   portrait=true -- /me/portrait, the self-portrait tool. No frame
//     history needed (an avatar isn't a sequence): Save just flattens the
//     current layers and POSTs the one image straight to /me/avatar.
//   post_target non-empty -- a post composer (/me/post/canvas, /feed/canvas,
//     /c/<slug>/canvas): the canvas itself IS the post's photo/video editor.
//     No frame history either; Save flattens the layers, pairs the image
//     with the caption textarea, and POSTs both as one multipart request
//     straight to post_target (/me/post, /feed, or /c/<slug>) -- the exact
//     same read_composer_post()-backed route a plain file-upload composer
//     already posts to, so no new backend ingestion path was needed for
//     this at all, only a richer way to produce the "media" file first.
// Everything else -- the whole brush/shape/layer/select/import engine -- is
// identical, built once here rather than forked into three copies that
// would drift apart.
std::string canvas_tool_page(const std::string& username, const std::string& err, bool portrait,
                             const std::string& post_target = "") {
    bool post_mode = !post_target.empty();
    int cw = portrait ? 400 : (post_mode ? 640 : 640);
    int ch = portrait ? 400 : (post_mode ? 480 : 440);
    std::string cws = std::to_string(cw), chs = std::to_string(ch);
    std::string aspect = cws + "/" + chs;
    std::string errHtml;
    if (!portrait && !post_mode) {
        if (err=="image") errHtml = "<p class=\"warn\">That frame wasn't a usable image.</p>";
        else if (err=="large") errHtml = "<p class=\"warn\">That frame was too large.</p>";
        else if (err=="full") errHtml = "<p class=\"warn\">This drawing already has the max 24 frames.</p>";
    }
    // Icon buttons, not text labels: each of these already has a symbol
    // recognized from every other editor -- currentColor so they follow
    // the button's own text colour (and the light/dark theme) for free.
    const std::string kIconCopy =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.3\" aria-hidden=\"true\"><rect x=\"5.5\" y=\"5.5\" width=\"8\" height=\"8\" rx=\"1\"/>"
        "<path d=\"M3 10.5V3a1 1 0 0 1 1-1h7.5\"/></svg>";
    const std::string kIconCut =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.3\" aria-hidden=\"true\"><circle cx=\"4\" cy=\"4\" r=\"1.6\"/>"
        "<circle cx=\"4\" cy=\"12\" r=\"1.6\"/><path d=\"M5.2 5.2 13 13M5.2 10.8 13 3\"/></svg>";
    const std::string kIconPaste =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.3\" aria-hidden=\"true\"><rect x=\"3\" y=\"3\" width=\"10\" height=\"11.5\" rx=\"1\"/>"
        "<rect x=\"6\" y=\"1.3\" width=\"4\" height=\"2.4\" rx=\"0.6\"/></svg>";
    const std::string kIconCrop =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.3\" aria-hidden=\"true\"><path d=\"M4 1v10.5a1 1 0 0 0 1 1H15\"/>"
        "<path d=\"M12 15V4.5a1 1 0 0 0-1-1H1\"/></svg>";
    // Same icon on all three layer tabs, deliberately -- a layer tab's job
    // is "switch to this one" and "is this the active one" (shown by the
    // ghost/highlighted background), not "remember which number this is".
    const std::string kIconLayer =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.1\" aria-hidden=\"true\"><path d=\"M8 1 14 4.2 8 7.4 2 4.2Z\"/>"
        "<path d=\"M2 7.6 8 10.8 14 7.6\"/><path d=\"M2 11 8 14.2 14 11\"/></svg>";
    // The tool picker, same reasoning: eleven recognizable shapes read
    // faster than eleven words in a dropdown you have to open first.
    const std::string kIconPen =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.2\" aria-hidden=\"true\"><path d=\"M3 13l.6-2.8L10.6 3.2a1 1 0 0 1 1.4 0l.8.8a1 1 0 0 1 0 1.4"
        "L5.8 12.4 3 13Z\"/><path d=\"M9.3 4.5 11.5 6.7\"/></svg>";
    // A bare thick diagonal line read as just "a thicker Line tool", not
    // "marker" -- a recognizable pen body (the barrel) plus its own ink
    // mark reads as the tool, not the stroke it happens to draw.
    const std::string kIconMarker =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.2\" aria-hidden=\"true\"><rect x=\"2.3\" y=\"2.3\" width=\"4\" height=\"10.5\" rx=\"1\" "
        "transform=\"rotate(40 4.3 7.55)\"/><path d=\"M2.3 13.8h4\" stroke-width=\"2\" stroke-linecap=\"round\"/></svg>";
    const std::string kIconSpray =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.2\" aria-hidden=\"true\"><circle cx=\"5.5\" cy=\"9.5\" r=\"2.2\"/>"
        "<circle cx=\"11\" cy=\"3\" r=\".7\" fill=\"currentColor\" stroke=\"none\"/>"
        "<circle cx=\"13\" cy=\"6\" r=\".7\" fill=\"currentColor\" stroke=\"none\"/>"
        "<circle cx=\"9.5\" cy=\"3.5\" r=\".7\" fill=\"currentColor\" stroke=\"none\"/>"
        "<circle cx=\"11.5\" cy=\"8.5\" r=\".7\" fill=\"currentColor\" stroke=\"none\"/></svg>";
    // A single wavy line just read as "squiggle," not "blend/drag" --
    // three parallel strokes fading out reads as a motion trail, the same
    // visual shorthand smudge/blur tools use elsewhere.
    const std::string kIconSmudge =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.4\" stroke-linecap=\"round\" aria-hidden=\"true\"><path d=\"M3.5 12.5 7.5 8.5\"/>"
        "<path d=\"M6 13.5 10 9.5\" opacity=\".6\"/><path d=\"M8.5 14.5 12.5 10.5\" opacity=\".3\"/></svg>";
    const std::string kIconEraser =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.2\" aria-hidden=\"true\">"
        "<rect x=\"3.3\" y=\"6.3\" width=\"9.4\" height=\"5.4\" rx=\"1\" transform=\"rotate(-28 8 9)\"/></svg>";
    const std::string kIconLine =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.3\" stroke-linecap=\"round\" aria-hidden=\"true\"><path d=\"M2.5 13.5 13.5 2.5\"/></svg>";
    const std::string kIconRect =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.3\" aria-hidden=\"true\"><rect x=\"2.5\" y=\"3.5\" width=\"11\" height=\"9\" rx=\"1\"/></svg>";
    const std::string kIconCircle =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.3\" aria-hidden=\"true\"><circle cx=\"8\" cy=\"8\" r=\"5.3\"/></svg>";
    // The old abstract polygon-plus-drip didn't read as a bucket at 16px --
    // a real trapezoid body with a rim and a single drip is the shape
    // paint-bucket icons actually use everywhere else.
    // The bucket's body and drip are filled with the actual selected
    // colour (kept in sync by ccolor's own input listener below), not a
    // static outline -- so the tool icon itself shows what it's about to
    // pour, the way a real paint bucket would.
    const std::string kIconBucket =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.2\" aria-hidden=\"true\"><path id=\"bucketpaint\" fill=\"#1a1814\" "
        "d=\"M3 5h8.5l-1.1 6.3a1 1 0 0 1-1 .8H5.1a1 1 0 0 1-1-.8L3 5Z\"/>"
        "<path d=\"M2.5 5 4.3 2h5.4l1.8 3\"/>"
        "<circle id=\"bucketdrip\" cx=\"13\" cy=\"12.2\" r=\"1.2\" fill=\"#1a1814\" stroke=\"none\"/></svg>";
    const std::string kIconSelect =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.3\" stroke-dasharray=\"2.3 2\" aria-hidden=\"true\">"
        "<rect x=\"2.5\" y=\"2.5\" width=\"11\" height=\"11\" rx=\"1\"/></svg>";
    const std::string kIconText =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.3\" stroke-linecap=\"round\" aria-hidden=\"true\">"
        "<path d=\"M2.8 3.5h10.4\"/><path d=\"M8 3.5v9\"/></svg>";
    // The standard "highlight colour" glyph everywhere from Word to Google
    // Docs: a capital A with a solid colour bar underneath it.
    const std::string kIconHighlight =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.2\" aria-hidden=\"true\"><path d=\"M4 10.5 7 3h2l3 7.5\"/><path d=\"M5.1 8h5.8\"/>"
        "<rect x=\"2.5\" y=\"12.3\" width=\"11\" height=\"1.8\" fill=\"currentColor\" stroke=\"none\"/></svg>";
    // One dial glyph, reused at two sizes for the size/opacity wheels below
    // -- a bigger circle reads as "coarser steps" and a smaller one as
    // "finer steps" on its own, without needing a second icon design to
    // tell them apart (and risk being just as unclear as an abstract pair
    // would be).
    const std::string kIconDial =
        "<svg viewBox=\"0 0 16 16\" width=\"100%\" height=\"100%\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.2\" aria-hidden=\"true\"><circle cx=\"8\" cy=\"8\" r=\"6.5\"/><path d=\"M8 2.5v3\"/></svg>";
    // The standard diagonal-dot resize grip every native textarea already
    // uses in its own bottom-right corner.
    const std::string kIconResizeGrip =
        "<svg viewBox=\"0 0 16 16\" width=\"100%\" height=\"100%\" fill=\"currentColor\" aria-hidden=\"true\">"
        "<circle cx=\"12\" cy=\"4\" r=\"1.1\"/><circle cx=\"12\" cy=\"8\" r=\"1.1\"/><circle cx=\"12\" cy=\"12\" r=\"1.1\"/>"
        "<circle cx=\"8\" cy=\"8\" r=\"1.1\"/><circle cx=\"8\" cy=\"12\" r=\"1.1\"/><circle cx=\"4\" cy=\"12\" r=\"1.1\"/></svg>";
    // Glow/Mirror/Fill were the last checkbox-plus-label modifiers left --
    // converted to the same toggle-button pattern as Bold/Italic/Underline
    // now, for the same reason: faster to scan as a row of symbols than a
    // row of checkboxes each needing its own word read next to it.
    const std::string kIconGlow =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.2\" aria-hidden=\"true\"><path d=\"M8 1.5v2.4M8 12.1v2.4M1.5 8h2.4M12.1 8h2.4\"/>"
        "<path d=\"M3.8 3.8l1.7 1.7M10.5 10.5l1.7 1.7M12.2 3.8l-1.7 1.7M5.5 10.5l-1.7 1.7\"/>"
        "<circle cx=\"8\" cy=\"8\" r=\"1.8\"/></svg>";
    const std::string kIconMirror =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.2\" aria-hidden=\"true\"><path d=\"M8 1v14\" stroke-dasharray=\"2 1.6\"/>"
        "<path d=\"M4 5 2 8l2 3M12 5l2 3-2 3\"/></svg>";
    const std::string kIconFillShape =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"currentColor\" aria-hidden=\"true\">"
        "<rect x=\"3\" y=\"3\" width=\"10\" height=\"10\" rx=\"1.5\"/></svg>";
    std::string title = portrait ? "Self-portrait" : (post_mode ? "Compose a post" : "Canvas");
    std::string intro = portrait
        ? "Draw your self-portrait with the same tools as the profile-background canvas &mdash; "
          "pen, shapes, layers, all of it."
        : post_mode
        ? "Draw, crop, layer, paste in a photo, or drop text on it &mdash; then post it, caption included."
        : "Draw, then save it as your profile background &mdash; a still image, "
          "or a loop of the drawing as it happened.";
    std::string html =
        "<div class=\"card\"><h1>"+title+"</h1>"
        "<p class=\"muted\">"+intro+"</p>"+errHtml+
        // Everything about "what am I about to draw with" lives in one
        // bounded panel -- tool, then that tool's own properties, then
        // (only for Text) that tool's extra properties -- separate from
        // the panel below it for "what can I do to the canvas as a
        // whole", and separate again from the canvas itself. Three
        // questions, three visually distinct answers, instead of one
        // long run of rows a reader has to parse to find which control
        // belongs to which question.
        // One frame around the whole workspace, not a separate panel
        // floating above the canvas: at desktop widths (see .canvas-frame
        // in css()) this becomes a real toolbox-and-canvas layout -- tools
        // in a column down the left, properties in a strip across the
        // top, canvas and its layer tabs filling the rest -- the same
        // wrap-the-canvas shape a real paint app uses, built as five grid
        // areas placed by CSS rather than by moving any of this markup;
        // below that width it's simply the original stacked column, no
        // media query applies at all.
        "<div class=\"card canvas-frame\">"
        // Three clusters, not one undifferentiated run of eleven icons:
        // freehand media, then shapes, then whole-canvas/utility tools --
        // each its own visual group with a hairline divider between,
        // read the same way a real paint app's toolbar groups by kind.
        "<div class=\"row ctools\" style=\"flex-wrap:wrap;gap:.3rem;align-items:center;margin-bottom:.4rem\">"
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"pen\" title=\"Pen\" "
        "style=\"padding:.5rem .55rem\">"+kIconPen+"</button>"
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"marker\" title=\"Marker\" "
        "style=\"padding:.5rem .55rem\">"+kIconMarker+"</button>"
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"spray\" title=\"Spray\" "
        "style=\"padding:.5rem .55rem\">"+kIconSpray+"</button>"
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"smudge\" title=\"Smudge\" "
        "style=\"padding:.5rem .55rem\">"+kIconSmudge+"</button>"
        // Eraser gets a colour of its own -- pink, not currentColor --
        // so it reads as visually distinct from the ink-laying tools
        // around it at a glance, not just by its shape.
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"eraser\" title=\"Eraser\" "
        "style=\"padding:.5rem .55rem;color:#ec6fa0\">"+kIconEraser+"</button>"
        "<span class=\"toolgroup-sep\" aria-hidden=\"true\"></span>"
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"line\" title=\"Line\" "
        "style=\"padding:.5rem .55rem\">"+kIconLine+"</button>"
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"rect\" title=\"Rectangle\" "
        "style=\"padding:.5rem .55rem\">"+kIconRect+"</button>"
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"circle\" title=\"Circle\" "
        "style=\"padding:.5rem .55rem\">"+kIconCircle+"</button>"
        "<span class=\"toolgroup-sep\" aria-hidden=\"true\"></span>"
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"bucket\" title=\"Paint bucket\" "
        "style=\"padding:.5rem .55rem\">"+kIconBucket+"</button>"
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"select\" title=\"Select\" "
        "style=\"padding:.5rem .55rem\">"+kIconSelect+"</button>"
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"text\" title=\"Text\" "
        "style=\"padding:.5rem .55rem\">"+kIconText+"</button>"
        "</div>"
        // The three property/modifier/text rows below share one wrapping
        // div, not separate grid placements -- multiple items placed at
        // the exact same grid-row/grid-column overlap each other rather
        // than stacking (that's what actually broke the first version of
        // this layout, live), so they need to be one single grid item
        // that stacks its own children by normal block flow internally.
        "<div class=\"cprops\">"
        "<div class=\"row\" style=\"flex-wrap:wrap;gap:.4rem 1rem;align-items:center;margin-bottom:.4rem\">"
        "<input type=\"color\" id=\"ccolor\" value=\"#1a1814\" aria-label=\"Color\" "
        "style=\"width:36px;min-height:36px;padding:2px;flex:0 0 auto\">"
        // Coarse/fine wheels instead of a single slider: drag one
        // vertically, or scroll it, and it nudges the number beside it --
        // the bigger circle moves in bigger steps, the smaller one in
        // smaller steps, same size-implies-precision logic as a real
        // jog/shuttle pair, so which is which never needs explaining.
        "<label>Size</label>"
        "<div class=\"row\" style=\"gap:.3rem;align-items:center\">"
        "<div class=\"wheel\" id=\"csizec\" data-target=\"csize\" data-step=\"5\" title=\"Size, coarse (drag or scroll, &plusmn;5)\" "
        "style=\"width:30px;height:30px;border-radius:50%;border:1px solid var(--rule);cursor:ns-resize;"
        "touch-action:none;flex:0 0 auto\">"+kIconDial+"</div>"
        "<div class=\"wheel\" id=\"csizef\" data-target=\"csize\" data-step=\"1\" title=\"Size, fine (drag or scroll, &plusmn;1)\" "
        "style=\"width:20px;height:20px;border-radius:50%;border:1px solid var(--rule);cursor:ns-resize;"
        "touch-action:none;flex:0 0 auto\">"+kIconDial+"</div>"
        "<input type=\"number\" id=\"csize\" min=\"1\" max=\"48\" value=\"8\" style=\"width:4rem\">"
        "</div>"
        "<label>Opacity</label>"
        "<div class=\"row\" style=\"gap:.3rem;align-items:center\">"
        "<div class=\"wheel\" id=\"copacityc\" data-target=\"copacity\" data-step=\"20\" "
        "title=\"Opacity, coarse (drag or scroll, &plusmn;20)\" "
        "style=\"width:30px;height:30px;border-radius:50%;border:1px solid var(--rule);cursor:ns-resize;"
        "touch-action:none;flex:0 0 auto\">"+kIconDial+"</div>"
        "<div class=\"wheel\" id=\"copacityf\" data-target=\"copacity\" data-step=\"5\" "
        "title=\"Opacity, fine (drag or scroll, &plusmn;5)\" "
        "style=\"width:20px;height:20px;border-radius:50%;border:1px solid var(--rule);cursor:ns-resize;"
        "touch-action:none;flex:0 0 auto\">"+kIconDial+"</div>"
        "<input type=\"number\" id=\"copacity\" min=\"5\" max=\"100\" value=\"100\" style=\"width:4rem\">"
        "</div>"
        // Glow/Mirror/Fill are stroke modifiers, not stroke properties,
        // but still belong in this same single strip -- a second row here
        // would make the wrap two rows thick instead of one.
        "<span class=\"toolgroup-sep\" aria-hidden=\"true\"></span>"
        "<button type=\"button\" class=\"ghost fmtbtn\" id=\"cglow\" data-on=\"0\" title=\"Glow\" "
        "style=\"padding:.5rem .55rem\">"+kIconGlow+"</button>"
        "<button type=\"button\" class=\"ghost fmtbtn\" id=\"csymmetry\" data-on=\"0\" title=\"Mirror\" "
        "style=\"padding:.5rem .55rem\">"+kIconMirror+"</button>"
        "<button type=\"button\" class=\"ghost fmtbtn\" id=\"cfill\" data-on=\"0\" title=\"Fill shape\" "
        "style=\"padding:.5rem .55rem\">"+kIconFillShape+"</button>"
        "</div>"
        // Text tool controls -- font/weight/style/underline/background all
        // apply at commit time (see addTextBox), read the same way colour
        // and size already are for every other tool. Drag the Text tool
        // itself on the canvas to size the box: vertical drag sets the
        // font size, horizontal sets the field width text wraps within.
        // Only meaningful for one of eleven tools, so this whole row stays
        // hidden (see refreshToolBtns) until the Text tool is actually
        // selected, instead of cluttering every other tool's controls.
        // Bold/Italic/Underline are their own established symbols --
        // literally the styled B/I/U every text editor uses -- so they're
        // toggle buttons showing that styling directly, not checkboxes
        // with a redundant word next to them; Background gets the
        // standard highlight-colour glyph for the same reason.
        "<div class=\"row\" id=\"ctextrow\" hidden "
        "style=\"flex-wrap:wrap;gap:.4rem 1rem;align-items:center;margin-bottom:.6rem\">"
        "<select id=\"cfont\" style=\"width:auto;min-height:36px\">"
        "<option value=\"sans-serif\">Sans-serif</option>"
        "<option value=\"serif\">Serif</option>"
        "<option value=\"monospace\">Monospace</option>"
        "<option value=\"cursive\">Cursive</option>"
        "</select>"
        "<div class=\"row\" style=\"gap:.3rem\">"
        "<button type=\"button\" class=\"ghost fmtbtn\" id=\"cbold\" data-on=\"0\" title=\"Bold\" "
        "style=\"padding:.5rem .75rem;font-weight:bold\">B</button>"
        "<button type=\"button\" class=\"ghost fmtbtn\" id=\"citalic\" data-on=\"0\" title=\"Italic\" "
        "style=\"padding:.5rem .75rem;font-style:italic\">I</button>"
        "<button type=\"button\" class=\"ghost fmtbtn\" id=\"cunderline\" data-on=\"0\" title=\"Underline\" "
        "style=\"padding:.5rem .75rem;text-decoration:underline\">U</button>"
        "<button type=\"button\" class=\"ghost fmtbtn\" id=\"ctextbg\" data-on=\"0\" title=\"Text background\" "
        "style=\"padding:.5rem .6rem\">"+kIconHighlight+"</button>"
        "<input type=\"color\" id=\"ctextbgcolor\" value=\"#ffffff\" aria-label=\"Text background colour\" "
        "style=\"width:36px;min-height:36px;padding:2px;flex:0 0 auto\">"
        "</div>"
        "</div>"
        "</div>"
        // Canvas, with its layer tabs on the right, as the fourth and
        // final area of .canvas-frame -- filling the rest of the frame
        // beside/under the tool column and properties strip above. No
        // numbers on the layer tabs themselves: switching to one and
        // seeing which is active (the highlighted background) is all a
        // tab needs to communicate, and all three do the same job -- a
        // number would just be one more thing to read, not new
        // information. Each still gets its own opacity slider, a real
        // per-layer property applied both live (CSS opacity on that
        // layer's own <canvas>) and in flatten() so a saved/posted image
        // matches what's on screen.
        "<div class=\"ccanvasrow\" style=\"display:flex;gap:.5rem;align-items:flex-start\">"
        "<div id=\"ccanvaswrap\" style=\"position:relative;flex:1;min-width:0;max-width:"+cws+"px;"
        "aspect-ratio:"+aspect+";background:#fff;border:1px solid var(--rule);"
        "border-radius:var(--rsm);overflow:hidden;touch-action:none\">"
        "<canvas id=\"clayer0\" width=\""+cws+"\" height=\""+chs+"\" "
        "style=\"position:absolute;inset:0;width:100%;height:100%\"></canvas>"
        "<canvas id=\"clayer1\" width=\""+cws+"\" height=\""+chs+"\" "
        "style=\"position:absolute;inset:0;width:100%;height:100%\"></canvas>"
        "<canvas id=\"clayer2\" width=\""+cws+"\" height=\""+chs+"\" "
        "style=\"position:absolute;inset:0;width:100%;height:100%\"></canvas>"
        "<canvas id=\"coverlay\" width=\""+cws+"\" height=\""+chs+"\" "
        "style=\"position:absolute;inset:0;width:100%;height:100%;cursor:crosshair\"></canvas>"
        "<div id=\"cresizehandle\" title=\"Drag to resize the canvas\" "
        "style=\"position:absolute;right:0;bottom:0;width:16px;height:16px;padding:2px;"
        "cursor:nwse-resize;z-index:11;touch-action:none;color:var(--stone)\">"+kIconResizeGrip+"</div>"
        "</div>"
        "<div style=\"display:flex;flex-direction:column;gap:.6rem;width:2.6rem\">"
        "<div style=\"display:flex;flex-direction:column;gap:.15rem;align-items:center\">"
        "<button type=\"button\" class=\"ghost layerbtn\" data-layer=\"0\" title=\"Switch to layer 1\" "
        "style=\"width:2.4rem;padding:.5rem 0\">"+kIconLayer+"</button>"
        "<input type=\"range\" class=\"layeropacity\" data-layer=\"0\" min=\"0\" max=\"100\" value=\"100\" "
        "title=\"Layer 1 opacity\" style=\"width:2.4rem\">"
        "</div>"
        "<div style=\"display:flex;flex-direction:column;gap:.15rem;align-items:center\">"
        "<button type=\"button\" class=\"ghost layerbtn\" data-layer=\"1\" title=\"Switch to layer 2\" "
        "style=\"width:2.4rem;padding:.5rem 0\">"+kIconLayer+"</button>"
        "<input type=\"range\" class=\"layeropacity\" data-layer=\"1\" min=\"0\" max=\"100\" value=\"100\" "
        "title=\"Layer 2 opacity\" style=\"width:2.4rem\">"
        "</div>"
        "<div style=\"display:flex;flex-direction:column;gap:.15rem;align-items:center\">"
        "<button type=\"button\" class=\"ghost layerbtn\" data-layer=\"2\" title=\"Switch to layer 3\" "
        "style=\"width:2.4rem;padding:.5rem 0\">"+kIconLayer+"</button>"
        "<input type=\"range\" class=\"layeropacity\" data-layer=\"2\" min=\"0\" max=\"100\" value=\"100\" "
        "title=\"Layer 3 opacity\" style=\"width:2.4rem\">"
        "</div>"
        "</div>"
        "</div>"
        "</div>"
        // Second panel, one clear step down in how often it gets touched:
        // bring something in, or act on a selection -- both operate on the
        // canvas as a whole rather than being "what the next stroke looks
        // like", which is why it's not part of the wrap-the-canvas frame
        // above.
        "<div class=\"card\">"
        "<div class=\"row\" style=\"flex-wrap:wrap;gap:.4rem 1rem;align-items:center\">"
        "<label class=\"ghost\" style=\"cursor:pointer;padding:.3rem .6rem;border-radius:var(--rsm)\">"
        "Import image/video<input type=\"file\" id=\"cimport\" accept=\"image/*,video/*\" "
        "style=\"display:none\"></label>"
        // Drag a rectangle with the Select tool first -- these four
        // buttons all act on that same selection (copy/cut sample it,
        // paste drops the last-copied piece at your next click, crop
        // trims the whole canvas down to it). Icons, not labels: each of
        // these already has a symbol everyone recognizes from every other
        // editor (clipboard stack, scissors, crop corners) -- a text
        // label would just be slower to scan than the glyph itself.
        "<div class=\"row\" style=\"gap:.3rem\">"
        "<button type=\"button\" class=\"ghost\" id=\"ccopy\" title=\"Copy selection\" "
        "style=\"padding:.5rem .55rem\">"+kIconCopy+"</button>"
        "<button type=\"button\" class=\"ghost\" id=\"ccut\" title=\"Cut selection\" "
        "style=\"padding:.5rem .55rem\">"+kIconCut+"</button>"
        "<button type=\"button\" class=\"ghost\" id=\"cpaste\" title=\"Paste\" "
        "style=\"padding:.5rem .55rem\">"+kIconPaste+"</button>"
        "<button type=\"button\" class=\"ghost\" id=\"ccrop\" title=\"Apply crop to selection\" "
        "style=\"padding:.5rem .55rem\">"+kIconCrop+"</button>"
        "</div>"
        "</div>"
        "<p class=\"muted\" style=\"margin:.5rem 0 0\">Ctrl+V also pastes an image straight from your "
        "OS clipboard onto the active layer.</p>"
        "</div>"
        // A starting texture under whatever gets drawn, the way real paper
        // has a grain before any ink touches it -- four options, Plain
        // (today's blank default) among them, exactly one active at a
        // time like the tool row's own .toolbtn pattern. Lives on
        // #ccanvaswrap's own CSS background (layers stay untouched, fully
        // transparent as always), so it's visible while editing and
        // switchable freely, but NOT yet part of flatten()'s output: a
        // save/post still composites the transparent layers onto a plain
        // white backing, same as before this control existed. Baking a
        // choice into the actual saved pixels is a separate, larger change
        // (drawing real texture onto layer 0 at canvas-open time) --
        // this is deliberately just the live, switchable preview of the idea.
        "<div class=\"row\" style=\"margin-bottom:.5rem;align-items:center;gap:.4rem;flex-wrap:wrap\">"
        "<label class=\"muted\" style=\"font-size:.8rem\">Canvas background</label>"
        "<button type=\"button\" class=\"ghost cbgbtn\" data-bg=\"hatch\">Hatch</button>"
        "<button type=\"button\" class=\"ghost cbgbtn\" data-bg=\"plain\" data-on=\"1\">Plain</button>"
        "<button type=\"button\" class=\"ghost cbgbtn\" data-bg=\"wash\">Wash</button>"
        "<button type=\"button\" class=\"ghost cbgbtn\" data-bg=\"ruled\">Ruled</button>"
        "</div>"
        "<div class=\"row\" style=\"margin-top:.6rem;gap:.4rem\">"
        "<button type=\"button\" class=\"ghost\" id=\"cundo\" title=\"Undo\">&#8617;</button>"
        "<button type=\"button\" class=\"ghost\" id=\"credo\" title=\"Redo\">&#8618;</button>"
        "<button type=\"button\" class=\"ghost\" id=\"cclear\">Clear layer</button>"
        "</div>";
    if (!portrait && !post_mode) {
        html +=
            "<p class=\"muted\" id=\"cframecount\" style=\"margin-top:.6rem\">0/24 frames captured</p>"
            "<label style=\"display:block;margin-top:.4rem\"><input type=\"checkbox\" id=\"ctimelapse\"> "
            "Loop the drawing as my background instead of a still image</label>";
    }
    if (post_mode) {
        html +=
            "<label style=\"display:block;margin-top:.6rem\">Caption</label>"
            "<textarea id=\"cpcaption\" maxlength=\"2000\" placeholder=\"What's on your mind?\"></textarea>";
    }
    std::string saveLabel = portrait ? "Save as self-portrait" : (post_mode ? "Post" : "Save to profile");
    html +=
        "<div class=\"row\" style=\"margin-top:.6rem\">"
        "<button type=\"button\" id=\"csave\">"+saveLabel+"</button>"
        "</div>"
        "<p class=\"muted\" id=\"cstatus\"></p></div>"
        "<script>(function(){"
        "var LAYN=3,layers=[];"
        "for(var li=0;li<LAYN;li++){"
        "var lc=document.getElementById('clayer'+li);"
        "layers.push({cv:lc,ctx:lc.getContext('2d'),undo:[],redo:[],opacity:1});"
        "}"
        "document.querySelectorAll('.layeropacity').forEach(function(s){"
        "s.addEventListener('input',function(){"
        "var L=layers[Number(s.dataset.layer)];"
        "L.opacity=Number(s.value)/100;L.cv.style.opacity=L.opacity;"
        "});"
        "});"
        "var CBG={"
        "plain:'#fff',"
        "wash:\"radial-gradient(circle at 30% 35%,rgba(201,168,106,.35) 0%,rgba(201,168,106,0) 65%),\""
        "+\"radial-gradient(circle at 75% 70%,rgba(162,130,59,.28) 0%,rgba(162,130,59,0) 55%),\""
        "+\"radial-gradient(circle at 60% 15%,rgba(110,59,162,.10) 0%,rgba(110,59,162,0) 45%),#f7f1e4\","
        "hatch:\"repeating-linear-gradient(45deg,rgba(26,24,20,.16) 0,rgba(26,24,20,.16) 1px,transparent 1px,transparent 9px),#f7f1e4\","
        "ruled:\"repeating-linear-gradient(to bottom,transparent 0,transparent 17px,rgba(26,24,20,.22) 17px,rgba(26,24,20,.22) 18px),#f7f1e4\""
        "};"
        "var cbgBtns=document.querySelectorAll('.cbgbtn');"
        "cbgBtns.forEach(function(b){"
        "b.addEventListener('click',function(){"
        "cbgBtns.forEach(function(o){o.dataset.on='0';o.style.background='';});"
        "b.dataset.on='1';b.style.background='var(--rule)';"
        "document.getElementById('ccanvaswrap').style.background=CBG[b.dataset.bg];"
        "});"
        "});"
        "cbgBtns.forEach(function(b){if(b.dataset.on==='1')b.style.background='var(--rule)';});"
        "var activeLayer=0;"
        "function acv(){return layers[activeLayer].cv;}"
        "function actx(){return layers[activeLayer].ctx;}"
        "var overlay=document.getElementById('coverlay'),octx=overlay.getContext('2d');"
        "function clearOverlay(){octx.clearRect(0,0,overlay.width,overlay.height);}"
        "var drawing=false,lastX=0,lastY=0,shapeStart=null;"
        "function snap(){"
        "var L=layers[activeLayer];"
        "L.undo.push(L.ctx.getImageData(0,0,L.cv.width,L.cv.height));"
        "if(L.undo.length>20)L.undo.shift();"
        "L.redo=[];"
        "}"
        "function pos(e){"
        "var r=overlay.getBoundingClientRect();var p=e.touches?e.touches[0]:e;"
        "return{x:(p.clientX-r.left)*overlay.width/r.width,y:(p.clientY-r.top)*overlay.height/r.height};"
        "}"
        "var curTool='pen';"
        "function toolVal(){return curTool;}"
        // The bucket icon's own fill tracks whatever colour is loaded,
        // live -- both on page load (the input's own default) and on
        // every change, so the tool always shows what it's about to pour.
        "function syncBucketColor(){"
        "var c=document.getElementById('ccolor').value;"
        "var p=document.getElementById('bucketpaint'),d=document.getElementById('bucketdrip');"
        "if(p)p.setAttribute('fill',c);if(d)d.setAttribute('fill',c);"
        "}"
        "document.getElementById('ccolor').addEventListener('input',syncBucketColor);"
        "syncBucketColor();"
        "var toolBtns=document.querySelectorAll('.toolbtn');"
        "function refreshToolBtns(){"
        "toolBtns.forEach(function(b){b.style.background=b.dataset.tool===curTool?'var(--rule)':'';});"
        // .row{display:flex} outranks the browser's default [hidden]{display:
        // none} in the cascade (same specificity, .row comes later in the
        // stylesheet), so the hidden attribute alone silently does nothing
        // on an element that also has class="row" -- toggling style.display
        // directly sidesteps that instead of fighting the cascade.
        "document.getElementById('ctextrow').style.display=curTool==='text'?'flex':'none';"
        "}"
        "toolBtns.forEach(function(b){"
        "b.addEventListener('click',function(){curTool=b.dataset.tool;refreshToolBtns();});"
        "});"
        "refreshToolBtns();"
        "document.querySelectorAll('.fmtbtn').forEach(function(b){"
        "b.addEventListener('click',function(){"
        "var on=b.dataset.on==='1';b.dataset.on=on?'0':'1';b.style.background=on?'':'var(--rule)';"
        "});"
        "});"
        "function drawSeg(ax,ay,bx,by){"
        "var b=toolVal(),color=document.getElementById('ccolor').value;"
        "var size=Number(document.getElementById('csize').value);"
        "var opacity=Number(document.getElementById('copacity').value)/100;"
        "var glow=document.getElementById('cglow').dataset.on==='1';"
        "var cx=actx();"
        "cx.save();"
        "if(glow){cx.shadowColor=color;cx.shadowBlur=size*1.4;}"
        "if(b==='eraser'){"
        "cx.globalCompositeOperation='destination-out';cx.globalAlpha=1;"
        "cx.lineCap='round';cx.lineJoin='round';cx.lineWidth=size*2;"
        "cx.beginPath();cx.moveTo(ax,ay);cx.lineTo(bx,by);cx.stroke();"
        "}else if(b==='spray'){"
        "cx.globalAlpha=opacity;cx.fillStyle=color;"
        "var n=Math.max(6,Math.floor(size));"
        "for(var i=0;i<n;i++){"
        "var ang=Math.random()*Math.PI*2,rad=Math.random()*size;"
        "cx.beginPath();cx.arc(bx+Math.cos(ang)*rad,by+Math.sin(ang)*rad,1,0,Math.PI*2);cx.fill();"
        "}"
        "}else{"
        "cx.lineCap='round';cx.lineJoin='round';cx.strokeStyle=color;"
        "if(b==='marker'){cx.lineWidth=size*1.6;cx.globalAlpha=Math.min(opacity,.45);}"
        "else{cx.lineWidth=size;cx.globalAlpha=opacity;}"
        "cx.beginPath();cx.moveTo(ax,ay);cx.lineTo(bx,by);cx.stroke();"
        "}"
        "cx.restore();"
        "}"
        "function strokeAt(x0,y0,x1,y1){"
        "drawSeg(x0,y0,x1,y1);"
        "if(document.getElementById('csymmetry').dataset.on==='1')"
        "drawSeg(acv().width-x0,y0,acv().width-x1,y1);"
        "}"
        // Shared by the live drag preview (drawn onto the overlay) and the
        // final commit (drawn onto the active layer) -- same geometry
        // either way, only the target context differs.
        "function drawShape(x0,y0,x1,y1,targetCtx){"
        "var t=toolVal(),color=document.getElementById('ccolor').value;"
        "var size=Number(document.getElementById('csize').value);"
        "var opacity=Number(document.getElementById('copacity').value)/100;"
        "var fill=document.getElementById('cfill').dataset.on==='1';"
        "var glow=document.getElementById('cglow').dataset.on==='1';"
        "targetCtx.save();"
        "targetCtx.globalAlpha=opacity;targetCtx.strokeStyle=color;targetCtx.fillStyle=color;"
        "targetCtx.lineWidth=size;targetCtx.lineCap='round';targetCtx.lineJoin='round';"
        "if(glow){targetCtx.shadowColor=color;targetCtx.shadowBlur=size*1.4;}"
        "if(t==='line'){"
        "targetCtx.beginPath();targetCtx.moveTo(x0,y0);targetCtx.lineTo(x1,y1);targetCtx.stroke();"
        "}else if(t==='rect'){"
        "var rx=Math.min(x0,x1),ry=Math.min(y0,y1),rw=Math.abs(x1-x0),rh=Math.abs(y1-y0);"
        "if(fill)targetCtx.fillRect(rx,ry,rw,rh);else targetCtx.strokeRect(rx,ry,rw,rh);"
        "}else if(t==='circle'){"
        "var mx=(x0+x1)/2,my=(y0+y1)/2,rxr=Math.abs(x1-x0)/2,ryr=Math.abs(y1-y0)/2;"
        "targetCtx.beginPath();targetCtx.ellipse(mx,my,rxr,ryr,0,0,Math.PI*2);"
        "if(fill)targetCtx.fill();else targetCtx.stroke();"
        "}"
        "targetCtx.restore();"
        "}"
        // Iterative 4-way flood fill on the active layer only, with a small
        // colour-distance tolerance so anti-aliased stroke edges don't
        // leave a ring of unfilled pixels around the bucket click.
        "function floodFill(fx,fy){"
        "var c=acv(),cx=actx();fx=Math.floor(fx);fy=Math.floor(fy);"
        "if(fx<0||fy<0||fx>=c.width||fy>=c.height)return;"
        "var img=cx.getImageData(0,0,c.width,c.height),data=img.data,w=c.width,h=c.height;"
        "var startI=(fy*w+fx)*4;"
        "var tr=data[startI],tg=data[startI+1],tb=data[startI+2],ta=data[startI+3];"
        "var hex=document.getElementById('ccolor').value;"
        "var opacity=Number(document.getElementById('copacity').value)/100;"
        "var fr=parseInt(hex.slice(1,3),16),fg=parseInt(hex.slice(3,5),16),fb=parseInt(hex.slice(5,7),16);"
        "var fa=Math.round(opacity*255);"
        "if(tr===fr&&tg===fg&&tb===fb&&ta===fa)return;"
        "var tol=32,tol2=tol*tol*4;"
        "function matches(i){"
        "var dr=data[i]-tr,dg=data[i+1]-tg,db=data[i+2]-tb,da=data[i+3]-ta;"
        "return dr*dr+dg*dg+db*db+da*da<=tol2;"
        "}"
        "var visited=new Uint8Array(w*h);visited[fy*w+fx]=1;"
        "var stack=[startI];"
        "while(stack.length){"
        "var i=stack.pop();data[i]=fr;data[i+1]=fg;data[i+2]=fb;data[i+3]=fa;"
        "var px=(i/4)|0,x=px%w,y=(px/w)|0;"
        "var nbrs=[[x-1,y],[x+1,y],[x,y-1],[x,y+1]];"
        "for(var k=0;k<4;k++){"
        "var nx=nbrs[k][0],ny=nbrs[k][1];"
        "if(nx<0||ny<0||nx>=w||ny>=h)continue;"
        "var vi=ny*w+nx;if(visited[vi])continue;"
        "var ni=vi*4;"
        "if(matches(ni)){visited[vi]=1;stack.push(ni);}"
        "}"
        "}"
        "cx.putImageData(img,0,0);"
        "}"
        // Smudge: sample a small patch of the active layer around the
        // stroke's previous point and re-draw it, partially transparent,
        // at the new point -- dragging existing pixels along the path
        // instead of laying down a fresh colour.
        "var smudgeBuf=document.createElement('canvas');"
        "function smudgeAt(x,y){"
        "var cx=actx(),c=acv();"
        "var r=Math.max(4,Number(document.getElementById('csize').value));"
        "var sx=Math.max(0,Math.floor(lastX-r)),sy=Math.max(0,Math.floor(lastY-r));"
        "var sw=Math.min(c.width-sx,r*2),sh=Math.min(c.height-sy,r*2);"
        "if(sw<=0||sh<=0)return;"
        "smudgeBuf.width=sw;smudgeBuf.height=sh;"
        "var sctx=smudgeBuf.getContext('2d');sctx.clearRect(0,0,sw,sh);"
        "sctx.drawImage(c,sx,sy,sw,sh,0,0,sw,sh);"
        "cx.save();cx.globalAlpha=.45;"
        "cx.drawImage(smudgeBuf,0,0,sw,sh,x-r,y-r,sw,sh);"
        "cx.restore();"
        "}"
        // Select: drag a marquee (no pixel change of its own -- just
        // records selRect and draws a dashed outline). Copy/Cut sample
        // that rectangle from the active layer into an internal clip
        // canvas; Paste arms a "next click places it" mode rather than
        // dropping it at a fixed spot, since a canvas has no cursor to
        // paste "at" the way a text editor does.
        "var selRect=null,clip=null,pastePending=false;"
        "function rectFrom(a,b){return{x:Math.min(a.x,b.x),y:Math.min(a.y,b.y),w:Math.abs(b.x-a.x),h:Math.abs(b.y-a.y)};}"
        "function drawMarquee(r){octx.save();octx.strokeStyle='#000';octx.lineWidth=1;octx.setLineDash([5,4]);"
        "octx.strokeRect(Math.round(r.x)+.5,Math.round(r.y)+.5,Math.round(r.w),Math.round(r.h));octx.restore();}"
        "function copySel(){"
        "if(!selRect||selRect.w<1||selRect.h<1)return;"
        "var c=document.createElement('canvas');c.width=Math.round(selRect.w);c.height=Math.round(selRect.h);"
        "c.getContext('2d').drawImage(acv(),selRect.x,selRect.y,c.width,c.height,0,0,c.width,c.height);"
        "clip=c;"
        "}"
        "document.getElementById('ccopy').addEventListener('click',copySel);"
        "document.getElementById('ccut').addEventListener('click',function(){"
        "if(!selRect||selRect.w<1||selRect.h<1)return;"
        "copySel();snap();actx().clearRect(selRect.x,selRect.y,selRect.w,selRect.h);captureFrame(false);"
        "});"
        "document.getElementById('cpaste').addEventListener('click',function(){if(clip)pastePending=true;});"
        "function placeClipboardAt(x,y){"
        "if(!clip)return;snap();actx().drawImage(clip,x-clip.width/2,y-clip.height/2);captureFrame(false);"
        "}"
        // Crop trims every layer's canvas down to the selection (sampled
        // BEFORE each layer's own canvas is resized, since resizing a
        // <canvas> element's width/height attribute always clears it).
        // Resize instead scales each layer's whole existing content into
        // a new canvas size -- no selection involved.
        "function resizeAllLayers(nw,nh,sampleRect){"
        "layers.forEach(function(L){"
        "var tmp=document.createElement('canvas');"
        "if(sampleRect){"
        "tmp.width=nw;tmp.height=nh;"
        "tmp.getContext('2d').drawImage(L.cv,sampleRect.x,sampleRect.y,nw,nh,0,0,nw,nh);"
        "}else{"
        "tmp.width=L.cv.width;tmp.height=L.cv.height;tmp.getContext('2d').drawImage(L.cv,0,0);"
        "}"
        "L.cv.width=nw;L.cv.height=nh;"
        "if(sampleRect)L.ctx.drawImage(tmp,0,0);else L.ctx.drawImage(tmp,0,0,tmp.width,tmp.height,0,0,nw,nh);"
        "L.undo=[];L.redo=[];"
        "});"
        "overlay.width=nw;overlay.height=nh;"
        "var wrap=document.getElementById('ccanvaswrap');"
        "wrap.style.maxWidth=nw+'px';wrap.style.aspectRatio=nw+'/'+nh;"
        "selRect=null;clearOverlay();captureFrame(false);"
        "}"
        "document.getElementById('ccrop').addEventListener('click',function(){"
        "if(!selRect||selRect.w<2||selRect.h<2)return;"
        "resizeAllLayers(Math.round(selRect.w),Math.round(selRect.h),"
        "{x:Math.round(selRect.x),y:Math.round(selRect.y)});"
        "});"
        // Drag the corner grip to resize the canvas -- live CSS-only
        // preview while dragging (cheap: just the wrap's own box, the
        // actual bitmaps untouched so the existing drawing visibly
        // stretches to match, same as dragging an image placeholder in
        // any editor), then the real per-layer resize commits once on
        // release via the same resizeAllLayers() the old numeric-input
        // form used.
        "(function(){"
        "var handle=document.getElementById('cresizehandle'),wrap=document.getElementById('ccanvaswrap');"
        "var dragging=false,startX=0,startY=0,startCw=0,startCh=0,cssToCanvas=1;"
        "handle.addEventListener('pointerdown',function(e){"
        "dragging=true;startX=e.clientX;startY=e.clientY;"
        "var r=wrap.getBoundingClientRect();"
        "startCw=r.width;startCh=r.height;cssToCanvas=acv().width/r.width;"
        "handle.setPointerCapture(e.pointerId);e.preventDefault();e.stopPropagation();"
        "});"
        "handle.addEventListener('pointermove',function(e){"
        "if(!dragging)return;"
        "var nw=Math.max(40,startCw+(e.clientX-startX));"
        "var nh=Math.max(40,startCh+(e.clientY-startY));"
        "wrap.style.maxWidth=nw+'px';wrap.style.aspectRatio=nw+'/'+nh;"
        "e.preventDefault();e.stopPropagation();"
        "});"
        "handle.addEventListener('pointerup',function(e){"
        "if(!dragging)return;dragging=false;"
        "var nw=Math.max(16,Math.min(2000,Math.round((startCw+(e.clientX-startX))*cssToCanvas)));"
        "var nh=Math.max(16,Math.min(2000,Math.round((startCh+(e.clientY-startY))*cssToCanvas)));"
        "resizeAllLayers(nw,nh,null);"
        "e.preventDefault();e.stopPropagation();"
        "});"
        "})();"
        // Coarse/fine wheels: drag vertically or scroll over one to nudge
        // its target number input by its own step, clamped to that
        // input's own min/max. A little rotation on each notch is purely
        // cosmetic feedback that the wheel actually registered the input.
        "document.querySelectorAll('.wheel').forEach(function(w){"
        "var target=document.getElementById(w.dataset.target),step=Number(w.dataset.step),rotation=0;"
        "function apply(dir){"
        "var min=Number(target.min)||0,max=Number(target.max)||9999;"
        "target.value=Math.max(min,Math.min(max,Number(target.value)+dir*step));"
        "rotation+=dir*20;w.style.transform='rotate('+rotation+'deg)';"
        "}"
        "w.addEventListener('wheel',function(e){e.preventDefault();apply(e.deltaY<0?1:-1);},{passive:false});"
        "var dragging=false,lastY=0;"
        "w.addEventListener('pointerdown',function(e){"
        "dragging=true;lastY=e.clientY;w.setPointerCapture(e.pointerId);e.preventDefault();"
        "});"
        "w.addEventListener('pointermove',function(e){"
        "if(!dragging)return;"
        "var dy=lastY-e.clientY;"
        "if(Math.abs(dy)>=6){apply(dy>0?1:-1);lastY=e.clientY;}"
        "});"
        "w.addEventListener('pointerup',function(){dragging=false;});"
        "});"
        // Text: drag the Text tool on the canvas -- vertical distance
        // sets the font size, horizontal sets the field width the text
        // wraps within (a plain click falls back to a sensible default
        // box instead of doing nothing). Opens a real <textarea> over the
        // canvas, styled live from the font/bold/italic/underline
        // controls, so what you see while typing already looks like what
        // commit() is about to bake onto the active layer as pixels
        // (word-wrapped to the box width, underline drawn by hand since
        // canvas text has no native underline, background filled first if
        // enabled) -- blur commits, Escape cancels without committing.
        "function addTextBox(x,y,boxW,fontPx){"
        "var r=overlay.getBoundingClientRect(),scale=r.width/overlay.width;"
        "var ta=document.createElement('textarea');"
        "ta.placeholder='Text';"
        "var bold=document.getElementById('cbold').dataset.on==='1';"
        "var italic=document.getElementById('citalic').dataset.on==='1';"
        "var underline=document.getElementById('cunderline').dataset.on==='1';"
        "var fam=document.getElementById('cfont').value;"
        "var color=document.getElementById('ccolor').value;"
        // width/min-height:auto overrides the site-wide
        // input,textarea{width:100%;min-height:44px} rule (meant for real
        // form fields), which would otherwise stretch this floating box
        // across the whole canvas -- the same class of bug already hit
        // input[type=color]/[type=checkbox] once each.
        "ta.style.cssText='position:absolute;z-index:10;width:auto;min-height:0;resize:both;overflow:auto;'+"
        "'padding:2px;box-sizing:border-box;background:rgba(255,255,255,.85);'+"
        "'border:1px dashed '+color+';color:'+color+';font-family:'+fam+';'+"
        "'font-size:'+(fontPx*scale)+'px;'+(bold?'font-weight:bold;':'')+(italic?'font-style:italic;':'')+"
        "(underline?'text-decoration:underline;':'');"
        "ta.style.left=(x*scale)+'px';ta.style.top=(y*scale)+'px';"
        "ta.style.width=(boxW*scale)+'px';ta.style.height=Math.max(fontPx*1.6,40)*scale+'px';"
        "document.getElementById('ccanvaswrap').appendChild(ta);"
        "ta.focus();"
        "var done=false;"
        "function commit(){"
        "if(done)return;done=true;"
        "var val=ta.value;ta.remove();"
        "if(!val)return;"
        "snap();var cx=actx();cx.save();"
        "cx.font=(italic?'italic ':'')+(bold?'bold ':'')+fontPx+'px '+fam;"
        "cx.textBaseline='top';"
        "var lineH=fontPx*1.25,lines=[];"
        "val.split('\\n').forEach(function(paragraph){"
        "var words=paragraph.split(' '),cur='';"
        "words.forEach(function(w){"
        "var test=cur?cur+' '+w:w;"
        "if(cur&&cx.measureText(test).width>boxW){lines.push(cur);cur=w;}else{cur=test;}"
        "});"
        "lines.push(cur);"
        "});"
        "if(document.getElementById('ctextbg').dataset.on==='1'){"
        "cx.fillStyle=document.getElementById('ctextbgcolor').value;"
        "cx.fillRect(x,y,boxW,lines.length*lineH);"
        "}"
        "cx.fillStyle=color;"
        "lines.forEach(function(line,i){"
        "var ly=y+i*lineH;cx.fillText(line,x,ly);"
        "if(underline){"
        "var w=cx.measureText(line).width;"
        "cx.fillRect(x,ly+fontPx*.92,w,Math.max(1,fontPx*.06));"
        "}"
        "});"
        "cx.restore();"
        "captureFrame(false);"
        "}"
        "ta.addEventListener('keydown',function(e){if(e.key==='Escape'){done=true;ta.remove();}});"
        "ta.addEventListener('blur',commit);"
        "}"
        // Paste from the OS clipboard (e.g. a screenshot copied outside
        // the browser) -- separate from the internal Copy/Cut/Paste
        // above, which only ever moves pixels around inside this canvas.
        "document.addEventListener('paste',function(e){"
        "var items=(e.clipboardData||{}).items||[];"
        "for(var i=0;i<items.length;i++){"
        "if(items[i].type.indexOf('image')!==0)continue;"
        "var file=items[i].getAsFile();if(!file)continue;"
        "var url=URL.createObjectURL(file),img=new Image();"
        "img.onload=function(){"
        "snap();var cvw=acv().width,cvh=acv().height;"
        "var scale=Math.min(cvw/img.width,cvh/img.height,1);"
        "var dw=img.width*scale,dh=img.height*scale;"
        "actx().drawImage(img,(cvw-dw)/2,(cvh-dh)/2,dw,dh);"
        "captureFrame(false);URL.revokeObjectURL(url);"
        "};"
        "img.src=url;e.preventDefault();break;"
        "}"
        "});"
        "overlay.addEventListener('pointerdown',function(e){"
        "var t=toolVal(),p=pos(e);"
        "if(pastePending){pastePending=false;placeClipboardAt(p.x,p.y);e.preventDefault();return;}"
        "if(t==='text'||t==='select'){shapeStart=p;e.preventDefault();return;}"
        "if(t==='bucket'){snap();floodFill(p.x,p.y);captureFrame(false);e.preventDefault();return;}"
        "if(t==='line'||t==='rect'||t==='circle'){snap();shapeStart=p;e.preventDefault();return;}"
        "snap();drawing=true;lastX=p.x;lastY=p.y;"
        "if(t!=='smudge')strokeAt(p.x,p.y,p.x+.1,p.y+.1);"
        "e.preventDefault();"
        "});"
        "overlay.addEventListener('pointermove',function(e){"
        "var t=toolVal(),p=pos(e);"
        "if(shapeStart&&(t==='select'||t==='text')){clearOverlay();drawMarquee(rectFrom(shapeStart,p));e.preventDefault();return;}"
        "if(shapeStart){clearOverlay();drawShape(shapeStart.x,shapeStart.y,p.x,p.y,octx);e.preventDefault();return;}"
        "if(!drawing)return;"
        "if(t==='smudge')smudgeAt(p.x,p.y);else strokeAt(lastX,lastY,p.x,p.y);"
        "lastX=p.x;lastY=p.y;e.preventDefault();"
        "});"
        "window.addEventListener('pointerup',function(e){"
        "if(shapeStart&&toolVal()==='select'){"
        "var p=pos(e);var r=rectFrom(shapeStart,p);clearOverlay();"
        "if(r.w>=2&&r.h>=2){selRect=r;drawMarquee(r);}"
        "shapeStart=null;return;"
        "}"
        "if(shapeStart&&toolVal()==='text'){"
        "var p=pos(e);var r=rectFrom(shapeStart,p);var start=shapeStart;clearOverlay();shapeStart=null;"
        // A near-zero drag (effectively just a click) falls back to a
        // sensible default box instead of creating an unusable sliver.
        "var fontPx=r.h>=6?Math.round(r.h):24;"
        "var boxW=r.w>=20?Math.round(r.w):200;"
        "var tx=r.w>=6?r.x:start.x,ty=r.h>=6?r.y:start.y;"
        "addTextBox(tx,ty,boxW,fontPx);"
        "return;"
        "}"
        "if(shapeStart){"
        "var p=pos(e);clearOverlay();drawShape(shapeStart.x,shapeStart.y,p.x,p.y,actx());"
        "shapeStart=null;captureFrame(false);return;"
        "}"
        "if(!drawing)return;drawing=false;captureFrame(false);"
        "});"
        "var layerBtns=document.querySelectorAll('.layerbtn');"
        "function refreshLayerBtns(){"
        "layerBtns.forEach(function(b){"
        "b.style.background=Number(b.dataset.layer)===activeLayer?'var(--rule)':'';"
        "});"
        "}"
        "layerBtns.forEach(function(b){"
        "b.addEventListener('click',function(){activeLayer=Number(b.dataset.layer);refreshLayerBtns();});"
        "});"
        "refreshLayerBtns();"
        "document.getElementById('cimport').addEventListener('change',function(e){"
        "var f=e.target.files[0];if(!f)return;"
        "var status=document.getElementById('cstatus');"
        "function place(imgLike,iw,ih){"
        "if(!iw||!ih){status.textContent='Could not read that file.';return;}"
        "snap();"
        "var cvw=acv().width,cvh=acv().height;"
        "var scale=Math.min(cvw/iw,cvh/ih);"
        "var dw=iw*scale,dh=ih*scale,dx=(cvw-dw)/2,dy=(cvh-dh)/2;"
        "actx().drawImage(imgLike,dx,dy,dw,dh);"
        "captureFrame(false);status.textContent='';"
        "}"
        "if(f.type.indexOf('video')===0){"
        "status.textContent='Reading video frame\\u2026';"
        "var url=URL.createObjectURL(f);"
        "var vid=document.createElement('video');"
        "vid.muted=true;vid.playsInline=true;vid.src=url;"
        "vid.addEventListener('loadeddata',function(){"
        "try{vid.currentTime=Math.min(.1,vid.duration||0);}catch(err){}"
        "});"
        "vid.addEventListener('seeked',function(){"
        "place(vid,vid.videoWidth,vid.videoHeight);URL.revokeObjectURL(url);"
        "});"
        "vid.addEventListener('error',function(){"
        "status.textContent='Could not read that video.';URL.revokeObjectURL(url);"
        "});"
        "}else{"
        "status.textContent='Loading image\\u2026';"
        "var reader=new FileReader();"
        "reader.onload=function(ev){"
        "var img=new Image();"
        "img.onload=function(){place(img,img.naturalWidth,img.naturalHeight);};"
        "img.onerror=function(){status.textContent='That file is not a usable image.';};"
        "img.src=ev.target.result;"
        "};"
        "reader.readAsDataURL(f);"
        "}"
        "e.target.value='';"
        "});"
        // Flattens all layers onto a fresh white-backed canvas -- storage
        // only ever holds one flat PNG per frame, so layers are purely a
        // client-side editing convenience that collapses down at capture
        // time, same as an image editor's "flatten" export. Shared by both
        // modes: the background tool's per-action frame capture below, and
        // the self-portrait tool's own one-shot Save handler.
        "function flatten(){"
        "var out=document.createElement('canvas');"
        "out.width=layers[0].cv.width;out.height=layers[0].cv.height;"
        "var octx2=out.getContext('2d');"
        "octx2.fillStyle='#fff';octx2.fillRect(0,0,out.width,out.height);"
        "for(var i=0;i<layers.length;i++){"
        "octx2.globalAlpha=layers[i].opacity;octx2.drawImage(layers[i].cv,0,0);"
        "}"
        "octx2.globalAlpha=1;"
        "return out;"
        "}"
        "document.getElementById('cundo').addEventListener('click',function(){"
        "var L=layers[activeLayer];if(!L.undo.length)return;"
        "L.redo.push(L.ctx.getImageData(0,0,L.cv.width,L.cv.height));"
        "L.ctx.putImageData(L.undo.pop(),0,0);"
        "});"
        "document.getElementById('credo').addEventListener('click',function(){"
        "var L=layers[activeLayer];if(!L.redo.length)return;"
        "L.undo.push(L.ctx.getImageData(0,0,L.cv.width,L.cv.height));"
        "L.ctx.putImageData(L.redo.pop(),0,0);"
        "});"
        "document.getElementById('cclear').addEventListener('click',function(){"
        "snap();var L=layers[activeLayer];L.ctx.clearRect(0,0,L.cv.width,L.cv.height);"
        "});";
    if (portrait) {
        // No frame history for a self-portrait -- captureFrame is called
        // from the shared pointer/import handlers above regardless of
        // mode, so it stays a real function here too, just a no-op; Save
        // does the only upload, straight to the existing /me/avatar route.
        html +=
            "function captureFrame(force,done){if(done)done();}"
            "document.getElementById('csave').addEventListener('click',function(){"
            "var status=document.getElementById('cstatus');status.textContent='Saving\\u2026';"
            "flatten().toBlob(function(blob){"
            "var fd=new FormData();fd.append('photo',blob,'portrait.png');"
            "fetch('/me/avatar',{method:'POST',body:fd}).then(function(r){"
            "window.location=r.url||'/me';"
            "}).catch(function(){status.textContent='Could not save. Try again.';});"
            "},'image/png');"
            "});";
    } else if (post_mode) {
        // No frame history for a post either -- one flattened image, paired
        // with the caption, POSTed as one multipart request straight to
        // whichever composer route this page was opened for. r.url is the
        // real final URL fetch landed on after following that route's own
        // redirect, so a media error still surfaces as that route's own
        // ?perr= message -- same pattern as the self-portrait Save above.
        html +=
            "function captureFrame(force,done){if(done)done();}"
            "document.getElementById('csave').addEventListener('click',function(){"
            "var status=document.getElementById('cstatus');status.textContent='Posting\\u2026';"
            "flatten().toBlob(function(blob){"
            "var fd=new FormData();"
            "fd.append('text',document.getElementById('cpcaption').value);"
            "fd.append('media',blob,'post.png');"
            "fetch("+js_string_literal(post_target)+",{method:'POST',body:fd}).then(function(r){"
            "window.location=r.url||"+js_string_literal(post_target)+";"
            "}).catch(function(){status.textContent='Could not post. Try again.';});"
            "},'image/png');"
            "});";
    } else {
        html +=
            "var clearedOnce=false,frameCount=0;"
            "function updateCounter(){"
            "document.getElementById('cframecount').textContent=frameCount+'/24 frames captured';"
            "}"
            // done() always fires, whether this capture actually uploaded
            // or was skipped (cap hit, no force) -- callers that need to
            // know the frame really landed (Save, below) depend on that.
            "function captureFrame(force,done){"
            "if(frameCount>=23&&!force){if(done)done();return;}"
            "flatten().toBlob(function(blob){"
            "var fd=new FormData();fd.append('frame',blob,'frame.png');"
            "function go(){"
            "fetch('/me/canvas/frame',{method:'POST',body:fd}).then(function(r){"
            "if(r.ok){frameCount++;updateCounter();}"
            "if(done)done();"
            "}).catch(function(){if(done)done();});"
            "}"
            "if(!clearedOnce){"
            "clearedOnce=true;"
            "fetch('/me/canvas/clear',{method:'POST'}).then(go).catch(go);"
            "}else{go();}"
            "},'image/png');"
            "}"
            "document.getElementById('csave').addEventListener('click',function(){"
            "var status=document.getElementById('cstatus');status.textContent='Saving\\u2026';"
            "var timelapse=document.getElementById('ctimelapse').checked?'1':'0';"
            "captureFrame(true,function(){"
            "fetch('/me/canvas/mode',{method:'POST',"
            "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
            "body:'timelapse='+timelapse}).then(function(){"
            "window.location='/u/'+"+js_string_literal(username)+";"
            "});"
            "});"
            "});";
    }
    html += "})();</script>";
    return html;
}

// World client page: a <canvas>, WASD/arrow movement, one WebSocket to
// /world/ws. Sends {"x":n,"y":n} on movement; server broadcasts the full
// player snapshot back on an interval regardless of whether this client
// moved, so remote players animate smoothly too. Matches the documented
// MVP: "One shared room, two people see each other move."
std::string world_page(const std::string& username) {
    std::string html =
        "<div class=\"card\"><h1>World</h1>"
        "<p class=\"muted\">One shared room. Arrow keys or WASD to move. Closing this page removes you.</p>"
        "<canvas id=\"wcanvas\" width=\"800\" height=\"500\" "
        "style=\"border:1px solid var(--rule);border-radius:var(--rsm);background:#eee7d6;"
        "max-width:100%;touch-action:none\"></canvas>"
        "<p class=\"muted\" id=\"wstatus\">Connecting&hellip;</p></div>"
        "<script>(function(){"
        "var me="+js_string_literal(username)+";"
        "var cv=document.getElementById('wcanvas'),ctx=cv.getContext('2d');"
        "var st=document.getElementById('wstatus');"
        "var players={};"
        "var proto=location.protocol==='https:'?'wss:':'ws:';"
        "var ws=new WebSocket(proto+'//'+location.host+'/world/ws');"
        "var keys={};"
        "ws.onopen=function(){st.textContent='Connected.';};"
        "ws.onclose=function(){st.textContent='Disconnected. Reload to rejoin.';};"
        "ws.onerror=function(){st.textContent='Connection error.';};"
        "ws.onmessage=function(ev){"
        "try{var data=JSON.parse(ev.data);players=data.players||{};}catch(e){}"
        "draw();"
        "};"
        "function draw(){"
        "ctx.clearRect(0,0,cv.width,cv.height);"
        "Object.keys(players).forEach(function(id){"
        "var p=players[id];"
        "ctx.beginPath();ctx.arc(p.x,p.y,14,0,Math.PI*2);"
        "ctx.fillStyle=p.color;ctx.fill();"
        "if(p.username===me){ctx.lineWidth=2;ctx.strokeStyle='#1a1814';ctx.stroke();}"
        "ctx.fillStyle='#1a1814';ctx.font='12px sans-serif';ctx.textAlign='center';"
        "ctx.fillText(p.username,p.x,p.y-20);"
        "});"
        "}"
        "document.addEventListener('keydown',function(e){keys[e.key.toLowerCase()]=true;});"
        "document.addEventListener('keyup',function(e){keys[e.key.toLowerCase()]=false;});"
        "var x=cv.width/2,y=cv.height/2;"
        "setInterval(function(){"
        "if(ws.readyState!==1)return;"
        "var dx=0,dy=0,speed=5;"
        "if(keys['arrowleft']||keys['a'])dx-=speed;"
        "if(keys['arrowright']||keys['d'])dx+=speed;"
        "if(keys['arrowup']||keys['w'])dy-=speed;"
        "if(keys['arrowdown']||keys['s'])dy+=speed;"
        "if(dx||dy){"
        "x=Math.max(14,Math.min(cv.width-14,x+dx));"
        "y=Math.max(14,Math.min(cv.height-14,y+dy));"
        "ws.send(JSON.stringify({x:x,y:y}));"
        "}"
        "},50);"
        "})();</script>";
    return html;
}

// One thread per connected /world/ws client. Runs the RFC 6455 handshake
// response, then loops: try to read this client's new position (short
// timeout so the loop can broadcast even when nobody moved), update the
// shared registry, and push everyone's current positions back to this
// client. Removes the client from the registry on disconnect.
void run_world_session(int fd, std::string username) {
    timeval tv{}; tv.tv_sec = 0; tv.tv_usec = 100000; // 100ms
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    {
        std::lock_guard<std::mutex> lock(g_world_mu);
        WorldPlayer p;
        p.username = username;
        p.color = world_color_for(username);
        g_world_players[fd] = p;
    }

    for (;;) {
        auto result = ws_read_frame(fd);
        if (result.status == WsReadStatus::Closed) break;
        if (result.status == WsReadStatus::Ok) {
            if (result.frame.opcode == WsOpcode::Close) break;
            if (result.frame.opcode == WsOpcode::Text) {
                // Minimal ad-hoc parse of {"x":<num>,"y":<num>} — the only
                // shape this endpoint ever receives, so a general JSON
                // parser would be pure overhead here.
                const std::string& s = result.frame.payload;
                auto extract = [&](const char* key) -> std::optional<double> {
                    auto p = s.find(key);
                    if (p == std::string::npos) return std::nullopt;
                    p = s.find(':', p);
                    if (p == std::string::npos) return std::nullopt;
                    try { return std::stod(s.substr(p + 1)); } catch (...) { return std::nullopt; }
                };
                auto nx = extract("\"x\"");
                auto ny = extract("\"y\"");
                if (nx && ny) {
                    std::lock_guard<std::mutex> lock(g_world_mu);
                    auto it = g_world_players.find(fd);
                    if (it != g_world_players.end()) { it->second.x = *nx; it->second.y = *ny; }
                }
            }
        }
        // status == Timeout falls through here too — always re-broadcast.

        std::string snapshot;
        {
            std::lock_guard<std::mutex> lock(g_world_mu);
            if (g_world_players.find(fd) == g_world_players.end()) break;
            std::ostringstream os;
            os << "{\"players\":{";
            bool first = true;
            for (auto& [id, p] : g_world_players) {
                if (!first) os << ",";
                first = false;
                os << "\"" << id << "\":{\"x\":" << p.x << ",\"y\":" << p.y
                   << ",\"username\":\"" << p.username << "\",\"color\":\"" << p.color << "\"}";
            }
            os << "}}";
            snapshot = os.str();
        }
        if (!ws_write_frame(fd, WsOpcode::Text, snapshot)) break;
    }

    {
        std::lock_guard<std::mutex> lock(g_world_mu);
        g_world_players.erase(fd);
    }
    close(fd);
}

// Minimal ad-hoc extractor for a top-level {"key":"value"} string field —
// same "only shape this endpoint ever receives" reasoning as the world
// socket's x/y number parser. Not escape-aware: fine for URLs, which never
// contain a literal quote character.
std::optional<std::string> extract_json_string_field(const std::string& s, const char* key) {
    auto p = s.find(key);
    if (p == std::string::npos) return std::nullopt;
    p = s.find(':', p);
    if (p == std::string::npos) return std::nullopt;
    auto open = s.find('"', p);
    if (open == std::string::npos) return std::nullopt;
    auto close_pos = s.find('"', open + 1);
    if (close_pos == std::string::npos) return std::nullopt;
    return s.substr(open + 1, close_pos - open - 1);
}

// One thread per connected station client (/community/ws for the "" site-
// wide station, /c/<slug>/jukebox/ws for a community's own) — same shape
// as run_world_session, entirely separate shared state (g_community_mu).
// Membership (for a real station) was already checked before this thread
// was spawned, same as everything else here trusts the caller. Any
// connected member can add a track (open queue within the station, same
// posture the site-wide one always had); the queue auto-advances when any
// connected client's player reports the current track ended.
void run_community_session(int fd, const std::string& username, const std::string& station) {
    timeval tv{}; tv.tv_sec = 0; tv.tv_usec = 500000; // 500ms — a shared queue doesn't need world's 100ms cadence
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    for (;;) {
        auto result = ws_read_frame(fd);
        if (result.status == WsReadStatus::Closed) break;
        if (result.status == WsReadStatus::Ok) {
            if (result.frame.opcode == WsOpcode::Close) break;
            if (result.frame.opcode == WsOpcode::Text) {
                const std::string& s = result.frame.payload;
                if (auto raw = extract_json_string_field(s, "\"add\"")) {
                    if (auto id = extract_youtube_id(trim_ws(*raw))) {
                        std::lock_guard<std::mutex> lock(g_community_mu);
                        auto& now_playing = g_community_now_by_station[station];
                        auto& upcoming = g_community_upcoming_by_station[station];
                        if (!now_playing.active && upcoming.empty()) {
                            now_playing = CommunityNowPlaying{*id, username,
                                std::chrono::duration_cast<std::chrono::seconds>(
                                    std::chrono::system_clock::now().time_since_epoch()).count(),
                                true};
                        } else {
                            upcoming.push_back(CommunityTrack{*id, username});
                        }
                    }
                } else if (s.find("\"ended\"") != std::string::npos) {
                    std::lock_guard<std::mutex> lock(g_community_mu);
                    if (g_community_now_by_station[station].active) {
                        community_advance_locked(station, std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count());
                    }
                }
            }
        }

        std::string snapshot;
        {
            std::lock_guard<std::mutex> lock(g_community_mu);
            const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            const auto& now_playing = g_community_now_by_station[station];
            std::ostringstream os;
            os << "{";
            if (now_playing.active) {
                os << "\"now\":{\"video_id\":\"" << now_playing.video_id
                   << "\",\"queued_by\":\"" << now_playing.queued_by
                   << "\",\"elapsed\":" << std::max<std::int64_t>(0, now - now_playing.started_at) << "}";
            } else {
                os << "\"now\":null";
            }
            os << ",\"upcoming\":" << g_community_upcoming_by_station[station].size() << "}";
            snapshot = os.str();
        }
        if (!ws_write_frame(fd, WsOpcode::Text, snapshot)) break;
    }
    close(fd);
}

// Floating jukebox widget: a phone-icon button, present on every logged-in
// page (injected from wrap()), that expands into a panel with two modes —
// "Community" (synced to the site-wide queue over /community/ws, same
// real-time pattern as /world/ws) and "My search" (private, local-only
// playback, never touches the server). One YT.Player instance is reused
// across both modes so play/pause/mute/volume always act on whatever's
// currently loaded.
//
// Honest limitation, not hidden: this site is deliberately a multi-page
// app (LIQUIDGOV_CONTEXT_HANDOFF.md's Jukebox section — no site-wide SPA).
// Navigating to a different page resets local playback; the widget
// reconnects and re-syncs to the live community track next time it's
// opened. True across-navigation persistence would need the browser's
// Picture-in-Picture API, deliberately not added here.
// Shared panel guts (player, tabs, controls, community/search sections) —
// identical whether embedded in the floating widget or filling the whole
// /listen popout window.
std::string jukebox_panel_body(bool standalone) {
    return
        "<div class=\"row\" style=\"justify-content:space-between;align-items:center\">"
        "<div class=\"row\" style=\"gap:.3rem\">"
        "<button type=\"button\" class=\"ghost\" id=\"jbtab-community\">Stations</button>"
        "<button type=\"button\" class=\"ghost\" id=\"jbtab-search\">My search</button>"
        "</div>"
        "<div class=\"row\" style=\"gap:.3rem\">"
        + std::string(standalone
            ? "<button type=\"button\" class=\"ghost\" id=\"jbfloat\" hidden>Float above</button>"
            : "<button type=\"button\" class=\"ghost\" id=\"jbpopout\" title=\"Open in its own window &mdash; keeps playing while you browse\">Pop out</button>")
        + "<button type=\"button\" class=\"ghost\" id=\"jbclose\" aria-label=\"Close\">\xE2\x9C\x95</button>"
        "</div>"
        "</div>"
        "<div id=\"jbplayer\" style=\"margin-top:.6rem;aspect-ratio:16/9;background:#000;"
        "border-radius:var(--rsm)\"></div>"
        "<div class=\"row\" style=\"margin-top:.5rem;align-items:center;gap:.4rem\">"
        "<button type=\"button\" class=\"ghost\" id=\"jbplaypause\">Play</button>"
        "<button type=\"button\" class=\"ghost\" id=\"jbmute\">Mute</button>"
        "<input type=\"range\" id=\"jbvolume\" min=\"0\" max=\"100\" value=\"70\" style=\"flex:1\">"
        "</div>"
        "<div id=\"jbcommunity-ui\">"
        // Priority-ordered, drag-to-reorder list of stations tuned in on
        // *this* widget -- see jukebox_script's stations block for the
        // reorder/click-to-temporarily-prioritize logic. Deliberately not
        // an account preference: "on the phone" is this device's own
        // widget config, in localStorage, same as it staying collapsed
        // across a soft-nav is this tab's own state.
        "<p class=\"muted\" style=\"margin:.5rem 0 .15rem;font-size:.72rem;letter-spacing:.08em;"
        "text-transform:uppercase\">Stations &mdash; drag to reorder, tap to tune in now</p>"
        "<div id=\"jbstationlist\" style=\"display:flex;flex-direction:column;gap:.2rem\"></div>"
        "<select id=\"jbstationadd\" style=\"margin-top:.35rem\"><option value=\"\">Add a station&hellip;</option></select>"
        "<p class=\"muted\" id=\"jbnow\" style=\"margin:.6rem 0 0\">Connecting&hellip;</p>"
        "<p class=\"muted\" id=\"jbupcoming\" style=\"margin:.1rem 0 0\"></p>"
        "<input id=\"jbadd\" placeholder=\"Add to the queue\" autocomplete=\"off\" "
        "style=\"margin-top:.4rem;width:100%\">"
        "<div id=\"jbaddresults\" style=\"margin-top:.3rem;display:flex;flex-direction:column;gap:.15rem\"></div>"
        "</div>"
        "<div id=\"jbsearch-ui\" hidden>"
        "<input id=\"jbsearch\" placeholder=\"Search or paste a link\" autocomplete=\"off\" "
        "style=\"margin-top:.5rem;width:100%\">"
        "<div id=\"jbsearchresults\" style=\"margin-top:.3rem;display:flex;flex-direction:column;gap:.15rem\"></div>"
        "</div>";
}

// Shared controller JS. `standalone` selects between the two hosting
// contexts: embedded (toggle button + collapsible panel, Pop-out button
// opens /listen) vs the /listen window itself (always visible, Close button
// really closes the window, and — since *this* window's own document never
// navigates away as long as it stays open — a working "Float above" button
// using the real Document Picture-in-Picture API, moving the whole panel
// into an always-on-top OS-level window for as long as this tab stays open).
std::string jukebox_script(const std::string& yt_key, bool standalone, const std::string& stations_js) {
    std::string s =
        "<script>(function(){"
        "var key="+js_string_literal(yt_key)+";"
        "var STANDALONE="+std::string(standalone ? "true" : "false")+";"
        "var tabC=document.getElementById('jbtab-community'),tabS=document.getElementById('jbtab-search');"
        "var playerDiv=document.getElementById('jbplayer');"
        "var playPauseBtn=document.getElementById('jbplaypause'),muteBtn=document.getElementById('jbmute');"
        "var volumeSlider=document.getElementById('jbvolume');"
        "var nowEl=document.getElementById('jbnow'),upcomingEl=document.getElementById('jbupcoming');"
        "var addInput=document.getElementById('jbadd'),addResults=document.getElementById('jbaddresults');"
        "var searchInput=document.getElementById('jbsearch'),searchResults=document.getElementById('jbsearchresults');"
        "var communityUi=document.getElementById('jbcommunity-ui'),searchUi=document.getElementById('jbsearch-ui');"
        "var stationList=document.getElementById('jbstationlist'),stationAdd=document.getElementById('jbstationadd');"
        "var mode='community',ytPlayer=null,ws=null,muted=false;"
        "var lastCommunityId=null,lastCommunityElapsed=0;"
        // Stations: which live jukeboxes (the site-wide one, id "", plus any
        // community this account belongs to) are tuned in on *this* widget,
        // in priority order (index 0 = plays). This is the device's own
        // widget config -- localStorage, not an account setting -- same
        // reasoning as the panel's own open/closed state never being saved
        // server-side. tempOverride is a click-to-listen-now override: set
        // by tapping a station's name, cleared by tapping it again or
        // choosing a different one, never persisted (it's explicitly
        // temporary -- see jukebox_panel_body's station row markup).
        "var STATIONS_AVAILABLE="+stations_js+";"
        "var STATIONS_KEY='liquidgov.jukebox.stations';"
        "var enabledOrder=[''];"
        "try{"
        "var saved=JSON.parse(localStorage.getItem(STATIONS_KEY)||'null');"
        "if(saved&&saved.length){"
        "var filtered=saved.filter(function(id){"
        "return STATIONS_AVAILABLE.some(function(s){return s.id===id;});"
        "});"
        "if(filtered.length)enabledOrder=filtered;"
        "}"
        "}catch(e){}"
        "var tempOverride=null,currentConnectedStation=undefined;"
        "function saveStations(){try{localStorage.setItem(STATIONS_KEY,JSON.stringify(enabledOrder));}catch(e){}}"
        "function stationName(id){"
        "var found=STATIONS_AVAILABLE.filter(function(s){return s.id===id;})[0];"
        "return found?found.name:id;"
        "}"
        "function activeStationId(){"
        "if(tempOverride!==null)return tempOverride;"
        "return enabledOrder.length?enabledOrder[0]:null;"
        "}"
        "function esc(s){var ta=document.createElement('textarea');ta.innerHTML=s;"
        "var d=document.createElement('div');d.textContent=ta.value;return d.innerHTML;}"
        "function looksLikeUrl(s){return /^https?:\\/\\//i.test(s)||/youtu\\.?be/i.test(s);}"
        "function updateTabs(){"
        "tabC.style.background=mode==='community'?'var(--accent)':'';tabC.style.color=mode==='community'?'var(--paper)':'';"
        "tabS.style.background=mode==='search'?'var(--accent)':'';tabS.style.color=mode==='search'?'var(--paper)':'';"
        "}"
        "function ensureYT(cb){"
        "if(window.YT&&window.YT.Player){cb();return;}"
        "if(!document.getElementById('jbytapi')){"
        "var s=document.createElement('script');s.id='jbytapi';s.src='https://www.youtube.com/iframe_api';"
        "document.head.appendChild(s);}"
        "var prev=window.onYouTubeIframeAPIReady;"
        "window.onYouTubeIframeAPIReady=function(){if(prev)prev();cb();};"
        "}"
        "function ensurePlayer(cb){"
        "if(ytPlayer){cb();return;}"
        "ensureYT(function(){"
        "ytPlayer=new YT.Player('jbplayer',{height:'100%',width:'100%',"
        "playerVars:{rel:0,playsinline:1},events:{"
        "onReady:function(){ytPlayer.setVolume(Number(volumeSlider.value));cb();},"
        "onStateChange:function(e){"
        "if(e.data===YT.PlayerState.PLAYING)playPauseBtn.textContent='Pause';"
        "else if(e.data===YT.PlayerState.PAUSED)playPauseBtn.textContent='Play';"
        "if(e.data===YT.PlayerState.ENDED&&mode==='community'&&ws&&ws.readyState===1)"
        "ws.send(JSON.stringify({ended:true}));"
        "}"
        "}});"
        "});"
        "}"
        "function teardownWs(){"
        "if(ws){try{ws.close();}catch(e){}ws=null;}"
        "lastCommunityId=null;"
        "}"
        "function teardown(){"
        "teardownWs();"
        "if(ytPlayer){try{ytPlayer.destroy();}catch(e){}ytPlayer=null;}"
        "}"
        "function connectWs(stationId){"
        "if(ws)return;"
        "var proto=location.protocol==='https:'?'wss:':'ws:';"
        "var path=stationId?('/c/'+encodeURIComponent(stationId)+'/jukebox/ws'):'/community/ws';"
        "ws=new WebSocket(proto+'//'+location.host+path);"
        "ws.onmessage=function(ev){"
        "var data;try{data=JSON.parse(ev.data);}catch(e){return;}"
        "if(data.now){"
        "nowEl.textContent='Now playing on '+esc(stationName(stationId))+' \\u2014 queued by '+esc(data.now.queued_by);"
        "if(data.now.video_id!==lastCommunityId){"
        "lastCommunityId=data.now.video_id;lastCommunityElapsed=data.now.elapsed;"
        "if(mode==='community')ensurePlayer(function(){"
        "ytPlayer.loadVideoById({videoId:lastCommunityId,startSeconds:lastCommunityElapsed});"
        "});"
        "}else{lastCommunityElapsed=data.now.elapsed;}"
        "}else{"
        "nowEl.textContent=esc(stationName(stationId))+': nothing playing \\u2014 add a track below.';"
        "lastCommunityId=null;"
        "if(mode==='community'&&ytPlayer)ytPlayer.stopVideo();"
        "}"
        "upcomingEl.textContent=data.upcoming?(data.upcoming+' track'+(data.upcoming===1?'':'s')+' queued next'):'';"
        "};"
        "}"
        // Called after enabledOrder or tempOverride changes: re-renders the
        // station list (so the active one's highlight is right) and, only
        // when the *actual* active station changed, tears down and
        // reconnects the live socket to it -- a reorder that doesn't move
        // anything above the active one, or a temp-override toggle back to
        // the same station that was already playing, is a no-op here.
        "function onStationsChanged(persist){"
        "if(persist)saveStations();"
        "renderStations();renderStationAdd();"
        "var id=activeStationId();"
        "if(id===currentConnectedStation)return;"
        "currentConnectedStation=id;"
        "if(mode==='community'){"
        "teardownWs();"
        "if(id!==null){connectWs(id);}"
        "else{nowEl.textContent='No station selected \\u2014 add one below.';upcomingEl.textContent='';if(ytPlayer)ytPlayer.stopVideo();}"
        "}"
        "}"
        "function renderStations(){"
        "var active=activeStationId();"
        "stationList.innerHTML='';"
        "enabledOrder.forEach(function(id){"
        "var row=document.createElement('div');"
        "row.className='jbstation-row'+(id===active?' active':'');"
        "row.dataset.id=id;"
        "var handle=document.createElement('span');handle.className='jbdraghandle';"
        "handle.textContent='\\u283F';handle.setAttribute('aria-hidden','true');"
        "var name=document.createElement('span');name.className='jbstation-name';"
        "name.textContent=stationName(id);name.title='Tap to listen to this now';"
        "name.addEventListener('click',function(){"
        "tempOverride=(tempOverride===id)?null:id;"
        "onStationsChanged(false);"
        "});"
        "var remove=document.createElement('button');remove.type='button';remove.className='jbstation-remove';"
        "remove.setAttribute('aria-label','Remove station');remove.textContent='\\u2715';"
        "remove.addEventListener('click',function(e){"
        "e.stopPropagation();"
        "enabledOrder=enabledOrder.filter(function(x){return x!==id;});"
        "if(tempOverride===id)tempOverride=null;"
        "onStationsChanged(true);"
        "});"
        "row.appendChild(handle);row.appendChild(name);row.appendChild(remove);"
        "stationList.appendChild(row);"
        "});"
        "}"
        "function renderStationAdd(){"
        "stationAdd.innerHTML='';"
        "var ph=document.createElement('option');ph.value='__placeholder__';ph.textContent='Add a station\\u2026';"
        "stationAdd.appendChild(ph);"
        "STATIONS_AVAILABLE.forEach(function(s){"
        "if(enabledOrder.indexOf(s.id)!==-1)return;"
        "var opt=document.createElement('option');opt.value=s.id;opt.textContent=s.name;"
        "stationAdd.appendChild(opt);"
        "});"
        "stationAdd.value='__placeholder__';"
        "}"
        "stationAdd.addEventListener('change',function(){"
        "var id=stationAdd.value;"
        "if(id==='__placeholder__')return;"
        "if(enabledOrder.indexOf(id)===-1){enabledOrder.push(id);onStationsChanged(true);}"
        "});"
        // Pointer-based reorder (not native HTML5 drag, which doesn't work
        // on touch): drag the handle, the row under the pointer's midpoint
        // is where it lands, dropped position becomes the new saved order.
        "(function attachStationDrag(){"
        "var dragRow=null;"
        "function rowUnder(clientY){"
        "var rows=Array.prototype.slice.call(stationList.querySelectorAll('.jbstation-row'));"
        "var after=null;"
        "rows.forEach(function(r){"
        "if(r===dragRow)return;"
        "var box=r.getBoundingClientRect();"
        "if(clientY>box.top+box.height/2)after=r;"
        "});"
        "return after;"
        "}"
        "stationList.addEventListener('pointerdown',function(e){"
        "var row=e.target.closest&&e.target.closest('.jbstation-row');"
        "var handle=e.target.closest&&e.target.closest('.jbdraghandle');"
        "if(!row||!handle)return;"
        "dragRow=row;"
        "try{row.setPointerCapture(e.pointerId);}catch(err){}"
        "row.style.zIndex='5';row.style.opacity='.85';"
        "});"
        "stationList.addEventListener('pointermove',function(e){"
        "if(!dragRow)return;"
        "var after=rowUnder(e.clientY);"
        "var target=after?after.nextSibling:stationList.firstChild;"
        "if(target!==dragRow)stationList.insertBefore(dragRow,target);"
        "});"
        "function drop(){"
        "if(!dragRow)return;"
        "dragRow.style.zIndex='';dragRow.style.opacity='';"
        "dragRow=null;"
        "enabledOrder=Array.prototype.slice.call(stationList.querySelectorAll('.jbstation-row'))"
        ".map(function(r){return r.dataset.id;});"
        "onStationsChanged(true);"
        "}"
        "stationList.addEventListener('pointerup',drop);"
        "stationList.addEventListener('pointercancel',drop);"
        "})();"
        "function switchMode(m){"
        "mode=m;updateTabs();"
        "communityUi.hidden=(m!=='community');searchUi.hidden=(m!=='search');"
        "if(m==='community'){"
        "var id=activeStationId();"
        "if(id!==currentConnectedStation||!ws){"
        "currentConnectedStation=id;teardownWs();"
        "if(id!==null)connectWs(id);"
        "}"
        "if(lastCommunityId)ensurePlayer(function(){"
        "ytPlayer.loadVideoById({videoId:lastCommunityId,startSeconds:lastCommunityElapsed});"
        "});"
        "}"
        "}"
        "function attachLiveSearch(input,resultsEl,onPick){"
        "var timer=null,gen=0;"
        "input.addEventListener('input',function(){"
        "var val=input.value.trim();clearTimeout(timer);"
        "if(!val||looksLikeUrl(val)){resultsEl.innerHTML='';return;}"
        "timer=setTimeout(function(){"
        "if(!key)return;"
        "var my=++gen;"
        "fetch('https://www.googleapis.com/youtube/v3/search?part=snippet&type=video&maxResults=6&q='"
        "+encodeURIComponent(val)+'&key='+key)"
        ".then(function(r){return r.json();})"
        ".then(function(data){"
        "if(my!==gen)return;"
        "resultsEl.innerHTML='';"
        "(data.items||[]).forEach(function(it){"
        "var id=it.id.videoId,title=it.snippet.title,thumb=it.snippet.thumbnails.default.url;"
        "var row=document.createElement('div');row.className='row';"
        "row.style.cssText='align-items:center;cursor:pointer;padding:.25rem;border-radius:var(--rsm)';"
        "row.innerHTML='<img src=\"'+thumb+'\" width=\"48\" height=\"36\" alt=\"\" style=\"border-radius:4px\">"
        "<span style=\"flex:1;font-size:.85rem\">'+esc(title)+'</span>';"
        "row.addEventListener('click',function(){"
        "input.value='';resultsEl.innerHTML='';onPick('https://www.youtube.com/watch?v='+id);"
        "});"
        "resultsEl.appendChild(row);"
        "});"
        "}).catch(function(){});"
        "},300);"
        "});"
        "input.addEventListener('keydown',function(e){"
        "if(e.key==='Enter'){"
        "e.preventDefault();"
        "var val=input.value.trim();"
        "if(val&&looksLikeUrl(val)){input.value='';resultsEl.innerHTML='';onPick(val);}"
        "}"
        "});"
        "}"
        "attachLiveSearch(addInput,addResults,function(url){"
        "if(ws&&ws.readyState===1)ws.send(JSON.stringify({add:url}));"
        "});"
        "attachLiveSearch(searchInput,searchResults,function(url){"
        "var m=url.match(/[?&]v=([A-Za-z0-9_-]{11})/)||url.match(/youtu\\.be\\/([A-Za-z0-9_-]{11})/);"
        "if(!m)return;"
        "switchMode('search');"
        "ensurePlayer(function(){ytPlayer.loadVideoById(m[1]);});"
        "});"
        "if(!STANDALONE){"
        "var toggle=document.getElementById('jbtoggle'),panel=document.getElementById('jbpanel');"
        "toggle.addEventListener('click',function(){"
        "panel.hidden=!panel.hidden;"
        "if(!panel.hidden){ensurePlayer(function(){});"
        "if(mode==='community'&&!ws){currentConnectedStation=activeStationId();connectWs(currentConnectedStation);}"
        "}"
        "});"
        // Closing the panel is a minimize, not a stop: audio keeps playing
        // in the background on this page, same as e.g. a music site's
        // player bar collapsing. Only Pop-out tears down -- a separate
        // /listen window is taking over, so this page's copy would
        // otherwise be a redundant, potentially state-confusing connection.
        "document.getElementById('jbclose').addEventListener('click',function(){panel.hidden=true;});"
        "var popoutBtn=document.getElementById('jbpopout');"
        "if(popoutBtn)popoutBtn.addEventListener('click',function(){"
        "window.open('/listen','lg_jukebox','width=360,height=560,menubar=no,toolbar=no,location=no,status=no');"
        "panel.hidden=true;teardown();"
        "});"
        // Soft navigation: intercept plain same-origin link clicks and swap
        // <main>'s content via fetch instead of a full page load, so the
        // widget (a sibling of <main>, see wrap()) and its live
        // WebSocket/YT player are simply never touched by an ordinary click
        // around the site. Forms/POSTs are untouched (this only hooks
        // anchor clicks) and still do a normal full-page round trip.
        "document.addEventListener('click',function(e){"
        "if(e.defaultPrevented||e.button!==0||e.metaKey||e.ctrlKey||e.shiftKey||e.altKey)return;"
        "var a=e.target.closest&&e.target.closest('a[href]');"
        "if(!a||a.target&&a.target!=='_self'||a.hasAttribute('download'))return;"
        "var url;try{url=new URL(a.href,location.href);}catch(err){return;}"
        "if(url.origin!==location.origin||url.pathname===location.pathname&&url.search===location.search&&url.hash)return;"
        "e.preventDefault();"
        "softNavigate(url.href,true);"
        "});"
        "window.addEventListener('popstate',function(){softNavigate(location.href,false);});"
        "function reExecuteScripts(container){"
        "Array.prototype.forEach.call(container.querySelectorAll('script'),function(old){"
        "var fresh=document.createElement('script');"
        "for(var i=0;i<old.attributes.length;i++)fresh.setAttribute(old.attributes[i].name,old.attributes[i].value);"
        "fresh.textContent=old.textContent;"
        "old.parentNode.replaceChild(fresh,old);"
        "});"
        "}"
        "function softNavigate(url,pushState){"
        "fetch(url,{headers:{'X-Soft-Nav':'1'}}).then(function(r){"
        "if(!r.ok)throw new Error('nav failed');"
        "return r.text().then(function(html){return{html:html,finalUrl:r.url};});"
        "}).then(function(res){"
        "var doc=new DOMParser().parseFromString(res.html,'text/html');"
        "var newMain=doc.querySelector('main.wrap');"
        "if(!newMain){location.href=url;return;}"
        "document.querySelector('main.wrap').innerHTML=newMain.innerHTML;"
        "reExecuteScripts(document.querySelector('main.wrap'));"
        "var newNav=doc.querySelector('header.top nav');"
        "var navEl=document.querySelector('header.top nav');"
        "if(navEl&&newNav)navEl.innerHTML=newNav.innerHTML;"
        "document.title=doc.title;"
        "if(pushState)history.pushState({},'',res.finalUrl);"
        "window.scrollTo(0,0);"
        "}).catch(function(){location.href=url;});"
        "}"
        "}else{"
        "document.getElementById('jbclose').addEventListener('click',function(){window.close();});"
        "ensurePlayer(function(){});"
        "currentConnectedStation=activeStationId();connectWs(currentConnectedStation);"
        "var floatBtn=document.getElementById('jbfloat');"
        "if('documentPictureInPicture' in window){"
        "floatBtn.hidden=false;"
        "floatBtn.addEventListener('click',function(){"
        "documentPictureInPicture.requestWindow({width:340,height:300}).then(function(pipWin){"
        "Array.prototype.forEach.call(document.styleSheets,function(sheet){"
        "try{"
        "if(sheet.ownerNode&&sheet.ownerNode.tagName==='STYLE'){"
        "var style=document.createElement('style');"
        "style.textContent=sheet.ownerNode.textContent;"
        "pipWin.document.head.appendChild(style);"
        "}"
        "}catch(e){}"
        "});"
        "pipWin.document.body.style.margin='0';"
        "pipWin.document.body.appendChild(document.getElementById('jbroot'));"
        "pipWin.addEventListener('pagehide',function(){"
        "document.body.appendChild(document.getElementById('jbroot'));"
        "});"
        "}).catch(function(e){"
        "console.error('Float above failed:',e);"
        "floatBtn.textContent='Couldn\\u2019t float \\u2014 try again';"
        "setTimeout(function(){floatBtn.textContent='Float above';},2500);"
        "});"
        "});"
        "}"
        "}"
        "tabC.addEventListener('click',function(){switchMode('community');});"
        "tabS.addEventListener('click',function(){switchMode('search');});"
        "playPauseBtn.addEventListener('click',function(){"
        "if(!ytPlayer)return;"
        "if(ytPlayer.getPlayerState()===YT.PlayerState.PLAYING){ytPlayer.pauseVideo();}"
        "else{if(mode==='community'&&lastCommunityId)ytPlayer.seekTo(lastCommunityElapsed,true);ytPlayer.playVideo();}"
        "});"
        "muteBtn.addEventListener('click',function(){"
        "if(!ytPlayer)return;"
        "muted=!muted;"
        "if(muted){ytPlayer.mute();muteBtn.textContent='Unmute';}else{ytPlayer.unMute();muteBtn.textContent='Mute';}"
        "});"
        "volumeSlider.addEventListener('input',function(){"
        "if(ytPlayer)ytPlayer.setVolume(Number(volumeSlider.value));"
        "});"
        // YouTube's own embedded player frequently pauses itself when its
        // tab goes to the background (a documented IFrame Player quirk,
        // distinct from — and in addition to — generic browser tab
        // throttling). Catch back up to the live community position
        // instead of leaving it stalled whenever the tab is visible again.
        "document.addEventListener('visibilitychange',function(){"
        "if(!document.hidden&&mode==='community'&&ytPlayer&&lastCommunityId){"
        "if(ytPlayer.getPlayerState()!==YT.PlayerState.PLAYING){"
        "ytPlayer.seekTo(lastCommunityElapsed,true);ytPlayer.playVideo();"
        "}"
        "}"
        "});"
        "updateTabs();renderStations();renderStationAdd();"
        "})();</script>";
    return s;
}

// The site-wide station (id "") plus every community this account belongs
// to, as a JS array-literal source -- not strict JSON, embedded directly
// into jukebox_script's <script> via string concatenation, so each field
// goes through js_string_literal the same way every other server string
// gets safely embedded into inline JS on this site.
std::string jukebox_stations_js(const std::string& user) {
    std::ostringstream os;
    os << "[{id:" << js_string_literal("") << ",name:" << js_string_literal("Site-wide") << "}";
    if (!user.empty()) {
        for (const auto& c : g_store.communities_of(user)) {
            os << ",{id:" << js_string_literal(c.slug) << ",name:" << js_string_literal(c.name) << "}";
        }
    }
    os << "]";
    return os.str();
}

// Floating embedded widget: phone-icon toggle + collapsible panel, injected
// on every logged-in page via wrap(). "Pop out" opens /listen in a real
// separate window — see jukebox_script's comment for why that, and not
// requestPictureInPicture, is what actually survives navigating this
// (deliberately multi-page) site.
std::string global_jukebox_widget(const std::string& yt_key, const std::string& user) {
    return
        "<button id=\"jbtoggle\" type=\"button\" aria-label=\"Jukebox\" "
        "style=\"position:fixed;right:1rem;bottom:1rem;z-index:30;width:52px;height:52px;"
        "border-radius:50%;background:var(--accent);color:var(--paper);border:0;"
        "font-size:1.4rem;line-height:52px;text-align:center;cursor:pointer;"
        "box-shadow:0 2px 10px rgba(0,0,0,.3)\">\xF0\x9F\x93\xB1</button>"
        "<div id=\"jbpanel\" hidden style=\"position:fixed;right:1rem;bottom:4.5rem;z-index:30;"
        "width:320px;max-width:92vw;max-height:min(75vh,540px);overflow-y:auto;"
        "background:var(--panel);border:1px solid var(--rule);"
        "border-radius:var(--rlg);padding:.9rem;box-shadow:0 6px 24px rgba(0,0,0,.35)\">"
        "<div id=\"jbroot\">" + jukebox_panel_body(false) + "</div>"
        "</div>"
        + jukebox_script(yt_key, false, jukebox_stations_js(user));
}

// The /listen popout: a real, independent top-level window. Once open, it
// keeps playing regardless of what the opener tab navigates to — this is
// what actually delivers "listen while browsing elsewhere" on a multi-page
// site, unlike requestPictureInPicture (blocked here by the YouTube embed
// being cross-origin) or the Document Picture-in-Picture API (tied to the
// opener document's lifetime — closes on real navigation, not just this
// site's choice). From inside *this* window, Float above uses Document PiP
// for real, since this window's own document doesn't navigate away.
std::string jukebox_standalone_page(const std::string& yt_key, const std::string& user) {
    return "<div id=\"jbroot\" style=\"max-width:400px;margin:0 auto\">"
           + jukebox_panel_body(true) + "</div>"
           + jukebox_script(yt_key, true, jukebox_stations_js(user));
}

// The "Add a track" input + live search box, shared between the owner's own
// queue and a guest's one-shot request -- identical UI, different POST
// target and different note underneath it.
std::string jukebox_add_form(const std::string& post_target, const std::string& note) {
    const char* yt_key = youtube_api_key_from_env();
    const char* placeholder = yt_key[0]
        ? "Paste a link, or type to search"
        : "Paste a YouTube link (search is off — site operator hasn't set YOUTUBE_API_KEY)";
    return "<div><label>Add a track</label>"
            "<div class=\"row\" style=\"align-items:flex-start\">"
            "<input id=\"jinput\" placeholder=\""+std::string(placeholder)+"\" autocomplete=\"off\" style=\"flex:1\">"
            "<button type=\"button\" id=\"jqueue\">Queue</button>"
            "</div>"
            "<div id=\"jsresults\" style=\"margin-top:.4rem;display:flex;flex-direction:column;gap:.2rem\"></div>"
            "</div>"
            "<p class=\"muted\">"+note+"</p>"
            "<script>(function(){"
            "var key="+js_string_literal(yt_key)+";"
            "var target="+js_string_literal(post_target)+";"
            "var input=document.getElementById('jinput'),out=document.getElementById('jsresults'),"
            "qbtn=document.getElementById('jqueue');"
            "var lastTopId=null,timer=null,gen=0;"
            "function esc(s){"
            "var ta=document.createElement('textarea');ta.innerHTML=s;"
            "var d=document.createElement('div');d.textContent=ta.value;return d.innerHTML;"
            "}"
            "function looksLikeUrl(s){return /^https?:\\/\\//i.test(s)||/youtu\\.?be/i.test(s);}"
            "function queueUrl(url){"
            "fetch(target,{method:'POST',"
            "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
            "body:'url='+encodeURIComponent(url)}).then(function(r){window.location=r.url||location.href;});"
            "}"
            "function render(items){"
            "out.innerHTML='';lastTopId=null;"
            "if(!items||!items.length)return;"
            "items.forEach(function(it,idx){"
            "var id=it.id.videoId,title=it.snippet.title,thumb=it.snippet.thumbnails.default.url;"
            "var row=document.createElement('div');row.className='row';"
            "row.style.cssText='align-items:center;cursor:pointer;padding:.3rem;border-radius:var(--rsm)';"
            "row.innerHTML='<img src=\"'+thumb+'\" width=\"60\" height=\"45\" alt=\"\" style=\"border-radius:4px\">"
            "<span style=\"flex:1\">'+esc(title)+'</span>';"
            "row.title='Click to queue this track';"
            "row.addEventListener('click',function(){"
            "queueUrl('https://www.youtube.com/watch?v='+id);"
            "});"
            "out.appendChild(row);"
            "if(idx===0){lastTopId=id;row.style.background='var(--parchment)';}"
            "});"
            "}"
            "function search(term){"
            "if(!key||!term){out.innerHTML='';return;}"
            "var my=++gen;"
            "fetch('https://www.googleapis.com/youtube/v3/search?part=snippet&type=video&maxResults=6&q='"
            "+encodeURIComponent(term)+'&key='+key)"
            ".then(function(r){return r.json();})"
            ".then(function(data){if(my!==gen)return;render(data.items);})"
            ".catch(function(){if(my!==gen)return;out.innerHTML='';});"
            "}"
            "input.addEventListener('input',function(){"
            "var val=input.value.trim();"
            "clearTimeout(timer);"
            "if(!val||looksLikeUrl(val)){out.innerHTML='';lastTopId=null;return;}"
            "timer=setTimeout(function(){search(val);},300);"
            "});"
            "function doQueue(){"
            "var val=input.value.trim();if(!val)return;"
            "queueUrl(looksLikeUrl(val)?val:(lastTopId?('https://www.youtube.com/watch?v='+lastTopId):val));"
            "}"
            "qbtn.addEventListener('click',doQueue);"
            "input.addEventListener('keydown',function(e){if(e.key==='Enter'){e.preventDefault();doQueue();}});"
            "})();</script>";
}

// Page-local jukebox widget (navigation option A: MPA, own instance per
// page, stops on navigate away — no site-wide SPA, no WebSocket sync).
// The owner always curates their own queue -- persistent, Prev/Next
// browsing, no cap beyond kJukeboxQueueCap. Depending on the owner's
// JukeboxPolicy, a signed-in friend or anyone else may also queue one
// track at a time: a one-shot guest request that's removed and logged to
// history the moment the embedded player reports it ended (see
// complete_jukebox_track). `viewer` is the signed-in username, or empty
// for an anonymous visitor.
std::string jukebox_widget(const std::string& owner, bool owner_view, const std::string& viewer,
                            const std::map<std::string,std::string>& qs) {
    auto tracks = g_store.jukebox_of(owner);
    auto owner_prefs = g_store.prefs_of(owner);
    const char* policy_badge = owner_prefs.jukebox_policy==JukeboxPolicy::Anyone ? "anyone may queue"
        : owner_prefs.jukebox_policy==JukeboxPolicy::Friends ? "friends may queue" : "owner curated";
    std::string html = "<div class=\"card jukebox\"><h2>Jukebox</h2>"
                        "<p class=\"muted eyebrow\">"+std::string(policy_badge)+"</p>";
    if (qs.count("jerr")) {
        const std::string& e = qs.at("jerr");
        if (e=="url") html += "<p class=\"warn\">That link wasn't recognized as a YouTube video.</p>";
        else if (e=="full") html += "<p class=\"warn\">Queue already holds "+std::to_string(kJukeboxQueueCap)+" tracks. Remove one first.</p>";
        else if (e=="denied") html += "<p class=\"warn\">You can't queue a track here.</p>";
        else if (e=="pending") html += "<p class=\"warn\">You already have a track queued. Wait for it to play first.</p>";
    }

    const bool is_guest = !owner_view && !viewer.empty();
    const bool is_friend = is_guest && g_store.are_friends(viewer, owner);
    const bool blocked = is_guest && g_store.is_blocked(owner, viewer);
    bool has_pending = false;
    if (is_guest) for (const auto& t : tracks) if (t.queued_by == viewer) { has_pending = true; break; }
    bool policy_allows = false;
    if (is_guest && !blocked) {
        if (owner_prefs.jukebox_policy == JukeboxPolicy::Anyone) policy_allows = true;
        else if (owner_prefs.jukebox_policy == JukeboxPolicy::Friends) policy_allows = is_friend;
    }
    const bool guest_can_add = is_guest && policy_allows && !has_pending;

    if (tracks.empty()) {
        html += "<p class=\"empty\">No tracks queued.</p>";
    } else {
        int idx = 0;
        auto it = qs.find("t");
        if (it != qs.end()) { try { idx = std::stoi(it->second); } catch (...) { idx = 0; } }
        if (idx < 0) idx = 0;
        if (idx >= static_cast<int>(tracks.size())) idx = static_cast<int>(tracks.size()) - 1;
        const auto& t = tracks[static_cast<std::size_t>(idx)];
        html += "<iframe id=\"jbplayer\" src=\"https://www.youtube-nocookie.com/embed/"+html_escape(t.video_id)+
                "?rel=0&enablejsapi=1\" "
                "title=\"Queued track\" allow=\"encrypted-media; picture-in-picture\" "
                "referrerpolicy=\"strict-origin-when-cross-origin\" allowfullscreen></iframe>"
                "<p class=\"muted\">Track "+std::to_string(idx+1)+" of "+std::to_string(tracks.size())+
                (t.queued_by != owner ? " · queued by "+html_escape(t.queued_by) : "")+"</p>"
                "<div class=\"row\">";
        if (idx > 0) html += "<a class=\"btn ghost\" href=\"?t="+std::to_string(idx-1)+"\">Prev</a>";
        if (idx+1 < static_cast<int>(tracks.size())) html += "<a class=\"btn ghost\" href=\"?t="+std::to_string(idx+1)+"\">Next</a>";
        if (owner_view) {
            html += "<form class=\"inline\" method=\"POST\" action=\"/me/jukebox/remove\">"
                    "<input type=\"hidden\" name=\"video_id\" value=\""+html_escape(t.video_id)+"\">"
                    "<button class=\"ghost\" type=\"submit\">Remove this track</button></form>";
        }
        html += "</div>";
        // Reports the currently playing track's real end (not just a click
        // away) so a guest's one-shot request actually gets one shot,
        // whoever is watching -- owner included. Requires being signed in;
        // an anonymous visitor can still watch, just never triggers it.
        if (!viewer.empty()) {
            html += "<script>(function(){"
                    "var owner="+js_string_literal(owner)+",videoId="+js_string_literal(t.video_id)+";"
                    "function fireComplete(){"
                    "fetch('/u/'+encodeURIComponent(owner)+'/jukebox/complete',{method:'POST',"
                    "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
                    "body:'video_id='+encodeURIComponent(videoId)}).then(function(){location.reload();});"
                    "}"
                    "var tag=document.createElement('script');tag.src='https://www.youtube.com/iframe_api';"
                    "document.head.appendChild(tag);"
                    "window.onYouTubeIframeAPIReady=function(){"
                    "new YT.Player('jbplayer',{events:{onStateChange:function(e){"
                    "if(e.data===YT.PlayerState.ENDED)fireComplete();"
                    "}}});"
                    "};"
                    "})();</script>";
        }
    }

    if (owner_view) {
        html += jukebox_add_form("/me/jukebox/add", "Visitors listen; navigating away stops playback.");
        auto sel = [&](JukeboxPolicy pol) -> std::string { return owner_prefs.jukebox_policy==pol?" checked":""; };
        // data-draft here is inert unless draft_publish_script() is also on
        // the page (only /me appends it) -- this same widget renders
        // unchanged, Save button and all, when owner_view is true on the
        // owner's own /u/<username> page, which has no draft bar to defer to.
        html += "<form method=\"POST\" action=\"/me/jukebox/policy\" data-draft=\"1\" style=\"margin-top:.75rem\">"
                "<label>Who else can queue a track here</label>"
                "<p><label><input type=\"radio\" name=\"policy\" value=\"0\""+sel(JukeboxPolicy::OwnerOnly)+"> Just me</label></p>"
                "<p><label><input type=\"radio\" name=\"policy\" value=\"1\""+sel(JukeboxPolicy::Friends)+"> Friends</label></p>"
                "<p><label><input type=\"radio\" name=\"policy\" value=\"2\""+sel(JukeboxPolicy::Anyone)+"> Anyone signed in</label></p>"
                "<p class=\"muted\">One request at a time per person; it plays once, then it's gone. "
                "Anyone you've blocked can never queue, regardless. Manage blocks in "
                "<a href=\"/settings\">Settings</a>.</p>"
                "<button>Save</button></form>";
    } else if (guest_can_add) {
        html += jukebox_add_form("/u/"+owner+"/jukebox/add", "One request at a time. It plays once, then it's gone.");
    } else if (is_guest && has_pending) {
        html += "<p class=\"muted\">You already have a track queued here.</p>";
    } else if (is_guest && !blocked && owner_prefs.jukebox_policy == JukeboxPolicy::Friends) {
        html += "<p class=\"muted\">Only "+html_escape(owner)+"'s friends can queue a track here.</p>";
    }

    auto hist = g_store.jukebox_history_of(owner);
    if (!hist.empty()) {
        std::reverse(hist.begin(), hist.end());
        html += "<div style=\"margin-top:1rem\"><label>Recently played</label>";
        std::size_t shown = 0;
        for (const auto& h : hist) {
            if (shown++ >= 10) break;
            html += "<p class=\"muted\" style=\"display:flex;align-items:center;gap:.4rem;margin:.3rem 0\">"
                    "<img src=\"https://img.youtube.com/vi/"+html_escape(h.video_id)+"/default.jpg\" "
                    "width=\"40\" height=\"30\" alt=\"\" style=\"border-radius:4px\">"
                    "<a href=\"https://www.youtube.com/watch?v="+html_escape(h.video_id)+"\" target=\"_blank\" rel=\"noopener\">watch again</a>"
                    " · "+(h.queued_by==owner?std::string("owner's pick"):html_escape(h.queued_by))+
                    " · "+html_escape(fmt_time(h.played_at))+"</p>";
        }
        html += "</div>";
    }

    html += "</div>";
    return html;
}

std::string genesis_body() {
    // A public stats strip, seen in the mobile design's landing screen --
    // real counts (account_count/all_communities), not the design's own
    // placeholder "1,284"; elections held stays honestly 0, same posture
    // as the /control voting card.
    std::string stats =
        "<div class=\"stattiles\">"
        "<div class=\"stattile\"><strong>" + std::to_string(g_store.account_count()) + "</strong><span>Accounts</span></div>"
        "<div class=\"stattile\"><strong>" + std::to_string(g_store.all_communities().size()) + "</strong><span>Communities</span></div>"
        "<div class=\"stattile\"><strong>0</strong><span>Elections held</span></div>"
        "</div>";
    // The scrolling problems ticker and the two pull-quotes, pulled from
    // the actual authored content in the mobile design's own source (its
    // PROBLEMS array and its two quote blocks) -- not from the rendered
    // preview, which never resolved its {{ }} bindings locally. Same list
    // doubled back-to-back so the CSS loop has no visible seam.
    static const std::vector<std::string> problems = {
        "Tax-funded pedophilia", "Propaganda", "Overclassification", "Corporate lobbying",
        "Bureaucracy", "Foreign wars", "Land lock-up", "Ag mismanagement", "Two-tier law",
        "Qualified immunity", "Emergency powers",
    };
    std::string ticker;
    for (int rep = 0; rep < 2; ++rep)
        for (const auto& p : problems)
            ticker += "<div>" + html_escape(p) + "</div>";
    std::string solution =
        "<div class=\"row\" style=\"align-items:flex-start;gap:1rem;flex-wrap:wrap\">"
        "<div style=\"flex:1;min-width:12rem\">"
        "<p class=\"eyebrow\" style=\"color:var(--accent)\">A legitimate solution to:</p>"
        "<div class=\"ticker-wrap\"><div class=\"ticker-roll\">" + ticker + "</div></div>"
        "</div>"
        "<div style=\"flex:0 0 12rem\" class=\"card\">"
        "<p class=\"eyebrow\" style=\"color:var(--accent)\">What it is not</p>"
        "<p style=\"margin:.6rem 0\"><strong>Not a party</strong><br>"
        "<span class=\"muted\">Nothing here asks you to pick a side, and no side owns it.</span></p>"
        "<p style=\"margin:.6rem 0\"><strong>Not a government</strong><br>"
        "<span class=\"muted\">No election has been held. Nothing here binds you to anything.</span></p>"
        "<p style=\"margin:.6rem 0\"><strong>Not a protest</strong><br>"
        "<span class=\"muted\">It is a structure you can test, argue with, and walk away from.</span></p>"
        "<p style=\"margin:.6rem 0\"><strong>Not for sale</strong><br>"
        "<span class=\"muted\">Your account is not a product and it is not anyone else's leverage.</span></p>"
        "</div></div>";
    // Two full-width cards, stacked -- not side by side. Matches the
    // source exactly: only the second (anonymous) quote is centered and
    // carries stars; the first (attributed) quote is left-aligned,
    // unstarred, on a plain card, same as the design's own two different
    // treatments for a named vs. an anonymous testimonial.
    std::string quotes =
        "<div class=\"card quote\"><p>&ldquo;These aren't &lsquo;people problems.&rsquo; They're structural "
        "problems. Good people placed inside broken structures produce broken outcomes. Broken people placed "
        "inside well-designed structures are naturally corrected by them.&rdquo;</p>"
        "<p class=\"attrib\">&mdash; Brandon Bottin</p></div>"
        "<div class=\"card quote centered\"><p class=\"stars\">&#9733;&#9733;&#9733;&#9733;&#9733;</p>"
        "<p>&ldquo;It's like a ballot box and Myspace had a baby.&rdquo;</p>"
        "<p class=\"attrib muted\">&mdash; Anonymous</p></div>";
    // The decorative emblem from the top of the design's own welcome
    // screen -- a triangle (the three laws) with a droplet at its
    // center and a ringed node at each vertex, wrapped in a faint
    // circle. Pure SVG with no text content, which is why it never
    // showed up in an accessibility-tree read of the design -- found by
    // reading the .dc.html's raw markup instead.
    std::string emblem =
        "<div style=\"text-align:center\"><svg viewBox=\"0 0 180 172\" width=\"140\" height=\"134\" aria-hidden=\"true\">"
        "<circle cx=\"90\" cy=\"96\" r=\"54\" fill=\"none\" style=\"stroke:var(--rule)\" stroke-width=\"1\"/>"
        "<path d=\"M90 30 L32 130 L148 130 Z\" fill=\"none\" style=\"stroke:var(--stone)\" stroke-width=\"1\"/>"
        "<path d=\"M90 70c0 0 13 17.5 13 24.5a13 13 0 0 1-26 0C77 87.5 90 70 90 70z\" style=\"fill:var(--accent)\"/>"
        "<circle cx=\"90\" cy=\"30\" r=\"6\" style=\"fill:var(--paper);stroke:var(--accent)\" stroke-width=\"1.4\"/>"
        "<circle cx=\"32\" cy=\"130\" r=\"6\" style=\"fill:var(--paper);stroke:var(--accent)\" stroke-width=\"1.4\"/>"
        "<circle cx=\"148\" cy=\"130\" r=\"6\" style=\"fill:var(--paper);stroke:var(--accent)\" stroke-width=\"1.4\"/>"
        "</svg></div>";
    // The punchy three-word law strip directly under the hero divider,
    // real content from the design's own `laws` array (01 Do not take /
    // 02 Do not wreck / 03 Do not force) -- a different, shorter phrasing
    // than the longer "Do not steal/damage/deprive of liberty" cards
    // further down the page, which is how the source itself uses them:
    // a punchy strip up top, the fuller explanation later.
    std::string lawstrip =
        "<div class=\"lawstrip\">"
        "<div><p class=\"n\">01</p><p class=\"t\">Do not take</p></div>"
        "<div><p class=\"n\">02</p><p class=\"t\">Do not wreck</p></div>"
        "<div><p class=\"n\">03</p><p class=\"t\">Do not force</p></div>"
        "</div>"
        "<p class=\"muted\" style=\"text-align:center;max-width:22rem;margin:.7rem auto 0\">"
        "Three rules hold. Whoever you are, wherever you are starting from, the rest is your call.</p>";
    // "What is inside" in the source is a 2x2 grid of feature cards --
    // Live rooms, People near you, Meetups, and a fourth. Only Live rooms
    // maps to something real (/world plus the jukebox stations); the
    // other two are exactly the geolocation/meetup concepts already set
    // aside as having no backend at all, so only this one card is built,
    // per Brandon's own call rather than presenting the other two as if
    // they existed. Icon and copy are the source's own (rect+two-dot
    // glyph, "Live rooms"), with "Studio" dropped from the description
    // since that feature doesn't exist either.
    std::string liveRoomsIcon =
        "<svg viewBox=\"0 0 24 24\" width=\"19\" height=\"19\" fill=\"none\" style=\"stroke:var(--accent)\" "
        "stroke-width=\"1.5\" stroke-linecap=\"round\" stroke-linejoin=\"round\">"
        "<rect x=\"3.2\" y=\"4.8\" width=\"17.6\" height=\"14.4\" rx=\"2.2\"/>"
        "<circle cx=\"9.2\" cy=\"11.4\" r=\"1.7\" style=\"fill:var(--accent)\" stroke=\"none\"/>"
        "<circle cx=\"15\" cy=\"15.2\" r=\"1.7\" style=\"fill:var(--accent)\" stroke=\"none\"/>"
        "</svg>";
    std::string inside =
        "<p class=\"eyebrow\" style=\"color:var(--accent)\">What is inside</p>"
        "<div class=\"card\" style=\"display:flex;gap:.8rem;align-items:flex-start\">"
        "<div style=\"width:2.2rem;height:2.2rem;border-radius:.5rem;background:var(--paper);"
        "border:1px solid var(--rule);display:flex;align-items:center;justify-content:center;flex-shrink:0\">" +
        liveRoomsIcon + "</div>"
        "<div><strong>Live rooms</strong>"
        "<p class=\"muted\" style=\"margin:.3rem 0 0\">The Quad and Juketube. Shared spaces you can actually "
        "be in. <a href=\"/world\">Walk into the Quad</a>.</p></div></div>";
    return emblem +
           "<p class=\"eyebrow\" style=\"color:var(--accent)\">A Voluntary Natural Democracy</p>"
           "<h1>This is genesis, not a government.</h1>"
           "<p class=\"lede\">LiquidGov.US is a voluntary association and working software for "
           "liquid, natural-law democracy. Anyone here is a guest or a member of that association.</p>"
           "<hr style=\"border:0;border-top:1px solid var(--rule);margin:1rem 0\">" +
           lawstrip +
           stats + solution + quotes + inside +
           "<div class=\"card\"><p>Nobody on this site is your city, state, or federal government. "
           "No office seated from these pages has legal authority over anyone who did not choose to associate.</p>"
           "<p>Website control is temporary. When a working election exists among enrolled citizens, "
           "that control is meant to end. The internal shape of that election is itself a citizen vote. "
           "Real governmental authority is a later, explicit landslide — Phase 2 — not this page and not "
           "the first people who show up.</p>"
           "<p class=\"warn\">Until then, genesis operators run the machines and say so in public. "
           "That is a disclosure, not a claim of sovereignty.</p></div>"
           "<div class=\"laws\">"
           "<div class=\"law\"><strong>Do not steal</strong><p class=\"muted\">Property stays with the person who holds it. Taking it by motion is the forbidden act.</p></div>"
           "<div class=\"law\"><strong>Do not damage</strong><p class=\"muted\">Harm to person or goods is outside the association’s license.</p></div>"
           "<div class=\"law\"><strong>Do not deprive of liberty</strong><p class=\"muted\">No one here may cage another’s lawful movement or speech by force of this software.</p></div>"
           "</div>"
           "<div class=\"clocks\">"
           "<div class=\"card\"><h2>Two clocks</h2><p><strong>Site control</strong> ends when a working election exists among enrolled citizens.</p>"
           "<p><strong>Governmental authority</strong> stays a later landslide — more than half of the eligible citizen population. However long that takes.</p></div>"
           "<div class=\"card\"><h2>This website is the door</h2><p>Feed, friends, mail, and communities live here. "
           "Voting weight, offices, land, and judgments live in a separate governance kernel. "
           "A platform account is not a CitizenId. Combined verification for civic acts is not live.</p></div>"
           "</div>";
}

// A real dashboard for a returning user, in the spirit of the mobile
// design's home screen (stat tiles, a live "rooms open now" list) --
// built from data the site already has (posts_by/friends_of/communities_of,
// the live /world player map, per-station jukebox now-playing state)
// rather than any of that design's new, unbuilt concepts (Studio, geolocated
// "people near you").
std::string home_dashboard(const std::string& user) {
    const auto posts = g_store.posts_by(user).size();
    const auto friends = g_store.friends_of(user).size();
    const auto committees = g_store.communities_of(user);

    size_t world_here = 0;
    {
        std::lock_guard<std::mutex> lock(g_world_mu);
        world_here = g_world_players.size();
    }

    // The design's home screen leads with a prominent Quad callout card
    // (gradient card, live headline, "Walk in" CTA) above the plain rooms
    // list, not just another row in it -- found in the raw source since
    // an accessibility-tree read shows the same text either way and
    // doesn't distinguish "one row among several" from "the lead card."
    // The design's own copy ("2 here now", "Somebody is in the Quad.")
    // was a static example, not template-bound -- built here with real
    // conditional copy off the actual live world_here count instead.
    std::string quadHeadline = world_here == 0 ? "Nobody's in the Quad right now."
        : world_here == 1 ? "Somebody is in the Quad."
        : std::to_string(world_here) + " people are in the Quad.";
    std::string quadCard =
        "<a class=\"card quadcard\" href=\"/world\">"
        "<p class=\"eyebrow\" style=\"color:var(--accent)\">The Quad &middot; " +
        (world_here == 0 ? std::string("quiet") : std::to_string(world_here) + " here now") + "</p>"
        "<p class=\"quadhead\">" + quadHeadline + "</p>"
        "<p class=\"muted\">Walk in and see who is around. No call to join, no scheduling.</p>"
        "<span class=\"btn\" style=\"margin-top:.6rem\">Walk in &rarr;</span></a>";

    std::string rooms;
    for (const auto& c : committees) {
        bool on_air = false;
        {
            std::lock_guard<std::mutex> lock(g_community_mu);
            auto it = g_community_now_by_station.find(c.slug);
            on_air = (it != g_community_now_by_station.end() && it->second.active);
        }
        char ini_c = c.name.empty() ? '?' : static_cast<char>(std::toupper(static_cast<unsigned char>(c.name[0])));
        rooms += "<a class=\"roomrow\" href=\"/c/" + html_escape(c.slug) + "\"><span class=\"roomini\">" +
                 html_escape(std::string(1, ini_c)) + "</span>"
                 "<span class=\"roomname\">" + html_escape(c.name) + "<br><span class=\"muted\">/" +
                 html_escape(c.slug) + "</span></span>"
                 "<span class=\"roomlive\">" + (on_air ? "on air" : "quiet") + "</span></a>";
    }

    return
        "<div class=\"card\"><p class=\"eyebrow\">Welcome back</p><h1>@" + html_escape(user) + "</h1>"
        "<div class=\"stattiles\">"
        "<div class=\"stattile\"><strong>" + std::to_string(posts) + "</strong><span>Posts</span></div>"
        "<div class=\"stattile\"><strong>" + std::to_string(friends) + "</strong><span>Friends</span></div>"
        "<div class=\"stattile\"><strong>" + std::to_string(committees.size()) + "</strong><span>Communities</span></div>"
        "</div>"
        "<p class=\"row\"><a class=\"btn\" href=\"/feed\">Open the feed</a> "
        "<a class=\"btn ghost\" href=\"/me\">Your profile</a></p></div>" +
        quadCard +
        (rooms.empty() ? "" : "<div class=\"card\"><h2>Rooms open now</h2>" + rooms + "</div>");
}

// The mobile design reads the charter as a chaptered, one-screen-at-a-time
// flow ("Chapter 2 of 8 · 6 min", a "You can stop here" closer). The
// design's own 8-title outline had no body text behind it -- the real
// source turned out to be the actual AVND charter document Brandon
// pointed to directly, which already splits into 14 real, numbered
// sections. Used those 14 as-is rather than forcing the design's looser
// 8-title grouping onto text that doesn't actually divide that way.
//
// The charter describes the full Phase 2 civic mechanism (blockchain
// voting, land ledger, sortition courts, donation-based pay) that this
// platform does not implement and isn't claiming to -- but the document
// already says so itself (Section 13: "This initial adoption period
// constitutes Phase 1... Phase 2 does not begin automatically"), which
// is exactly this site's own existing posture elsewhere ("Real
// governmental authority is a later, explicit landslide -- Phase 2").
// Ported the substantive 14 sections verbatim; cut only the document's
// own closing meta paragraphs (a call for a co-builder, a P.S. email
// solicitation) since those are about writing the document, not part of
// its civic content.
struct LawChapter { std::string title; int minutes; std::string body; };

const std::vector<LawChapter>& law_chapters() {
    static const std::vector<LawChapter> chapters = {
        {"The Problem", 2,
         "<p>Every governance system in human history has faced the same fundamental vulnerability: power without consequence.</p>"
         "<p>When those who govern are insulated from the direct results of their decisions, three things reliably follow. "
         "Corruption takes root, because concentrated power attracts those seeking to exploit it. Incentives misalign, because "
         "officials become focused on keeping their position rather than serving the people who granted it. And accountability "
         "disappears, because the feedback loop between citizen dissatisfaction and official behavior is too slow, too indirect, "
         "and too easy to manipulate.</p>"
         "<p>These aren't &ldquo;people problems.&rdquo; They're structural problems. Good people placed inside broken structures "
         "produce broken outcomes. Broken people placed inside well-designed structures are naturally corrected by them.</p>"
         "<p>Representative democracy was designed for a world without the technology to allow direct citizen participation at "
         "scale. That world no longer exists. Yet we keep operating systems built on the assumption that citizens can't govern "
         "themselves — that they need professional intermediaries to translate their will into policy. Those intermediaries have, "
         "predictably, accumulated power, shielded themselves from consequence, and optimized for their own survival instead of "
         "the people they were meant to serve.</p>"
         "<p>Complex legal systems compound the problem. Law that requires professional interpretation to navigate inevitably "
         "benefits whoever can afford that navigation, while everyone else pays the toll. Law becomes a tool of consolidation "
         "instead of protection.</p>"
         "<p>This framework proposes something different — not a reform of existing systems, but a reconstruction from the ground "
         "up, built on natural law, voluntary participation, radical transparency, and real-time accountability. A system where "
         "everyone in it, from the newest citizen to the highest official, reaps exactly what they sow.</p>"},
        {"Foundational Principles", 2,
         "<p>A Voluntary Natural Democracy isn't built on ideology. It's built on observation. Ask one question — what kind of "
         "structure produces the best outcomes for the most people over the longest time? — and these principles are the answer.</p>"
         "<p><strong>Voluntarism.</strong> No participation is coerced. Citizens opt in. Officials serve because citizens choose "
         "to support them. Laws beyond the three universal laws are recommendations, not mandates. A government that has to earn "
         "its legitimacy every day is a government that can't afford to stop serving its people.</p>"
         "<p><strong>Anti-Authoritarianism.</strong> Authority here is always elected, temporary, conditional, and reversible. No "
         "official holds power that can't be withdrawn in real time. No law exists that can't be revised.</p>"
         "<p><strong>Radical Transparency.</strong> Every vote, donation, land transaction, and official decision is publicly "
         "documented on the national blockchain. Corruption requires darkness. Accountability requires light.</p>"
         "<p><strong>Natural Consequence.</strong> You reap what you sow. This runs through the whole system — the universal "
         "laws, the donation-based pay system, the land system, the judicial outcome record.</p>"
         "<p><strong>Diversity and Flexibility.</strong> Monoculture is toxic to any living system. This framework doesn't "
         "prescribe one way of living. It prescribes a structure within which communities determine those things for themselves, "
         "evolve, and correct their own mistakes.</p>"},
        {"The Three Universal Laws", 2,
         "<p>Most legal systems fail the same way governance systems fail — they accumulate complexity, and complexity "
         "concentrates power in the hands of whoever can navigate it.</p>"
         "<p>A Voluntary Natural Democracy recognizes three universal laws. They apply to every person on national territory, "
         "citizen or not, regardless of wealth or position. No one is above them, and no one is beneath their protection:</p>"
         "<p>If you steal, something of yours may be stolen.<br>If you give damage, you may take damage.<br>"
         "If you strip someone of their liberty, similarly, you may find your liberty being stripped from you.</p>"
         "<p><strong>&ldquo;May&rdquo; is deliberate.</strong> These laws set a ceiling on proportional response, not a mandatory "
         "sentence. A judge isn't required to impose the maximum — they're empowered to judge gracefully and creatively within "
         "the range the law allows.</p>"
         "<p><strong>Consequence is judicial, not personal.</strong> The universal laws describe what a court may impose — they "
         "are not a license for a wronged party to take matters into their own hands. Private retaliation is a new violation of "
         "the law, not an exercise of it.</p>"
         "<p><strong>The laws are self-referential.</strong> A judge who systematically strips liberty from any particular group "
         "is themselves in violation of the third law. The enforcers are bound by them as tightly as anyone they judge.</p>"
         "<p><strong>Simplicity as equality.</strong> Three laws a child can memorize are three laws any citizen can invoke "
         "without a lawyer. That simplicity is itself a form of justice.</p>"},
        {"Identity", 2,
         "<p>Every mechanism in this framework — voting, land, courts, donations — depends on one thing working correctly: "
         "knowing that each citizen is one real, unique person. Get this wrong and every other safeguard is decoration.</p>"
         "<p><strong>No national ID authority.</strong> A single issuing body is a single point of capture. Instead, identity "
         "verification is a private, competitive market, governed by the same anti-monopolism rules as any other service.</p>"
         "<p><strong>How it works.</strong> Independent verifiers confirm a registrant is a unique living person, typically "
         "through a biometric check, and bind that check to the citizen's blockchain key. The biometric itself never touches "
         "the chain — only a signed, hashed proof of &ldquo;this key belongs to one unique person&rdquo; does.</p>"
         "<p><strong>Fraud is expensive, not free.</strong> A verifier that knowingly certifies a fake or duplicate identity is "
         "enabling theft of voting power and is prosecutable under the first universal law.</p>"
         "<p><strong>Non-citizens have standing too.</strong> Legal identity and citizenship are separate things. Anyone present "
         "on the territory enrolls with a verifier to have legal standing in court, since the three universal laws bind everyone "
         "regardless of status. Citizenship — voting rights, land entitlement, point accumulation — is a separate, later opt-in "
         "layered on top.</p>"},
        {"Governance Structure", 3,
         "<p>Three branches, each with a distinct function, each checking the others, all of them accountable to the citizenry "
         "in real time. There is no legislative branch — the citizenry performs that function directly through the blockchain "
         "platform.</p>"
         "<p><strong>The Executive Branch</strong> administers the nation, enforces the universal laws, maintains the military, "
         "and implements the outcomes of citizen votes. It's headed by a president and vice president serving as a unified pair. "
         "No official draws a fixed salary — every official and service member earns exactly what citizens choose to donate to "
         "them individually.</p>"
         "<p><strong>The Officials' Contract.</strong> Before assuming office, every official signs a lawful, binding guarantee "
         "of honesty for the duration of their term, with no exception clause. If the guarantee is broken, consequence follows, "
         "determined by citizen vote.</p>"
         "<p><strong>The Judicial Branch</strong> interprets and applies the universal laws at local and national levels. Local "
         "judges handle day-to-day cases with wide creative discretion. Higher review for cases involving officials or systemic "
         "violations is handled by <strong>sortition panels</strong> — citizens randomly selected for a single case, the way a "
         "jury is called today. No permanent seat exists to bribe or pressure, because no panel outlives its case.</p>"
         "<p><strong>The Civic Branch</strong> is structurally independent from the other two. Its sole job is to maintain, "
         "communicate, and honestly document the reasoning and historical record of the nation. It holds no enforcement power "
         "and controls no resources beyond its own donation-based operating funds. It doesn't propagandize. It documents.</p>"},
        {"The Citizen Legislature and Vote Points", 2,
         "<p>There's no professional legislative class here. Citizens propose, debate, and vote on policy directly through the "
         "blockchain. Any citizen can submit a proposal; every proposal is public; the most supported proposals rise through "
         "engagement, not lobbying: citizen proposes &rarr; blockchain vote &rarr; executive implements &rarr; judiciary reviews "
         "for universal-law compliance.</p>"
         "<p><strong>Vote points.</strong> Every citizen earns one vote point per complete year of citizenship, regardless of "
         "age or background — a point total is tied to a living person and disappears at death. It can't be willed, sold, or "
         "inherited.</p>"
         "<p>Points can be applied in full to one candidate or proposal, split across several, or delegated to a representative "
         "who can use or further delegate them, and withdrawn or reassigned at any time.</p>"
         "<p><strong>Ballots are secret; delegation is not a loophole for coercion.</strong> A citizen's own vote is encrypted — "
         "they can verify it themselves but can't be compelled to prove it to anyone else. Who delegated to whom is not "
         "individually traceable, only the delegate's total power is public. No delegate may hold more than a fixed ceiling of "
         "total national vote points, and delegation chains are one-directional — no citizen can delegate back into a loop that "
         "returns power to themselves.</p>"
         "<p>There's no minimum voting age and no criminal disqualification. The only requirement is that a citizen is capable "
         "of truly and safely casting their own vote.</p>"},
        {"Succession, and the National Blockchain Platform", 1,
         "<p><strong>Succession.</strong> The president and vice president serve as a pair. If the president loses majority "
         "support, the office passes immediately to the new majority leader and their partner. For all other positions, "
         "succession moves to the next role in the organizational hierarchy in a caretaker capacity, restricted from major "
         "unilateral decisions until a new official earns majority approval.</p>"
         "<p><strong>The platform.</strong> The blockchain is the voting system, the public record, the donation mechanism, the "
         "idea registry, the judicial outcome database, the civic communication channel, and the land ledger, all at once. Every "
         "citizen's profile documents their vote history, idea record, land holdings, donations given and received, and public "
         "civic contributions — accessible continuously, because accountability doesn't keep business hours.</p>"},
        {"The Land System", 4,
         "<p>Land is the foundation of independence. A person with secure access to arable land can feed and shelter themselves "
         "without depending on an employer, landlord, or government. This framework treats that as a design constraint, not an "
         "afterthought.</p>"
         "<p>Total land divides into three pools: <strong>citizen-allocable land</strong> (the pool the per-citizen cap is "
         "calculated against), <strong>reserved nature land</strong> (held in permanent public trust, never transactable by "
         "anyone, including the government), and <strong>infrastructure land</strong> (what actual government function needs). "
         "The reserved and infrastructure pools may only grow from land that is unclaimed or voluntarily dedicated, never from "
         "land already held under an individual's bond.</p>"
         "<p><strong>The cap.</strong> Every citizen is entitled to an allotment equal to the citizen-allocable pool divided by "
         "the population, measured in total assessed property bond value — not raw acreage. No individual may hold above the "
         "cap regardless of wealth or status. Within it, land is freely divided, combined, bought, sold, or gifted.</p>"
         "<p><strong>Bonds and auctions.</strong> Every parcel is held with a bond equal to its assessed value, reassessed "
         "periodically using recent blockchain transaction history. When a parcel sells, it's listed at bond value and "
         "auctioned publicly.</p>"
         "<p><strong>Inheritance and gifting.</strong> Property passing to an heir still goes through public auction — the "
         "recipient must meet bond value or match the highest bid. <strong>Exception:</strong> an heir or long-term occupant of "
         "a primary residence has right of first refusal at bond value.</p>"
         "<p><strong>No corporate land ownership.</strong> Businesses lease from individual citizen owners at market rate. "
         "Aggregate leasehold size per business is tracked and published on the blockchain.</p>"
         "<p><strong>Community infrastructure.</strong> Roads, parks, and shared spaces are built through voluntary dedication "
         "— citizens allocate part of their personal allotment to shared use, publicly recorded.</p>"
         "<p><strong>Transition at adoption.</strong> Existing owners keep their current holdings; nothing is seized. Owners "
         "over the new cap divest the excess through the standard auction process within a transition window set by citizen "
         "vote.</p>"},
        {"The Economic Framework", 3,
         "<p>A Voluntary Natural Democracy is a free market. Nearly all goods and services come from private individuals and "
         "businesses competing openly. The framework's job is to keep the conditions for genuine competition structurally "
         "intact, not to run the economy itself.</p>"
         "<p><strong>Privatization.</strong> Everything beyond the executive office and military is privately owned and "
         "operated. Citizens fund the two institutions that can't reasonably be privatized — governance and defense — through "
         "voluntary donation.</p>"
         "<p><strong>Anti-monopolism.</strong> A monopoly isn't the product of a free market; it's the product of a market "
         "that's stopped being free. Full transparency of revenue, market share, and pricing on the blockchain lets citizens "
         "and competitors see concentration developing in real time; judicial review under the third universal law applies "
         "when control of a necessity crosses from inconvenient into a form of leverage over people's lives.</p>"
         "<p><strong>Donation-based compensation.</strong> Officials and service members earn only what citizens choose to "
         "donate them individually, fully visible on the blockchain. Reputational weight for officials rewards "
         "<strong>breadth of support over raw donation size</strong>, so an official funded by ten thousand ordinary citizens "
         "outweighs one funded by ten wealthy ones giving the same total.</p>"
         "<p><strong>Idea credit.</strong> Every adopted proposal is permanently credited to its originator, building a public "
         "record of civic contribution that naturally surfaces capable people for public roles.</p>"},
        {"Cultural Infrastructure", 2,
         "<p>Structure alone doesn't sustain a nation — culture does. A well-designed system inside a culture of apathy gets "
         "hollowed out over time. A well-designed system inside a culture of voluntary participation and honest accountability "
         "strengthens with every generation.</p>"
         "<p>The foundational values — voluntarism and anti-authoritarianism — aren't slogans. They're practical orientations "
         "toward power that have to be actively taught, not assumed. That teaching is the civic branch's entire job.</p>"
         "<p>The transition from Phase 1 into Phase 2 is, itself, ultimately a cultural shift more than a structural one — and "
         "when the time comes, it should be pushed to happen quickly and gracefully, or we risk permanent mission failure.</p>"
         "<p>Accountability alone isn't the whole picture. Once Phase 1 closes and Phase 2 begins, this culture has to actively "
         "cultivate an environment in which officials are genuinely well-regarded, not merely tolerated or presumed suspect.</p>"
         "<p>Diversity of community norms isn't a flaw here — it's how the system learns. Different communities try different "
         "approaches within the universal-law floor, generate real outcome data, and that data is visible to every other "
         "community deciding what to try next.</p>"},
        {"Population and Immigration", 2,
         "<p>Open citizenship is a core value here, but unmanaged growth has a real cost: since land entitlement is calculated "
         "per capita, every new citizen dilutes existing citizens' allotment. The fix can't be a gatekeeper deciding who's "
         "worthy of entry — that recreates exactly the kind of unaccountable authority this system exists to prevent.</p>"
         "<p><strong>The rate is set democratically, not administratively.</strong> Immigration quota is a standing proposal "
         "in the citizen legislature, voted on and re-votable through the same blockchain pipeline as any other policy.</p>"
         "<p><strong>The cap is equal for everyone, and restraint is cultural, not mandated.</strong> Every citizen is entitled "
         "to the same per-capita cap. Whatever portion of a citizen's cap they choose not to claim — their "
         "<strong>personal safety buffer</strong> — is pooled back into the citizen-allocable land, forming the practical "
         "reserve new citizens draw their entitlement from.</p>"
         "<p>There's also a natural brake built into the system for free: as population grows, per-capita land shrinks, bond "
         "values rise, and leasing gets more expensive — a price signal visible to prospective migrants before they ever "
         "arrive.</p>"},
        {"Emergency Provisions", 1,
         "<p>Direct citizen legislation is a strength in ordinary governance and a liability in a genuine crisis — a natural "
         "disaster or an active attack doesn't wait for a blockchain vote to resolve.</p>"
         "<p>To close that gap without creating a permanent emergency-powers backdoor, the executive may be granted "
         "<strong>narrow, time-boxed authority</strong> — but only under objective trigger conditions defined in advance by "
         "citizen vote, never self-declared by the executive in the moment. Emergency authority automatically sunsets after a "
         "fixed window unless renewed by a fresh citizen vote, and every action taken under it is fully logged and subject to "
         "post-hoc judicial review. An emergency clause that lets the executive decide when the emergency exists isn't an "
         "emergency clause — it's a coup provision. This one is built so it can't become that.</p>"},
        {"Initial Adoption Votes", 2,
         "<p>This framework defines the structure. It deliberately does not pre-decide every parameter a real community needs "
         "to run on — those are values a community sets for itself, through the same citizen-vote pipeline that governs "
         "everything afterward.</p>"
         "<p>This initial adoption period constitutes <strong>Phase 1</strong>. <strong>Phase 2</strong> does not begin "
         "automatically or by informal consensus — it requires a formal citizen vote demonstrating landslide support, defined "
         "as support exceeding 50% of the eligible citizen population, confirming the framework and its Phase 1 parameters as "
         "settled, ongoing practice. Until that vote passes, the nation continues operating under Phase 1, however long that "
         "takes.</p>"
         "<p>Items voted at adoption span land pool sizes and reassessment intervals, immigration quota and verifier "
         "accreditation standards, which existing government functions map to which branch, the specific trigger conditions "
         "and sunset window for emergency authority, and the selection method for sortition-based higher review panels. "
         "Citizens vote on each with full visibility into the current default, who it affects, and how reversible it is — "
         "informed consent to the rules they're about to live under.</p>"},
        {"A Living Document", 1,
         "<p>This framework is a living recommendation, not a monument. It's a starting point, not a final word. Every element "
         "of it is open to revision, improvement, and democratic evolution by the people who choose to live inside it. Good "
         "ideas that improve it should be proposed, credited to whoever proposed them, debated openly, and adopted if they "
         "earn majority support.</p>"
         "<p>The only elements not subject to revision are the three universal laws. They're the constitutional floor beneath "
         "which no majority may reach — because a majority that can vote away the prohibition on theft, damage, and liberty "
         "violation is a majority that can vote itself into tyranny. Everything else is negotiable. Everything else can be "
         "improved. That's the point.</p>"
         "<p>A Voluntary Natural Democracy isn't a destination. It's a direction — toward a society where power is always "
         "accountable, participation is always meaningful, the land beneath your feet is genuinely yours, and the most "
         "reliable path to personal prosperity is making the people around you more prosperous too.</p>"
         "<p>The rest is up to you!</p>"},
    };
    return chapters;
}

std::string laws_body() {
    const auto& chapters = law_chapters();
    std::string list;
    for (size_t i = 0; i < chapters.size(); ++i) {
        list += "<a class=\"roomrow\" href=\"/laws/" + std::to_string(i+1) + "\">"
                "<span class=\"roomini\">" + std::to_string(i+1) + "</span>"
                "<span class=\"roomname\">" + html_escape(chapters[i].title) + "</span>"
                "<span class=\"roomlive\">~" + std::to_string(chapters[i].minutes) +
                (chapters[i].minutes==1?" min":" min") + "</span></a>";
    }
    return "<h1>A Voluntary Natural Democracy</h1>"
           "<p class=\"lede\">The three universal laws are the floor — the one part of this no majority can vote away. "
           "Everything past that is proposed structure, not something this platform runs today. Read as much or as little "
           "as you want.</p>"
           "<div class=\"card\"><h2>Chapters</h2>" + list + "</div>"
           "<p class=\"muted\"><a href=\"/control\">Who holds control of this site</a> · <a href=\"/\">Genesis disclosure</a></p>";
}

std::string laws_chapter_body(size_t n) {
    const auto& chapters = law_chapters();
    const auto& ch = chapters[n-1];
    std::string nav = "<p class=\"row\"><a class=\"btn ghost\" href=\"/laws\">&larr; Back to overview</a>";
    if (n < chapters.size())
        nav += " <a class=\"btn\" href=\"/laws/" + std::to_string(n+1) + "\">Next: " +
               html_escape(chapters[n].title) + " &rarr;</a>";
    nav += "</p>";
    return "<p class=\"eyebrow\">Chapter " + std::to_string(n) + " of " + std::to_string(chapters.size()) + "</p>"
           "<h1>" + html_escape(ch.title) + "</h1>"
           "<div class=\"card\">" + ch.body + "</div>" +
           (n == chapters.size()
             ? "<p class=\"muted\">You can stop here. Most people do, and that's fine — the rest is mechanism, "
               "and most of that is still a citizen vote.</p>"
             : "") +
           nav;
}

std::string control_body() {
    return "<h1>Who holds control</h1>"
           "<p class=\"lede\">Genesis operators run the machines and say so in public.</p>"
           "<div class=\"card\"><p><strong>Operator of this copy:</strong> Brandon Bottin.</p>"
           "<p><strong>As of:</strong> September 2026.</p>"
           "<p>That name is a disclosure of who can change this software and this store today. "
           "It is not an office. It is not a claim that the association already governs anyone.</p>"
           "<p class=\"warn\">Control of the website is meant to end when a working election exists among enrolled citizens. "
           "Governmental authority is a later landslide, not this page.</p>"
           "<p class=\"muted\">This is a Beta build of the association’s door — some of what's described here is still being built.</p></div>"
           "<div class=\"card\"><p class=\"eyebrow\">Who runs this</p>"
           "<table class=\"record\">"
           "<tr><td>This site</td><td>Held by one person. Temporary, and replaced the moment there is anything to replace it with.</td></tr>"
           "<tr><td>Your account</td><td>A platform account. Not a citizen identity, not a vote, not standing.</td></tr>"
           "<tr><td>Elections</td><td>None held. Real authority would come later, from a working election.</td></tr>"
           "<tr><td>Your data</td><td>Your number is never stored. <a href=\"/export\">Export</a> or delete everything whenever you want.</td></tr>"
           "</table></div>"
           "<div class=\"card\"><p class=\"eyebrow\">Voting</p><h3>Nothing is awaiting your vote.</h3>"
           "<p class=\"muted\">No election has been held. This exists so the mechanism is testable, not "
           "because anyone here holds a seat.</p>"
           "<p class=\"muted\">Your account is a platform account. It is not a citizen identity and it "
           "casts no vote.</p>"
           "<p><a href=\"/proposals\">Proposals</a> are the one real mechanism live today — an unofficial "
           "straw poll any member can open or vote on, explicitly not a binding vote.</p></div>";
}

std::string signup_page(const std::string& navuser, int unread, const std::string& err,
                        const std::string& username, const std::string& phone) {
    std::string e;
    if (!err.empty()) e = "<p class=\"warn\">"+html_escape(err)+"</p>";
    return wrap("Sign up", navuser, unread,
        "<div class=\"card\"><h1>Create an account</h1>"
        "<p class=\"muted\">Username is one word: letters, numbers, dots, underscores, hyphens. "
        "No spaces, no @. Phone is a signup binding, not the daily login. "
        "This is a platform account, not a CitizenId.</p>"+e+
        "<form method=\"POST\" action=\"/signup\">"
        "<label>Username</label><input name=\"username\" required maxlength=\"32\" "
        "pattern=\"[A-Za-z0-9._-]+\" title=\"Letters, numbers, dots, underscores, hyphens. No spaces.\" "
        "autocomplete=\"username\" placeholder=\"alice\" value=\""+html_escape(username)+"\">"
        "<label>Password</label><input type=\"password\" name=\"password\" required autocomplete=\"new-password\">"
        "<label>Phone</label><input name=\"phone\" type=\"tel\" required autocomplete=\"tel\" value=\""+html_escape(phone)+"\">"
        "<button>Continue</button></form></div>");
}

// viewer is the logged-in username rendering this post (empty for an
// anonymous visitor to a public profile) -- only a logged-in viewer gets a
// Repost button. Reposting always credits the true original author (see
// the repost_of_author comment on Post), so a chain of reposts never grows
// deeper than one hop of attribution.
std::string post_block(const Post& p, const std::string& viewer) {
    std::string where;
    if (!p.community.empty())
        where = " in <a href=\"/c/"+html_escape(p.community)+"\">"+html_escape(p.community)+"</a>";
    std::string when = fmt_time(p.ts);
    std::string repostLine;
    if (!p.repost_of_author.empty())
        repostLine = "<p class=\"muted\">&#8635; Reposted from <a href=\"/u/"+html_escape(p.repost_of_author)+
                     "\">"+html_escape(p.repost_of_author)+"</a></p>";
    std::string repostForm;
    if (!viewer.empty())
        repostForm = "<form class=\"inline\" method=\"POST\" action=\"/me/repost\">"
                     "<input type=\"hidden\" name=\"post_id\" value=\""+std::to_string(p.id)+"\">"
                     "<button class=\"ghost\" type=\"submit\">&#8635; Repost</button></form>";
    const int seconds = g_store.second_count(p.id);
    std::string secondForm;
    if (!viewer.empty()) {
        const bool seconded = g_store.has_seconded(p.id, viewer);
        secondForm = "<form class=\"inline\" method=\"POST\" action=\"/post/"+std::to_string(p.id)+"/second\">"
                     "<button class=\""+std::string(seconded?"":"ghost")+"\" type=\"submit\">"+
                     std::to_string(seconds)+(seconds==1?" second":" seconded")+
                     (seconded?" &#10003;":"")+"</button></form>";
    } else if (seconds > 0) {
        secondForm = "<p class=\"muted\">"+std::to_string(seconds)+(seconds==1?" second":" seconded")+"</p>";
    }
    std::string actionsRow;
    if (!repostForm.empty() || !secondForm.empty())
        actionsRow = "<div class=\"row\" style=\"margin-top:.3rem\">"+repostForm+secondForm+"</div>";
    std::string repliesBlock;
    {
        auto replies = g_store.replies_of(p.id);
        std::string list;
        for (const auto& r : replies) {
            std::string rwhen = fmt_time(r.ts);
            list += "<div class=\"who\" style=\"margin:.5rem 0\">"+portrait_tag(r.author,"sm")+
                    "<div><a href=\"/u/"+html_escape(r.author)+"\"><strong>"+html_escape(r.author)+
                    "</strong></a> <span class=\"muted\">"+html_escape(rwhen)+"</span>"
                    "<p style=\"margin:.2rem 0 0\">"+html_escape(r.text)+"</p></div></div>";
        }
        std::string replyForm;
        if (!viewer.empty()) {
            replyForm = "<form method=\"POST\" action=\"/post/"+std::to_string(p.id)+"/reply\" style=\"margin-top:.3rem\">"
                        "<textarea name=\"text\" maxlength=\"1000\" placeholder=\"Write a reply\"></textarea>"
                        "<button class=\"ghost\" type=\"submit\">Reply</button></form>";
        }
        if (!replies.empty() || !viewer.empty()) {
            repliesBlock = "<details style=\"margin-top:.4rem\"><summary class=\"muted\" style=\"cursor:pointer\">"+
                std::to_string(replies.size())+(replies.size()==1?" reply":" replies")+"</summary>"
                "<div style=\"padding-left:.8rem;border-left:2px solid var(--rule);margin-top:.3rem\">"+
                list+replyForm+"</div></details>";
        }
    }
    std::string mediaHtml;
    if (!p.attachment_blob_id.empty()) {
        std::string src = "/post-media/"+html_escape(p.attachment_blob_id);
        // width:100% (not max-width) so a small source image still fills
        // the card instead of rendering at its native pixel size -- the
        // bug that first surfaced this: a 120x80 test PNG showed up
        // genuinely postage-stamp-sized, since max-width alone only ever
        // scales large images down, never scales small ones up.
        mediaHtml = (p.attachment_mime.rfind("video/",0)==0)
            ? "<video controls preload=\"metadata\" style=\"width:100%;height:auto;display:block;border-radius:var(--rsm);margin-top:.4rem\" src=\""+src+"\"></video>"
            : "<img style=\"width:100%;height:auto;display:block;border-radius:var(--rsm);margin-top:.4rem\" src=\""+src+"\" alt=\"\">";
    }
    return "<article class=\"post\"><p class=\"eyebrow\" style=\"width:100%\">Entry "+std::to_string(p.id)+
           "</p><div class=\"who\">"+portrait_tag(p.author,"sm")+"<div><a href=\"/u/"+html_escape(p.author)+"\"><strong>"+
           html_escape(p.author)+"</strong></a>"+where+
           (when.empty()?"":" <span class=\"muted\">"+html_escape(when)+"</span>")+
           repostLine+
           "<p>"+html_escape(p.text)+"</p>"+mediaHtml+actionsRow+repliesBlock+"</div></div></article>";
}

// A proportional Yes/No/Abstain bar plus one-click vote buttons, the
// current voter's own choice picked out with a filled button instead of a
// ghost one. viewer empty (logged out) renders the tally with no voting UI.
std::string proposal_block(const Proposal& p, const std::string& viewer, const ProposalTally& t,
                            std::optional<ProposalVote> my_vote) {
    const int total = t.yes + t.no + t.abstain;
    std::string bar = "<div style=\"display:flex;height:8px;border-radius:4px;overflow:hidden;background:var(--rule);margin:.5rem 0\">";
    if (total > 0) {
        if (t.yes) bar += "<div style=\"flex:"+std::to_string(t.yes)+";background:var(--ok)\"></div>";
        if (t.no) bar += "<div style=\"flex:"+std::to_string(t.no)+";background:var(--danger)\"></div>";
        if (t.abstain) bar += "<div style=\"flex:"+std::to_string(t.abstain)+";background:var(--stone)\"></div>";
    }
    bar += "</div><p class=\"muted\">"+std::to_string(t.yes)+" yes · "+std::to_string(t.no)+" no · "+
           std::to_string(t.abstain)+" abstain · "+std::to_string(total)+" total</p>";
    std::string voteForm;
    if (!viewer.empty()) {
        auto btn = [&](ProposalVote v, const char* label) {
            const bool active = my_vote.has_value() && *my_vote == v;
            return "<form class=\"inline\" method=\"POST\" action=\"/proposals/vote\">"
                   "<input type=\"hidden\" name=\"id\" value=\""+std::to_string(p.id)+"\">"
                   "<input type=\"hidden\" name=\"choice\" value=\""+std::to_string(static_cast<int>(v))+"\">"
                   "<button class=\""+std::string(active ? "" : "ghost")+"\" type=\"submit\">"+label+
                   (active ? " &#10003;" : "")+"</button></form>";
        };
        voteForm = "<div class=\"row\">"+btn(ProposalVote::Yes,"Yes")+btn(ProposalVote::No,"No")+
                   btn(ProposalVote::Abstain,"Abstain")+"</div>";
    }
    std::string when = fmt_time(p.ts);
    return "<p class=\"muted eyebrow\">Proposal "+std::to_string(p.id)+"</p>"
           "<h3 style=\"margin:.1rem 0\">"+html_escape(p.title)+"</h3>"
           "<p class=\"muted\">by <a href=\"/u/"+html_escape(p.author)+"\">"+html_escape(p.author)+"</a>"+
           (when.empty()?"":" · "+html_escape(when))+"</p>"
           "<p style=\"white-space:pre-wrap\">"+html_escape(p.body)+"</p>"+bar+voteForm;
}

// Shared by every post composer (/me/post, /feed, /c/<slug>): pulls the
// text field and an optional media file out of either a multipart or
// urlencoded body, and -- if a file was attached -- ingests it through the
// exact same pipeline as a mail attachment (content-sniffed, malware
// scanned, images normalized; video stored as-is after a real container
// signature match, see file_transfer.hpp). ok==false means the media
// itself failed and no post should be created at all, rather than
// silently dropping the attachment and posting text-only.
struct ComposerPost {
    bool ok = true;
    std::string text;
    std::string attachment_blob_id;
    std::string attachment_mime;
    const char* err = nullptr; // "large" | "type" | "active" | "scan"
};
ComposerPost read_composer_post(const std::string& hdrs, const std::string& body) {
    ComposerPost r;
    std::string ctype = header_value(hdrs, "Content-Type");
    std::optional<std::vector<std::uint8_t>> file;
    if (ctype.find("multipart/form-data") != std::string::npos) {
        r.text = multipart_field(ctype, body, "text");
        file = multipart_file(ctype, body, "media");
    } else {
        r.text = parse_form(body)["text"];
    }
    if (r.text.size() > 2000) r.text.resize(2000);
    if (file && !file->empty()) {
        auto res = ingest_file(g_blobs, g_mail_scanner, g_mail_caps, *file);
        if (res.error != FileTransferError::None || !res.blob_id) {
            r.ok = false;
            switch (res.error) {
                case FileTransferError::TooLarge: r.err = "large"; break;
                case FileTransferError::ActiveContentBlocked: r.err = "active"; break;
                case FileTransferError::FailedScan: r.err = "scan"; break;
                default: r.err = "type"; break;
            }
            return r;
        }
        r.attachment_blob_id = *res.blob_id;
        r.attachment_mime = res.stored_mime_type.value_or("");
    }
    return r;
}

// Human copy for a composer's ?perr= media-error code, shared by /me,
// /feed and /c/<slug> so the three pages stay consistent.
std::string composer_media_error(const std::string& perr) {
    if (perr=="large") return "<p class=\"warn\">That file was too large (images 10&nbsp;MiB, video 50&nbsp;MiB).</p>";
    if (perr=="active") return "<p class=\"warn\">That document contains active content and was rejected.</p>";
    if (perr=="scan") return "<p class=\"warn\">That file could not be stored.</p>";
    if (perr=="type") return "<p class=\"warn\">That file wasn't a supported image or video type.</p>";
    return "";
}

// Wires up /me's "Editing /me" bar (#medraftbar/#medraftcount/#mediscard/
// #mepublish, see the GET /me handler) against every form[data-draft] on
// the page -- currently Profile CSS and the Jukebox permissions form.
// Nothing here writes anything; it defers each form's own submit (already
// wired to its real, unchanged endpoint) until Publish revision is
// clicked, sending whichever forms actually changed from what the page
// loaded with, in order, then reloading -- so the reload shows the true
// saved state rather than trusting the client's own idea of it. Discard
// is just a reload too: nothing was written, so there's nothing to
// actually roll back, only unsaved input to throw away.
std::string draft_publish_script() {
    return
        "<script>(function(){"
        "var bar=document.getElementById('medraftbar');if(!bar)return;"
        "var countEl=document.getElementById('medraftcount');"
        "var discardBtn=document.getElementById('mediscard'),publishBtn=document.getElementById('mepublish');"
        "var forms=Array.prototype.slice.call(document.querySelectorAll('form[data-draft]'));"
        "function serialize(f){"
        "var fd=new FormData(f),parts=[];"
        "fd.forEach(function(v,k){parts.push(k+'='+(typeof v==='string'?v:'[file]'));});"
        "return parts.sort().join('&');"
        "}"
        "var initial=forms.map(serialize);"
        // The form's own submit button (still there, still real, for the
        // one other context this same markup renders in unscripted) would
        // otherwise fire a real navigation right past the draft bar --
        // hidden here, not removed, so nothing else about the form changes.
        "forms.forEach(function(f){"
        "var btn=f.querySelector('button[type=submit],button:not([type])');"
        "if(btn)btn.hidden=true;"
        "});"
        "function dirtyIndexes(){"
        "var out=[];"
        "forms.forEach(function(f,i){if(serialize(f)!==initial[i])out.push(i);});"
        "return out;"
        "}"
        "function refresh(){"
        "var n=dirtyIndexes().length;"
        "countEl.textContent=n?(n+' unsaved change'+(n===1?'':'s')):'No unsaved changes';"
        "discardBtn.disabled=publishBtn.disabled=(n===0);"
        "}"
        "forms.forEach(function(f){"
        "f.addEventListener('input',refresh);"
        "f.addEventListener('change',refresh);"
        "f.addEventListener('submit',function(e){e.preventDefault();});"
        "});"
        "discardBtn.addEventListener('click',function(){location.reload();});"
        "publishBtn.addEventListener('click',function(){"
        "var idx=dirtyIndexes();"
        "if(!idx.length)return;"
        "publishBtn.disabled=true;discardBtn.disabled=true;"
        "publishBtn.textContent='Publishing\\u2026';"
        "function next(i){"
        "if(i>=idx.length){location.reload();return;}"
        "var f=forms[idx[i]];"
        // These forms have no enctype, so a normal submit sends url-encoded
        // -- the body parse_form() everywhere else in this app expects. A
        // raw FormData body on fetch() always sends multipart instead,
        // regardless of the form's own enctype, which parse_form() can't
        // read at all (silently parses as empty rather than erroring).
        "fetch(f.getAttribute('action'),{method:'POST',"
        "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
        "body:new URLSearchParams(new FormData(f)).toString()})"
        ".then(function(){next(i+1);}).catch(function(){next(i+1);});"
        "}"
        "next(0);"
        "});"
        "refresh();"
        "})();</script>";
}

std::string handle(const std::string& req, const std::string& peer_ip) {
    auto line_end=req.find("\r\n");
    if(line_end==std::string::npos) line_end=req.find('\n');
    if(line_end==std::string::npos) return http_ok(wrap("Error","",0,"<p>bad request</p>"));
    std::string method, rawpath; { std::istringstream is(req.substr(0,line_end)); is>>method>>rawpath; }
    std::string path = rawpath;
    auto qpos=path.find('?'); if(qpos!=std::string::npos) path=path.substr(0,qpos);
    auto qs = parse_qs(rawpath);
    std::string body;
    auto sep=req.find("\r\n\r\n");
    if(sep!=std::string::npos) body=req.substr(sep+4);
    else { sep=req.find("\n\n"); if(sep!=std::string::npos) body=req.substr(sep+2); }
    std::lock_guard<std::mutex> lock(g_mu);
    auto acc = account_from_req(req);
    std::string user = acc ? acc->username : "";
    const std::string sid = cookie_of(req);
    const int unread = acc ? g_store.unread_count(user) : 0;
    g_theme_mode = theme_of(req);
    auto pg = [&](const std::string& title, const std::string& b) {
        return http_ok(wrap(title, user, unread, b));
    };

    if(method=="GET"&&path=="/favicon.svg")
        return http_raw("200 OK","image/svg+xml", favicon_svg());

    if(method=="GET"&&path=="/assets/intro.mp4") {
        static std::string cached;
        static bool tried = false;
        if (!tried) {
            tried = true;
            std::ifstream in("assets/intro.mp4", std::ios::binary);
            if (in) { std::ostringstream ss; ss << in.rdbuf(); cached = ss.str(); }
        }
        if (cached.empty()) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        return http_raw("200 OK", "video/mp4", cached,
            "Cache-Control: public, max-age=31536000, immutable\r\n");
    }

    if(method=="GET"&&(path=="/theme/dark"||path=="/theme/light")) {
        const std::string mode = path.substr(7);
        std::string back = header_value(req, "Referer");
        if (back.empty() || back.rfind("http",0)!=0) back = "/";
        return http_redir(back, "Set-Cookie: lg_theme="+mode+
            "; Path=/; SameSite=Lax; Max-Age=31536000\r\n");
    }

    if(method=="GET"&&path=="/") {
        if(acc)
            return pg("Home", home_dashboard(user)+
                "<div class=\"card\"><h2>What this is</h2>"
                "<p class=\"muted\">Genesis, not a government. Three rules, and the rest is up to you.</p>"
                "<p class=\"muted\"><a href=\"/laws\">Read the disclosure</a></p></div>");
        std::string extra =
            "<div class=\"card\"><p class=\"row\"><a class=\"btn\" href=\"/signup\">Create an account</a> "
            "<a class=\"btn ghost\" href=\"/login\">Log in</a></p></div>";
        return pg("Genesis", genesis_body()+extra);
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
    if(method=="GET"&&path=="/signup") return http_ok(signup_page(user, unread, "", "", ""));
    if(method=="GET"&&path=="/login") {
        std::string err;
        // Deliberately one generic message for bad-username and
        // bad-password alike (and phone-not-verified folded in too) --
        // splitting them would let a caller enumerate which usernames
        // exist. Rate-limit is the one case safe to name specifically:
        // it doesn't depend on whether the account exists.
        if (qs["err"]=="login") err = "<p class=\"warn\">Invalid username or password.</p>";
        else if (qs["err"]=="ratelimit") err = "<p class=\"warn\">Too many attempts. Wait a moment and try again.</p>";
        return pg("Log in", "<div class=\"card\"><h1>Log in</h1>"
            "<p class=\"muted\">Username and password. Phone is not the daily credential.</p>"+err+
            "<form method=\"POST\" action=\"/login\"><label>Username</label>"
            "<input name=\"username\" required autocomplete=\"username\" value=\""+html_escape(qs["username"])+"\">"
            "<label>Password</label><input type=\"password\" name=\"password\" required autocomplete=\"current-password\">"
            "<button>Continue</button></form>"
            "<p class=\"muted\" style=\"margin-top:.7rem\">New here? <a href=\"/signup\">Create an account</a>.</p></div>");
    }
    if(method=="GET"&&path=="/logout") {
        logout(g_store, sid);
        return http_redir("/", clear_cookie());
    }

    if(method=="GET"&&path=="/world") {
        if(!acc) return http_redir("/login");
        return pg("World", world_page(user));
    }
    if(method=="GET"&&path=="/me/canvas") {
        if(!acc) return http_redir("/login");
        return pg("Canvas", canvas_tool_page(user, qs["err"], false));
    }
    if(method=="GET"&&path=="/me/portrait") {
        if(!acc) return http_redir("/login");
        return pg("Self-portrait", canvas_tool_page(user, qs["err"], true));
    }
    if(method=="GET"&&path=="/me/post/canvas") {
        if(!acc) return http_redir("/login");
        return pg("Compose a post", canvas_tool_page(user, "", false, "/me/post"));
    }
    if(method=="GET"&&path=="/feed/canvas") {
        if(!acc) return http_redir("/login");
        return pg("Compose a post", canvas_tool_page(user, "", false, "/feed"));
    }
    if(method=="GET"&&path=="/listen") {
        // Deliberately not wrap()/pg(): this window IS the player, full-page
        // nav chrome and a second (redundant) floating-widget toggle would
        // just be noise in a small popout.
        if(!acc) return http_redir("/login");
        return http_ok(page_wrap("Listen", "",
            "<div style=\"padding:.75rem\">" + jukebox_standalone_page(youtube_api_key_from_env(), user) + "</div>"));
    }
    if(method=="GET"&&path=="/feed") {
        if(!acc) return http_redir("/login");
        std::string filter_key = qs.count("filter") ? qs.at("filter") : "all";
        FeedFilter filter = FeedFilter::All;
        if (filter_key == "friends") filter = FeedFilter::Friends;
        else if (filter_key == "committees") filter = FeedFilter::Committees;
        else if (filter_key == "drawings") filter = FeedFilter::Drawings;
        else filter_key = "all";
        auto tab = [&](const char* key, const char* label) {
            const bool active = filter_key == key;
            return "<a class=\"btn"+std::string(active?"":" ghost")+"\" href=\"/feed?filter="+key+"\">"+label+"</a>";
        };
        std::string html = "<h1>Feed</h1><p class=\"muted eyebrow\">"+html_escape(fmt_time(now()))+
            " &middot; "+std::to_string(g_store.account_count())+" members</p>"
            "<div class=\"row\" style=\"margin:.5rem 0 1rem\">"+
            tab("all","All")+tab("friends","Friends")+tab("committees","Committees")+tab("drawings","Drawings")+
            "</div>"+
            composer_media_error(qs["perr"])+
            "<div class=\"card\"><p class=\"row\" style=\"justify-content:space-between;margin:0 0 .3rem\">"
            "<label style=\"margin:0\">Write to people who know you</label>"
            "<span class=\"chip\">Friends only</span></p>"
            "<form method=\"POST\" action=\"/feed\" enctype=\"multipart/form-data\">"
            "<textarea name=\"text\" maxlength=\"2000\" placeholder=\"Plain text. Friends see this.\" "
            "oninput=\"document.getElementById('feedchars').textContent=(2000-this.value.length)+' left'\"></textarea>"
            "<p class=\"muted\" id=\"feedchars\" style=\"margin:.2rem 0 0;text-align:right\">2000 left</p>"
            "<label>Photo or video (optional)</label><input type=\"file\" name=\"media\" accept=\"image/*,video/*\">"
            "<button>Post</button></form>"
            "<p class=\"muted\" style=\"margin-top:.5rem\">Or <a href=\"/feed/canvas\">open the canvas</a> "
            "to draw, crop, layer, or paste one together first.</p>"
            "<p class=\"muted\">Personal posts reach you and your friends. Communities have their own pages. "
            "This is not a civic act.</p></div>";
        auto posts = g_store.feed_view(user, 80, filter);
        if (posts.empty()) html += "<p class=\"empty\">Nothing yet. Write something, or add a friend.</p>";
        for (const auto& p : posts) html += "<div class=\"card\">"+post_block(p, user)+"</div>";
        return pg("Feed", html);
    }

    if(method=="GET"&&path=="/proposals") {
        if(!acc) return http_redir("/login");
        std::string html = "<h1>Proposals</h1>"
            "<p class=\"muted\">Unofficial straw polls &mdash; a way to gauge how people feel about "
            "something not yet decided. Not a CitizenId vote, not binding, not this platform's Phase 2.</p>";
        if (qs["err"]=="empty") html += "<p class=\"warn\">Need a title and a body.</p>";
        html += "<div class=\"card\"><form method=\"POST\" action=\"/proposals\">"
                "<label>Title</label><input name=\"title\" maxlength=\"140\" required>"
                "<label>What's being proposed</label>"
                "<textarea name=\"body\" maxlength=\"4000\" required placeholder=\"Plain text.\"></textarea>"
                "<button>Open for votes</button></form></div>";
        auto all = g_store.all_proposals();
        std::reverse(all.begin(), all.end());
        if (all.empty()) html += "<p class=\"empty\">No proposals yet.</p>";
        for (const auto& p : all) {
            auto t = g_store.tally_of(p.id);
            auto mv = g_store.proposal_vote_of(p.id, user);
            html += "<div class=\"card\">"+proposal_block(p, user, t, mv)+"</div>";
        }
        return pg("Proposals", html);
    }

    if(method=="GET"&&path.rfind("/portrait/",0)==0) {
        const std::string who = path.substr(10);
        if (!valid_username(who)) return http_code("404 Not Found", "not found");
        auto found = g_store.find_by_username(who);
        if (!found) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        if (acc && g_store.is_blocked(user, found->username))
            return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        auto av = g_store.get(found->id);
        if (!av) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        auto bytes = g_blobs.get(av->blob_id);
        if (!bytes) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        std::string png(bytes->begin(), bytes->end());
        return http_raw("200 OK", "image/png", png,
            "Cache-Control: private, max-age=300\r\nContent-Disposition: inline; filename=\"portrait.png\"\r\n");
    }

    // /canvas-frame/<username>/<index> -- one saved drawing snapshot.
    // Requested by the sandboxed theme document's own background-image
    // CSS (server-generated, not user CSS -- see theme_background_css),
    // so this has to be reachable without needing script access.
    if(method=="GET"&&path.rfind("/canvas-frame/",0)==0) {
        auto rest = path.substr(14);
        auto slash = rest.find('/');
        std::string body404 = "not found";
        if (slash == std::string::npos) return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        const std::string who = rest.substr(0, slash);
        int idx = -1;
        try { idx = std::stoi(rest.substr(slash + 1)); } catch (...) { idx = -1; }
        if (!valid_username(who) || idx < 0) return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        auto found = g_store.find_by_username(who);
        if (!found) return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        if (acc && g_store.is_blocked(user, found->username))
            return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        auto frames = g_store.canvas_frames(found->id);
        if (static_cast<std::size_t>(idx) >= frames.size()) return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        auto bytes = g_blobs.get(frames[static_cast<std::size_t>(idx)]);
        if (!bytes) return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        std::string png(bytes->begin(), bytes->end());
        return http_raw("200 OK", "image/png", png,
            "Cache-Control: private, max-age=300\r\nContent-Disposition: inline; filename=\"frame.png\"\r\n");
    }

    // /post-media/<blob_id> -- an image or video attached to a post.
    // Public and unauthenticated, same posture as /portrait/ and
    // /canvas-frame/: a post's own visibility is already decided by
    // wherever it renders (feed, group, profile), and the blob id itself
    // is a content hash, not a secret. blob_id comes straight from the
    // URL, though, so it's validated as a bare hex sha256 digest before
    // ever reaching BlobStore -- FilesystemBlobStore::path_for()
    // concatenates the id directly into a filesystem path with no
    // sanitization of its own, so anything looser here would be a real
    // path-traversal hole, not just a style nit.
    if(method=="GET"&&path.rfind("/post-media/",0)==0) {
        const std::string blob_id = path.substr(12);
        bool valid_hex = blob_id.size() == 64;
        if (valid_hex) {
            for (unsigned char c : blob_id) {
                if (!std::isxdigit(c)) { valid_hex = false; break; }
            }
        }
        if (!valid_hex) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        auto meta = g_blobs.meta(blob_id);
        auto bytes = g_blobs.get(blob_id);
        if (!meta || !bytes) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        std::string data(bytes->begin(), bytes->end());
        return http_raw("200 OK", meta->mime_type.c_str(), data,
            "Cache-Control: private, max-age=300\r\nContent-Disposition: inline\r\n");
    }

    if(method=="GET"&&path=="/me") {
        if(!acc) return http_redir("/login");
        std::string err;
        if (qs["err"]=="image") err = "<p class=\"warn\">That file was not a usable image (PNG/JPEG/GIF, 2&nbsp;MiB, 1024px). SVG is not accepted.</p>";
        else if (qs["err"]=="large") err = "<p class=\"warn\">That file is too large. Cap is 2&nbsp;MiB and 1024 pixels on the long edge.</p>";
        else if (qs["err"]=="scan") err = "<p class=\"warn\">That file could not be stored.</p>";
        std::string html;
        // Draft/publish: the two forms directly on this page (Profile CSS,
        // Jukebox permissions) no longer save themselves on their own
        // submit -- draftScript below intercepts them, tracks which are
        // actually changed from what the page loaded with, and only this
        // bar's Publish button sends them (sequentially, to their existing
        // endpoints -- no new storage, this is purely a review-before-
        // committing step in front of what was already there). Discard is
        // just a reload: nothing is written until Publish, so there is
        // nothing to actually roll back.
        html += "<div id=\"medraftbar\" class=\"card\" style=\"position:sticky;top:0;z-index:8;"
                "display:flex;align-items:center;justify-content:space-between;gap:.6rem;flex-wrap:wrap\">"
                "<div><p class=\"eyebrow\" style=\"margin:0\">Editing /me</p>"
                "<p id=\"medraftcount\" class=\"muted\" style=\"margin:.1rem 0 0\">No unsaved changes</p></div>"
                "<div class=\"row\"><button type=\"button\" id=\"mediscard\" class=\"ghost\" disabled>Discard</button>"
                "<button type=\"button\" id=\"mepublish\" disabled>Publish revision</button></div></div>";
        html += "<div class=\"card\"><div class=\"who\">"+portrait_tag(user,"")+"<div><h1>"+html_escape(user)+"</h1>"
            "<p class=\"muted\">Platform profile · <a href=\"/u/"+html_escape(user)+"\">public page</a> · member since "+
            html_escape(fmt_time(acc->created_at))+"</p></div></div>"+err+
            "<p class=\"muted\">Self-portrait &mdash; draw one with the same tools as the "
            "<a href=\"/me/portrait\">portrait tool</a>.</p>";
        html += "<p class=\"muted\">A platform account is not a CitizenId.</p></div>";
        {
            auto n_frames = g_store.canvas_frames(acc->id).size();
            std::string canvas_status = n_frames == 0
                ? "No drawing yet &mdash; your profile uses the default background."
                : (std::to_string(n_frames) + " frame" + (n_frames==1?"":"s") + " saved, "
                   + (g_store.canvas_timelapse(acc->id) ? "looping as your background." : "last one shown as your background."));
            html += "<div class=\"card\"><h2>Canvas</h2>"
                "<p class=\"muted\">"+canvas_status+"</p>"
                "<p class=\"row\"><a class=\"btn\" href=\"/me/canvas\">Open the canvas</a></p></div>";
        }
        {
            auto current_css = g_store.author_css(acc->id);
            std::string css_msg;
            if (qs["ok"]=="css") css_msg = "<p class=\"ok\">Saved. See it at <a href=\"/u/"+html_escape(user)+"\">your public profile</a>.</p>";
            else if (qs["err"]=="css") css_msg = "<p class=\"warn\">"+html_escape(css_reject_message(qs["reason"]))+"</p>";
            html += "<div class=\"card\"><h2>Profile CSS</h2>"
                "<p class=\"muted\">Restyles your public profile page (/u/"+html_escape(user)+
                "), shown inside a sandboxed frame that can never run scripts. "
                "@import, url(), and anything script-adjacent are rejected outright, not silently stripped. "
                "Limit " + std::to_string(kAuthorCssMaxBytes/1000) + "KB. Leave empty to use the default look.</p>"
                + css_msg +
                "<form method=\"POST\" action=\"/me/css\" data-draft=\"1\">"
                "<textarea name=\"css\" rows=\"8\" spellcheck=\"false\" "
                "style=\"font-family:ui-monospace,Consolas,monospace;font-size:.85rem\" "
                "placeholder=\".lg-theme h1{color:#a23b3b}\">"+html_escape(current_css)+"</textarea>"
                "</form></div>";
        }
        html += jukebox_widget(user, true, user, qs);
        html += draft_publish_script();
        return pg("Profile", html);
    }

    // Checked before the general /u/ handler below, since that one treats
    // everything after "/u/" as a bare username.
    if(method=="GET"&&path.rfind("/u/",0)==0&&path.size()>7&&path.compare(path.size()-6,6,"/theme")==0) {
        const std::string who = path.substr(3, path.size()-3-6);
        std::string body;
        if (!valid_username(who)) {
            body = "<!doctype html><html><head><meta charset=\"utf-8\"></head>"
                   "<body><p>No such profile.</p></body></html>";
        } else {
            auto found = g_store.find_by_username(who);
            if (!found) {
                body = "<!doctype html><html><head><meta charset=\"utf-8\"></head>"
                       "<body><p>No such profile.</p></body></html>";
            } else {
                auto author_css = g_store.author_css(found->id);
                auto canvas_frames = g_store.canvas_frames(found->id);
                std::string canvas_css = theme_background_css(
                    found->username, canvas_frames.size(), g_store.canvas_timelapse(found->id));
                // Deliberately not wrap()/page_wrap(): no nav, no jukebox
                // widget, no session cookie context, no scripts at all --
                // this document only ever renders inside the sandboxed,
                // scriptless iframe below. See sanitize_author_css's
                // comment for what's actually defended here and why a gap
                // in the text filter still can't reach anything.
                // canvas_css sits between the base style and author_css --
                // server-generated, not sanitized (it isn't user text),
                // and ordered so a user's own .lg-theme background: rule
                // still wins on the normal cascade if they set one.
                body = "<!doctype html><html><head><meta charset=\"utf-8\">"
                       "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                       "<style>body{margin:0;padding:1rem;font:16px/1.5 system-ui,sans-serif;"
                       "background:#fff;color:#1a1814}h1{margin:.2rem 0}"
                       "p{white-space:pre-wrap}</style>"
                       "<style>" + canvas_css + "</style>"
                       "<style>" + author_css + "</style></head>"
                       "<body><div class=\"lg-theme\"><h1>" + html_escape(found->username) + "</h1>"
                       "</div></body></html>";
            }
        }
        return http_raw("200 OK", "text/html; charset=utf-8", body);
    }
    if(method=="GET"&&path.rfind("/u/",0)==0) {
        const std::string who = path.substr(3);
        if (!valid_username(who))
            return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>No such profile.</p></div>"));
        auto found = g_store.find_by_username(who);
        if (!found)
            return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>No such profile.</p></div>"));
        if (acc && g_store.is_blocked(user, found->username))
            return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>No such profile.</p></div>"));
        const bool is_owner = acc && acc->username == found->username;
        const bool is_friend = acc && !is_owner && g_store.are_friends(user, found->username);
        auto target_prefs = g_store.prefs_of(found->username);
        if (!visible_to(target_prefs.profile_visibility, is_owner, acc.has_value(), is_friend)) {
            return http_code("200 OK", wrap(found->username, user, unread,
                "<div class=\"card\"><p>This profile is private.</p></div>"));
        }
        auto committees = g_store.communities_of(found->username);
        std::string committees_html;
        if (!committees.empty()) {
            committees_html = "<p class=\"eyebrow\" style=\"margin-top:1rem\">Committees</p>"
                               "<div style=\"display:flex;flex-wrap:wrap;gap:.4rem\">";
            for (const auto& c : committees) {
                committees_html += "<a class=\"chip\" href=\"/c/"+html_escape(c.slug)+"\" style=\"text-decoration:none\">"+
                                    html_escape(c.name)+"</a>";
            }
            committees_html += "</div>";
        }
        std::string html = "<div class=\"card\"><div class=\"who\">"+portrait_tag(found->username,"")+
            "<div><h1>"+html_escape(found->username)+"</h1>"
            "<table class=\"record\">"
            "<tr><td>Standing</td><td>Associate &middot; good</td></tr>"
            "<tr><td>Enrolled</td><td>"+html_escape(fmt_time(found->created_at))+"</td></tr>"
            "</table></div></div>"+committees_html;
        if (!g_store.author_css(found->id).empty()) {
            // sandbox="allow-same-origin", not sandbox="" (opaque origin):
            // confirmed live that opaque-origin sandboxed iframes get
            // silently dropped by real-world browser privacy/tracking
            // protections (reproduced in two independent browsers, not a
            // one-off) -- the iframe element rendered but its document
            // never loaded. allow-same-origin fixes that. This does NOT
            // reopen the security hole it looks like: allow-scripts is
            // still withheld, so nothing can ever execute inside this
            // frame regardless of origin -- and with zero script
            // execution possible, having the same origin as the rest of
            // the site is not exploitable (there's no JS to act on it).
            // The two real defenses -- sanitize_author_css rejecting
            // script-adjacent CSS, and no script execution ever being
            // possible in this frame -- are both still fully intact.
            html += "<iframe src=\"/u/"+html_escape(found->username)+"/theme\" sandbox=\"allow-same-origin\" "
                    "title=\"Profile theme\" "
                    "style=\"width:100%;min-height:220px;border:1px solid var(--rule);"
                    "border-radius:var(--rsm);background:#fff\"></iframe>";
        }
        if (acc && acc->username == found->username) {
            html += "<p><a href=\"/me\">Edit profile</a></p>";
        } else if (acc && g_store.are_friends(user, found->username)) {
            html += "<p class=\"row\"><a class=\"btn\" href=\"/mail?with="+html_escape(found->username)+"\">Message</a>"
                    "<form class=\"inline\" method=\"POST\" action=\"/friends/unfriend\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(found->username)+"\">"
                    "<button class=\"ghost\" type=\"submit\">Unfriend</button></form></p>";
        } else if (acc) {
            html += "<form method=\"POST\" action=\"/friends\"><input type=\"hidden\" name=\"who\" value=\""+
                    html_escape(found->username)+"\"><button>Send friend request</button></form>";
        }
        html += "</div>";
        html += jukebox_widget(found->username, is_owner, user, qs);
        {
            if (visible_to(target_prefs.posts_visibility, is_owner, acc.has_value(), is_friend)) {
                auto theirPosts = g_store.posts_by(found->username);
                std::reverse(theirPosts.begin(), theirPosts.end());
                html += "<h2>Posts</h2><p class=\"muted eyebrow\">"+std::to_string(theirPosts.size())+
                        (theirPosts.size()==1?" entry":" entries")+
                        (target_prefs.posts_visibility==Visibility::Public?" &middot; public":"")+"</p>";
                for (const auto& p : theirPosts) html += "<div class=\"card\">"+post_block(p, user)+"</div>";
            }
        }
        return pg(found->username, html);
    }

    if(method=="GET"&&path=="/friends") {
        if(!acc) return http_redir("/login");
        std::string err;
        if (qs["err"]=="inner") err = "<p class=\"warn\">Inner circle holds twelve. Take someone out first.</p>";
        std::string html = "<h1>People</h1>"+err+
            "<div class=\"card\"><h2>Find someone</h2>"
            "<form method=\"GET\" action=\"/search\"><label>Exact username, or three letters for typeahead</label>"
            "<div class=\"row\"><input name=\"q\" required><button>Search</button></div></form></div>"
            "<div class=\"card\"><h2>Add by exact name</h2>"
            "<form method=\"POST\" action=\"/friends\"><input name=\"who\" placeholder=\"username\" required>"
            "<button>Send request</button></form></div>";
        html += "<div class=\"card\"><h2>Friends</h2>";
        auto pals = g_store.friends_of(user);
        auto inner = g_store.inner_of(user);
        std::set<std::string> inner_set(inner.begin(), inner.end());
        html += "<p class=\"muted eyebrow\">"+std::to_string(pals.size())+
                (pals.size()==1?" friend":" friends")+" &middot; "+std::to_string(inner_set.size())+" inner</p>";
        if(pals.empty()) html += "<p class=\"empty\">None yet.</p>";
        for (auto& other : pals) {
            const bool inn = inner_set.count(other);
            html += "<div class=\"person\"><div class=\"who\">"+portrait_tag(other,"sm")+"<div><a href=\"/u/"+html_escape(other)+"\"><strong>"+html_escape(other)+"</strong></a>"
                    +(inn?" <span class=\"muted\">inner circle</span>":"")+
                    "</div></div><div class=\"row\">"
                    "<a class=\"btn ghost\" href=\"/mail?with="+html_escape(other)+"\">Mail</a>";
            if (inn) {
                html += "<form class=\"inline\" method=\"POST\" action=\"/friends/inner-remove\">"
                        "<input type=\"hidden\" name=\"who\" value=\""+html_escape(other)+"\">"
                        "<button class=\"ghost\" type=\"submit\">From inner</button></form>";
            } else {
                html += "<form class=\"inline\" method=\"POST\" action=\"/friends/inner\">"
                        "<input type=\"hidden\" name=\"who\" value=\""+html_escape(other)+"\">"
                        "<button class=\"ghost\" type=\"submit\">Inner circle</button></form>";
            }
            html += "<form class=\"inline\" method=\"POST\" action=\"/friends/unfriend\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(other)+"\">"
                    "<button class=\"ghost\" type=\"submit\">Unfriend</button></form>"
                    "<form class=\"inline\" method=\"POST\" action=\"/friends/block\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(other)+"\">"
                    "<button class=\"danger\" type=\"submit\">Block</button></form></div></div>";
        }
        html += "<p class=\"muted\">Inner circle is capped at twelve. Outer circle is everyone else you have accepted — derived, not stored as a second list.</p></div>";
        html += "<div class=\"card\"><h2>Incoming</h2>";
        auto inc = g_store.incoming_requests(user);
        if(inc.empty()) html += "<p class=\"empty\">None.</p>";
        for (auto& from : inc) {
            html += "<div class=\"person\"><span>"+html_escape(from)+"</span>"
                    "<form method=\"POST\" action=\"/friends/accept\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(from)+"\"><button>Accept</button></form></div>";
        }
        html += "</div><div class=\"card\"><h2>Outgoing</h2>";
        auto outg = g_store.outgoing_requests(user);
        if(outg.empty()) html += "<p class=\"empty\">None.</p>";
        for (auto& to : outg) html += "<div class=\"person\"><span>"+html_escape(to)+"</span><span class=\"muted\">waiting</span></div>";
        html += "<p class=\"muted\">There is no declined notice. Silence and a declined request look the same.</p></div>";
        return pg("People", html);
    }

    if(method=="GET"&&path=="/search") {
        if(!acc) return http_redir("/login");
        auto term = trim_ws(qs["q"]);
        std::string html = "<h1>Search</h1><div class=\"card\"><form method=\"GET\" action=\"/search\">"
            "<label>Find a person</label><div class=\"row\"><input name=\"q\" value=\""+html_escape(term)+"\">"
            "<button>Search</button></div></form><p class=\"muted\">Friends always appear. "
            "Everyone else follows their findability. A miss is a miss — missing, opted out, and blocked look the same.</p><ul class=\"plain\">";
        std::vector<std::string> hits;
        if (!term.empty()) {
            if (g_store.is_findable_exact(user, term)) {
                if (auto found = g_store.find_by_username(term)) hits.push_back(found->username);
            }
            if (term.size() >= 3) {
                for (auto& n : g_store.typeahead(user, term)) {
                    if (std::find(hits.begin(), hits.end(), n)==hits.end()) hits.push_back(n);
                }
            }
        }
        if (term.empty()) html += "<li class=\"muted\">Type a name.</li>";
        else if (hits.empty()) html += "<li class=\"muted\">No match.</li>";
        for (auto& n : hits) {
            html += "<li class=\"who\">"+portrait_tag(n,"sm")+"<a href=\"/u/"+html_escape(n)+"\">"+html_escape(n)+"</a></li>";
        }
        html += "</ul></div>";
        return pg("Search", html);
    }

    if(method=="GET"&&path=="/mail") {
        if(!acc) return http_redir("/login");
        auto other = qs["with"];
        std::string html = "<h1>Mail</h1><div class=\"card\"><p class=\"muted\">Direct messages are friends-only in this release. "
            "Open messaging waits on age assurance so a stranger cannot reach a minor. We will not treat a checkbox as age.</p><ul class=\"plain\">";
        auto pals = g_store.friends_of(user);
        if (pals.empty()) html += "<li class=\"muted\">Friend someone first.</li>";
        for (auto& n : pals) html += "<li><a href=\"/mail?with="+html_escape(n)+"\">"+html_escape(n)+"</a></li>";
        html += "</ul></div>";
        if (!other.empty() && g_store.are_friends(user, other) && !g_store.is_blocked(user, other)) {
            html += "<div class=\"card\"><h2>"+html_escape(other)+"</h2>";
            if (qs["err"]=="type") html += "<p class=\"warn\">That attachment isn't a usable type. Allowed: PNG/JPEG/GIF, PDF, plain text, macro-free Word/Excel/PowerPoint. No SVG, no archives.</p>";
            else if (qs["err"]=="large") html += "<p class=\"warn\">That attachment is too large.</p>";
            else if (qs["err"]=="active") html += "<p class=\"warn\">That document was refused: it declares a macro, embedded object, or OLE content.</p>";
            else if (qs["err"]=="scan") html += "<p class=\"warn\">That attachment could not be stored.</p>";
            auto thread = g_store.mail_with(user, other);
            if (thread.empty()) html += "<p class=\"empty\">No messages yet.</p>";
            for (auto& m : thread) {
                html += "<div class=\"msg\"><div><strong>"+html_escape(m.from)+"</strong> "
                        "<span class=\"muted\">"+html_escape(fmt_time(m.ts))+"</span>";
                if (!m.text.empty()) html += "<p>"+html_escape(m.text)+"</p>";
                if (m.attachment_blob_id) {
                    html += "<p><a href=\"/mail/attachment/"+html_escape(*m.attachment_blob_id)+"\">Download attachment</a> "
                            "<span class=\"muted\">"+html_escape(m.attachment_mime.value_or(""))+"</span></p>";
                }
                html += "</div></div>";
            }
            html += "<form method=\"POST\" action=\"/mail\" enctype=\"multipart/form-data\">"
                    "<input type=\"hidden\" name=\"to\" value=\""+html_escape(other)+"\">"
                    "<label>Message</label><input name=\"text\" maxlength=\"2000\">"
                    "<label>Attachment (optional)</label>"
                    "<input type=\"file\" name=\"attachment\" "
                    "accept=\"image/png,image/jpeg,image/gif,application/pdf,text/plain,.docx,.xlsx,.pptx\">"
                    "<p class=\"muted\">Scanned, content-sniffed, default-deny. No SVG. No archives.</p>"
                    "<button>Send</button></form></div>";
        }

        html += "<div class=\"card\"><h2>Groups</h2>"
            "<p class=\"muted\">Invite a friend and they join right away. Invite someone you're not "
            "friends with yet and they get a pending invite to accept before they can post. Adding "
            "someone into a room that would put a minor with a stranger is refused with a generic "
            "message — start a new group instead.</p>";
        auto groups = g_store.conversations_for(acc->id);
        std::string list;
        for (auto& conv : groups) {
            if (!conv.is_group) continue;
            bool pending = false;
            for (auto& m : conv.members) if (m.account == acc->id) pending = m.state == MemberState::Pending;
            list += "<li><a href=\"/mail/g/" + std::to_string(conv.id) + "\">" +
                    html_escape(group_label(conv, acc->id)) + "</a>" +
                    (pending ? " <span class=\"muted\">invited</span>" : "") + "</li>";
        }
        html += "<ul class=\"plain\">" + (list.empty() ? "<li class=\"muted\">No groups yet.</li>" : list) +
                "</ul>"
                "<form method=\"POST\" action=\"/mail/group/new\">"
                "<label>Start a group — usernames, space or comma separated</label>"
                "<input name=\"who\" placeholder=\"alice bob carol\">"
                "<button>Create</button></form></div>";
        return pg("Mail", html);
    }

    if(method=="GET"&&path.rfind("/mail/attachment/",0)==0) {
        if(!acc) return http_redir("/login");
        const std::string blob_id = path.substr(17);
        auto found = g_store.find_mail_attachment(user, blob_id);
        if (!found) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        const std::string other = (found->from == user) ? found->to : found->from;
        // Same gate as the thread view: attachments do not outlive the
        // friendship they were sent inside of. Do not open stranger DMs.
        if (!g_store.are_friends(user, other) || g_store.is_blocked(user, other) || g_store.is_blocked(other, user))
            return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        const std::string mime = found->attachment_mime.value_or("application/octet-stream");
        auto bytes = g_blobs.get(blob_id);
        if (!bytes) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        std::string data(bytes->begin(), bytes->end());
        std::string extra = "Content-Disposition: attachment; filename=\""+attachment_filename_for_mime(mime)+"\"\r\n"
                             "Cache-Control: private, no-store\r\n";
        return http_raw("200 OK", mime.c_str(), data, extra);
    }

    if(method=="GET"&&path.rfind("/mail/g/",0)==0) {
        if(!acc) return http_redir("/login");
        ConversationId conv_id = 0;
        try { conv_id = std::stoull(path.substr(8)); } catch(...) { conv_id = 0; }
        auto conv = conv_id ? g_store.get_conversation(conv_id) : std::nullopt;
        const ConversationMember* self_member = nullptr;
        if (conv) for (auto& m : conv->members) if (m.account == acc->id) self_member = &m;
        if (!conv || !conv->is_group || !self_member)
            return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>No such group.</p></div>"));
        const std::string base = "/mail/g/" + std::to_string(conv_id);
        const bool active = self_member->state == MemberState::Active;

        std::string html = "<h1>"+html_escape(group_label(*conv, acc->id))+"</h1>"
            "<p class=\"muted\">Platform group conversation. Not an AVND locality.</p>";
        if (qs["err"]=="invite") html += "<p class=\"warn\">Conflicts with another member’s contact settings.</p>";
        html += "<div class=\"card\"><h2>Members</h2><ul class=\"plain\">";
        for (auto& m : conv->members) {
            html += "<li>"+html_escape(display_name_for(m.account))+
                    (m.state==MemberState::Pending?" <span class=\"muted\">invited</span>":"")+"</li>";
        }
        html += "</ul>";
        if (!active) {
            html += "<p class=\"muted\">You’re invited to this group. Accept to join, or decline — declining is never announced to the room.</p>"
                    "<form method=\"POST\" action=\""+base+"/accept\"><button>Accept</button></form>"
                    "<form method=\"POST\" action=\""+base+"/leave\"><button class=\"ghost\">Decline</button></form>";
        } else {
            html += "<form method=\"POST\" action=\""+base+"/invite\"><label>Invite</label>"
                    "<div class=\"row\"><input name=\"who\" required><button>Invite</button></div></form>"
                    "<form method=\"POST\" action=\""+base+"/leave\"><button class=\"ghost\">Leave group</button></form>";
        }
        html += "</div>";

        if (active) {
            html += "<div class=\"card\">";
            auto msgs = g_store.messages_in(conv_id);
            bool any = false;
            for (auto& m : msgs) {
                if (m.deletion==DeletionState::DeletedForSender && m.sender==acc->id) continue;
                any = true;
                html += "<div class=\"msg\"><div><strong>"+html_escape(display_name_for(m.sender))+"</strong> "
                        "<span class=\"muted\">"+html_escape(fmt_time(m.sent_at))+"</span>";
                if (m.deletion==DeletionState::DeletedForEveryone) {
                    html += "<p class=\"muted\"><em>Message deleted.</em></p>";
                } else {
                    html += "<p>"+html_escape(m.text)+"</p>";
                    if (m.edited_at>0) html += "<p class=\"muted\">(edited)</p>";
                    if (m.sender==acc->id) {
                        const std::string mbase = base+"/message/"+std::to_string(m.id);
                        html += "<form method=\"POST\" action=\""+mbase+"/edit\" class=\"row\">"
                                "<input name=\"text\" value=\""+html_escape(m.text)+"\" maxlength=\"2000\">"
                                "<button>Save edit</button></form>"
                                "<form method=\"POST\" action=\""+mbase+"/delete\">"
                                "<button class=\"ghost\">Delete</button></form>";
                    }
                }
                html += "</div></div>";
            }
            if (!any) html += "<p class=\"empty\">No messages yet.</p>";
            html += "<form method=\"POST\" action=\""+base+"/send\">"
                    "<label>Message</label><input name=\"text\" maxlength=\"2000\">"
                    "<button>Send</button></form></div>";
        }
        return pg("Group", html);
    }

    if(method=="GET"&&path=="/communities") {
        if(!acc) return http_redir("/login");
        std::string err;
        if (qs["err"]=="slug") err = "<p class=\"warn\">Need a free slug: letters, numbers, dots, underscores, hyphens.</p>";
        std::string html = "<h1>Communities</h1>"+err+
            "<p class=\"lede\">These are platform groups. They are not AVND localities and they do not confer standing.</p>"
            "<div class=\"card\"><h2>Start one</h2><form method=\"POST\" action=\"/communities\">"
            "<label>Slug</label><input name=\"slug\" required maxlength=\"32\" pattern=\"[A-Za-z0-9._-]+\" placeholder=\"town-hall\">"
            "<label>Name</label><input name=\"name\" required maxlength=\"80\">"
            "<label>About</label><textarea name=\"about\" maxlength=\"2000\"></textarea>"
            "<button>Create</button></form></div><div class=\"card\"><h2>All communities</h2>";
        auto all = g_store.all_communities();
        if (all.empty()) html += "<p class=\"empty\">None yet.</p>";
        for (auto& c : all) {
            html += "<div class=\"person\"><div><a href=\"/c/"+html_escape(c.slug)+"\"><strong>"+html_escape(c.name)+
                    "</strong></a> <span class=\"muted\">/"+html_escape(c.slug)+"</span>"
                    "<p class=\"muted\">"+html_escape(c.about)+"</p></div></div>";
        }
        html += "</div>";
        return pg("Communities", html);
    }

    if(method=="GET"&&path.rfind("/c/",0)==0&&path.size()>10&&path.compare(path.size()-7,7,"/canvas")==0) {
        if(!acc) return http_redir("/login");
        const std::string slug = path.substr(3, path.size()-3-7);
        if (!g_store.community(slug) || !g_store.is_member(slug, user)) return http_redir("/c/"+slug);
        return pg("Compose a post", canvas_tool_page(user, "", false, "/c/"+slug));
    }

    if(method=="GET"&&path.rfind("/c/",0)==0) {
        if(!acc) return http_redir("/login");
        const std::string slug = path.substr(3);
        auto c = g_store.community(slug);
        if (!c)
            return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>No such community.</p></div>"));
        const bool member = g_store.is_member(slug, user);
        const bool owner = c->owner == user;
        std::string html = "<h1>"+html_escape(c->name)+"</h1>"
            "<p class=\"muted\">/"+html_escape(c->slug)+" · started by "+html_escape(c->owner)+
            " · a platform community, not a locality</p>";
        if (qs["err"]=="invite") html += "<p class=\"warn\">Conflicts with another member’s contact settings.</p>";
        html += composer_media_error(qs["perr"]);
        html += "<div class=\"card\"><p>"+html_escape(c->about.empty()?"No description.":c->about)+"</p>";
        if (!member) {
            html += "<form method=\"POST\" action=\"/c/"+html_escape(slug)+"/join\"><button>Join</button></form>";
        } else if (!owner) {
            html += "<form method=\"POST\" action=\"/c/"+html_escape(slug)+"/leave\"><button class=\"ghost\">Leave</button></form>";
        } else {
            html += "<form method=\"POST\" action=\"/c/"+html_escape(slug)+"/delete\" onsubmit=\"return confirm('Delete this community?');\">"
                    "<button class=\"danger\">Delete community</button></form>";
        }
        html += "</div>";
        if (member) {
            html += "<div class=\"card\"><form method=\"POST\" action=\"/c/"+html_escape(slug)+"\" enctype=\"multipart/form-data\">"
                    "<label>Post in "+html_escape(c->name)+"</label>"
                    "<textarea name=\"text\" maxlength=\"2000\"></textarea>"
                    "<label>Photo or video (optional)</label><input type=\"file\" name=\"media\" accept=\"image/*,video/*\">"
                    "<button>Post</button></form>"
                    "<p class=\"muted\" style=\"margin-top:.5rem\">Or <a href=\"/c/"+html_escape(slug)+"/canvas\">open the canvas</a> "
                    "to draw, crop, layer, or paste one together first.</p></div>";
            html += "<div class=\"card\"><h2>Invite</h2>"
                    "<form method=\"POST\" action=\"/c/"+html_escape(slug)+"/invite\">"
                    "<label>Username</label><div class=\"row\"><input name=\"who\" required><button>Invite</button></div></form>"
                    "<p class=\"muted\">People can also join themselves. An invite that conflicts with contact settings is refused with a generic error.</p></div>";
        }
        html += "<div class=\"card\"><h2>Members</h2><ul class=\"plain\">";
        for (auto& n : g_store.members_of(slug)) {
            html += "<li><a href=\"/u/"+html_escape(n)+"\">"+html_escape(n)+"</a>"+(n==c->owner?" <span class=\"muted\">starter</span>":"")+"</li>";
        }
        html += "</ul></div>";
        auto posts = g_store.posts_in(slug, 80);
        if (posts.empty()) html += "<p class=\"empty\">No posts yet.</p>";
        for (const auto& p : posts) html += "<div class=\"card\">"+post_block(p, user)+"</div>";
        return pg(c->name, html);
    }

    if(method=="GET"&&path=="/notices") {
        if(!acc) return http_redir("/login");
        g_store.mark_notices_read(user);
        std::string html = "<h1>Notices</h1><div class=\"card\">";
        auto notes = g_store.notices_for(user, 50);
        if (notes.empty()) html += "<p class=\"empty\">None.</p>";
        for (auto& n : notes) {
            std::string line;
            if (n.type=="friend_request") line = html_escape(n.from)+" sent a friend request.";
            else if (n.type=="friend_accept") line = html_escape(n.from)+" accepted your request.";
            else if (n.type=="message") line = html_escape(n.from)+" sent mail.";
            else if (n.type=="group_invite") line = html_escape(n.from)+" added you to /"+html_escape(n.extra)+".";
            else line = html_escape(n.type);
            html += "<div class=\"person\"><div>"+line+"<div class=\"muted\">"+html_escape(fmt_time(n.ts))+"</div></div></div>";
        }
        html += "<p class=\"muted\">There is no notice for a declined friend request.</p></div>";
        return pg("Notices", html);
    }

    if(method=="GET"&&path=="/settings") {
        if(!acc) return http_redir("/login");
        auto p = g_store.prefs_of(user);
        auto find_sel = [&](Findability f){ return p.findability==f?" checked":""; };
        std::string html = "<h1>Settings</h1>"
            "<p class=\"muted\">Five sections. Not a junk drawer. Not kernel seats.</p>";
        if (qs["ok"]=="password") html += "<p class=\"ok\">Password changed.</p>";
        if (qs["err"]=="password") html += "<p class=\"warn\">Could not change password. Check the current one.</p>";
        if (qs["err"]=="delete") html += "<p class=\"warn\">Type your username to delete.</p>";

        html += "<div class=\"card\"><h2>Security</h2>"
            "<form method=\"POST\" action=\"/settings/password\">"
            "<label>Current password</label><input type=\"password\" name=\"current\" required autocomplete=\"current-password\">"
            "<label>New password</label><input type=\"password\" name=\"next\" required autocomplete=\"new-password\">"
            "<button>Change password</button></form><h2>Sessions</h2>";
        auto all = g_store.sessions_for(acc->id);
        int shown = 0;
        for (const auto& s : all) {
            if (s.revoked) continue;
            if (s.expires_at <= now()) continue;
            ++shown;
            const bool current = s.id == sid;
            html += "<div class=\"sess\"><div><strong>"+html_escape(s.device_label.empty()?"web":s.device_label)+"</strong>"
                    +(current?" <span class=\"muted\">this device</span>":"")+
                    "<div class=\"muted\">Started "+html_escape(fmt_time(s.created_at))+
                    " · last seen "+html_escape(fmt_time(s.last_seen))+"</div></div>";
            if (!current) {
                html += "<form method=\"POST\" action=\"/settings/revoke\">"
                        "<input type=\"hidden\" name=\"sid\" value=\""+html_escape(s.id)+"\">"
                        "<button class=\"danger\" type=\"submit\">Revoke</button></form>";
            }
            html += "</div>";
        }
        if (!shown) html += "<p class=\"muted\">None.</p>";
        html += "<form method=\"POST\" action=\"/settings/logout-all\" style=\"margin-top:1rem\">"
                "<button class=\"danger\" type=\"submit\">Log out other devices</button></form>"
                "<p class=\"muted\">Phone change with delay is not in this slice. Passkeys are later.</p></div>";

        html += std::string("<div class=\"card\"><h2>Privacy</h2><form method=\"POST\" action=\"/settings/privacy\">")
            + "<label>Who can find you</label>"
            + "<p class=\"muted\">One axis, three states. Each level includes the one above it.</p>"
            + "<p><label><input type=\"radio\" name=\"findability\" value=\"0\"" + find_sel(Findability::FriendsOnly) + "> Friends only</label>"
            + "<br><span class=\"muted\">Only people who are already your friends can find you. Strangers can't search you up at all.</span></p>"
            + "<p><label><input type=\"radio\" name=\"findability\" value=\"1\"" + find_sel(Findability::ExactMatch) + "> Exact match</label>"
            + "<br><span class=\"muted\">Anyone who types your exact username can find you, but you won't show up as they type.</span></p>"
            + "<p><label><input type=\"radio\" name=\"findability\" value=\"2\"" + find_sel(Findability::Typeahead) + "> Typeahead</label>"
            + "<br><span class=\"muted\">Anyone can find you, including as a partial match while they're still typing.</span></p>"
            + "<p><label><input type=\"checkbox\" name=\"group_invite_confirm\" value=\"1\"" + (p.group_invite_confirm?" checked":"") + "> "
            + "Don't add me to groups without asking</label></p>"
            + "<p><label><input type=\"checkbox\" name=\"presence_hide\" value=\"1\"" + (p.presence_hide?" checked":"") + "> "
            + "Hide presence (presence is not shown in this release)</label></p>"
            + "<p class=\"muted\">Direct messages are friends-only for everyone until age can be assured. "
            + "That is policy, not a self-reported minor toggle.</p>"
            + "<label>Visibility register</label>"
            + "<p class=\"muted\">Each level admits every level after it too: public sees everything members do, "
            + "members see everything friends do, friends see everything only-me hides from everyone else.</p>";
        auto vis_select = [&](const char* field, Visibility current) {
            auto opt = [&](Visibility v, const char* label) {
                return std::string("<option value=\"") + std::to_string(static_cast<int>(v)) + "\""
                    + (current == v ? " selected" : "") + ">" + label + "</option>";
            };
            return std::string("<select name=\"") + field + "\">"
                + opt(Visibility::Public, "Public")
                + opt(Visibility::Members, "Members")
                + opt(Visibility::Friends, "Friends")
                + opt(Visibility::OnlyMe, "Only me")
                + "</select>";
        };
        html += "<table class=\"register\"><tr><th>Surface</th><th>Who can see it</th></tr>"
                "<tr><td>Profile</td><td>" + vis_select("profile_visibility", p.profile_visibility) + "</td></tr>"
                "<tr><td>Posts</td><td>" + vis_select("posts_visibility", p.posts_visibility) + "</td></tr>"
                "</table>"
                "<button>Save privacy</button></form><h2>Block list</h2>";
        auto bl = g_store.block_entries_of(user);
        html += "<p class=\"muted eyebrow\">"+std::to_string(bl.size())+
                (bl.size()==1?" member":" members")+" &middot; not visible to them</p>";
        if (bl.empty()) html += "<p class=\"empty\">No one blocked.</p>";
        for (auto& b : bl) {
            html += "<div class=\"person\"><div><span>"+html_escape(b.other)+"</span>"
                    "<div class=\"muted\">Blocked "+html_escape(fmt_time(b.at))+
                    (b.reason.empty() ? "" : " · " + html_escape(b.reason))+"</div></div>"
                    "<form method=\"POST\" action=\"/settings/unblock\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(b.other)+"\">"
                    "<button class=\"ghost\">Unblock</button></form></div>";
        }
        html += "<form method=\"POST\" action=\"/settings/block\" style=\"margin-top:.75rem\">"
                "<div class=\"row\"><input name=\"who\" placeholder=\"username\" required style=\"flex:1\">"
                "<button class=\"danger\" type=\"submit\">Add block</button></div>"
                "<input name=\"reason\" placeholder=\"Reason (optional, only you see it)\" maxlength=\"200\"></form>";
        html += "</div>";

        html += "<div class=\"card\"><h2>Identity</h2>"
            "<p>No civic identity is linked to this platform account.</p>"
            "<p class=\"muted\">Combined verification for voting is not live. Unlink is not available because there is nothing to unlink. "
            "Unlink would drop voting access until re-verified; it would not resign citizenship. "
            "This page does not write CitizenId, GrantCitizenship, points, seats, or parcels.</p></div>";

        html += "<div class=\"card\"><h2>Data</h2>"
            "<p><a class=\"btn ghost\" href=\"/export\">Download your export</a></p>"
            "<p class=\"muted\">Export includes username, blurb, profile CSS, your friend list, communities you started or joined, "
            "posts you wrote, settings, and messages you sent. It does not include other people’s text or your password hash.</p>"
            "<form method=\"POST\" action=\"/settings/delete\">"
            "<label>Delete account — type your username</label>"
            "<input name=\"confirm\" placeholder=\""+html_escape(user)+"\" autocomplete=\"off\">"
            "<button class=\"danger\">Delete account</button></form>"
            "<p class=\"muted\">Sessions end immediately. Mail you sent is tombstoned as [deleted account]. "
            "The phone binding is held ninety days so a delete cannot shed a suspension. This is not ResignCitizen.</p></div>";

        html += std::string("<div class=\"card\"><h2>Notifications</h2><form method=\"POST\" action=\"/settings/notify\">")
            + "<p><label><input type=\"checkbox\" name=\"notify_friend\" value=\"1\"" + (p.notify_friend?" checked":"") + "> Friend requests and accepts</label></p>"
            + "<p><label><input type=\"checkbox\" name=\"notify_message\" value=\"1\"" + (p.notify_message?" checked":"") + "> New mail</label></p>"
            + "<p><label><input type=\"checkbox\" name=\"notify_group\" value=\"1\"" + (p.notify_group?" checked":"") + "> Community invites</label></p>"
            + "<p class=\"muted\">New-device login, moderation, and age re-verification are always on when those events exist. "
            + "There is no FriendRequestDeclined.</p>"
            + "<button>Save notifications</button></form></div>";
        return pg("Settings", html);
    }

    if(method=="GET"&&path=="/export") {
        if(!acc) return http_redir("/login");
        std::ostringstream o;
        o << "LiquidGov.US platform export\n";
        o << "This is platform data. It is not the governance kernel.\n\n";
        o << "username: " << user << "\n";
        o << "created_at: " << fmt_time(acc->created_at) << "\n";
        o << "blurb: " << g_store.blurb(acc->id) << "\n";
        o << "profile css:\n" << g_store.author_css(acc->id) << "\n\n";
        o << "friends:\n";
        for (auto& n : g_store.friends_of(user)) o << "  " << n << "\n";
        o << "\ncommunities:\n";
        for (auto& c : g_store.communities_of(user)) o << "  " << c.slug << " " << c.name << "\n";
        o << "\nposts you wrote:\n";
        for (auto& p : g_store.posts_by(user)) {
            o << "  [" << fmt_time(p.ts) << "] ";
            if (!p.community.empty()) o << "/" << p.community << " ";
            o << p.text << "\n";
        }
        o << "\nmessages you sent:\n";
        for (auto& m : g_store.mail_sent_by(user)) {
            o << "  to " << m.to << " [" << fmt_time(m.ts) << "] " << m.text << "\n";
        }
        auto pr = g_store.prefs_of(user);
        o << "\nsettings:\n";
        o << "  findability: " << static_cast<int>(pr.findability) << "\n";
        o << "  group_invite_confirm: " << (pr.group_invite_confirm?"yes":"no") << "\n";
        std::string extra = "Content-Disposition: attachment; filename=\"liquidgov-export.txt\"\r\n";
        extra += "Cache-Control: no-store\r\n";
        return http_raw("200 OK", "text/plain; charset=utf-8", o.str(), extra);
    }

    if(method=="POST"&&path=="/signup") {
        auto f=parse_form(body);
        auto uname=trim_ws(f["username"]);
        auto phone=trim_ws(f["phone"]);
        auto pass=f["password"];
        if (!valid_username(uname)) {
            std::string err = uname.empty()
                ? "Need a username: one word, 1–32 letters/numbers (dots, underscores, hyphens ok). No spaces, no @."
                : ("“"+uname+"” isn’t a valid username. One word: letters, numbers, dots, underscores, hyphens. No spaces.");
            return http_ok(signup_page(user, unread, err, uname, phone));
        }
        if (pass.empty() || phone.empty())
            return http_ok(signup_page(user, unread, "Need username, password, and phone.", uname, phone));
        if (g_store.phone_quarantined(fingerprint_phone(phone)))
            return http_ok(signup_page(user, unread, "Could not create that account.", uname, phone));
        auto r=create_account(g_store,g_breach,uname,pass,phone);
        if(r.error==AuthError::None) {
            if (auto created = g_store.find_by_username(uname))
                g_store.set_phone_fp(created->id, fingerprint_phone(phone));
            r=login(g_store,g_limiter,uname,pass,device_label_from(req,peer_ip),peer_ip);
        }
        if(r.error==AuthError::UsernameTaken)
            return http_ok(signup_page(user, unread, "That username is taken. Pick another.", uname, phone));
        if(r.error!=AuthError::None||!r.session)
            return http_ok(signup_page(user, unread, "Need username, password, and phone.", uname, phone));
        return http_redir("/me", set_cookie(r.session->id));
    }
    if(method=="POST"&&path=="/login") {
        auto f=parse_form(body);
        auto uname = trim_ws(f["username"]);
        auto r=login(g_store,g_limiter,uname,f["password"],device_label_from(req,peer_ip),peer_ip);
        if(r.error!=AuthError::None||!r.session) {
            const char* code = (r.error==AuthError::RateLimited) ? "ratelimit" : "login";
            return http_redir("/login?err=" + std::string(code) + "&username=" + url_encode(uname));
        }
        return http_redir("/feed", set_cookie(r.session->id));
    }
    if(method=="POST"&&path=="/me/post") {
        if(!acc) return http_redir("/login");
        auto hdr_end = req.find("\r\n\r\n");
        std::string hdrs = hdr_end==std::string::npos ? req : req.substr(0, hdr_end);
        auto c = read_composer_post(hdrs, body);
        if (!c.ok) return http_redir(std::string("/me?perr=")+c.err);
        if (!c.text.empty() || !c.attachment_blob_id.empty())
            g_store.add_post(user, c.text, "", "", 0, c.attachment_blob_id, c.attachment_mime);
        return http_redir("/me");
    }
    if(method=="POST"&&path=="/me/repost") {
        if(!acc) return http_redir("/login");
        auto f = parse_form(body);
        std::uint64_t pid = 0;
        try { pid = std::stoull(f["post_id"]); } catch (...) { return http_redir("/me"); }
        auto target = g_store.post_by_id(pid);
        if (!target) return http_redir("/me");
        // A repost of a repost still credits the true original author, not
        // the intermediate reposter -- see the repost_of_author comment on
        // Post.
        const std::string& origAuthor = target->repost_of_author.empty() ? target->author : target->repost_of_author;
        std::uint64_t origId = target->repost_of_author.empty() ? target->id : target->repost_of_id;
        if (g_store.is_blocked(user, origAuthor)) return http_redir("/me");
        std::string blob_id, mime;
        if (!target->attachment_blob_id.empty()) {
            // put() is content-addressed and idempotent -- re-putting the
            // same bytes lands on the exact same blob id while correctly
            // incrementing its refcount for this new post referencing it.
            auto bytes = g_blobs.get(target->attachment_blob_id);
            if (bytes) {
                blob_id = g_blobs.put(*bytes, target->attachment_mime);
                mime = target->attachment_mime;
            }
        }
        g_store.add_post(user, target->text, "", origAuthor, origId, blob_id, mime);
        return http_redir("/me");
    }
    // Reply and second both need to land back on whatever page the post was
    // actually being viewed on (feed, a profile, a community) -- post_block
    // renders in all of those, so unlike most POST handlers here a fixed
    // redirect target would be wrong more often than not. Referer, same
    // pattern the theme toggle already uses, falling back to /feed.
    if(method=="POST"&&path.rfind("/post/",0)==0&&path.size()>12&&
       path.compare(path.size()-6,6,"/reply")==0) {
        if(!acc) return http_redir("/login");
        std::string back = header_value(req, "Referer");
        if (back.empty() || back.rfind("http",0)!=0) back = "/feed";
        std::uint64_t pid = 0;
        try { pid = std::stoull(path.substr(6, path.size()-6-6)); } catch (...) { return http_redir(back); }
        if (!g_store.post_by_id(pid)) return http_redir(back);
        auto text = trim_ws(parse_form(body)["text"]);
        if (!text.empty()) g_store.add_reply(pid, user, text);
        return http_redir(back);
    }
    if(method=="POST"&&path.rfind("/post/",0)==0&&path.size()>13&&
       path.compare(path.size()-7,7,"/second")==0) {
        if(!acc) return http_redir("/login");
        std::string back = header_value(req, "Referer");
        if (back.empty() || back.rfind("http",0)!=0) back = "/feed";
        std::uint64_t pid = 0;
        try { pid = std::stoull(path.substr(6, path.size()-6-7)); } catch (...) { return http_redir(back); }
        if (!g_store.post_by_id(pid)) return http_redir(back);
        g_store.toggle_second(pid, user);
        return http_redir(back);
    }
    if(method=="POST"&&path=="/me/css") {
        if(!acc) return http_redir("/login");
        auto f = parse_form(body);
        const std::string& raw_css = f["css"];
        auto check = sanitize_author_css(raw_css);
        if (!check.ok) return http_redir("/me?err=css&reason=" + check.code);
        g_store.set_author_css(acc->id, raw_css);
        return http_redir("/me?ok=css");
    }
    if(method=="POST"&&path=="/me/avatar") {
        if(!acc) return http_redir("/login");
        auto hdr_end = req.find("\r\n\r\n");
        std::string hdrs = hdr_end==std::string::npos ? req : req.substr(0, hdr_end);
        auto file = multipart_file(header_value(hdrs, "Content-Type"), body, "photo");
        if (!file || file->empty()) return http_redir("/me?err=image");
        auto r = save_avatar(g_blobs, g_store, g_scanner, acc->id, *file);
        if (r.error == SaveAvatarError::TooLarge) return http_redir("/me?err=large");
        if (r.error == SaveAvatarError::FailedScan) return http_redir("/me?err=scan");
        if (r.error != SaveAvatarError::None) return http_redir("/me?err=image");
        return http_redir("/me");
    }
    if(method=="POST"&&path=="/me/avatar/clear") {
        if(!acc) return http_redir("/login");
        clear_avatar(g_blobs, g_store, acc->id);
        return http_redir("/me");
    }
    if(method=="POST"&&path=="/me/canvas/clear") {
        if(!acc) return http_redir("/login");
        g_store.clear_canvas_frames(acc->id);
        return http_redir("/me/canvas");
    }
    if(method=="POST"&&path=="/me/canvas/frame") {
        if(!acc) return http_redir("/login");
        // Reuses save_avatar's exact normalize/scan/size-cap pipeline (the
        // security-relevant part) via a throwaway AvatarStore that just
        // captures the resulting blob id instead of writing the account's
        // real single avatar slot -- canvas frames are an ordered list,
        // not a single value, so FileStore's own AvatarStore side can't be
        // reused directly for this.
        struct CaptureBlobId : AvatarStore {
            std::optional<Avatar> get(AccountId) const override { return std::nullopt; }
            void set(const Avatar& a) override { blob_id = a.blob_id; }
            void erase(AccountId) override {}
            std::string blob_id;
        } capture;
        auto hdr_end = req.find("\r\n\r\n");
        std::string hdrs = hdr_end==std::string::npos ? req : req.substr(0, hdr_end);
        auto file = multipart_file(header_value(hdrs, "Content-Type"), body, "frame");
        if (!file || file->empty()) return http_redir("/me/canvas?err=image");
        if (g_store.canvas_frames(acc->id).size() >= kCanvasMaxFrames)
            return http_redir("/me/canvas?err=full");
        auto r = save_avatar(g_blobs, capture, g_scanner, acc->id, *file);
        if (r.error == SaveAvatarError::TooLarge) return http_redir("/me/canvas?err=large");
        if (r.error != SaveAvatarError::None) return http_redir("/me/canvas?err=image");
        g_store.add_canvas_frame(acc->id, capture.blob_id);
        return http_ok("ok");
    }
    if(method=="POST"&&path=="/me/canvas/mode") {
        if(!acc) return http_redir("/login");
        auto f = parse_form(body);
        g_store.set_canvas_timelapse(acc->id, f["timelapse"]=="1");
        return http_redir("/me/canvas?ok=1");
    }
    if(method=="POST"&&path=="/me/jukebox/add") {
        if(!acc) return http_redir("/login");
        auto url = trim_ws(parse_form(body)["url"]);
        auto id = extract_youtube_id(url);
        if (!id) return http_redir("/me?jerr=url");
        if (!g_store.add_jukebox_track(user, *id, user)) return http_redir("/me?jerr=full");
        return http_redir("/me");
    }
    if(method=="POST"&&path=="/me/jukebox/remove") {
        if(!acc) return http_redir("/login");
        g_store.remove_jukebox_track(user, parse_form(body)["video_id"]);
        return http_redir("/me");
    }
    if(method=="POST"&&path=="/me/jukebox/policy") {
        if(!acc) return http_redir("/login");
        auto f = parse_form(body);
        int pv = 0;
        try { pv = std::stoi(f["policy"]); } catch (...) { pv = 0; }
        if (pv < 0 || pv > 2) pv = 0;
        auto p = g_store.prefs_of(user);
        p.jukebox_policy = static_cast<JukeboxPolicy>(pv);
        g_store.set_prefs(user, p);
        return http_redir("/me");
    }
    // A guest's one-shot request on someone else's jukebox. Gated by the
    // owner's JukeboxPolicy, the owner's block list, and one outstanding
    // request per guest -- add_jukebox_track itself only enforces the
    // account/id/cap checks it always has. Also reachable by the owner
    // themselves (posting from their own /u/<username> page rather than
    // /me), in which case none of the guest gates apply -- same as /me/jukebox/add.
    if(method=="POST"&&path.rfind("/u/",0)==0&&path.size()>15&&
       path.compare(path.size()-12,12,"/jukebox/add")==0) {
        const std::string who = path.substr(3, path.size()-3-12);
        if(!acc) return http_redir("/login");
        if (!valid_username(who)) return http_redir("/");
        auto found = g_store.find_by_username(who);
        if (!found) return http_redir("/");
        if (who != user) {
            if (g_store.is_blocked(who, user)) return http_redir("/u/"+who+"?jerr=denied");
            auto policy = g_store.prefs_of(who).jukebox_policy;
            bool allowed = policy == JukeboxPolicy::Anyone ||
                           (policy == JukeboxPolicy::Friends && g_store.are_friends(user, who));
            if (!allowed) return http_redir("/u/"+who+"?jerr=denied");
            for (const auto& t : g_store.jukebox_of(who)) {
                if (t.queued_by == user) return http_redir("/u/"+who+"?jerr=pending");
            }
        }
        auto url = trim_ws(parse_form(body)["url"]);
        auto id = extract_youtube_id(url);
        if (!id) return http_redir("/u/"+who+"?jerr=url");
        if (!g_store.add_jukebox_track(who, *id, user)) return http_redir("/u/"+who+"?jerr=full");
        return http_redir("/u/"+who);
    }
    // Fired by the embedded player when the currently displayed track
    // actually finishes -- see jukebox_widget's onStateChange script.
    // complete_jukebox_track looks up the track's own stored queued_by
    // itself (never trusts anything from the client beyond the video id),
    // so this is safe to call from anyone signed in, owner or visitor.
    if(method=="POST"&&path.rfind("/u/",0)==0&&path.size()>20&&
       path.compare(path.size()-17,17,"/jukebox/complete")==0) {
        const std::string who = path.substr(3, path.size()-3-17);
        if(!acc) return http_redir("/login");
        if (valid_username(who)) g_store.complete_jukebox_track(who, parse_form(body)["video_id"]);
        return http_ok("ok");
    }
    if(method=="POST"&&path=="/feed") {
        if(!acc) return http_redir("/login");
        auto hdr_end = req.find("\r\n\r\n");
        std::string hdrs = hdr_end==std::string::npos ? req : req.substr(0, hdr_end);
        auto c = read_composer_post(hdrs, body);
        if (!c.ok) return http_redir(std::string("/feed?perr=")+c.err);
        auto text = trim_ws(c.text);
        if (!text.empty() || !c.attachment_blob_id.empty())
            g_store.add_post(user, text, "", "", 0, c.attachment_blob_id, c.attachment_mime);
        return http_redir("/feed");
    }
    if(method=="POST"&&path=="/proposals") {
        if(!acc) return http_redir("/login");
        auto f = parse_form(body);
        auto title = trim_ws(f["title"]);
        auto ptext = trim_ws(f["body"]);
        if (title.empty() || ptext.empty()) return http_redir("/proposals?err=empty");
        g_store.add_proposal(user, title, ptext);
        return http_redir("/proposals");
    }
    if(method=="POST"&&path=="/proposals/vote") {
        if(!acc) return http_redir("/login");
        auto f = parse_form(body);
        std::uint64_t id = 0;
        try { id = std::stoull(f["id"]); } catch (...) { return http_redir("/proposals"); }
        if (!g_store.proposal_by_id(id)) return http_redir("/proposals");
        int cv = 2;
        try { cv = std::stoi(f["choice"]); } catch (...) { cv = 2; }
        if (cv < 0 || cv > 2) cv = 2;
        g_store.cast_proposal_vote(id, user, static_cast<ProposalVote>(cv));
        return http_redir("/proposals");
    }
    if(method=="POST"&&path=="/friends") {
        if(!acc) return http_redir("/login");
        auto who=trim_ws(parse_form(body)["who"]);
        if(valid_username(who) && who!=user && g_store.find_by_username(who)
           && !g_store.is_blocked(user, who) && !g_store.is_blocked(who, user)) {
            g_store.add_friend_request(user, who);
        }
        return http_redir("/friends");
    }
    if(method=="POST"&&path=="/friends/accept") {
        if(!acc) return http_redir("/login");
        auto who=parse_form(body)["who"];
        g_store.accept_friend_request(who, user);
        return http_redir("/friends");
    }
    if(method=="POST"&&path=="/friends/unfriend") {
        if(!acc) return http_redir("/login");
        g_store.unfriend(user, parse_form(body)["who"]);
        return http_redir("/friends");
    }
    if(method=="POST"&&path=="/friends/block") {
        if(!acc) return http_redir("/login");
        auto who=parse_form(body)["who"];
        if (valid_username(who) && who!=user) g_store.block(user, who);
        return http_redir("/friends");
    }
    if(method=="POST"&&path=="/friends/inner") {
        if(!acc) return http_redir("/login");
        if (!g_store.add_inner(user, parse_form(body)["who"]))
            return http_redir("/friends?err=inner");
        return http_redir("/friends");
    }
    if(method=="POST"&&path=="/friends/inner-remove") {
        if(!acc) return http_redir("/login");
        g_store.remove_inner(user, parse_form(body)["who"]);
        return http_redir("/friends");
    }
    if(method=="POST"&&path=="/mail") {
        if(!acc) return http_redir("/login");
        auto hdr_end = req.find("\r\n\r\n");
        std::string hdrs = hdr_end==std::string::npos ? req : req.substr(0, hdr_end);
        std::string ctype = header_value(hdrs, "Content-Type");
        std::string to, text;
        std::optional<std::vector<std::uint8_t>> file;
        if (ctype.find("multipart/form-data") != std::string::npos) {
            to = multipart_field(ctype, body, "to");
            text = multipart_field(ctype, body, "text");
            file = multipart_file(ctype, body, "attachment");
        } else {
            auto f = parse_form(body);
            to = f["to"]; text = f["text"];
        }
        if(text.size()>2000) text.resize(2000);
        // Do not open stranger DMs: friends-only, both directions unblocked,
        // exactly as before attachments existed.
        if(to.empty() || !g_store.are_friends(user,to) || g_store.is_blocked(user,to) || g_store.is_blocked(to,user))
            return http_redir("/mail?with="+to);

        std::optional<std::string> blob_id, mime;
        if (file && !file->empty()) {
            auto r = ingest_file(g_blobs, g_mail_scanner, g_mail_caps, *file);
            if (r.error != FileTransferError::None || !r.blob_id) {
                const char* code = "type";
                switch (r.error) {
                    case FileTransferError::TooLarge: code = "large"; break;
                    case FileTransferError::ActiveContentBlocked: code = "active"; break;
                    case FileTransferError::FailedScan: code = "scan"; break;
                    default: code = "type"; break;
                }
                return http_redir("/mail?with="+to+"&err="+code);
            }
            blob_id = r.blob_id;
            mime = r.stored_mime_type;
        }
        if (!text.empty() || blob_id) g_store.add_mail(user,to,text,blob_id,mime);
        return http_redir("/mail?with="+to);
    }

    if(method=="POST"&&path=="/mail/group/new") {
        if(!acc) return http_redir("/login");
        const auto who = parse_form(body)["who"];
        std::vector<AccountId> invitees;
        std::string cur;
        auto flush_tok = [&]() {
            const auto name = trim_ws(cur);
            cur.clear();
            if (name.empty() || name == user) return;
            auto found = g_store.find_by_username(name);
            if (!found) return; // unknown username: silently dropped
            if (g_store.is_blocked(user, name) || g_store.is_blocked(name, user)) return;
            for (auto id : invitees) if (id == found->id) return; // dedupe
            invitees.push_back(found->id);
        };
        for (char c : who) {
            if (c==','||c==' '||c=='\t'||c=='\n'||c=='\r') flush_tok();
            else cur += c;
        }
        flush_tok();
        auto result = create_conversation(g_store, g_social_graph, acc->id, invitees, /*is_group=*/true);
        return http_redir("/mail/g/"+std::to_string(*result.conversation_id));
    }

    if(method=="POST"&&path.rfind("/mail/g/",0)==0) {
        if(!acc) return http_redir("/login");
        std::string rest = path.substr(8);
        auto slash = rest.find('/');
        const std::string id_str = (slash==std::string::npos) ? rest : rest.substr(0,slash);
        const std::string action = (slash==std::string::npos) ? std::string() : rest.substr(slash+1);
        ConversationId conv_id = 0;
        try { conv_id = std::stoull(id_str); } catch(...) { return http_redir("/mail"); }
        auto conv = g_store.get_conversation(conv_id);
        if (!conv || !conv->is_group) return http_redir("/mail");
        const ConversationMember* self_member = nullptr;
        for (auto& m : conv->members) if (m.account == acc->id) self_member = &m;
        const std::string base = "/mail/g/"+id_str;

        if (action=="accept") {
            if (!self_member) return http_redir("/mail");
            accept_invite(g_store, acc->id, conv_id);
            return http_redir(base);
        }
        if (action=="leave") {
            if (!self_member) return http_redir("/mail");
            leave_conversation(g_store, acc->id, conv_id); // Pending "decline" and Active "leave" alike; never notified
            return http_redir("/mail");
        }
        // Everything below requires being an Active member.
        if (!self_member || self_member->state != MemberState::Active) return http_redir("/mail");

        if (action=="invite") {
            const auto who = trim_ws(parse_form(body)["who"]);
            auto found = g_store.find_by_username(who);
            bool refuse = !found || g_store.is_blocked(user, who) || g_store.is_blocked(who, user);
            if (!refuse) {
                auto r = add_member(g_store, g_social_graph, acc->id, found->id, conv_id);
                refuse = r.error != MessengerError::None;
            }
            return http_redir(refuse ? (base+"?err=invite") : base);
        }
        if (action=="send") {
            auto text = trim_ws(parse_form(body)["text"]);
            if (!text.empty()) {
                if (text.size()>2000) text.resize(2000);
                send_message(g_store, acc->id, conv_id, text);
            }
            return http_redir(base);
        }
        if (action.rfind("message/",0)==0) {
            const std::string mrest = action.substr(8);
            const auto mslash = mrest.find('/');
            if (mslash==std::string::npos) return http_redir(base);
            MessageId msg_id = 0;
            try { msg_id = std::stoull(mrest.substr(0,mslash)); } catch(...) { return http_redir(base); }
            const std::string mact = mrest.substr(mslash+1);
            if (mact=="edit") {
                auto text = trim_ws(parse_form(body)["text"]);
                if (!text.empty()) {
                    if (text.size()>2000) text.resize(2000);
                    edit_message(g_store, acc->id, msg_id, text);
                }
            } else if (mact=="delete") {
                delete_message(g_store, acc->id, msg_id, /*for_everyone=*/true);
            }
            return http_redir(base);
        }
        return http_redir(base);
    }
    if(method=="POST"&&path=="/communities") {
        if(!acc) return http_redir("/login");
        auto f=parse_form(body);
        auto slug=trim_ws(f["slug"]);
        auto name=trim_ws(f["name"]);
        auto about=f["about"]; if(about.size()>2000) about.resize(2000);
        Community c;
        c.slug = slug; c.name = name.empty()?slug:name; c.owner = user; c.about = about; c.created_at = now();
        if (!g_store.create_community(c)) return http_redir("/communities?err=slug");
        return http_redir("/c/"+slug);
    }
    if(method=="POST"&&path.rfind("/c/",0)==0) {
        if(!acc) return http_redir("/login");
        std::string rest = path.substr(3);
        std::string slug, action;
        auto slash = rest.find('/');
        if (slash==std::string::npos) { slug = rest; action = "post"; }
        else { slug = rest.substr(0,slash); action = rest.substr(slash+1); }
        if (!g_store.community(slug)) return http_redir("/communities");
        if (action=="join") { g_store.join_community(slug, user); return http_redir("/c/"+slug); }
        if (action=="leave") { g_store.leave_community(slug, user); return http_redir("/c/"+slug); }
        if (action=="delete") { g_store.delete_community(slug, user); return http_redir("/communities"); }
        if (action=="invite") {
            auto who=trim_ws(parse_form(body)["who"]);
            if (!g_store.invite_community(slug, user, who)) return http_redir("/c/"+slug+"?err=invite");
            return http_redir("/c/"+slug);
        }
        if (action=="post") {
            if (!g_store.is_member(slug, user)) return http_redir("/c/"+slug);
            auto hdr_end = req.find("\r\n\r\n");
            std::string hdrs = hdr_end==std::string::npos ? req : req.substr(0, hdr_end);
            auto c = read_composer_post(hdrs, body);
            if (!c.ok) return http_redir("/c/"+slug+"?perr="+c.err);
            auto text = trim_ws(c.text);
            if (!text.empty() || !c.attachment_blob_id.empty())
                g_store.add_post(user, text, slug, "", 0, c.attachment_blob_id, c.attachment_mime);
            return http_redir("/c/"+slug);
        }
        return http_redir("/c/"+slug);
    }
    if(method=="POST"&&path=="/settings/revoke") {
        if(!acc) return http_redir("/login");
        auto target = parse_form(body)["sid"];
        bool current = target == sid;
        bool owned = false;
        for (const auto& s : g_store.sessions_for(acc->id)) {
            if (s.id == target) { owned = true; break; }
        }
        if (owned) logout(g_store, target);
        if (current) return http_redir("/login", clear_cookie());
        return http_redir("/settings");
    }
    if(method=="POST"&&path=="/settings/logout-all") {
        if(!acc) return http_redir("/login");
        logout_all_devices(g_store, acc->id, sid);
        return http_redir("/settings");
    }
    if(method=="POST"&&path=="/settings/password") {
        if(!acc) return http_redir("/login");
        auto f=parse_form(body);
        if (!verify_password(f["current"], acc->password_hash) || f["next"].empty())
            return http_redir("/settings?err=password");
        if (g_breach.is_breached(f["next"]))
            return http_redir("/settings?err=password");
        g_store.set_password_hash(acc->id, hash_password(f["next"]));
        return http_redir("/settings?ok=password");
    }
    if(method=="POST"&&path=="/settings/privacy") {
        if(!acc) return http_redir("/login");
        auto f=parse_form(body);
        auto p = g_store.prefs_of(user);
        int fv = 1;
        try { fv = std::stoi(f["findability"]); } catch (...) { fv = 1; }
        if (fv < 0 || fv > 2) fv = 1;
        p.findability = static_cast<Findability>(fv);
        p.group_invite_confirm = f["group_invite_confirm"] == "1";
        p.presence_hide = f["presence_hide"] == "1";
        auto vis_of = [&](const std::string& field, Visibility fallback) {
            int v = static_cast<int>(fallback);
            try { v = std::stoi(f[field]); } catch (...) { v = static_cast<int>(fallback); }
            if (v < 0 || v > 3) v = static_cast<int>(fallback);
            return static_cast<Visibility>(v);
        };
        p.profile_visibility = vis_of("profile_visibility", p.profile_visibility);
        p.posts_visibility = vis_of("posts_visibility", p.posts_visibility);
        g_store.set_prefs(user, p);
        return http_redir("/settings");
    }
    if(method=="POST"&&path=="/settings/notify") {
        if(!acc) return http_redir("/login");
        auto f=parse_form(body);
        auto p = g_store.prefs_of(user);
        p.notify_friend = f["notify_friend"]=="1";
        p.notify_message = f["notify_message"]=="1";
        p.notify_group = f["notify_group"]=="1";
        g_store.set_prefs(user, p);
        return http_redir("/settings");
    }
    if(method=="POST"&&path=="/settings/unblock") {
        if(!acc) return http_redir("/login");
        g_store.unblock(user, parse_form(body)["who"]);
        return http_redir("/settings");
    }
    if(method=="POST"&&path=="/settings/block") {
        if(!acc) return http_redir("/login");
        auto f = parse_form(body);
        auto who = trim_ws(f["who"]);
        if (valid_username(who) && who != user) g_store.block(user, who, trim_ws(f["reason"]));
        return http_redir("/settings");
    }
    if(method=="POST"&&path=="/settings/delete") {
        if(!acc) return http_redir("/login");
        auto confirm = trim_ws(parse_form(body)["confirm"]);
        if (confirm != user) return http_redir("/settings?err=delete");
        clear_avatar(g_blobs, g_store, acc->id);
        g_store.delete_account(acc->id);
        return http_redir("/", clear_cookie());
    }
    return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>Not found.</p></div>"));
}
}
int main() {
    // FilesystemBlobStore::get()/meta() both gate on an in-memory refcount
    // entry that ONLY put() or adopt() ever create -- bytes sitting on
    // disk from a previous process are otherwise invisible, not just
    // unreferenced. Every blob id this server can later re-fetch by id
    // (as opposed to one only ever read back inside the same put() call
    // that created it) has to be re-adopted here on every startup, or it
    // 404s the moment the process restarts. Canvas frames and post
    // attachments were missed when those features were first built --
    // found via a live report of a "missing" image that was actually
    // sitting on disk the whole time, invisible only because of this.
    for (const auto& av : g_store.all_avatars())
        g_blobs.adopt(av.blob_id, "image/png");
    for (const auto& m : g_store.all_mail_with_attachments())
        g_blobs.adopt(*m.attachment_blob_id, m.attachment_mime.value_or("application/octet-stream"));
    for (const auto& blob_id : g_store.all_canvas_frame_blob_ids())
        g_blobs.adopt(blob_id, "image/png");
    for (const auto& p : g_store.all_posts_with_attachments())
        g_blobs.adopt(p.attachment_blob_id, p.attachment_mime);
    int fd=socket(AF_INET,SOCK_STREAM,0); if(fd<0) return 1;
    int yes=1; setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
    sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_port=htons(kPort); addr.sin_addr.s_addr=htonl(INADDR_ANY);
    if(bind(fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))<0){ perror("bind"); return 1; }
    listen(fd,16);
    std::cerr<<"LiquidGov http://0.0.0.0:"<<kPort<<" store="<<g_store.path()
             <<" blobs="<<blob_path_from_env()<<" (auth.cpp + FileStore LGP1)\n";
    for(;;){
        sockaddr_in peer{}; socklen_t plen=sizeof(peer);
        int c=accept(fd,reinterpret_cast<sockaddr*>(&peer),&plen); if(c<0) continue;
        timeval tv{}; tv.tv_sec=30; tv.tv_usec=0;
        setsockopt(c,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
        char ipbuf[INET_ADDRSTRLEN]="0.0.0.0"; inet_ntop(AF_INET,&peer.sin_addr,ipbuf,sizeof(ipbuf));
        std::string req;
        read_http_request(c, req);

        // /world/ws is the one route that doesn't get the ordinary
        // request/response treatment: a successful upgrade hands the fd to
        // its own thread and keeps it open, instead of sending one response
        // and closing. Every other route is untouched — still handled
        // synchronously on this single accept-loop thread, exactly as
        // before this feature existed.
        {
            auto line_end = req.find("\r\n");
            std::string method, rawpath;
            if (line_end != std::string::npos) {
                std::istringstream is(req.substr(0, line_end));
                is >> method >> rawpath;
            }
            if (method == "GET" && rawpath == "/world/ws" &&
                header_value(req, "Upgrade").find("websocket") != std::string::npos) {
                std::string username;
                {
                    std::lock_guard<std::mutex> lock(g_mu);
                    auto acc = account_from_req(req);
                    if (acc) username = acc->username;
                }
                auto key = header_value(req, "Sec-WebSocket-Key");
                if (!username.empty() && !key.empty()) {
                    std::string accept_val = websocket_accept_value(key);
                    std::ostringstream resp101;
                    resp101 << "HTTP/1.1 101 Switching Protocols\r\n"
                               "Upgrade: websocket\r\nConnection: Upgrade\r\n"
                               "Sec-WebSocket-Accept: " << accept_val << "\r\n\r\n";
                    std::string r = resp101.str();
                    if (send(c, r.data(), r.size(), 0) > 0) {
                        std::thread(run_world_session, c, username).detach();
                        continue; // socket now owned by the world session thread
                    }
                }
                close(c);
                continue;
            }
            if (method == "GET" && rawpath == "/community/ws" &&
                header_value(req, "Upgrade").find("websocket") != std::string::npos) {
                std::string username;
                {
                    std::lock_guard<std::mutex> lock(g_mu);
                    auto acc = account_from_req(req);
                    if (acc) username = acc->username;
                }
                auto key = header_value(req, "Sec-WebSocket-Key");
                if (!username.empty() && !key.empty()) {
                    std::string accept_val = websocket_accept_value(key);
                    std::ostringstream resp101;
                    resp101 << "HTTP/1.1 101 Switching Protocols\r\n"
                               "Upgrade: websocket\r\nConnection: Upgrade\r\n"
                               "Sec-WebSocket-Accept: " << accept_val << "\r\n\r\n";
                    std::string r = resp101.str();
                    if (send(c, r.data(), r.size(), 0) > 0) {
                        std::thread(run_community_session, c, username, std::string("")).detach();
                        continue;
                    }
                }
                close(c);
                continue;
            }
            // A group's own live jukebox -- same real-time station as
            // /community/ws above, just keyed by this community's slug
            // instead of the "" site-wide one, and gated to members (the
            // site-wide station stays open to anyone logged in; a group's
            // own station is for the group).
            if (method == "GET" && rawpath.rfind("/c/",0)==0 && rawpath.size()>11 &&
                rawpath.compare(rawpath.size()-11,11,"/jukebox/ws")==0 &&
                header_value(req, "Upgrade").find("websocket") != std::string::npos) {
                const std::string slug = rawpath.substr(3, rawpath.size()-3-11);
                std::string username;
                bool member = false;
                {
                    std::lock_guard<std::mutex> lock(g_mu);
                    auto acc = account_from_req(req);
                    if (acc) {
                        username = acc->username;
                        member = valid_username(slug) && g_store.is_member(slug, username);
                    }
                }
                auto key = header_value(req, "Sec-WebSocket-Key");
                if (!username.empty() && member && !key.empty()) {
                    std::string accept_val = websocket_accept_value(key);
                    std::ostringstream resp101;
                    resp101 << "HTTP/1.1 101 Switching Protocols\r\n"
                               "Upgrade: websocket\r\nConnection: Upgrade\r\n"
                               "Sec-WebSocket-Accept: " << accept_val << "\r\n\r\n";
                    std::string r = resp101.str();
                    if (send(c, r.data(), r.size(), 0) > 0) {
                        std::thread(run_community_session, c, username, slug).detach();
                        continue;
                    }
                }
                close(c);
                continue;
            }
        }

        std::string resp;
        try {
            resp = handle(req,ipbuf);
        } catch (const std::exception& e) {
            std::cerr<<"handle error: "<<e.what()<<"\n";
            resp = http_code("500 Internal Server Error",
                page_wrap("Error", nav("",0), "<div class=\"card\"><p>Could not save. Try again.</p></div>"));
        }
        send(c,resp.data(),resp.size(),0); close(c);
    }
}
