#include "house.hpp"

#include "sqlx.hpp"

// Every prepare/bind/step/COMMIT result here is checked (sqlx.hpp): a failed
// statement logs the sqlite message and throws sqlx::DbError, which
// HttpServer::dispatch turns into a 500 for that one request. It never aborts
// the daemon, and a multi-row write is rolled back rather than left half-done.
namespace commons {

int HouseStore::find_or_create_player(const std::string& username) {
    {
        sqlx::Stmt sel(db_, "SELECT player_id FROM players WHERE lg_username = ?1;");
        sel.text(1, username);
        if (sel.row()) return sel.column_int(0);
    }
    sqlx::Stmt ins(db_,
        "INSERT INTO players (lg_username, display_name, is_guest, created_at) "
        "VALUES (?1, ?1, 0, strftime('%s','now'));");
    ins.text(1, username);
    ins.done();
    return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

int HouseStore::house_instance_for(const std::string& username) {
    int player_id = find_or_create_player(username);

    {
        sqlx::Stmt sel(db_, "SELECT house_instance_id FROM players WHERE player_id = ?1;");
        sel.integer(1, player_id);
        if (sel.row() && !sel.column_null(0)) return sel.column_int(0);
    }

    int map_id = 0;
    {
        sqlx::Stmt sel(db_, "SELECT map_id FROM maps WHERE slug = 'house';");
        // schema always seeds 'house' before the daemon accepts requests -- this should not happen
        if (!sel.row()) return -1;
        map_id = sel.column_int(0);
    }

    // GROK_PERSIST_REVIEW.md must-fix: schema.sql already requires any
    // multi-row mutation to be BEGIN/COMMIT -- this INSERT+UPDATE pair must
    // not be split, or a crash between the two leaves an orphan
    // map_instances row and the next entry creates a *second* house. Tx rolls
    // back on any failure, including a thrown statement error.
    sqlx::Tx tx(db_);
    {
        sqlx::Stmt ins(db_,
            "INSERT INTO map_instances (map_id, owner_player_id, created_at) "
            "VALUES (?1, ?2, strftime('%s','now'));");
        ins.integer(1, map_id).integer(2, player_id);
        ins.done();
    }
    int instance_id = static_cast<int>(sqlite3_last_insert_rowid(db_));
    {
        sqlx::Stmt up(db_, "UPDATE players SET house_instance_id = ?1 WHERE player_id = ?2;");
        up.integer(1, instance_id).integer(2, player_id);
        up.done();
    }
    tx.commit();
    return instance_id;
}

std::optional<int> HouseStore::existing_house_instance_for(const std::string& username) {
    sqlx::Stmt sel(db_,
        "SELECT house_instance_id FROM players WHERE lg_username = ?1 AND house_instance_id IS NOT NULL;");
    sel.text(1, username);
    if (sel.row()) return sel.column_int(0);
    return std::nullopt;
}

std::optional<HouseStore::Position> HouseStore::last_house_position(const std::string& username) {
    sqlx::Stmt sel(db_,
        "SELECT last_x, last_y, last_facing FROM players "
        "WHERE lg_username = ?1 AND house_instance_id IS NOT NULL "
        "AND last_instance_id = house_instance_id "
        "AND last_x IS NOT NULL AND last_y IS NOT NULL;");
    sel.text(1, username);
    if (!sel.row()) return std::nullopt;
    Position p;
    p.x = sel.column_int(0);
    p.y = sel.column_int(1);
    p.facing = sel.column_int(2);
    return p;
}

void HouseStore::snapshot_leave(const std::string& username, int x, int y, int facing) {
    sqlx::Stmt up(db_,
        "UPDATE players SET last_instance_id = house_instance_id, "
        "last_x = ?1, last_y = ?2, last_facing = ?3, "
        "last_grid_col = NULL, last_grid_row = NULL "
        "WHERE lg_username = ?4 AND house_instance_id IS NOT NULL;");
    up.integer(1, x).integer(2, y).integer(3, facing).text(4, username);
    up.done();
}

std::string HouseStore::visit_policy_of(const std::string& username) {
    sqlx::Stmt sel(db_, "SELECT visit_policy FROM players WHERE lg_username = ?1;");
    sel.text(1, username);
    if (sel.row()) {
        std::string v = sel.column_text(0);
        if (v == "locked" || v == "open") return v;
    }
    return "friends"; // the default, and what any unknown value reads as
}

bool HouseStore::set_visit_policy(const std::string& username, const std::string& policy) {
    if (policy != "friends" && policy != "locked" && policy != "open") return false;
    sqlx::Stmt up(db_, "UPDATE players SET visit_policy = ?1 WHERE lg_username = ?2;");
    up.text(1, policy).text(2, username);
    up.done();
    return sqlite3_changes(db_) > 0;
}

std::vector<std::string> HouseStore::all_usernames() {
    std::vector<std::string> out;
    sqlx::Stmt sel(db_, "SELECT lg_username FROM players WHERE lg_username IS NOT NULL ORDER BY player_id;");
    while (sel.row()) out.push_back(sel.column_text(0));
    return out;
}

HouseStore::Footprint HouseStore::footprint_of(const std::string& username) {
    Footprint f;
    int player_id = -1;
    {
        sqlx::Stmt sel(db_, "SELECT player_id, house_instance_id FROM players WHERE lg_username = ?1;");
        sel.text(1, username);
        if (!sel.row()) return f;
        player_id = sel.column_int(0);
        if (!sel.column_null(1)) f.house_instance = sel.column_int(1);
    }
    {
        sqlx::Stmt sel(db_, "SELECT COUNT(*), COALESCE(SUM(quantity), 0) FROM inventory WHERE player_id = ?1;");
        sel.integer(1, player_id);
        if (sel.row()) { f.inventory_rows = sel.column_int(0); f.inventory_qty = sel.column_int(1); }
    }
    if (f.house_instance >= 0) {
        sqlx::Stmt sel(db_, "SELECT COUNT(*) FROM placed WHERE instance_id = ?1;");
        sel.integer(1, f.house_instance);
        if (sel.row()) f.placed_rows = sel.column_int(0);
    }
    return f;
}

HouseStore::ReapResult HouseStore::reap_orphan(const std::string& username) {
    ReapResult r;
    int player_id = -1, house = -1;
    {
        sqlx::Stmt sel(db_, "SELECT player_id, house_instance_id FROM players WHERE lg_username = ?1;");
        sel.text(1, username);
        if (!sel.row()) return r; // nothing to reap
        player_id = sel.column_int(0);
        if (!sel.column_null(1)) house = sel.column_int(1);
    }
    sqlx::Tx tx(db_); // rolls everything back unless commit() is reached
    {
        sqlx::Stmt del(db_, "DELETE FROM inventory WHERE player_id = ?1;");
        del.integer(1, player_id);
        del.done();
        r.inventory_rows = sqlite3_changes(db_);
    }
    if (house >= 0) {
        {
            sqlx::Stmt del(db_, "DELETE FROM placed WHERE instance_id = ?1;");
            del.integer(1, house);
            del.done();
            r.placed_rows = sqlite3_changes(db_);
        }
        // No other player should point at this house (last_instance_id is only ever
        // written for one's own house), but never leave a dangling reference.
        sqlx::Stmt rel(db_, "UPDATE players SET last_instance_id = NULL WHERE last_instance_id = ?1 AND player_id != ?2;");
        rel.integer(1, house).integer(2, player_id);
        rel.done();
    }
    {
        sqlx::Stmt del(db_, "DELETE FROM players WHERE player_id = ?1;");
        del.integer(1, player_id);
        del.done();
        r.player_row = sqlite3_changes(db_) > 0;
    }
    if (house >= 0) {
        sqlx::Stmt del(db_, "DELETE FROM map_instances WHERE instance_id = ?1 AND owner_player_id = ?2;");
        del.integer(1, house).integer(2, player_id);
        del.done();
        r.house_row = sqlite3_changes(db_) > 0;
    }
    tx.commit();
    return r;
}

bool HouseStore::add_pickup(const std::string& username, const std::string& item_slug, long long now_ms) {
    int player_id = find_or_create_player(username);
    // MIN(.., 99): a lookup station can be pressed forever; the count is a
    // souvenir, not a balance worth letting a stuck key grow without bound.
    sqlx::Stmt up(db_,
        "INSERT INTO inventory (player_id, item_id, quantity, updated_at) "
        "SELECT ?1, item_id, 1, ?3 FROM items WHERE slug = ?2 "
        "ON CONFLICT(player_id, item_id) DO UPDATE SET "
        "quantity = MIN(quantity + 1, 99), updated_at = excluded.updated_at;");
    up.integer(1, player_id).text(2, item_slug).integer64(3, now_ms);
    up.done();
    return sqlite3_changes(db_) > 0; // 0 = not a real catalog slug
}

std::vector<HouseStore::InventoryEntry> HouseStore::inventory_of(const std::string& username) {
    std::vector<InventoryEntry> out;
    sqlx::Stmt sel(db_,
        "SELECT i.slug, inv.quantity FROM inventory inv "
        "JOIN players p ON p.player_id = inv.player_id "
        "JOIN items i ON i.item_id = inv.item_id "
        "WHERE p.lg_username = ?1 AND inv.quantity > 0 "
        "ORDER BY inv.updated_at DESC, inv.item_id DESC;");
    sel.text(1, username);
    while (sel.row()) {
        InventoryEntry e;
        e.slug = sel.column_text(0);
        e.quantity = sel.column_int(1);
        out.push_back(e);
    }
    return out;
}

std::string HouseStore::get_wall_slot(int instance_id) {
    sqlx::Stmt sel(db_,
        "SELECT i.slug FROM placed p JOIN items i ON i.item_id = p.item_id "
        "WHERE p.instance_id = ?1 AND p.is_wall_slot = 1 LIMIT 1;");
    sel.integer(1, instance_id);
    if (sel.row()) return sel.column_text(0);
    return std::string();
}

void HouseStore::set_wall_slot(int instance_id, const std::string& item_slug) {
    // schema.sql: "Any multi-row mutation ... must be BEGIN/COMMIT" -- the
    // clear-then-set below is exactly that shape. Tx rolls the clear back if
    // the insert fails, so a failed place never leaves the wall emptied.
    sqlx::Tx tx(db_);
    {
        sqlx::Stmt del(db_, "DELETE FROM placed WHERE instance_id = ?1 AND is_wall_slot = 1;");
        del.integer(1, instance_id);
        del.done();
    }
    if (!item_slug.empty()) {
        sqlx::Stmt ins(db_,
            "INSERT INTO placed (instance_id, item_id, x, y, rot90, is_wall_slot) "
            "SELECT ?1, item_id, 0, 0, 0, 1 FROM items WHERE slug = ?2;");
        ins.integer(1, instance_id).text(2, item_slug);
        ins.done();
    }
    tx.commit();
}

std::vector<HouseStore::FloorItem> HouseStore::list_floor(int instance_id) {
    std::vector<FloorItem> out;
    sqlx::Stmt sel(db_,
        "SELECT i.slug, p.x, p.y FROM placed p JOIN items i ON i.item_id = p.item_id "
        "WHERE p.instance_id = ?1 AND p.is_wall_slot = 0 ORDER BY p.placed_id;");
    sel.integer(1, instance_id);
    while (sel.row()) {
        FloorItem f;
        f.slug = sel.column_text(0);
        f.x = sel.column_int(1);
        f.y = sel.column_int(2);
        out.push_back(f);
    }
    return out;
}

std::string HouseStore::place_floor(int instance_id, const std::string& item_slug, int x, int y) {
    // Interior is 16x16 with a 1-tile wall; the door tiles are 7,14 / 7,15.
    if (x < 1 || y < 1 || x > 14 || y > 14) return "bad_tile";
    if (x == 7 && (y == 14 || y == 15)) return "bad_tile";
    {
        sqlx::Stmt n(db_,
            "SELECT COUNT(*) FROM placed WHERE instance_id = ?1 AND is_wall_slot = 0;");
        n.integer(1, instance_id);
        if (n.row() && n.column_int(0) >= 8) return "full";
    }
    {
        sqlx::Stmt hit(db_,
            "SELECT 1 FROM placed WHERE instance_id = ?1 AND is_wall_slot = 0 AND x = ?2 AND y = ?3 LIMIT 1;");
        hit.integer(1, instance_id).integer(2, x).integer(3, y);
        if (hit.row()) return "occupied";
    }
    sqlx::Stmt ins(db_,
        "INSERT INTO placed (instance_id, item_id, x, y, rot90, is_wall_slot) "
        "SELECT ?1, item_id, ?2, ?3, 0, 0 FROM items WHERE slug = ?4;");
    ins.integer(1, instance_id).integer(2, x).integer(3, y).text(4, item_slug);
    ins.done();
    if (sqlite3_changes(db_) == 0) return "unknown_item";
    return std::string();
}

std::string HouseStore::take_floor(int instance_id, int x, int y) {
    std::string slug;
    {
        sqlx::Stmt sel(db_,
            "SELECT i.slug FROM placed p JOIN items i ON i.item_id = p.item_id "
            "WHERE p.instance_id = ?1 AND p.is_wall_slot = 0 AND p.x = ?2 AND p.y = ?3 LIMIT 1;");
        sel.integer(1, instance_id).integer(2, x).integer(3, y);
        if (sel.row()) slug = sel.column_text(0);
    }
    if (slug.empty()) return std::string();
    sqlx::Stmt del(db_,
        "DELETE FROM placed WHERE instance_id = ?1 AND is_wall_slot = 0 AND x = ?2 AND y = ?3;");
    del.integer(1, instance_id).integer(2, x).integer(3, y);
    del.done();
    return slug;
}

} // namespace commons
