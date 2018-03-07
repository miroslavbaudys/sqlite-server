//
// Created by Miroslav Baudys on 05/03/2018.
//

#ifndef SQLITE_SERVER_SQLEXCEPTION_H
#define SQLITE_SERVER_SQLEXCEPTION_H

#include <string>
#include <utility>

class SQLException final
{
public:
	explicit SQLException(const int code, std::string what) : m_code(code), m_what(std::move(what))
	{
	}

	inline const auto code() const { return m_code; }
	inline const auto what() const { return m_what; }

private:
	const int m_code;
	const std::string m_what;
};

#endif