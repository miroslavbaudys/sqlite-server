//
// Created by Miroslav Baudys on 05/03/2018.
//

#ifndef SQLITE_SERVER_SQLDATABASE_H
#define SQLITE_SERVER_SQLDATABASE_H

#include <memory>
#include "SQLException.h"
#include "SQLStatement.h"
#include "../sqlite3/sqlite3.h"

class SQLDatabase final
{
public:
	explicit SQLDatabase(const std::string& path)
	{
		const auto rc = sqlite3_open(path.c_str(), &m_db);
		if (rc != SQLITE_OK)
		{
			throw SQLException(sqlite3_errcode(m_db), sqlite3_errmsg(m_db));
		}
	}

	~SQLDatabase()
	{
		sqlite3_close(m_db);
	}

	inline auto prepare(const std::string& query) const
	{
		return std::make_unique<SQLStatement>(m_db, query);
	}

private:
	sqlite3* m_db;
};

#endif
