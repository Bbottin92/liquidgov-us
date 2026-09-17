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
// Friends/cookie forwarding (glue.md, cookie.md, GROK_GLUE_REVIEW.md) is
// intentionally NOT implemented yet: there is no civic route to call
// (adding one would mean touching http_server.cpp, out of scope), and
// there is no droplet to receive a forwarded cookie against. The shape
// is documented in glue.md; wiring it is later, explicit work.

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "civic.hpp"
#include "db.hpp"
#include "guest.hpp"
#include "http.hpp"
#include "json.hpp"
#include "placed.hpp"
#include "presence.hpp"
#include "reqjson.hpp"

namespace {

long long now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string items_json(sqlite3* db) {
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db,
        "SELECT slug, display_name, category, wall_slot_ok, sprite_ref FROM items ORDER BY item_id;",
        -1, &st, nullptr);
    std::ostringstream out;
    out << "[";
    bool first = true;
    while (sqlite3_step(st) == SQLITE_ROW) {
        if (!first) out << ",";
        first = false;
        auto text = [&](int i) { return reinterpret_cast<const char*>(sqlite3_column_text(st, i)); };
        out << "{\"slug\":" << commons::json::str(text(0))
            << ",\"display_name\":" << commons::json::str(text(1))
            << ",\"category\":" << commons::json::str(text(2))
            << ",\"wall_slot_ok\":" << sqlite3_column_int(st, 3)
            << ",\"sprite_ref\":" << commons::json::str(text(4)) << "}";
    }
    out << "]";
    sqlite3_finalize(st);
    return out.str();
}

std::string recipes_json(sqlite3* db) {
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db,
        "SELECT r.slug, r.display_name, r.station, i.slug, ro.quantity "
        "FROM recipes r "
        "LEFT JOIN recipe_outputs ro ON ro.recipe_id = r.recipe_id "
        "LEFT JOIN items i ON i.item_id = ro.item_id "
        "ORDER BY r.recipe_id;",
        -1, &st, nullptr);
    // Small enough (three V1 recipes, one output each) that grouping by
    // hand here beats a second query or a real JSON tree builder.
    std::vector<std::string> slugs, names, stations, out_slugs;
    std::vector<int> out_qty;
    while (sqlite3_step(st) == SQLITE_ROW) {
        auto text = [&](int i) { return reinterpret_cast<const char*>(sqlite3_column_text(st, i)); };
        slugs.push_back(text(0));
        names.push_back(text(1));
        stations.push_back(text(2));
        out_slugs.push_back(sqlite3_column_type(st, 3) == SQLITE_NULL ? "" : text(3));
        out_qty.push_back(sqlite3_column_int(st, 4));
    }
    sqlite3_finalize(st);
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
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db,
        "SELECT slug, display_name, kind, width_tiles, height_tiles, grid_col, grid_row FROM maps ORDER BY map_id;",
        -1, &st, nullptr);
    std::ostringstream out;
    out << "[";
    bool first = true;
    while (sqlite3_step(st) == SQLITE_ROW) {
        if (!first) out << ",";
        first = false;
        auto text = [&](int i) { return reinterpret_cast<const char*>(sqlite3_column_text(st, i)); };
        out << "{\"slug\":" << commons::json::str(text(0))
            << ",\"display_name\":" << commons::json::str(text(1))
            << ",\"kind\":" << commons::json::str(text(2))
            << ",\"width_tiles\":" << sqlite3_column_int(st, 3)
            << ",\"height_tiles\":" << sqlite3_column_int(st, 4)
            << ",\"grid_col\":" << (sqlite3_column_type(st, 5) == SQLITE_NULL ? "null" : std::to_string(sqlite3_column_int(st, 5)))
            << ",\"grid_row\":" << (sqlite3_column_type(st, 6) == SQLITE_NULL ? "null" : std::to_string(sqlite3_column_int(st, 6)))
            << "}";
    }
    out << "]";
    sqlite3_finalize(st);
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
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db, "SELECT 1 FROM items WHERE slug = ? LIMIT 1;", -1, &st, nullptr);
    sqlite3_bind_text(st, 1, slug.c_str(), -1, SQLITE_TRANSIENT);
    bool found = sqlite3_step(st) == SQLITE_ROW;
    sqlite3_finalize(st);
    return found;
}

