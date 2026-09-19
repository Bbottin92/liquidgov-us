// The Commons -- game daemon. Sibling process to liquidgov_http, never
// the same one (sibling.md). This round: local-only. No systemd, no
// nginx, no droplet contact -- see notes.md, 2026-09-16.
//
// Owns: presence (RAM), the SQLite ledger (catalogs + instances). Does
// NOT own: static files (nginx's job), tile collision/movement (stays
// client-side JS in commons/index.html; wasm.md explains why a shared
// core only earns its slot once this daemon is authoritative and a
// client needs to predict against the same rules). Does NOT open LGP1,
// ever, from any code path here.
//
// Cookie forwarding (glue.md, cookie.md, cookie-bind.md) is implemented:
// civic.cpp forwards a visitor's own Cookie to civic's same-box
// /internal/session-user and /internal/friends routes -- the daemon never
// parses a session id or opens LGP1 itself. The friends answer is only ever
// held in a short RAM TTL cache (friends.cpp), never persisted.

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "civic.hpp"
#include "db.hpp"
#include "friends.hpp"
#include "guest.hpp"
#include "house.hpp"
#include "http.hpp"
#include "json.hpp"
#include "placed.hpp"
#include "presence.hpp"
#include "reqjson.hpp"
#include "sqlx.hpp"

namespace {

long long now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

// The catalog readers below check every prepare/step result (sqlx.hpp): a
// failure is logged and becomes a 500 for that request, never an abort.
std::string items_json(sqlite3* db) {
    commons::sqlx::Stmt st(db,
        "SELECT slug, display_name, category, wall_slot_ok, sprite_ref FROM items ORDER BY item_id;");
    std::ostringstream out;
    out << "[";
    bool first = true;
    while (st.row()) {
        if (!first) out << ",";
        first = false;
        out << "{\"slug\":" << commons::json::str(st.column_text(0))
            << ",\"display_name\":" << commons::json::str(st.column_text(1))
            << ",\"category\":" << commons::json::str(st.column_text(2))
            << ",\"wall_slot_ok\":" << st.column_int(3)
            << ",\"sprite_ref\":" << commons::json::str(st.column_text(4)) << "}";
    }
    out << "]";
    return out.str();
}

std::string recipes_json(sqlite3* db) {
    std::vector<std::string> slugs, names, stations, out_slugs;
    std::vector<int> out_qty;
    {
        commons::sqlx::Stmt st(db,
            "SELECT r.slug, r.display_name, r.station, i.slug, ro.quantity "
            "FROM recipes r "
            "LEFT JOIN recipe_outputs ro ON ro.recipe_id = r.recipe_id "
            "LEFT JOIN items i ON i.item_id = ro.item_id "
            "ORDER BY r.recipe_id;");
        // Small enough (three V1 recipes, one output each) that grouping by
        // hand here beats a second query or a real JSON tree builder.
        while (st.row()) {
            slugs.push_back(st.column_text(0));
            names.push_back(st.column_text(1));
            stations.push_back(st.column_text(2));
            out_slugs.push_back(st.column_null(3) ? "" : st.column_text(3));
            out_qty.push_back(st.column_int(4));
        }
    }
    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < slugs.size(); ++i) {
        if (i) out << ",";
        out << "{\"slug\":" << commons::json::str(slugs[i])
            << ",\"display_name\":" << commons::json::str(names[i])
            << ",\"station\":" << commons::json::str(stations[i])
            << ",\"output_item\":" << commons::json::str(out_slugs[i])
            << ",\"output_qty\":" << out_qty[i] << "}";
    }
    out << "]";
    return out.str();
}

std::string maps_json(sqlite3* db) {
    commons::sqlx::Stmt st(db,
        "SELECT slug, display_name, kind, width_tiles, height_tiles, grid_col, grid_row FROM maps ORDER BY map_id;");
    std::ostringstream out;
    out << "[";
    bool first = true;
    while (st.row()) {
        if (!first) out << ",";
        first = false;
        out << "{\"slug\":" << commons::json::str(st.column_text(0))
            << ",\"display_name\":" << commons::json::str(st.column_text(1))
            << ",\"kind\":" << commons::json::str(st.column_text(2))
            << ",\"width_tiles\":" << st.column_int(3)
            << ",\"height_tiles\":" << st.column_int(4)
            << ",\"grid_col\":" << (st.column_null(5) ? "null" : std::to_string(st.column_int(5)))
            << ",\"grid_row\":" << (st.column_null(6) ? "null" : std::to_string(st.column_int(6)))
            << "}";
    }
    out << "]";
    return out.str();
}

