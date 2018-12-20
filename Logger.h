//
// Created by Miroslav Baudys on 06/03/2018.
//

#ifndef SQLITE_SERVER_LOGGER_H
#define SQLITE_SERVER_LOGGER_H

#include <fmt/format.h>

/*
 * 11:48:17 [level] [source]: [message]
 */
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
        fmt::print("{} [DEBUG] ", get_formatted_time());
        fmt::print(format, args...);
    }

    template <typename... Args>
    void warning(const char *format, const Args & ... args)
    {
        fmt::print("{} [WARNING] ", get_formatted_time());
        fmt::print(format, args...);
    }

    template <typename... Args>
    void error(const char *format, const Args & ... args)
    {
        fmt::print("{} [ERROR] ", get_formatted_time());
        fmt::print(format, args...);
    }

private:
    inline const std::string get_formatted_time() const
    {
        time_t unixtime;
        struct tm time_info{};
        char buff[32];

        time(&unixtime);
        localtime_r(&unixtime, &time_info);
        strftime(buff, sizeof(buff), "%Y-%m-%d %H:%M:%S", &time_info);
        return std::string(buff);
    }
};

#define Log Logger::instance()

#endif //SQLITE_SERVER_LOGGER_H
