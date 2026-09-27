#pragma once
// Small RAII wrappers so every sqlite3_prepare/bind/step/COMMIT result is
// actually looked at (BUILD rest slice 2, GROK_REST_2.md: "fail the request,
// log, do not abort() the daemon"). Header-only on purpose -- no CMake change.
//
// A failure logs the real sqlite message to stderr (the journal) and throws
// DbError with a generic message. HttpServer::dispatch already turns any
// std::exception from a handler into a 500 and keeps serving, and the periodic
// hook already catches and logs, so one bad statement fails one request, never
// the process.
#include <iostream>
#include <stdexcept>
#include <string>

#include "sqlite3.h"

namespace commons::sqlx {

struct DbError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

[[noreturn]] inline void fail(sqlite3* db, const char* what) {
    std::cerr << "commons-daemon: sqlite " << what << " failed: " << sqlite3_errmsg(db)
              << " (code " << sqlite3_errcode(db) << ")\n";
    throw DbError("database error"); // generic: the detail stays in the log, not in a response
}

// One prepared statement. Finalized on scope exit, including when a step throws.
class Stmt {
public:
    Stmt(sqlite3* db, const char* sql) : db_(db) {
        if (sqlite3_prepare_v2(db_, sql, -1, &st_, nullptr) != SQLITE_OK) {
            st_ = nullptr;
            fail(db_, "prepare");
        }
    }
    ~Stmt() { sqlite3_finalize(st_); } // finalize(nullptr) is a no-op
    Stmt(const Stmt&) = delete;
    Stmt& operator=(const Stmt&) = delete;

    Stmt& text(int i, const std::string& v) {
        if (sqlite3_bind_text(st_, i, v.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) fail(db_, "bind");
        return *this;
    }
    Stmt& integer(int i, int v) {
        if (sqlite3_bind_int(st_, i, v) != SQLITE_OK) fail(db_, "bind");
        return *this;
    }
    Stmt& integer64(int i, long long v) {
        if (sqlite3_bind_int64(st_, i, v) != SQLITE_OK) fail(db_, "bind");
        return *this;
    }

    // Step a read: true = a row is ready, false = no (more) rows. Any other
    // result (BUSY, IO, CORRUPT, ...) throws.
    bool row() {
        int rc = sqlite3_step(st_);
        if (rc == SQLITE_ROW) return true;
        if (rc == SQLITE_DONE) return false;
        fail(db_, "step");
    }
    // Step a write to completion; anything but DONE throws.
    void done() {
        if (sqlite3_step(st_) != SQLITE_DONE) fail(db_, "step");
    }

    sqlite3_stmt* get() const { return st_; }
    int column_int(int i) const { return sqlite3_column_int(st_, i); }
    // A unix-ms timestamp already exceeds INT32_MAX (2026 is ~1.77e12): anything stored as
    // milliseconds (started_ms, ready_ms, next_ms, first_ms, updated_at) must be read with this, not
    // column_int, or it silently truncates.
    long long column_int64(int i) const { return sqlite3_column_int64(st_, i); }
    bool column_null(int i) const { return sqlite3_column_type(st_, i) == SQLITE_NULL; }
    std::string column_text(int i) const {
        const unsigned char* v = sqlite3_column_text(st_, i);
        return v ? std::string(reinterpret_cast<const char*>(v)) : std::string();
    }

private:
    sqlite3* db_;
    sqlite3_stmt* st_ = nullptr;
};

// BEGIN ... COMMIT, rolled back automatically if the scope exits without a
// successful commit() (an exception in between included), so a failed statement
// can never leave a half-applied multi-row write or an open transaction.
class Tx {
public:
    explicit Tx(sqlite3* db) : db_(db) {
        if (sqlite3_exec(db_, "BEGIN;", nullptr, nullptr, nullptr) != SQLITE_OK) fail(db_, "BEGIN");
        open_ = true;
    }
    ~Tx() {
        if (open_) sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    }
    Tx(const Tx&) = delete;
    Tx& operator=(const Tx&) = delete;

    void commit() {
        if (sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK) fail(db_, "COMMIT"); // dtor rolls back
        open_ = false;
    }

private:
    sqlite3* db_;
    bool open_ = false;
};

} // namespace commons::sqlx