// V2 lab placeables: a placed item slug must be a real catalog item, so
// a client that doesn't recognize the slug can't happen anyway, and so
// a broken/typo'd request doesn't wedge the one shared slot with
// something nothing can render. Not the same thing as items.wall_slot_ok
// -- that flag is reserved for the real, later profile-canvas wall slot
// (glue.md/canvas-frame.md); this placed slot is explicitly "not a civic
// PNG" (GROK_V2_ANSWER.md), any catalog item is eligible.
bool item_exists(sqlite3* db, const std::string& slug) {
    commons::sqlx::Stmt st(db, "SELECT 1 FROM items WHERE slug = ? LIMIT 1;");
    st.text(1, slug);
    return st.row();
}

// Cookie-bind.md: "the two auth paths never mix." A resolved cookie always
// wins and derives the id itself ("u:" + lg_username) -- the client's own
// id/secret fields are not even consulted in that case, bound or not. Only
// when nothing resolves does this fall back to the existing GuestRegistry
// check, exactly as every route here did before cookie bind existed.
// nullopt means the request is unauthorized outright (bad guest credential,
// no cookie).
//
// Used by /placed/ POST only: that route never touches PresenceStore, so
// there is no existing guest row a cookie-wins check could orphan --
// join/rekey/move/leave use the two functions below instead, which guard
// against exactly that (GROK_COOKIE_RESOLVER_REVIEW.md must-fix).
std::optional<std::string> effective_id(const std::string& headers_raw, const std::string& claimed_id,
                                         const std::string& secret, commons::GuestRegistry& guests) {
    if (auto username = commons::civic::resolve_username(headers_raw)) return "u:" + *username;
    if (guests.verify(claimed_id, secret)) return claimed_id;
    return std::nullopt;
}

// join/rekey only -- the two places identity is actually established.
// A resolved cookie still wins, but a mid-session login (guest walks in,
// then logs in without ever leaving) must not leave the old g_... row
// behind as a ghost: if the client can still prove it owned that guest
// row (its secret verifies), retire it here, at the moment the bound row
// replaces it. Cookie-bind.md: "promote at room change" -- this is that
// room change.
std::optional<std::string> bind_or_promote(const std::string& headers_raw, const std::string& claimed_id,
                                            const std::string& secret, commons::GuestRegistry& guests,
                                            commons::PresenceStore& presence) {
    if (auto username = commons::civic::resolve_username(headers_raw)) {
        if (guests.verify(claimed_id, secret)) presence.forget(claimed_id);
        return "u:" + *username;
    }
    if (guests.verify(claimed_id, secret)) return claimed_id;
    return std::nullopt;
}

// move/leave only. A resolved cookie only takes over once the bound id has
// actually landed in *this* region via a join/rekey -- promotion happens
// there, not here. Until then a guest who just logged in but hasn't
// changed rooms yet keeps moving under their existing, still-live guest
// row (verified the normal way) instead of every move 403ing because
// u:<name> isn't in this region yet (GROK_COOKIE_RESOLVER_REVIEW.md
// must-fix: "do not ok:false a live walker").
std::optional<std::string> effective_id_in_region(const std::string& headers_raw, const std::string& region,
                                                   const std::string& claimed_id, const std::string& secret,
                                                   commons::GuestRegistry& guests, commons::PresenceStore& presence) {
    if (auto username = commons::civic::resolve_username(headers_raw)) {
        std::string bound_id = "u:" + *username;
        if (presence.contains(region, bound_id)) return bound_id;
    }
    if (guests.verify(claimed_id, secret)) return claimed_id;
    return std::nullopt;
}

// GROK_V5_ANSWER.md: "persist the bound house ... for instance:house:
// <username> only." nullopt for outdoor keys, "instance:demo",
// "instance:lab", or anything malformed -- those all stay exactly as
// they were, RAM only.
std::optional<std::string> house_username_from_region(const std::string& region) {
    static const std::string prefix = "instance:house:";
    if (region.rfind(prefix, 0) != 0) return std::nullopt;
    std::string rest = region.substr(prefix.size());
    if (rest.empty()) return std::nullopt;
    return rest;
}