// Cookie-bind.md: "the two auth paths never mix." A resolved cookie always
// wins and derives the id itself ("u:" + lg_username) -- the client's own
// id/secret fields are not even consulted in that case, bound or not. Only
// when nothing resolves does this fall back to the existing GuestRegistry
// check, exactly as every route here did before cookie bind existed.
// nullopt means the request is unauthorized outright (bad guest credential,
// no cookie).
std::optional<std::string> effective_id(const std::string& headers_raw, const std::string& claimed_id,
                                         const std::string& secret, commons::GuestRegistry& guests) {
    if (auto username = commons::civic::resolve_username(headers_raw)) return "u:" + *username;
    if (guests.verify(claimed_id, secret)) return claimed_id;
    return std::nullopt;
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
    std::cerr << "commons-daemon: DB ready at " << db_path << "\n";

    commons::PresenceStore presence;
    commons::GuestRegistry guests;
    commons::PlacedStore placed;
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
        auto eff_id = effective_id(req.headers_raw, *id, *secret, guests);
        if (!eff_id) {
            r.status = 403; r.status_text = "Forbidden";
            r.body = "{\"ok\":false,\"reason\":\"bad_credential\"}";
            return r;
        }
        commons::Occupant occ;
        occ.id = *eff_id;
        // A bound occupant's display_name is the real username, not
        // whatever the client happened to submit (cookie-bind.md).
        occ.display_name = (eff_id->rfind("u:", 0) == 0)
            ? eff_id->substr(2)
            : field_str(req.body, "display_name").value_or(*eff_id);
        occ.x = *x; occ.y = *y;
        occ.facing = static_cast<int>(field_num(req.body, "facing").value_or(0));
        occ.last_update_ms = now_ms();
        std::string accepted_name;
        auto result = presence.join(*region, occ, &accepted_name);
        // GROK_NAME_REVIEW.md: echo the *accepted* name (post-disambiguation)
        // so a self-nameplate draws what other clients actually see, not
        // the raw typed input.
        r.body = std::string("{\"ok\":") + (result == commons::PresenceStore::JoinResult::Ok ? "true" : "false")
            + ",\"reason\":\"" + (result == commons::PresenceStore::JoinResult::Ok ? "" :
                result == commons::PresenceStore::JoinResult::RegionFull ? "region_full" : "global_full") + "\""
            + (result == commons::PresenceStore::JoinResult::Ok ? ",\"display_name\":" + commons::json::str(accepted_name) : "")
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
        auto eff_id = effective_id(req.headers_raw, *id, *secret, guests);
        if (!eff_id) {
            r.status = 403; r.status_text = "Forbidden";
            r.body = "{\"ok\":false,\"reason\":\"bad_credential\"}";
            return r;
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
        auto eff_id = effective_id(req.headers_raw, *id, *secret, guests);
        if (!eff_id) {
            r.status = 403; r.status_text = "Forbidden";
            r.body = "{\"ok\":false,\"reason\":\"bad_credential\"}";
            return r;
        }
        commons::Occupant occ;
        occ.id = *eff_id;
        occ.display_name = (eff_id->rfind("u:", 0) == 0)
            ? eff_id->substr(2)
            : field_str(req.body, "display_name").value_or(*eff_id);
        occ.x = *x; occ.y = *y;
        occ.facing = static_cast<int>(field_num(req.body, "facing").value_or(0));
        occ.last_update_ms = now_ms();
        std::string accepted_name;
        auto result = presence.rekey(*from_region, *to_region, occ, &accepted_name);
        r.body = std::string("{\"ok\":") + (result == commons::PresenceStore::JoinResult::Ok ? "true" : "false")
            + ",\"reason\":\"" + (result == commons::PresenceStore::JoinResult::Ok ? "" :
                result == commons::PresenceStore::JoinResult::RegionFull ? "region_full" : "global_full") + "\""
            + (result == commons::PresenceStore::JoinResult::Ok ? ",\"display_name\":" + commons::json::str(accepted_name) : "")
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
        auto eff_id = effective_id(req.headers_raw, *id, *secret, guests);
        if (!eff_id) {
            r.status = 403; r.status_text = "Forbidden";
            r.body = "{\"ok\":false,\"reason\":\"bad_credential\"}";
            return r;
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
        if (!effective_id(req.headers_raw, *id, *secret, guests)) {
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
        placed.set(instance, *item);
        r.body = "{\"ok\":true}";
        return r;
    }, /*prefix=*/true);

    server.run();
    return 0;
}
