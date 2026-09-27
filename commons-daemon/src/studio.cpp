#include "studio.hpp"

#include <algorithm>
#include <cstdint>
#include <map>

#include "sqlx.hpp"

// Every prepare/bind/step/COMMIT result here is checked (sqlx.hpp), same as house.cpp: a failed
// statement throws sqlx::DbError, which HttpServer::dispatch turns into a 500 for that one request
// and a multi-row write rolls back rather than leaving something half-done.
namespace commons {

namespace {

// The fixed gather-spot table (studio.md "Where things live"). Not client-authored: the client (from
// slice 3 on) sends one of these exact slugs, matching a marker it draws at a fixed tile, the same way
// world.js's SCATTER/pickup markers are hardcoded today. oak_gall / seashell reuse World Pickups D's
// own item slugs and, from slice 3, their exact existing spots (studio.md: "no second marker on the
// same tile"); the two Lab spots exist here so gather/bench/collect are already fully testable end to
// end in slice 1, even though the Lab's own client stations are not converted until slice 3.
struct GatherSpot { const char* item; int qty; long long cooldown_ms; };
const std::map<std::string, GatherSpot>& gather_spots() {
    static const std::map<std::string, GatherSpot> spots = {
        {"north_woods_galls",  {"oak_gall",       3, 600000}},
        {"north_woods_twigs",  {"willow_twigs",   3, 600000}},
        {"orchard_keg",        {"vinegar",        3, 86400000}},
        {"shore_shells",       {"seashell",       3, 600000}},
        {"quarry_ochre",       {"ochre_earth",    3, 600000}},
        {"quarry_alum",        {"alum_stone",     3, 600000}},
        {"garden_madder",      {"madder_root",    3, 600000}},
        {"garden_weld",        {"weld",           3, 600000}},
        {"garden_woad",        {"woad_leaves",    3, 600000}},
        {"workshop_iron",      {"scrap_iron",     3, 600000}},
        {"workshop_copper",    {"copper_sheet",   3, 600000}},
        {"lab_gum",            {"gum_arabic_raw", 3, 600000}},
        {"lab_soot",           {"soot",           3, 600000}},
    };
    return spots;
}

// Side-mix fixed fail texts (studio.md), keyed by the sorted (duplicates kept) slug list. A single
// canonical join of the sorted vector is the map key -- cheap, exact, no ambiguity from separator
// characters since no real slug in this catalog contains one.
std::string canonical_key(std::vector<std::string> slugs) {
    std::sort(slugs.begin(), slugs.end());
    std::string out;
    for (const auto& s : slugs) { out += s; out += '|'; }
    return out;
}

} // namespace

int StudioStore::find_or_create_player(const std::string& username) {
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

std::optional<int> StudioStore::find_player_id(const std::string& username) {
    sqlx::Stmt sel(db_, "SELECT player_id FROM players WHERE lg_username = ?1;");
    sel.text(1, username);
    if (sel.row()) return sel.column_int(0);
    return std::nullopt;
}

StudioStore::GatherResult StudioStore::gather(const std::string& username, const std::string& spot, long long now_ms) {
    GatherResult r;
    auto it = gather_spots().find(spot);
    if (it == gather_spots().end()) { r.reason = "unknown_spot"; return r; }
    const GatherSpot& g = it->second;
    int player_id = find_or_create_player(username);

    sqlx::Tx tx(db_); // the cooldown set and the inventory grant are one transaction (schema.sql)
    long long next = now_ms + g.cooldown_ms;
    // A first-ever gather at this spot always succeeds (INSERT branch, no WHERE to satisfy); a later
    // one only if the existing row's next_ms has already passed (the UPDATE's WHERE) -- so two racing
    // POSTs for the same player+spot cannot both pass: only one can be the statement that actually
    // changes the row, and sqlite3_changes() below is checked on THIS statement, in THIS transaction.
    {
        sqlx::Stmt up(db_,
            "INSERT INTO gather_cooldowns (player_id, spot, next_ms) VALUES (?1, ?2, ?3) "
            "ON CONFLICT(player_id, spot) DO UPDATE SET next_ms = excluded.next_ms "
            "WHERE gather_cooldowns.next_ms <= ?4;");
        up.integer(1, player_id).text(2, spot).integer64(3, next).integer64(4, now_ms);
        up.done();
        if (sqlite3_changes(db_) == 0) {
            // Still on cooldown: report the EXISTING next_ms, not the one we just failed to set.
            sqlx::Stmt sel(db_, "SELECT next_ms FROM gather_cooldowns WHERE player_id = ?1 AND spot = ?2;");
            sel.integer(1, player_id).text(2, spot);
            if (sel.row()) r.next_ms = sel.column_int64(0);
            r.reason = "on_cooldown";
            return r; // Tx's destructor rolls back the no-op INSERT/UPDATE attempt; nothing changed
        }
    }
    {
        sqlx::Stmt up(db_,
            "INSERT INTO inventory (player_id, item_id, quantity, updated_at) "
            "SELECT ?1, item_id, ?3, ?4 FROM items WHERE slug = ?2 "
            "ON CONFLICT(player_id, item_id) DO UPDATE SET "
            "quantity = MIN(quantity + ?3, 99), updated_at = excluded.updated_at;");
        up.integer(1, player_id).text(2, g.item).integer(3, g.qty).integer64(4, now_ms);
        up.done();
    }
    tx.commit();
    r.ok = true; r.item = g.item; r.qty = g.qty; r.next_ms = next;
    return r;
}

StudioStore::BenchResult StudioStore::start_batch(const std::string& username, const std::string& recipe_slug, long long now_ms) {
    BenchResult r;
    int recipe_id = -1; long long duration_s = 0;
    {
        // Only a recipe with duration_s set is benchable -- a Lab-A lookup (duration_s NULL) is a
        // different mechanic entirely and the bench must refuse it, not silently "start" a lookup.
        sqlx::Stmt sel(db_, "SELECT recipe_id, duration_s FROM recipes WHERE slug = ?1 AND duration_s IS NOT NULL;");
        sel.text(1, recipe_slug);
        if (!sel.row()) { r.reason = "unknown_recipe"; return r; }
        recipe_id = sel.column_int(0);
        duration_s = sel.column_int(1);
    }
    int player_id = find_or_create_player(username);

    sqlx::Tx tx(db_); // slot check, input check, every deduction and the batch insert: one transaction
    {
        sqlx::Stmt n(db_, "SELECT COUNT(*) FROM batches WHERE player_id = ?1 AND collected = 0;");
        n.integer(1, player_id);
        if (n.row() && n.column_int(0) >= 3) { r.reason = "slots_full"; return r; } // Tx rolls back (never opened a write)
    }
    // Every input, in order: fail the whole thing on the first one this player does not hold enough
    // of. A guarded UPDATE (quantity >= needed) re-checks inside THIS transaction, not just the read
    // above, so a second bench racing the same inventory cannot both succeed on the same units.
    {
        sqlx::Stmt ins(db_, "SELECT item_id, quantity FROM recipe_inputs WHERE recipe_id = ?1;");
        ins.integer(1, recipe_id);
        std::vector<std::pair<int, int>> inputs; // (item_id, quantity)
        while (ins.row()) inputs.emplace_back(ins.column_int(0), ins.column_int(1));
        for (const auto& in : inputs) {
            sqlx::Stmt up(db_,
                "UPDATE inventory SET quantity = quantity - ?1 "
                "WHERE player_id = ?2 AND item_id = ?3 AND quantity >= ?1;");
            up.integer(1, in.second).integer(2, player_id).integer(3, in.first);
            up.done();
            if (sqlite3_changes(db_) == 0) { r.reason = "missing_inputs"; return r; } // Tx rolls back every prior deduction too
        }
    }
    long long ready = now_ms + duration_s * 1000;
    {
        sqlx::Stmt ins(db_,
            "INSERT INTO batches (player_id, recipe_id, started_ms, ready_ms, collected) "
            "VALUES (?1, ?2, ?3, ?4, 0);");
        ins.integer(1, player_id).integer(2, recipe_id).integer64(3, now_ms).integer64(4, ready);
        ins.done();
    }
    int batch_id = static_cast<int>(sqlite3_last_insert_rowid(db_));
    tx.commit();
    r.ok = true; r.batch_id = batch_id; r.ready_ms = ready;
    return r;
}

std::string StudioStore::mix(std::vector<std::string> slugs) const {
    // Fixed texts, keyed by the exact sorted-with-duplicates combination (studio.md). Water is never a
    // slug (it is implicit and never tracked), so it can never appear here either.
    static const std::map<std::string, std::string> texts = {
        {canonical_key({"shell_white", "vinegar"}), "It fizzes and dissolves -- that is calcium acetate, not paint."},
        {canonical_key({"chalk_block", "vinegar"}), "It fizzes and dissolves -- that is calcium acetate, not paint."},
        {canonical_key({"scrap_iron", "vinegar"}), "A rust-brown solution -- without tannin, this is not ink."},
        {canonical_key({"tannin"}), "A weak brown stain. On its own, tannin is not much of anything."},
    };
    auto it = texts.find(canonical_key(std::move(slugs)));
    return it != texts.end() ? it->second : "Nothing useful comes of it.";
}

StudioStore::CollectResult StudioStore::collect(const std::string& username, int batch_id, long long now_ms) {
    CollectResult r;
    auto player_id = find_player_id(username);
    if (!player_id) { r.reason = "not_found"; return r; }

    int recipe_id = -1; long long ready_ms = 0; int owner = -1, collected = 0;
    {
        sqlx::Stmt sel(db_, "SELECT player_id, recipe_id, ready_ms, collected FROM batches WHERE batch_id = ?1;");
        sel.integer(1, batch_id);
        if (!sel.row()) { r.reason = "not_found"; return r; }
        owner = sel.column_int(0);
        recipe_id = sel.column_int(1);
        ready_ms = sel.column_int64(2);
        collected = sel.column_int(3);
    }
    if (owner != *player_id) { r.reason = "not_yours"; return r; }
    if (collected) { r.reason = "already_collected"; return r; }
    // Authoritative on the row's OWN ready_ms; the client's countdown is display only (studio.md).
    if (now_ms < ready_ms) { r.reason = "not_ready"; return r; }

    sqlx::Tx tx(db_);
    {
        // The WHERE collected = 0 makes this the one statement that can flip it -- if two collects for
        // the same batch_id somehow race, only one changes a row, so the grant below runs at most once.
        sqlx::Stmt up(db_, "UPDATE batches SET collected = 1 WHERE batch_id = ?1 AND collected = 0;");
        up.integer(1, batch_id);
        up.done();
        if (sqlite3_changes(db_) == 0) { r.reason = "already_collected"; return r; } // lost the race; Tx rolls back (nothing else ran)
    }
    {
        sqlx::Stmt outs(db_,
            "SELECT i.slug, i.item_id, ro.quantity FROM recipe_outputs ro "
            "JOIN items i ON i.item_id = ro.item_id WHERE ro.recipe_id = ?1;");
        outs.integer(1, recipe_id);
        while (outs.row()) {
            std::string slug = outs.column_text(0);
            int item_id = outs.column_int(1), qty = outs.column_int(2);
            sqlx::Stmt up(db_,
                "INSERT INTO inventory (player_id, item_id, quantity, updated_at) "
                "VALUES (?1, ?2, ?3, ?4) "
                "ON CONFLICT(player_id, item_id) DO UPDATE SET "
                "quantity = MIN(quantity + ?3, 99), updated_at = excluded.updated_at;");
            up.integer(1, *player_id).integer(2, item_id).integer(3, qty).integer64(4, now_ms);
            up.done();
            r.outputs.emplace_back(slug, qty);
        }
    }
    {
        sqlx::Stmt ins(db_, "INSERT OR IGNORE INTO discoveries (player_id, recipe_id, first_ms) VALUES (?1, ?2, ?3);");
        ins.integer(1, *player_id).integer(2, recipe_id).integer64(3, now_ms);
        ins.done();
        r.first_discovery = sqlite3_changes(db_) > 0; // 0 = they had already discovered this recipe
    }
    tx.commit();
    r.ok = true;
    return r;
}

std::vector<StudioStore::BatchView> StudioStore::open_batches(const std::string& username) {
    std::vector<BatchView> out;
    auto player_id = find_player_id(username);
    if (!player_id) return out;
    sqlx::Stmt sel(db_,
        "SELECT b.batch_id, r.slug, b.ready_ms FROM batches b "
        "JOIN recipes r ON r.recipe_id = b.recipe_id "
        "WHERE b.player_id = ?1 AND b.collected = 0 ORDER BY b.batch_id;");
    sel.integer(1, *player_id);
    while (sel.row()) out.push_back({sel.column_int(0), sel.column_text(1), sel.column_int64(2)});
    return out;
}

std::vector<StudioStore::DiscoveryView> StudioStore::discoveries_of(const std::string& username) {
    std::vector<DiscoveryView> out;
    auto player_id = find_player_id(username);
    if (!player_id) return out;
    sqlx::Stmt sel(db_,
        "SELECT r.slug, d.first_ms FROM discoveries d "
        "JOIN recipes r ON r.recipe_id = d.recipe_id "
        "WHERE d.player_id = ?1 ORDER BY d.first_ms;");
    sel.integer(1, *player_id);
    while (sel.row()) out.push_back({sel.column_text(0), sel.column_int64(1)});
    return out;
}

std::vector<StudioStore::CooldownView> StudioStore::cooldowns_of(const std::string& username, long long now_ms) {
    std::vector<CooldownView> out;
    auto player_id = find_player_id(username);
    if (!player_id) return out;
    sqlx::Stmt sel(db_,
        "SELECT spot, next_ms FROM gather_cooldowns WHERE player_id = ?1 AND next_ms > ?2 ORDER BY spot;");
    sel.integer(1, *player_id).integer64(2, now_ms);
    while (sel.row()) out.push_back({sel.column_text(0), sel.column_int64(1)});
    return out;
}

std::vector<StudioStore::RecipeView> StudioStore::makeable_now(const std::string& username) {
    std::vector<RecipeView> out;
    auto player_id = find_player_id(username);
    if (!player_id) return out;
    // Every Studio recipe (duration_s IS NOT NULL) whose EVERY input this player holds >= its
    // quantity. A recipe with no recipe_inputs row at all is vacuously makeable (none exist among the
    // Studio recipes today, but the query stays correct if one ever has zero inputs).
    sqlx::Stmt sel(db_,
        "SELECT r.slug, r.display_name, r.duration_s FROM recipes r "
        "WHERE r.duration_s IS NOT NULL AND NOT EXISTS ("
        "  SELECT 1 FROM recipe_inputs ri LEFT JOIN inventory inv "
        "    ON inv.player_id = ?1 AND inv.item_id = ri.item_id "
        "  WHERE ri.recipe_id = r.recipe_id AND COALESCE(inv.quantity, 0) < ri.quantity"
        ") ORDER BY r.tier, r.recipe_id;");
    sel.integer(1, *player_id);
    while (sel.row()) out.push_back({sel.column_text(0), sel.column_text(1), sel.column_int(2)});
    return out;
}

std::vector<StudioStore::PaletteEntry> StudioStore::unlocked_palette(const std::string& username) {
    std::vector<PaletteEntry> out;
    auto player_id = find_player_id(username);
    if (!player_id) return out;
    // Discovered AND the output is actually paintable (pigment_hex set) -- an intermediate like
    // tannin or binder is discovered too, but never shows up here (it has no hex to paint with).
    sqlx::Stmt sel(db_,
        "SELECT DISTINCT i.slug, i.pigment_hex FROM discoveries d "
        "JOIN recipe_outputs ro ON ro.recipe_id = d.recipe_id "
        "JOIN items i ON i.item_id = ro.item_id "
        "WHERE d.player_id = ?1 AND i.pigment_hex IS NOT NULL ORDER BY i.slug;");
    sel.integer(1, *player_id);
    while (sel.row()) out.push_back({sel.column_text(0), sel.column_text(1)});
    return out;
}

namespace {

struct CharcoalSwatch { int index; const char* slug; const char* hex; };
const CharcoalSwatch CHARCOAL[] = {
    {1, "charcoal_black",  "#1a1a1a"},
    {2, "charcoal_grey_1", "#4d4d4d"},
    {3, "charcoal_grey_2", "#7a7a7a"},
    {4, "charcoal_grey_3", "#adadad"},
};

const char* charcoal_slug(int index) {
    for (const auto& c : CHARCOAL) if (c.index == index) return c.slug;
    return nullptr;
}

int inventory_qty(sqlite3* db, int player_id, const char* slug) {
    sqlx::Stmt sel(db,
        "SELECT inv.quantity FROM inventory inv "
        "JOIN items i ON i.item_id = inv.item_id "
        "WHERE inv.player_id = ?1 AND i.slug = ?2;");
    sel.integer(1, player_id).text(2, slug);
    if (sel.row()) return sel.column_int(0);
    return 0;
}

} // namespace

std::vector<StudioStore::Swatch> charcoal_held(sqlite3* db, int player_id) {
    std::vector<StudioStore::Swatch> out;
    for (const auto& c : CHARCOAL) {
        StudioStore::Swatch s;
        s.index = c.index;
        s.item = c.slug;
        s.hex = c.hex;
        s.qty = inventory_qty(db, player_id, c.slug);
        out.push_back(s);
    }
    return out;
}

StudioStore::Portrait StudioStore::portrait_of(const std::string& username) {
    Portrait p;
    auto player_id = find_player_id(username);
    if (!player_id) return p;
    p.palette = charcoal_held(db_, *player_id);
    sqlx::Stmt sel(db_,
        "SELECT pixels FROM paintings WHERE player_id = ?1 AND kind = 'portrait';");
    sel.integer(1, *player_id);
    if (!sel.row()) return p;
    auto blob = sel.column_blob(0);
    if (static_cast<int>(blob.size()) != PORTRAIT_N) return p;
    p.found = true;
    p.pixels = std::move(blob);
    return p;
}

StudioStore::Portrait StudioStore::ensure_portrait(const std::string& username, long long now_ms) {
    int player_id = find_or_create_player(username);
    {
        sqlx::Stmt sel(db_,
            "SELECT pixels FROM paintings WHERE player_id = ?1 AND kind = 'portrait';");
        sel.integer(1, player_id);
        if (sel.row()) return portrait_of(username);
    }
    std::vector<unsigned char> blank(PORTRAIT_N, 0);
    sqlx::Tx tx(db_);
    {
        sqlx::Stmt ins(db_,
            "INSERT INTO paintings (player_id, kind, w, h, pixels, updated_ms) "
            "VALUES (?1, 'portrait', 32, 32, ?2, ?3);");
        ins.integer(1, player_id).blob(2, blank.data(), PORTRAIT_N).integer64(3, now_ms);
        ins.done();
    }
    {
        // 2 units of black = 400 pixels. Once, because this insert only runs when no portrait row exists.
        sqlx::Stmt up(db_,
            "INSERT INTO inventory (player_id, item_id, quantity, updated_at) "
            "SELECT ?1, item_id, ?2, ?3 FROM items WHERE slug = 'charcoal_black' "
            "ON CONFLICT(player_id, item_id) DO UPDATE SET "
            "quantity = MIN(quantity + ?2, 99), updated_at = excluded.updated_at;");
        up.integer(1, player_id).integer(2, STUB_BLACK_UNITS).integer64(3, now_ms);
        up.done();
        if (sqlite3_changes(db_) == 0) {
            // catalog row missing: roll the blank portrait back rather than hand out a stub-less canvas
            throw sqlx::DbError("charcoal_black missing");
        }
    }
    tx.commit();
    return portrait_of(username);
}

StudioStore::SaveResult StudioStore::save_portrait(const std::string& username,
                                                   const std::vector<unsigned char>& pixels,
                                                   long long now_ms) {
    SaveResult r;
    if (static_cast<int>(pixels.size()) != PORTRAIT_N) { r.reason = "bad_length"; return r; }
    int player_id = find_or_create_player(username);
    std::vector<unsigned char> old(PORTRAIT_N, 0);
    bool existed = false;
    {
        sqlx::Stmt sel(db_,
            "SELECT pixels FROM paintings WHERE player_id = ?1 AND kind = 'portrait';");
        sel.integer(1, player_id);
        if (sel.row()) {
            auto blob = sel.column_blob(0);
            if (static_cast<int>(blob.size()) == PORTRAIT_N) { old = std::move(blob); existed = true; }
        }
    }
    int need[5] = {0, 0, 0, 0, 0};
    for (int i = 0; i < PORTRAIT_N; ++i) {
        unsigned char neu = pixels[static_cast<std::size_t>(i)];
        unsigned char was = old[static_cast<std::size_t>(i)];
        if (neu == was) continue;
        if (neu == 0) continue;  // erase to blank: free, and never a refund of `was`
        if (!charcoal_slug(neu)) { r.reason = "locked_index"; return r; }
        need[neu] += 1;
    }
    sqlx::Tx tx(db_);
    for (const auto& c : CHARCOAL) {
        int n = need[c.index];
        if (n <= 0) continue;
        int units = (n + PIXELS_PER_UNIT - 1) / PIXELS_PER_UNIT;  // ceil
        sqlx::Stmt up(db_,
            "UPDATE inventory SET quantity = quantity - ?2, updated_at = ?3 "
            "WHERE player_id = ?1 AND item_id = (SELECT item_id FROM items WHERE slug = ?4) "
            "AND quantity >= ?2;");
        up.integer(1, player_id).integer(2, units).integer64(3, now_ms).text(4, c.slug);
        up.done();
        if (sqlite3_changes(db_) == 0) { r.reason = "insufficient"; return r; }  // dtor rolls back
    }
    if (existed) {
        sqlx::Stmt up(db_,
            "UPDATE paintings SET pixels = ?2, updated_ms = ?3 "
            "WHERE player_id = ?1 AND kind = 'portrait';");
        up.integer(1, player_id).blob(2, pixels.data(), PORTRAIT_N).integer64(3, now_ms);
        up.done();
    } else {
        sqlx::Stmt ins(db_,
            "INSERT INTO paintings (player_id, kind, w, h, pixels, updated_ms) "
            "VALUES (?1, 'portrait', 32, 32, ?2, ?3);");
        ins.integer(1, player_id).blob(2, pixels.data(), PORTRAIT_N).integer64(3, now_ms);
        ins.done();
    }
    tx.commit();
    r.ok = true;
    r.palette = charcoal_held(db_, player_id);
    return r;
}

StudioStore::Footprint StudioStore::footprint_of(const std::string& username) {
    Footprint f;
    auto player_id = find_player_id(username);
    if (!player_id) return f;
    {
        sqlx::Stmt sel(db_, "SELECT COUNT(*) FROM batches WHERE player_id = ?1;");
        sel.integer(1, *player_id);
        if (sel.row()) f.batches = sel.column_int(0);
    }
    {
        sqlx::Stmt sel(db_, "SELECT COUNT(*) FROM discoveries WHERE player_id = ?1;");
        sel.integer(1, *player_id);
        if (sel.row()) f.discoveries = sel.column_int(0);
    }
    {
        sqlx::Stmt sel(db_, "SELECT COUNT(*) FROM gather_cooldowns WHERE player_id = ?1;");
        sel.integer(1, *player_id);
        if (sel.row()) f.cooldowns = sel.column_int(0);
    }
    {
        sqlx::Stmt sel(db_, "SELECT COUNT(*) FROM paintings WHERE player_id = ?1;");
        sel.integer(1, *player_id);
        if (sel.row()) f.paintings = sel.column_int(0);
    }
    return f;
}

StudioStore::ReapResult StudioStore::reap_orphan(const std::string& username) {
    ReapResult r;
    auto player_id = find_player_id(username);
    if (!player_id) return r; // nothing to reap
    sqlx::Tx tx(db_);
    {
        sqlx::Stmt del(db_, "DELETE FROM batches WHERE player_id = ?1;");
        del.integer(1, *player_id); del.done();
        r.batches = sqlite3_changes(db_);
    }
    {
        sqlx::Stmt del(db_, "DELETE FROM discoveries WHERE player_id = ?1;");
        del.integer(1, *player_id); del.done();
        r.discoveries = sqlite3_changes(db_);
    }
    {
        sqlx::Stmt del(db_, "DELETE FROM gather_cooldowns WHERE player_id = ?1;");
        del.integer(1, *player_id); del.done();
        r.cooldowns = sqlite3_changes(db_);
    }
    {
        sqlx::Stmt del(db_, "DELETE FROM paintings WHERE player_id = ?1;");
        del.integer(1, *player_id); del.done();
        r.paintings = sqlite3_changes(db_);
    }
    tx.commit();
    return r;
}

} // namespace commons
