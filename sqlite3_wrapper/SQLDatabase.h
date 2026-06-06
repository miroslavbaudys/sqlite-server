//
// Created by Miroslav Baudys on 05/03/2018.
//

#ifndef SQLITE_SERVER_SQLDATABASE_H
#define SQLITE_SERVER_SQLDATABASE_H

#include <memory>
#include "SQLException.h"
#include "SQLStatement.h"

class SQLDatabase final {
public:
    explicit SQLDatabase(const std::string &path) : m_db(nullptr) {
        const auto rc = sqlite3_open(path.c_str(), &m_db);
        if (rc != SQLITE_OK) {
            // sqlite3_open allocates the handle even on failure, so it must be released
            // before we abandon construction (the destructor will not run after a throw).
            SQLException ex(sqlite3_errcode(m_db), sqlite3_errmsg(m_db));
            sqlite3_close(m_db);
            throw ex;
        }
    }

    ~SQLDatabase() {
        sqlite3_close(m_db);
    }

    [[nodiscard]] auto prepare(const std::string &query) const {
        return std::make_unique<SQLStatement>(m_db, query);
    }

private:
    sqlite3 *m_db;
};

#endif
