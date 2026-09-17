#include "db.hpp"

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

} // namespace commons
