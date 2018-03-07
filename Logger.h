//
// Created by Miroslav Baudys on 06/03/2018.
//

#ifndef SQLITE_SERVER_LOGGER_H
#define SQLITE_SERVER_LOGGER_H

#include <fmt/format.h>

class Logger final
{
private:
    Logger() = default;

public:
    static Logger& instance()
    {
        static Logger logger;
        return logger;
    }

    template <typename... Args>
    void debug(const char *format, const Args & ... args)
    {
        fmt::print("Debug: ");
        fmt::print(format, args...);
    }

    template <typename... Args>
    void warning(const char *format, const Args & ... args)
    {
        fmt::print("Warning: ");
        fmt::print(format, args...);
    }

    template <typename... Args>
    void error(const char *format, const Args & ... args)
    {
        fmt::print("Error: ");
        fmt::print(format, args...);
    }
};

#define Log Logger::instance()

#endif //SQLITE_SERVER_LOGGER_H
