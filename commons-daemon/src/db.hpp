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
