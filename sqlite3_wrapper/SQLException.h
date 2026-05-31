//
// Created by Miroslav Baudys on 05/03/2018.
//

#ifndef SQLITE_SERVER_SQLEXCEPTION_H
#define SQLITE_SERVER_SQLEXCEPTION_H

#include <string>

class SQLException final : public std::exception {
public:
    explicit SQLException(const int code, std::string what) : m_code(code), m_what(std::move(what)) {
    }

    auto code() const noexcept { return m_code; }

    const char *what() const noexcept override { return m_what.c_str(); }

private:
    const int m_code;
    const std::string m_what;
};

#endif
