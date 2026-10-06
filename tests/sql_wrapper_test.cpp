//
// Tests of the sqlite3 wrapper: a failing statement must throw, and a write must wait for a lock
// held by another connection (busy_timeout) instead of failing - before both were fixed, a write
// that met another connection's write returned an empty "success" and was silently lost.
//
// Plain asserts, no framework: the executable exits non-zero on the first failed check (ctest).
//

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#include "../sqlite3_wrapper/SQLDatabase.h"

namespace {
    int failures = 0;

    void check(const bool condition, const std::string &what) {
        std::cout << (condition ? "  ok   " : "  FAIL ") << what << std::endl;
        if (!condition) {
            ++failures;
        }
    }

    // runs a statement to the end like RequestHandler does
    void exec(const SQLDatabase &db, const std::string &query) {
        const auto statement = db.prepare(query);
        while (statement->next_row()) {
        }
    }

    long long scalar(const SQLDatabase &db, const std::string &query) {
        const auto statement = db.prepare(query);
        return statement->next_row() ? statement->value_int64(0) : -1;
    }

    // a fresh database file in WAL mode (like the production databases)
    std::string temp_database(const std::string &name) {
        const auto dir = std::filesystem::temp_directory_path() / "sqlite-server-test";
        std::filesystem::create_directories(dir);
        const auto path = dir / name;
        for (const auto *suffix: {"", "-wal", "-shm", "-journal"}) {
            std::filesystem::remove(path.string() + suffix);
        }
        const SQLDatabase db(path.string());
        exec(db, "PRAGMA journal_mode=WAL");
        exec(db, "CREATE TABLE t (id INTEGER PRIMARY KEY, value TEXT)");
        return path.string();
    }

    void constraint_violation_throws() {
        std::cout << "constraint violation" << std::endl;
        const SQLDatabase db(temp_database("constraint.db"));
        exec(db, "INSERT INTO t VALUES (1, 'a')");
        try {
            exec(db, "INSERT INTO t VALUES (1, 'b')");
            check(false, "second insert with the same primary key throws");
        } catch (const SQLException &e) {
            check(e.code() == SQLITE_CONSTRAINT_PRIMARYKEY, "throws SQLITE_CONSTRAINT_PRIMARYKEY");
        }
        check(scalar(db, "SELECT COUNT(*) FROM t WHERE id = 1 AND value = 'a'") == 1 &&
              scalar(db, "SELECT COUNT(*) FROM t") == 1, "the first row is unchanged");
    }

    void write_waits_for_a_lock() {
        std::cout << "write waits for another connection's lock" << std::endl;
        const auto path = temp_database("busy_wait.db");
        const SQLDatabase holder(path);
        const SQLDatabase writer(path, 5000);

        exec(holder, "BEGIN IMMEDIATE");
        exec(holder, "INSERT INTO t VALUES (1, 'holder')");
        std::thread release([&holder] {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            exec(holder, "COMMIT");
        });

        const auto start = std::chrono::steady_clock::now();
        bool threw = false;
        try {
            exec(writer, "INSERT INTO t VALUES (2, 'writer')");
        } catch (const SQLException &) {
            threw = true;
        }
        const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        release.join();

        check(!threw, "the waiting insert succeeds");
        check(waited >= 150, "it waited for the lock (" + std::to_string(waited) + " ms)");
        check(scalar(writer, "SELECT COUNT(*) FROM t") == 2, "both rows are stored");
    }

    void lock_held_too_long_throws_busy() {
        std::cout << "lock held longer than the busy timeout" << std::endl;
        const auto path = temp_database("busy_timeout.db");
        const SQLDatabase holder(path);
        const SQLDatabase writer(path, 100);

        exec(holder, "BEGIN IMMEDIATE");
        exec(holder, "INSERT INTO t VALUES (1, 'holder')");
        try {
            exec(writer, "INSERT INTO t VALUES (2, 'writer')");
            check(false, "the insert throws instead of returning an empty success");
        } catch (const SQLException &e) {
            check((e.code() & 0xff) == SQLITE_BUSY, "throws SQLITE_BUSY");
        }
        exec(holder, "COMMIT");
        check(scalar(holder, "SELECT COUNT(*) FROM t WHERE id = 2") == 0, "the failed row is not stored");
    }
}

int main() {
    constraint_violation_throws();
    write_waits_for_a_lock();
    lock_held_too_long_throws_busy();
    std::cout << (failures == 0 ? "all passed" : std::to_string(failures) + " failed") << std::endl;
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