// Visiting, "friends" policy (Brandon: "BUILD visiting friends"): a bound
// player may be in another player's house iff that owner is one of their
// mutual, non-blocked friends -- civic's answer, via the same 15s cache
// the lot doors use, never a client-supplied claim. Entering additionally
// requires the owner to be in the game right now (the same "friend is
// online" condition that lights the door in the first place); staying does
// not, so a visitor isn't thrown out just because the owner logged off.
// The owner's own `players.visit_policy` gates all of it (friends|locked).
// Read-only: a visit never creates a players/map_instances row and never
// snapshots -- only the owner's own entry does either.
bool visit_allowed(const std::string& headers_raw, const std::string& owner,
                   commons::FriendsCache& friends, commons::PresenceStore& presence,
                   commons::HouseStore& house, bool require_owner_online) {
    // House policy first (BUILD house friends/locked A): a locked house lets
    // in nobody but its owner, friends included -- and the move re-check
    // below is what walks out anyone already inside when it gets locked.
    if (house.visit_policy_of(owner) == "locked") return false;
    auto mine = friends.get(headers_raw);
    if (std::find(mine.begin(), mine.end(), owner) == mine.end()) return false;
    if (!require_owner_online) return true;
    auto online = presence.bound_usernames();
    return std::find(online.begin(), online.end(), owner) != online.end();
}

// A position snapshot is best effort. sqlx has already logged the real sqlite
// error; a failed write must not fail a leave/rekey (which would strand the
// player in a room they are trying to walk out of) or stop the periodic pass
// for everyone else, so it is swallowed here and nowhere else.
void snapshot_quietly(commons::HouseStore& house, const std::string& user, int x, int y, int facing) {
    try {
        house.snapshot_leave(user, x, y, facing);
    } catch (const commons::sqlx::DbError&) {
    }
}

std::string occupants_json(const std::vector<commons::Occupant>& occ) {
    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < occ.size(); ++i) {
        if (i) out << ",";
        const auto& o = occ[i];
        out << "{\"id\":" << commons::json::str(o.id)
            << ",\"display_name\":" << commons::json::str(o.display_name)
            << ",\"x\":" << o.x << ",\"y\":" << o.y
            << ",\"facing\":" << o.facing
            << ",\"last_update_ms\":" << o.last_update_ms << "}";
    }
    out << "]";
    return out.str();
}

} // namespace

