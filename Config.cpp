//
// Created by Miroslav Baudys on 06/03/2018.
//

#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <boost/algorithm/string/trim.hpp>
#include "Config.h"

using json = nlohmann::json;

namespace {
    // Splits a comma separated list, trimming whitespace and dropping empty items.
    std::vector<std::string> split_csv(const std::string &s) {
        std::vector<std::string> out;
        std::stringstream ss(s);
        std::string item;
        while (std::getline(ss, item, ',')) {
            boost::algorithm::trim(item);
            if (!item.empty()) {
                out.push_back(item);
            }
        }
        return out;
    }
}

boost::asio::ip::tcp::endpoint resolve_endpoint(const std::string &listen_ip, const uint16_t listen_port) {
    boost::asio::io_context io_service;
    boost::asio::ip::tcp::resolver resolver(io_service);
    const auto endpoint_iterator = resolver.resolve(
        listen_ip,
        std::to_string(listen_port)
    );
    for (const auto &it: endpoint_iterator) {
        if (it.endpoint().address().is_v4()) {
            return it.endpoint();
        }
    }
    return {boost::asio::ip::make_address("127.0.0.1"), 3333};
}

boost::filesystem::path resolve_database_path(const std::string &path) {
    const auto databases_folder = boost::filesystem::path(boost::filesystem::system_complete(path));
    if (!boost::filesystem::exists(databases_folder)) {
        throw ConfigException(fmt::format("Databases folder does not exist in: {}", databases_folder.string()));
    }
    return databases_folder;
}

auto get_config_value(const json &j, const std::string &key) {
    if (j.find(key) == j.end()) {
        throw ConfigException(fmt::format("Missing key: {} in config", key));
    }
    return j[key];
}

void Config::init(const boost::program_options::variables_map &vm) {
    //load from config if specified or use cmd line arguments
    const auto config = vm.find("config");
    if (config != vm.end()) {
        const auto config_path = config->second.as<std::string>();
        if (!boost::filesystem::exists(config_path)) {
            throw ConfigException(fmt::format("Config file does not exist in: {}", config_path));
        }

        json j;
        try {
            j = json::parse(std::ifstream(config_path));
        } catch (const nlohmann::detail::parse_error &e) {
            throw ConfigException(fmt::format("Config parse error: {}", e.what()));
        }

        client_max_packet_size = get_config_value(j, "client_max_packet_size");
        workers = get_config_value(j, "workers");
        listen_endpoint = resolve_endpoint(get_config_value(j, "listen_ip"), get_config_value(j, "listen_port"));
        databases_folder = resolve_database_path(get_config_value(j, "databases_folder"));
        auth = j.value("auth", std::string{});
        if (j.find("ip_whitelist") != j.end()) {
            const auto &whitelist = j["ip_whitelist"];
            if (!whitelist.is_array()) {
                throw ConfigException("Config key ip_whitelist must be an array of CIDR strings");
            }
            for (const auto &item: whitelist) {
                if (!item.is_string()) {
                    throw ConfigException("Config key ip_whitelist must contain only strings");
                }
                add_whitelist_entry(item.get<std::string>());
            }
        }
    } else {
        client_max_packet_size = vm["client-max-packet-size"].as<uint32_t>();
        workers = vm["workers"].as<uint16_t>();
        listen_endpoint = resolve_endpoint(vm["ip"].as<std::string>(), vm["port"].as<uint16_t>());
        databases_folder = resolve_database_path(vm["databases-folder"].as<std::string>());
        auth = vm["auth"].as<std::string>();
        for (const auto &entry: split_csv(vm["ip-whitelist"].as<std::string>())) {
            add_whitelist_entry(entry);
        }
    }
}

void Config::add_whitelist_entry(const std::string &entry) {
    namespace ip = boost::asio::ip;
    boost::system::error_code ec;
    const bool is_v6 = entry.find(':') != std::string::npos;
    const bool has_prefix = entry.find('/') != std::string::npos;

    if (is_v6) {
        if (has_prefix) {
            const auto net = ip::make_network_v6(entry, ec);
            if (!ec) ip_whitelist_v6.push_back(net.canonical());
        } else {
            const auto addr = ip::make_address_v6(entry, ec);
            if (!ec) ip_whitelist_v6.emplace_back(addr, 128);
        }
    } else {
        if (has_prefix) {
            const auto net = ip::make_network_v4(entry, ec);
            if (!ec) ip_whitelist_v4.push_back(net.canonical());
        } else {
            const auto addr = ip::make_address_v4(entry, ec);
            if (!ec) ip_whitelist_v4.emplace_back(addr, 32);
        }
    }

    if (ec) {
        throw ConfigException(fmt::format("Invalid ip_whitelist entry: {}", entry));
    }
}

bool Config::is_ip_allowed(const boost::asio::ip::address &address) const {
    namespace ip = boost::asio::ip;
    if (ip_whitelist_v4.empty() && ip_whitelist_v6.empty()) {
        return true;
    }

    if (address.is_v4()) {
        const auto addr = address.to_v4();
        return std::any_of(
            ip_whitelist_v4.begin(),
            ip_whitelist_v4.end(),
            [&](const auto &net) {
                return ip::network_v4(addr, net.prefix_length()).canonical() == net;
            }
        );
    }

    const auto addr = address.to_v6();
    return std::any_of(
        ip_whitelist_v6.begin(),
        ip_whitelist_v6.end(),
        [&](const auto &net) {
            return ip::network_v6(addr, net.prefix_length()).canonical() == net;
        }
    );
}

std::string Config::ip_whitelist_repr() const {
    if (ip_whitelist_v4.empty() && ip_whitelist_v6.empty()) {
        return "(any)";
    }
    std::string out;
    for (const auto &net: ip_whitelist_v4) {
        if (!out.empty()) out += ", ";
        out += net.to_string();
    }
    for (const auto &net: ip_whitelist_v6) {
        if (!out.empty()) out += ", ";
        out += net.to_string();
    }
    return out;
}
