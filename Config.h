//
// Created by Miroslav Baudys on 06/03/2018.
//

#ifndef SQLITE_SERVER_CONFIG_H
#define SQLITE_SERVER_CONFIG_H

#include <string>
#include <vector>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio.hpp>
#include <fmt/ostream.h>

class ConfigException final : public std::exception {
public:
    explicit ConfigException(std::string what) : m_what(std::move(what)) {
    }

    [[nodiscard]] const char *what() const noexcept override { return m_what.c_str(); }

private:
    const std::string m_what;
};

class Config final {
private:
    Config() = default;

public:
    static Config &instance() {
        static Config config;
        return config;
    }

    void init(const boost::program_options::variables_map &vm);

    // Returns true if the address is permitted to connect. An empty whitelist
    // means the feature is disabled and every client is allowed.
    [[nodiscard]] bool is_ip_allowed(const boost::asio::ip::address &address) const;

    // Human-readable representation of the configured whitelist (for logging).
    [[nodiscard]] std::string ip_whitelist_repr() const;

    friend std::ostream &operator<<(std::ostream &os, const Config &c) {
        return os
               << "\tListen IP:                " << c.listen_endpoint.address().to_string() << std::endl
               << "\tListen port:              " << c.listen_endpoint.port() << std::endl
               << "\tWorkers:                  " << c.workers << std::endl
               << "\tDatabases folder:         " << c.databases_folder << std::endl
               << "\tClient max packet size:   " << c.client_max_packet_size << std::endl
               << "\tAuth:                     " << c.auth << std::endl
               << "\tIP whitelist:             " << c.ip_whitelist_repr() << std::endl;
    }

    uint32_t client_max_packet_size{};
    uint16_t workers{};
    boost::asio::ip::tcp::endpoint listen_endpoint;
    boost::filesystem::path databases_folder;
    std::string auth{};
    std::vector<boost::asio::ip::network_v4> ip_whitelist_v4{};
    std::vector<boost::asio::ip::network_v6> ip_whitelist_v6{};

private:
    // Parses a single CIDR ("10.0.0.0/8") or bare address ("127.0.0.1", treated
    // as a /32 or /128) and appends it to the matching whitelist.
    void add_whitelist_entry(const std::string &entry);
};

template<>
struct fmt::formatter<Config> : ostream_formatter {
};

#endif //SQLITE_SERVER_CONFIG_H
