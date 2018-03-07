//
// Created by Miroslav Baudys on 06/03/2018.
//

#include <nlohmann/json.hpp>
#include <fmt/printf.h>
#include "Config.h"

using json = nlohmann::json;

boost::asio::ip::tcp::endpoint resolve_endpoint(const std::string& listen_ip, const uint16_t listen_port)
{
    boost::asio::io_service io_service;
    boost::asio::ip::tcp::resolver resolver(io_service);
    const auto endpoint_iterator = resolver.resolve(boost::asio::ip::tcp::resolver::query(listen_ip, std::to_string(listen_port)));
    for(const auto& it : endpoint_iterator)
    {
        if(it.endpoint().address().is_v4())
        {
            return it.endpoint();
        }
    }
    return boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 3333);
}

boost::filesystem::path resolve_database_path(const std::string& path)
{
    const auto databases_folder = boost::filesystem::path(boost::filesystem::system_complete(path));
    if(!boost::filesystem::exists(databases_folder))
    {
        throw ConfigException(fmt::format("Databases folder does not exist in: {}", databases_folder));
    }
    return databases_folder;
}

auto get_config_value(const json& j, const std::string& key)
{
    if(j.find(key) == j.end())
    {
        throw ConfigException(fmt::format("Missing key: {} in config", key));
    }
    return j[key];
}

void Config::init(const boost::program_options::variables_map& vm)
{
    //load from config if specified or use cmd line arguments
    const auto config = vm.find("config");
    if(config != vm.end())
    {
        const auto config_path = config->second.as<std::string>();
        if(!boost::filesystem::exists(config_path))
        {
            throw ConfigException(fmt::format("Config file does not exist in: {}", config_path));
        }

        json j;
        try {
            j = json::parse(std::ifstream(config_path));
        }catch(const nlohmann::detail::parse_error& e) {
            throw ConfigException(fmt::format("Config parse error: {}", e.what()));
        }

        client_max_packet_size  = get_config_value(j, "client_max_packet_size");
        workers                 = get_config_value(j, "workers");
        listen_endpoint         = resolve_endpoint(get_config_value(j, "listen_ip"), get_config_value(j, "listen_port"));
        databases_folder        = resolve_database_path(get_config_value(j, "databases_folder"));
    }
    else
    {
        client_max_packet_size  = vm["client-max-packet-size"].as<uint32_t>();
        workers                 = vm["workers"].as<uint16_t>();
        listen_endpoint         = resolve_endpoint(vm["ip"].as<std::string>(), vm["port"].as<uint16_t>());
        databases_folder        = resolve_database_path(vm["databases-folder"].as<std::string>());
    }
}