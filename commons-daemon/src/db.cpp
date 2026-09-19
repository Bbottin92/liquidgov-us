#include "db.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace commons {

Database::Database(const std::string& path) {
    // WAL per sibling.md: concurrent read/write without the writer
    // blocking every reader on a single request.
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        std::string msg = db_ ? sqlite3_errmsg(db_) : "sqlite3_open failed";
        if (db_) sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("Database: failed to open '" + path + "': " + msg);
    }
    exec("PRAGMA journal_mode = WAL;");
    exec("PRAGMA foreign_keys = ON;");
}

Database::~Database() {
    if (db_) sqlite3_close(db_);
}

void Database::exec(const std::string& sql) {
    char* err = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "unknown sqlite error";
        sqlite3_free(err);
        throw std::runtime_error("Database::exec failed: " + msg);
    }
}

void Database::exec_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Database::exec_file: cannot open " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    exec(ss.str());
}

bool Database::table_exists(const std::string& name) {
    sqlite3_stmt* st = nullptr;
    bool found = false;
    const char* sql = "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?1;";
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, name.c_str(), -1, SQLITE_TRANSIENT);
        found = sqlite3_step(st) == SQLITE_ROW;
    }
    sqlite3_finalize(st);
    return found;
}

bool Database::table_is_empty(const std::string& name) {
    sqlite3_stmt* st = nullptr;
    bool empty = true;
    std::string sql = "SELECT 1 FROM " + name + " LIMIT 1;"; // name is a fixed literal at call sites, never user input
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &st, nullptr) == SQLITE_OK) {
        empty = sqlite3_step(st) != SQLITE_ROW;
    }
    sqlite3_finalize(st);
    return empty;
}

void Database::migrate(const std::string& schema_path, const std::string& seed_path) {
    if (!table_exists("items")) {
        exec_file(schema_path);
    }
    if (table_is_empty("items")) {
        exec_file(seed_path);
    }
}

void Database::ensure_inventory_recency() {
    sqlite3_stmt* st = nullptr;
    bool has_column = false;
    if (sqlite3_prepare_v2(db_, "SELECT 1 FROM pragma_table_info('inventory') WHERE name = 'updated_at';",
                           -1, &st, nullptr) == SQLITE_OK) {
        has_column = sqlite3_step(st) == SQLITE_ROW;
    }
    sqlite3_finalize(st);
    if (!has_column) exec("ALTER TABLE inventory ADD COLUMN updated_at INTEGER NOT NULL DEFAULT 0;");
}

bool Database::backup_to(const std::string& dest) {
    sqlite3* out = nullptr;
    if (sqlite3_open(dest.c_str(), &out) != SQLITE_OK) {
        if (out) sqlite3_close(out);
        std::remove(dest.c_str());
        return false;
    }
    bool ok = false;
    if (sqlite3_backup* b = sqlite3_backup_init(out, "main", db_, "main")) {
        int rc = sqlite3_backup_step(b, -1);
        sqlite3_backup_finish(b);
        ok = (rc == SQLITE_DONE);
    }
    if (ok) {
        sqlite3_stmt* st = nullptr;
        ok = false;
        if (sqlite3_prepare_v2(out, "PRAGMA integrity_check;", -1, &st, nullptr) == SQLITE_OK) {
            if (sqlite3_step(st) == SQLITE_ROW) {
                const unsigned char* v = sqlite3_column_text(st, 0);
                ok = v && std::string(reinterpret_cast<const char*>(v)) == "ok";
            }
        }
        sqlite3_finalize(st);
    }
    sqlite3_close(out);
    if (!ok) std::remove(dest.c_str());
    return ok;
}

void Database::ensure_visit_policy() {
    sqlite3_stmt* st = nullptr;
    bool has_column = false;
    if (sqlite3_prepare_v2(db_, "SELECT 1 FROM pragma_table_info('players') WHERE name = 'visit_policy';",
                           -1, &st, nullptr) == SQLITE_OK) {
        has_column = sqlite3_step(st) == SQLITE_ROW;
    }
    sqlite3_finalize(st);
    if (!has_column) exec("ALTER TABLE players ADD COLUMN visit_policy TEXT NOT NULL DEFAULT 'friends';");
}

} // namespace commons