int main(int argc, char** argv) {
    // Defaults match this round's "local only" scope: a data/ dir beside
    // the binary, loopback bind, a dev port. Production values (a real
    // path under /var/lib/commons, a real port nginx proxies to) are a
    // deploy-time decision, not hardcoded here.
    std::string db_path = argc > 1 ? argv[1] : "data/commons.sqlite";
    std::string bind_addr = "127.0.0.1";
    int port = 8765;

    commons::Database db(db_path);
    db.migrate("schema.sql", "seed.sql");
    db.ensure_inventory_recency();
    db.ensure_visit_policy();
    std::cerr << "commons-daemon: DB ready at " << db_path << "\n";

    commons::PresenceStore presence;
    commons::GuestRegistry guests;
    commons::PlacedStore placed;
    commons::HouseStore house(db.handle());
    commons::FriendsCache friends_cache;
    commons::HttpServer server(bind_addr, port);

    server.route("GET", "/health", [](const commons::HttpRequest&) {
        commons::HttpResponse r;
        r.body = "{\"status\":\"ok\"}";
        return r;
    });

    server.route("GET", "/items", [&](const commons::HttpRequest&) {
        commons::HttpResponse r;
        r.body = items_json(db.handle());
        return r;
    });

    server.route("GET", "/recipes", [&](const commons::HttpRequest&) {
        commons::HttpResponse r;
        r.body = recipes_json(db.handle());
        return r;
    });

    server.route("GET", "/maps", [&](const commons::HttpRequest&) {
        commons::HttpResponse r;
        r.body = maps_json(db.handle());
        return r;
    });

    // Must-fix (should-fix #3): mints a fresh, server-issued (id, secret)
    // pair. `id` is safe to broadcast (it's what every /presence/<region>
    // GET returns for rendering); `secret` is returned only here and must
    // accompany every join/move/rekey/leave call below. Closes the gap
    // where a client could otherwise pick any id -- including one it
    // observed belonging to someone else -- and hijack that occupant.
    server.route("POST", "/presence/guest", [&](const commons::HttpRequest&) {
        commons::HttpResponse r;
        auto cred = guests.mint();
        r.body = "{\"id\":" + commons::json::str(cred.id) +
                 ",\"secret\":" + commons::json::str(cred.secret) + "}";
        return r;
    });

    // Bound inventory (GROK_V6_ANSWER.md: "bound lab pickups still vanish").
    // Identity is the forwarded session cookie and nothing else: no id, no
    // secret, no username parameter, and no guest fallback -- a guest's
    // pickups stay RAM-only in the client, by design ("Do not invent guest
    // persistence"). Both live under /presence/ so the existing nginx
    // location (which already forwards Cookie) covers them: no nginx change.
    // GET must be registered before the /presence/<region> prefix route
    // (first-match dispatch would read "inventory" as a region).
    server.route("GET", "/presence/inventory", [&](const commons::HttpRequest& req) {
        commons::HttpResponse r;
        std::string body = "{\"items\":[";
        if (auto username = commons::civic::resolve_username(req.headers_raw)) {
            bool first = true;
            for (const auto& e : house.inventory_of(*username)) {
                if (!first) body += ",";
                first = false;
                body += "{\"item\":" + commons::json::str(e.slug) + ",\"qty\":" + std::to_string(e.quantity) + "}";
            }
        }
        r.body = body + "]}";
        return r;
    });

    server.route("POST", "/presence/pickup", [&](const commons::HttpRequest& req) {
        using namespace commons::reqjson;
        commons::HttpResponse r;
        auto item = field_str(req.body, "item");
        if (!item) {
            r.status = 400; r.status_text = "Bad Request";
            r.body = "{\"ok\":false,\"reason\":\"missing_field\"}";
            return r;
        }
        auto username = commons::civic::resolve_username(req.headers_raw);
        if (!username) {
            r.status = 403; r.status_text = "Forbidden";
            r.body = "{\"ok\":false,\"reason\":\"not_bound\"}";
            return r;
        }
        if (!house.add_pickup(*username, *item, now_ms())) {
            r.status = 400; r.status_text = "Bad Request";
            r.body = "{\"ok\":false,\"reason\":\"unknown_item\"}";
            return r;
        }
        r.body = "{\"ok\":true}";
        return r;
    });

    // House policy (BUILD house friends/locked A, GROK_HOUSE_POLICY_ANSWER.md):
    // the owner locks or unlocks their own house. Identity is the forwarded
    // session cookie and nothing else -- there is no username in the body to
    // read, so no one can set another player's door, and guests (no cookie)
    // get 403. Only "friends" and "locked" exist; "open" is refused. Lives
    // under /presence/ so the existing nginx location covers it (no nginx
    // change). Locking takes effect on a visitor's next move (not_welcome,
    // the same path as an unfriend) and drops the door out of /presence/lots.
    server.route("POST", "/presence/house-policy", [&](const commons::HttpRequest& req) {
        using namespace commons::reqjson;
        commons::HttpResponse r;
        auto username = commons::civic::resolve_username(req.headers_raw);
        if (!username) {
            r.status = 403; r.status_text = "Forbidden";
            r.body = "{\"ok\":false,\"reason\":\"not_bound\"}";
            return r;
        }
        auto policy = field_str(req.body, "policy");
        if (!policy || (*policy != "friends" && *policy != "locked")) {
            r.status = 400; r.status_text = "Bad Request";
            r.body = "{\"ok\":false,\"reason\":\"bad_policy\"}";
            return r;
        }
        if (!house.set_visit_policy(*username, *policy)) {
            r.status = 409; r.status_text = "Conflict";
            r.body = "{\"ok\":false,\"reason\":\"no_house\"}";
            return r;
        }
        r.body = "{\"ok\":true,\"policy\":" + commons::json::str(*policy) + "}";
        return r;
    });

    // Friends query (GROK_V6_ANSWER.md: "mutual accept, RAM TTL, Homestead
    // lots 2-7"). Which of *your* mutual friends are in the game right now,
    // in a stable order, for the six dynamic Homestead doors. Identity is
    // only ever the forwarded session cookie -- there is no username
    // parameter to ask about anyone else (glue.md: that would be a roster
    // scrape). Guests, and any request civic doesn't resolve, get an empty
    // list. Deliberately under /presence/ so the existing nginx location
    // (which already forwards Cookie) covers it with no config change.
    // Must be registered before the /presence/<region> prefix route below:
    // dispatch is first-match, and "lots" would otherwise read as a region.
    server.route("GET", "/presence/lots", [&](const commons::HttpRequest& req) {
        commons::HttpResponse r;
        auto friends = friends_cache.get(req.headers_raw);
        auto online = presence.bound_usernames();
        std::vector<std::string> lots;
        for (const auto& f : friends) {
            // A locked house drops out of the lots, so its door goes dark.
            if (house.visit_policy_of(f) == "locked") continue;
            if (std::find(online.begin(), online.end(), f) != online.end() &&
                std::find(lots.begin(), lots.end(), f) == lots.end())
                lots.push_back(f);
        }
        // Stable, case-insensitive order so a door doesn't swap who it
        // shows between polls; six doors, lots 2-7.
        std::sort(lots.begin(), lots.end(), [](const std::string& a, const std::string& b) {
            std::string la = a, lb = b;
            for (auto& c : la) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            for (auto& c : lb) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return la != lb ? la < lb : a < b;
        });
        if (lots.size() > 6) lots.resize(6);
        r.body = "{\"lots\":[";
        for (std::size_t i = 0; i < lots.size(); ++i) {
            if (i) r.body += ",";
            r.body += commons::json::str(lots[i]);
        }
        r.body += "]}";
        return r;
    });

    // GET /presence/<region> -- region is whatever's after the prefix,
    // e.g. "1,1" for outdoor col=1,row=1, or "instance:3" for an interior.
    server.route("GET", "/presence/", [&](const commons::HttpRequest& req) {
        std::string region = req.path.substr(std::string("/presence/").size());
        commons::HttpResponse r;
        r.body = occupants_json(presence.occupants(region));
        return r;
    }, /*prefix=*/true);

    server.route("POST", "/presence/join", [&](const commons::HttpRequest& req) {
        using namespace commons::reqjson;
        commons::HttpResponse r;
        auto id = field_str(req.body, "id");
        auto secret = field_str(req.body, "secret");
        auto region = field_str(req.body, "region");
        auto x = field_num(req.body, "x");
        auto y = field_num(req.body, "y");
        if (!id || !secret || !region || !x || !y) {
            r.status = 400; r.status_text = "Bad Request";
            r.body = "{\"ok\":false,\"reason\":\"missing_field\"}";
            return r;
        }
        auto eff_id = bind_or_promote(req.headers_raw, *id, *secret, guests, presence);
        if (!eff_id) {
            r.status = 403; r.status_text = "Forbidden";
            r.body = "{\"ok\":false,\"reason\":\"bad_credential\"}";
            return r;
        }
        const bool is_bound = eff_id->rfind("u:", 0) == 0;

        // GROK_V5_ANSWER.md: a house instance belongs to its bound owner.
        // This is a mutation route (it creates presence, and on the
        // owner's first-ever entry a durable players/map_instances row), so
        // anyone who is neither the owner nor an allowed visitor (below) is
        // refused outright -- unlike the read-only GET, which stays public
        // and unscoped, matching /placed/'s own existing "low stakes" stance.
        double resume_x = *x, resume_y = *y;
        int resume_facing = static_cast<int>(field_num(req.body, "facing").value_or(0));
        bool has_resume = false;
        std::string house_policy; // set only when the owner enters their own house
        if (auto house_user = house_username_from_region(*region)) {
            const bool is_owner = is_bound && *house_user == eff_id->substr(2);
            // Visiting (friends policy): anyone else who is bound, a mutual
            // friend of the owner, with the owner in the game. Guests never
            // qualify -- they have no friends. Refused with the same 403 as
            // before; the client tells a refused visit apart by its reason.
            if (!is_owner && !(is_bound && visit_allowed(req.headers_raw, *house_user, friends_cache, presence, house, true))) {
                r.status = 403; r.status_text = "Forbidden";
                r.body = "{\"ok\":false,\"reason\":\"not_your_house\"}";
                return r;
            }
            if (is_owner) {
                house.house_instance_for(*house_user);
                house_policy = house.visit_policy_of(*house_user);
                // presence.md: "walking back in later resumes there, not the
                // fixed entry tile every time" -- only the position, never the
                // client-submitted facing/display_name, which stay as-is.
                if (auto pos = house.last_house_position(*house_user)) {
                    resume_x = pos->x; resume_y = pos->y; resume_facing = pos->facing;
                    has_resume = true;
                }
            }
        }

        commons::Occupant occ;
        occ.id = *eff_id;
        // A bound occupant's display_name is the real username, not
        // whatever the client happened to submit (cookie-bind.md).
        occ.display_name = is_bound ? eff_id->substr(2) : field_str(req.body, "display_name").value_or(*eff_id);
        occ.x = resume_x; occ.y = resume_y;
        occ.facing = resume_facing;
        occ.last_update_ms = now_ms();
        std::string accepted_name;
        auto result = presence.join(*region, occ, &accepted_name);
        // GROK_NAME_REVIEW.md: echo the *accepted* name (post-disambiguation)
        // so a self-nameplate draws what other clients actually see, not
        // the raw typed input. GROK_V4_ANSWER.md: also echo whether this
        // occupant is bound. GROK_HOUSE_REVIEW.md must-fix: display_name is
        // the *disambiguated* label for this region -- a guest already
        // occupying it as "brandon" makes a bound player's display_name
        // "brandon (2)", which is not a valid house key. "username" is the
        // raw, unsuffixed lg_username, safe to build instance:house:<...>
        // from; the client must never use display_name for that.
        // GROK_V5_ANSWER.md: "resume_*" tells the client where to actually
        // draw itself, since the client's own entry-tile guess (sent as
        // this request's x/y) is not what got stored once a snapshot wins.
        r.body = std::string("{\"ok\":") + (result == commons::PresenceStore::JoinResult::Ok ? "true" : "false")
            + ",\"reason\":\"" + (result == commons::PresenceStore::JoinResult::Ok ? "" :
                result == commons::PresenceStore::JoinResult::RegionFull ? "region_full" : "global_full") + "\""
            + (result == commons::PresenceStore::JoinResult::Ok
                ? ",\"display_name\":" + commons::json::str(accepted_name) +
                  ",\"bound\":" + (is_bound ? "true" : "false") +
                  (is_bound ? ",\"username\":" + commons::json::str(eff_id->substr(2)) : "") +
                  (!house_policy.empty() ? ",\"house_policy\":" + commons::json::str(house_policy) : "") +
                  (has_resume ? ",\"resume_x\":" + std::to_string(static_cast<int>(resume_x)) +
                                 ",\"resume_y\":" + std::to_string(static_cast<int>(resume_y)) +
                                 ",\"resume_facing\":" + std::to_string(resume_facing) : "")
                : "")
            + "}";
        return r;
    });

    server.route("POST", "/presence/move", [&](const commons::HttpRequest& req) {
        using namespace commons::reqjson;
        commons::HttpResponse r;
        auto id = field_str(req.body, "id");
        auto secret = field_str(req.body, "secret");
        auto region = field_str(req.body, "region");
        auto x = field_num(req.body, "x");
        auto y = field_num(req.body, "y");
        if (!id || !secret || !region || !x || !y) {
            r.status = 400; r.status_text = "Bad Request";
            r.body = "{\"ok\":false,\"reason\":\"missing_field\"}";
            return r;
        }
        auto eff_id = effective_id_in_region(req.headers_raw, *region, *id, *secret, guests, presence);
        if (!eff_id) {
            r.status = 403; r.status_text = "Forbidden";
            r.body = "{\"ok\":false,\"reason\":\"bad_credential\"}";
            return r;
        }
        // Visiting stays revocable: a visitor in someone else's house is
        // re-checked against the (15s-cached) friends answer on every move,
        // so an unfriend or a block ends the visit within one TTL instead of
        // whenever the visitor happens to leave. Owner-online is deliberately
        // not re-required here -- only friendship. Answered 200 with a
        // reason, not 403: the client reads 403 as "my credential died" and
        // would reauth instead of walking the visitor out.
        if (auto house_user = house_username_from_region(*region)) {
            const bool is_owner = eff_id->rfind("u:", 0) == 0 && *house_user == eff_id->substr(2);
            if (!is_owner && !visit_allowed(req.headers_raw, *house_user, friends_cache, presence, house, false)) {
                presence.leave(*region, *eff_id);
                r.body = "{\"ok\":false,\"reason\":\"not_welcome\"}";
                return r;
            }
        }
        int facing = static_cast<int>(field_num(req.body, "facing").value_or(0));
        bool ok = presence.move(*region, *eff_id, *x, *y, facing);
        r.body = std::string("{\"ok\":") + (ok ? "true" : "false") + "}";
        return r;
    });

    server.route("POST", "/presence/rekey", [&](const commons::HttpRequest& req) {
        using namespace commons::reqjson;
        commons::HttpResponse r;
        auto id = field_str(req.body, "id");
        auto secret = field_str(req.body, "secret");
        auto from_region = field_str(req.body, "from_region");
        auto to_region = field_str(req.body, "to_region");
        auto x = field_num(req.body, "x");
        auto y = field_num(req.body, "y");
        if (!id || !secret || !from_region || !to_region || !x || !y) {
            r.status = 400; r.status_text = "Bad Request";
            r.body = "{\"ok\":false,\"reason\":\"missing_field\"}";
            return r;
        }
        auto eff_id = bind_or_promote(req.headers_raw, *id, *secret, guests, presence);
        if (!eff_id) {
            r.status = 403; r.status_text = "Forbidden";
            r.body = "{\"ok\":false,\"reason\":\"bad_credential\"}";
            return r;
        }
        const bool is_bound = eff_id->rfind("u:", 0) == 0;

        // Same house-ownership + resume-position handling as join, above.
        double resume_x = *x, resume_y = *y;
        int resume_facing = static_cast<int>(field_num(req.body, "facing").value_or(0));
        bool has_resume = false;
        std::string house_policy; // set only when the owner enters their own house
        if (auto house_user = house_username_from_region(*to_region)) {
            const bool is_owner = is_bound && *house_user == eff_id->substr(2);
            if (!is_owner && !(is_bound && visit_allowed(req.headers_raw, *house_user, friends_cache, presence, house, true))) {
                r.status = 403; r.status_text = "Forbidden";
                r.body = "{\"ok\":false,\"reason\":\"not_your_house\"}";
                return r;
            }
            if (is_owner) {
                house.house_instance_for(*house_user);
                house_policy = house.visit_policy_of(*house_user);
                if (auto pos = house.last_house_position(*house_user)) {
                    resume_x = pos->x; resume_y = pos->y; resume_facing = pos->facing;
                    has_resume = true;
                }
            }
        }

        // GROK_PERSIST_REVIEW.md must-fix: walking out the door is a
        // rekey, not a leave -- exitToOutdoor() never calls netLeave(),
        // only the next frame's rekey-on-region-mismatch fires (V2's own
        // "no explicit network call needed" design). Snapshot-on-leave
        // alone would only ever fire on tab close, never on ordinary
        // walk-out, so a real session (enter, place, walk out, restart
        // later) would still resume at the fixed entry tile. Captured the
        // same way leave does: before the row this position lives in is
        // erased by the rekey below.
        if (auto leaving_house_user = house_username_from_region(*from_region)) {
            if (is_bound && *leaving_house_user == eff_id->substr(2)) {
                for (const auto& o : presence.occupants(*from_region)) {
                    if (o.id == *eff_id) {
                        snapshot_quietly(house, *leaving_house_user, static_cast<int>(o.x), static_cast<int>(o.y), o.facing);
                        break;
                    }
                }
            }
        }

        commons::Occupant occ;
        occ.id = *eff_id;
        occ.display_name = is_bound ? eff_id->substr(2) : field_str(req.body, "display_name").value_or(*eff_id);
        occ.x = resume_x; occ.y = resume_y;
        occ.facing = resume_facing;
        occ.last_update_ms = now_ms();
        std::string accepted_name;
        auto result = presence.rekey(*from_region, *to_region, occ, &accepted_name);
        r.body = std::string("{\"ok\":") + (result == commons::PresenceStore::JoinResult::Ok ? "true" : "false")
            + ",\"reason\":\"" + (result == commons::PresenceStore::JoinResult::Ok ? "" :
                result == commons::PresenceStore::JoinResult::RegionFull ? "region_full" : "global_full") + "\""
            + (result == commons::PresenceStore::JoinResult::Ok
                ? ",\"display_name\":" + commons::json::str(accepted_name) +
                  ",\"bound\":" + (is_bound ? "true" : "false") +
                  (is_bound ? ",\"username\":" + commons::json::str(eff_id->substr(2)) : "") +
                  (!house_policy.empty() ? ",\"house_policy\":" + commons::json::str(house_policy) : "") +
                  (has_resume ? ",\"resume_x\":" + std::to_string(static_cast<int>(resume_x)) +
                                 ",\"resume_y\":" + std::to_string(static_cast<int>(resume_y)) +
                                 ",\"resume_facing\":" + std::to_string(resume_facing) : "")
                : "")
            + "}";
        return r;
    });

    server.route("POST", "/presence/leave", [&](const commons::HttpRequest& req) {
        using namespace commons::reqjson;
        commons::HttpResponse r;
        auto id = field_str(req.body, "id");
        auto secret = field_str(req.body, "secret");
        auto region = field_str(req.body, "region");
        if (!id || !secret || !region) {
            r.status = 400; r.status_text = "Bad Request";
            r.body = "{\"ok\":false,\"reason\":\"missing_field\"}";
            return r;
        }
        auto eff_id = effective_id_in_region(req.headers_raw, *region, *id, *secret, guests, presence);
        if (!eff_id) {
            r.status = 403; r.status_text = "Forbidden";
            r.body = "{\"ok\":false,\"reason\":\"bad_credential\"}";
            return r;
        }
        // GROK_V5_ANSWER.md: "snapshot-on-leave ... last position onto that
        // players row." Only when actually leaving from a bound player's
        // own house -- captured before presence.leave() removes the row
        // this occupant's current position lives in.
        if (auto house_user = house_username_from_region(*region)) {
            if (eff_id->rfind("u:", 0) == 0 && *house_user == eff_id->substr(2)) {
                for (const auto& o : presence.occupants(*region)) {
                    if (o.id == *eff_id) {
                        snapshot_quietly(house, *house_user, static_cast<int>(o.x), static_cast<int>(o.y), o.facing);
                        break;
                    }
                }
            }
        }
        presence.leave(*region, *eff_id);
        r.body = "{\"ok\":true}";
        return r;
    });

    // V2 lab placeables (GROK_V2_ANSWER.md): one placed item slug per
    // shared interior instance, RAM only, "not a civic PNG." GET is
    // public same as /presence/<region> (an item slug is not sensitive);
    // POST still requires a valid guest credential so an arbitrary
    // stranger can't grief the one shared slot, but does not check that
    // the caller is actually *in* that instance right now -- a light
    // integrity bar consistent with how low the stakes are here (a
    // display string, not a security boundary).
    server.route("GET", "/placed/", [&](const commons::HttpRequest& req) {
        std::string instance = req.path.substr(std::string("/placed/").size());
        commons::HttpResponse r;
        // GROK_V5_ANSWER.md: a house's placed item is durable (schema.sql's
        // `placed` table), not the RAM PlacedStore demo/lab use. Read-only
        // and unscoped, same public stance as every other GET here --
        // existing_house_instance_for() never creates a house just because
        // someone looked.
        if (auto house_user = house_username_from_region(instance)) {
            auto instance_id = house.existing_house_instance_for(*house_user);
            std::string item = instance_id ? house.get_wall_slot(*instance_id) : std::string();
            r.body = "{\"item\":" + commons::json::str(item) + "}";
            return r;
        }
        r.body = "{\"item\":" + commons::json::str(placed.get(instance)) + "}";
        return r;
    }, /*prefix=*/true);

    server.route("POST", "/placed/", [&](const commons::HttpRequest& req) {
        using namespace commons::reqjson;
        std::string instance = req.path.substr(std::string("/placed/").size());
        commons::HttpResponse r;
        auto id = field_str(req.body, "id");
        auto secret = field_str(req.body, "secret");
        auto item = field_str(req.body, "item");
        if (!id || !secret || !item) {
            r.status = 400; r.status_text = "Bad Request";
            r.body = "{\"ok\":false,\"reason\":\"missing_field\"}";
            return r;
        }
        auto eff_id = effective_id(req.headers_raw, *id, *secret, guests);
        if (!eff_id) {
            r.status = 403; r.status_text = "Forbidden";
            r.body = "{\"ok\":false,\"reason\":\"bad_credential\"}";
            return r;
        }
        // Empty string clears the slot; anything else must be a real,
        // known catalog item -- never an arbitrary client string nothing
        // can render.
        if (!item->empty() && !item_exists(db.handle(), *item)) {
            r.status = 400; r.status_text = "Bad Request";
            r.body = "{\"ok\":false,\"reason\":\"unknown_item\"}";
            return r;
        }
        // GROK_V5_ANSWER.md: placing into a house mutates someone's
        // durable, private room -- this needs the same ownership check
        // join/rekey already enforce, not the shared demo/lab slot's
        // "any valid credential, no instance check" stance.
        if (auto house_user = house_username_from_region(instance)) {
            if (eff_id->rfind("u:", 0) != 0 || eff_id->substr(2) != *house_user) {
                r.status = 403; r.status_text = "Forbidden";
                r.body = "{\"ok\":false,\"reason\":\"not_your_house\"}";
                return r;
            }
            int instance_id = house.house_instance_for(*house_user);
            house.set_wall_slot(instance_id, *item);
            r.body = "{\"ok\":true}";
            return r;
        }
        placed.set(instance, *item);
        r.body = "{\"ok\":true}";
        return r;
    }, /*prefix=*/true);

    // presence.md's "periodic safety snapshot, ~every 30s, of bound players
    // only, for crash recovery". The leave and rekey-away snapshots cover
    // ordinary play; this is the half that survives a crash (or a kill)
    // while someone is standing in their own house -- without it they would
    // resume from wherever they last walked *out*. Same write, same guard as
    // leave (owner's own house only, bound only, nothing for guests, visitors
    // or the shared rooms). Runs on the accept loop between requests, so it
    // needs no thread and no extra locking; an exception here is caught by
    // HttpServer and cannot take the daemon down.
    constexpr long long kSnapshotIntervalMs = 30000;
    server.set_periodic(kSnapshotIntervalMs, [&]() {
        for (const auto& user : presence.bound_usernames()) {
            const std::string region = "instance:house:" + user;
            const std::string bound_id = "u:" + user;
            for (const auto& o : presence.occupants(region)) {
                if (o.id == bound_id) {
                    snapshot_quietly(house, user, static_cast<int>(o.x), static_cast<int>(o.y), o.facing);
                    break;
                }
            }
        }
    });

    server.run();
    return 0;
}
