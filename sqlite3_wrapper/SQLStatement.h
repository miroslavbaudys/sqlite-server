//
// Created by Miroslav Baudys on 05/03/2018.
//

#ifndef SQLITE_SERVER_SQLSTATEMENT_H
#define SQLITE_SERVER_SQLSTATEMENT_H

#include "../sqlite3/sqlite3.h"
#include "SQLException.h"

class SQLStatement final {
public:
    explicit SQLStatement(sqlite3 *db, const std::string &query) : m_stmt(nullptr) {
        int rc;
        do {
            rc = sqlite3_prepare_v2(db, query.c_str(), (int) query.length(), &m_stmt, nullptr);
        } while (rc == SQLITE_BUSY);

        if (rc != SQLITE_OK) {
            throw SQLException(sqlite3_errcode(db), sqlite3_errmsg(db));
        }
    }

    ~SQLStatement() {
        sqlite3_clear_bindings(m_stmt);
        sqlite3_finalize(m_stmt);
    }

    inline auto next_row() const noexcept {
        return sqlite3_step(m_stmt) == SQLITE_ROW;
    }

    inline auto column_count() const noexcept {
        return sqlite3_column_count(m_stmt);
    }

    inline auto column_name(const int index) const noexcept {
        return sqlite3_column_name(m_stmt, index);
    }

    inline auto column_type(const int index) const noexcept {
        return sqlite3_column_type(m_stmt, index);
    }

    inline auto column_value(const int index) const noexcept {
        return sqlite3_column_value(m_stmt, index);
    }

    inline auto value_int64(sqlite3_value *value) const noexcept {
        return sqlite3_value_int64(value);
    }

    inline auto value_double(sqlite3_value *value) const noexcept {
        return sqlite3_value_double(value);
    }

    inline auto value_text(sqlite3_value *value) const noexcept {
        return (const char *) sqlite3_value_text(value);
    }

    inline auto value_bytes(sqlite3_value *value) const noexcept {
        return sqlite3_value_bytes(value);
    }

    inline auto value_blob(sqlite3_value *value) const noexcept {
        return (const char *) sqlite3_value_blob(value);
    }

private:
    sqlite3_stmt *m_stmt;
};

#endif
