//
// Created by Miroslav Baudys on 06/03/2018.
//

#ifndef SQLITE_SERVER_CONFIG_H
#define SQLITE_SERVER_CONFIG_H

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

class ConfigException final : public std::exception
{
public:
    explicit ConfigException(std::string what) : m_what(std::move(what)) {}

    inline const char* what() const noexcept override { return m_what.c_str(); }

private:
    const std::string m_what;
};

class Config final
{
private:
    Config() = default;

public:
    static Config& instance()
    {
        static Config config;
        return config;
    }

    void init(const boost::program_options::variables_map& vm);

    friend std::ostream &operator<<(std::ostream &os, const Config &c)
    {
        return os
                << "\tListen IP:                " << c.listen_endpoint.address().to_string() << std::endl
                << "\tListen port:              " << c.listen_endpoint.port() << std::endl
                << "\tWorkers:                  " << c.workers << std::endl
                << "\tDatabases folder:         " << c.databases_folder << std::endl
                << "\tClient max packet size:   " << c.client_max_packet_size << std::endl;
    }

    uint32_t client_max_packet_size;
    uint16_t workers;
    boost::asio::ip::tcp::endpoint listen_endpoint;
    boost::filesystem::path databases_folder;
};

#endif //SQLITE_SERVER_CONFIG_H
