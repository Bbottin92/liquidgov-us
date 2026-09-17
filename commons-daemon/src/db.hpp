#pragma once
#include <string>
#include "../third_party/sqlite3/sqlite3.h"

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
    void exec_file(const std::string& path);
    void exec(const std::string& sql);

private:
    sqlite3* db_ = nullptr;
    bool table_exists(const std::string& name);
    bool table_is_empty(const std::string& name);
};

} // namespace commons
