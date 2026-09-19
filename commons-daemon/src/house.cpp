#include "house.hpp"

namespace commons {

int HouseStore::find_or_create_player(const std::string& username) {
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_, "SELECT player_id FROM players WHERE lg_username = ?1;", -1, &st, nullptr);
    sqlite3_bind_text(st, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    int player_id = (sqlite3_step(st) == SQLITE_ROW) ? sqlite3_column_int(st, 0) : -1;
    sqlite3_finalize(st);
    if (player_id != -1) return player_id;

    sqlite3_prepare_v2(db_,
        "INSERT INTO players (lg_username, display_name, is_guest, created_at) "
        "VALUES (?1, ?1, 0, strftime('%s','now'));",
        -1, &st, nullptr);
    sqlite3_bind_text(st, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
    return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

int HouseStore::house_instance_for(const std::string& username) {
    int player_id = find_or_create_player(username);

    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_, "SELECT house_instance_id FROM players WHERE player_id = ?1;", -1, &st, nullptr);
    sqlite3_bind_int(st, 1, player_id);
    bool has_row = sqlite3_step(st) == SQLITE_ROW;
    bool has_instance = has_row && sqlite3_column_type(st, 0) != SQLITE_NULL;
    int instance_id = has_instance ? sqlite3_column_int(st, 0) : -1;
    sqlite3_finalize(st);
    if (has_instance) return instance_id;

    sqlite3_prepare_v2(db_, "SELECT map_id FROM maps WHERE slug = 'house';", -1, &st, nullptr);
    bool got_map = sqlite3_step(st) == SQLITE_ROW;
    int map_id = got_map ? sqlite3_column_int(st, 0) : 0;
    sqlite3_finalize(st);

    // GROK_PERSIST_REVIEW.md must-fix: schema.sql already requires any
    // multi-row mutation to be BEGIN/COMMIT (set_wall_slot already does
    // this) -- this INSERT+UPDATE pair was the one place that skipped it.
    // A crash between the two would otherwise leave an orphan
    // map_instances row and no players.house_instance_id pointing at it,
    // so the next entry creates a *second* house instead of reusing the
    // first (wall slot stranded on the orphan, everything new on the
    // second).
    sqlite3_exec(db_, "BEGIN;", nullptr, nullptr, nullptr);

    sqlite3_prepare_v2(db_,
        "INSERT INTO map_instances (map_id, owner_player_id, created_at) "
        "VALUES (?1, ?2, strftime('%s','now'));",
        -1, &st, nullptr);
    sqlite3_bind_int(st, 1, map_id);
    sqlite3_bind_int(st, 2, player_id);
    bool inserted = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    instance_id = static_cast<int>(sqlite3_last_insert_rowid(db_));

    bool updated = false;
    if (got_map && inserted) {
        sqlite3_prepare_v2(db_, "UPDATE players SET house_instance_id = ?1 WHERE player_id = ?2;", -1, &st, nullptr);
        sqlite3_bind_int(st, 1, instance_id);
        sqlite3_bind_int(st, 2, player_id);
        updated = sqlite3_step(st) == SQLITE_DONE;
        sqlite3_finalize(st);
    }

    if (got_map && inserted && updated) {
        sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
    } else {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        instance_id = -1; // schema always seeds 'house' before the daemon accepts requests -- this should not happen
    }

    return instance_id;
}

std::optional<int> HouseStore::existing_house_instance_for(const std::string& username) {
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT house_instance_id FROM players WHERE lg_username = ?1 AND house_instance_id IS NOT NULL;",
        -1, &st, nullptr);
    sqlite3_bind_text(st, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    std::optional<int> instance_id;
    if (sqlite3_step(st) == SQLITE_ROW) instance_id = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    return instance_id;
}

std::optional<HouseStore::Position> HouseStore::last_house_position(const std::string& username) {
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT last_x, last_y, last_facing FROM players "
        "WHERE lg_username = ?1 AND house_instance_id IS NOT NULL "
        "AND last_instance_id = house_instance_id "
        "AND last_x IS NOT NULL AND last_y IS NOT NULL;",
        -1, &st, nullptr);
    sqlite3_bind_text(st, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    std::optional<Position> pos;
    if (sqlite3_step(st) == SQLITE_ROW) {
        Position p;
        p.x = sqlite3_column_int(st, 0);
        p.y = sqlite3_column_int(st, 1);
        p.facing = sqlite3_column_int(st, 2);
        pos = p;
    }
    sqlite3_finalize(st);
    return pos;
}

void HouseStore::snapshot_leave(const std::string& username, int x, int y, int facing) {
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "UPDATE players SET last_instance_id = house_instance_id, "
        "last_x = ?1, last_y = ?2, last_facing = ?3, "
        "last_grid_col = NULL, last_grid_row = NULL "
        "WHERE lg_username = ?4 AND house_instance_id IS NOT NULL;",
        -1, &st, nullptr);
    sqlite3_bind_int(st, 1, x);
    sqlite3_bind_int(st, 2, y);
    sqlite3_bind_int(st, 3, facing);
    sqlite3_bind_text(st, 4, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

std::string HouseStore::visit_policy_of(const std::string& username) {
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_, "SELECT visit_policy FROM players WHERE lg_username = ?1;", -1, &st, nullptr);
    sqlite3_bind_text(st, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    std::string policy = "friends";
    if (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char* v = sqlite3_column_text(st, 0);
        if (v && std::string(reinterpret_cast<const char*>(v)) == "locked") policy = "locked";
    }
    sqlite3_finalize(st);
    return policy;
}

bool HouseStore::set_visit_policy(const std::string& username, const std::string& policy) {
    if (policy != "friends" && policy != "locked") return false;
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_, "UPDATE players SET visit_policy = ?1 WHERE lg_username = ?2;", -1, &st, nullptr);
    sqlite3_bind_text(st, 1, policy.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
    return sqlite3_changes(db_) > 0;
}

bool HouseStore::add_pickup(const std::string& username, const std::string& item_slug, long long now_ms) {
    int player_id = find_or_create_player(username);
    sqlite3_stmt* st = nullptr;
    // MIN(.., 99): a lookup station can be pressed forever; the count is a
    // souvenir, not a balance worth letting a stuck key grow without bound.
    sqlite3_prepare_v2(db_,
        "INSERT INTO inventory (player_id, item_id, quantity, updated_at) "
        "SELECT ?1, item_id, 1, ?3 FROM items WHERE slug = ?2 "
        "ON CONFLICT(player_id, item_id) DO UPDATE SET "
        "quantity = MIN(quantity + 1, 99), updated_at = excluded.updated_at;",
        -1, &st, nullptr);
    sqlite3_bind_int(st, 1, player_id);
    sqlite3_bind_text(st, 2, item_slug.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 3, now_ms);
    sqlite3_step(st);
    sqlite3_finalize(st);
    return sqlite3_changes(db_) > 0;
}

std::vector<HouseStore::InventoryEntry> HouseStore::inventory_of(const std::string& username) {
    std::vector<InventoryEntry> out;
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT i.slug, inv.quantity FROM inventory inv "
        "JOIN players p ON p.player_id = inv.player_id "
        "JOIN items i ON i.item_id = inv.item_id "
        "WHERE p.lg_username = ?1 AND inv.quantity > 0 "
        "ORDER BY inv.updated_at DESC, inv.item_id DESC;",
        -1, &st, nullptr);
    sqlite3_bind_text(st, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(st) == SQLITE_ROW) {
        InventoryEntry e;
        e.slug = reinterpret_cast<const char*>(sqlite3_column_text(st, 0));
        e.quantity = sqlite3_column_int(st, 1);
        out.push_back(e);
    }
    sqlite3_finalize(st);
    return out;
}

std::string HouseStore::get_wall_slot(int instance_id) {
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT i.slug FROM placed p JOIN items i ON i.item_id = p.item_id "
        "WHERE p.instance_id = ?1 AND p.is_wall_slot = 1 LIMIT 1;",
        -1, &st, nullptr);
    sqlite3_bind_int(st, 1, instance_id);
    std::string slug;
    if (sqlite3_step(st) == SQLITE_ROW) {
        slug = reinterpret_cast<const char*>(sqlite3_column_text(st, 0));
    }
    sqlite3_finalize(st);
    return slug;
}

void HouseStore::set_wall_slot(int instance_id, const std::string& item_slug) {
    // schema.sql: "Any multi-row mutation ... must be BEGIN/COMMIT" --
    // the clear-then-set below is exactly that shape.
    sqlite3_exec(db_, "BEGIN;", nullptr, nullptr, nullptr);

    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_, "DELETE FROM placed WHERE instance_id = ?1 AND is_wall_slot = 1;", -1, &st, nullptr);
    sqlite3_bind_int(st, 1, instance_id);
    sqlite3_step(st);
    sqlite3_finalize(st);

    if (!item_slug.empty()) {
        sqlite3_prepare_v2(db_,
            "INSERT INTO placed (instance_id, item_id, x, y, rot90, is_wall_slot) "
            "SELECT ?1, item_id, 0, 0, 0, 1 FROM items WHERE slug = ?2;",
            -1, &st, nullptr);
        sqlite3_bind_int(st, 1, instance_id);
        sqlite3_bind_text(st, 2, item_slug.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }

    sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
}

} // namespace commons
