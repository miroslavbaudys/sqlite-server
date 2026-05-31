//
// Created by Miroslav Baudys on 06/03/2018.
//

#ifndef SQLITE_SERVER_LOGGER_H
#define SQLITE_SERVER_LOGGER_H

#include <fmt/core.h>
#include <fmt/chrono.h>

/*
 * 11:48:17 [level] [source]: [message]
 */
class Logger final {
private:
    Logger() = default;

public:
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    static Logger &instance() {
        static Logger logger;
        return logger;
    }

    template<typename... Args>
    void debug(const char *format, const Args &... args) {
        fmt::print("{} [DEBUG] ", get_formatted_time());
        fmt::print(format, args...);
    }

    template<typename... Args>
    void warning(const char *format, const Args &... args) {
        fmt::print("{} [WARNING] ", get_formatted_time());
        fmt::print(format, args...);
    }

    template<typename... Args>
    void error(const char *format, const Args &... args) {
        fmt::print("{} [ERROR] ", get_formatted_time());
        fmt::print(format, args...);
    }

private:
    static inline std::string get_formatted_time() {
        auto now = std::chrono::system_clock::now();
        return fmt::format("{}", now);
    }
};

#ifndef NDEBUG
#define LogDebug(...) Logger::instance().debug(__VA_ARGS__)
#else
#define LogDebug(...) ((void)0)
#endif

#define LogError(...) Logger::instance().error(__VA_ARGS__)

#define Log Logger::instance()

#endif //SQLITE_SERVER_LOGGER_H
