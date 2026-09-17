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

#include "db.hpp"
#include "http.hpp"
#include "json.hpp"
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
        auto region = field_str(req.body, "region");
        auto x = field_num(req.body, "x");
        auto y = field_num(req.body, "y");
        if (!id || !region || !x || !y) {
            r.status = 400; r.status_text = "Bad Request";
            r.body = "{\"ok\":false,\"reason\":\"missing_field\"}";
            return r;
        }
        commons::Occupant occ;
        occ.id = *id;
        occ.display_name = field_str(req.body, "display_name").value_or(*id);
        occ.x = *x; occ.y = *y;
        occ.facing = static_cast<int>(field_num(req.body, "facing").value_or(0));
        occ.last_update_ms = now_ms();
        auto result = presence.join(*region, occ);
        r.body = std::string("{\"ok\":") + (result == commons::PresenceStore::JoinResult::Ok ? "true" : "false")
            + ",\"reason\":\"" + (result == commons::PresenceStore::JoinResult::Ok ? "" :
                result == commons::PresenceStore::JoinResult::RegionFull ? "region_full" : "global_full") + "\"}";
        return r;
    });

    server.route("POST", "/presence/move", [&](const commons::HttpRequest& req) {
        using namespace commons::reqjson;
        commons::HttpResponse r;
        auto id = field_str(req.body, "id");
        auto region = field_str(req.body, "region");
        auto x = field_num(req.body, "x");
        auto y = field_num(req.body, "y");
        if (!id || !region || !x || !y) {
            r.status = 400; r.status_text = "Bad Request";
            r.body = "{\"ok\":false,\"reason\":\"missing_field\"}";
            return r;
        }
        int facing = static_cast<int>(field_num(req.body, "facing").value_or(0));
        bool ok = presence.move(*region, *id, *x, *y, facing);
        r.body = std::string("{\"ok\":") + (ok ? "true" : "false") + "}";
        return r;
    });

    server.route("POST", "/presence/rekey", [&](const commons::HttpRequest& req) {
        using namespace commons::reqjson;
        commons::HttpResponse r;
        auto id = field_str(req.body, "id");
        auto from_region = field_str(req.body, "from_region");
        auto to_region = field_str(req.body, "to_region");
        auto x = field_num(req.body, "x");
        auto y = field_num(req.body, "y");
        if (!id || !from_region || !to_region || !x || !y) {
            r.status = 400; r.status_text = "Bad Request";
            r.body = "{\"ok\":false,\"reason\":\"missing_field\"}";
            return r;
        }
        commons::Occupant occ;
        occ.id = *id;
        occ.display_name = field_str(req.body, "display_name").value_or(*id);
        occ.x = *x; occ.y = *y;
        occ.facing = static_cast<int>(field_num(req.body, "facing").value_or(0));
        occ.last_update_ms = now_ms();
        auto result = presence.rekey(*from_region, *to_region, occ);
        r.body = std::string("{\"ok\":") + (result == commons::PresenceStore::JoinResult::Ok ? "true" : "false")
            + ",\"reason\":\"" + (result == commons::PresenceStore::JoinResult::Ok ? "" :
                result == commons::PresenceStore::JoinResult::RegionFull ? "region_full" : "global_full") + "\"}";
        return r;
    });

    server.route("POST", "/presence/leave", [&](const commons::HttpRequest& req) {
        using namespace commons::reqjson;
        commons::HttpResponse r;
        auto id = field_str(req.body, "id");
        auto region = field_str(req.body, "region");
        if (!id || !region) {
            r.status = 400; r.status_text = "Bad Request";
            r.body = "{\"ok\":false,\"reason\":\"missing_field\"}";
            return r;
        }
        presence.leave(*region, *id);
        r.body = "{\"ok\":true}";
        return r;
    });

    server.run();
    return 0;
}
