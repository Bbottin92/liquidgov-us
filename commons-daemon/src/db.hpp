#pragma once
#include <string>
#include "sqlite3.h"

namespace commons {

// Owns the one SQLite connection. schema.sql / seed.sql are executed as
// whole-file statements (no bound params -- these are fixed local files,
// never user input) and are idempotent: migrate() is safe to call on
// every daemon startup, whether the DB file already exists or not.
//
// sibling.md: "Never rsync the live DB." This class doesn't touch deploy
// concerns at all -- it just opens whatever path it's given.
class Database {
public:
    explicit Database(const std::string& path);
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    sqlite3* handle() const { return db_; }

    void migrate(const std::string& schema_path, const std::string& seed_path);

    // migrate() only ever runs schema.sql on a brand-new DB, so a column
    // added to an existing table later needs its own idempotent step:
    // inventory.updated_at (bound inventory). No-op when the column is
    // already there (fresh DBs get it from schema.sql itself).
    void ensure_inventory_recency();
    // Same shape, for players.visit_policy (house friends/locked). Existing
    // rows take the DEFAULT, so every current house stays friends-only.
    void ensure_visit_policy();
    // Same shape again, for players.shirt_pigment_index (BUILD wear a pigment). Existing rows take
    // NULL, so every existing player keeps the default blue until they wear something themselves.
    void ensure_shirt_pigment();
    // The Studio (BUILD studio 1): items.pigment_hex, and recipes.duration_s /
    // .tier / .real_range_note / .history_note. Same shape again -- a fresh
    // database already has these from schema.sql; this only reaches an
    // existing one. All nullable, so every existing row (including the five
    // Lab-A lookups) is unaffected.
    void ensure_studio_columns();
    // CREATE TABLE IF NOT EXISTS for batches / discoveries / gather_cooldowns /
    // paintings -- a whole new table, not a column, reaching an existing
    // database. A fresh database already has them from schema.sql too, so
    // this is a no-op there.
    void ensure_studio_tables();
    // Catalog rows added after the first seed. seed.sql only runs when
    // `items` is empty, so production never sees new lookups without this.
    // catalog.sql is INSERT OR IGNORE; calling it every start is a no-op
    // once the five rows exist. No lime (pushback.md).
    void ensure_catalog(const std::string& catalog_path);
    // A consistent, online copy of the live database (the SQLite backup API,
    // never a file copy, so it is safe in WAL mode with the daemon running) to
    // `dest`, verified with integrity_check. False on any failure; a partial
    // file is removed. Used to take a backup before the orphan reap deletes rows.
    bool backup_to(const std::string& dest);
    void exec_file(const std::string& path);
    void exec(const std::string& sql);

private:
    sqlite3* db_ = nullptr;
    bool table_exists(const std::string& name);
    bool table_is_empty(const std::string& name);
};

} // namespace commons
