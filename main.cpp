#include <thread>
#include <boost/program_options.hpp>
#include <fmt/printf.h>
#include <iostream>
#include "Network.h"
#include "SQLiteSocket.h"
#include "Config.h"
#include "Logger.h"

namespace po = boost::program_options;

namespace {
    std::function<void(int)> shutdown_handler;
    void signal_handler(int signal)
    {
        shutdown_handler(signal);
    }
}

po::variables_map process_program_options(int argc, const char *argv[])
{
    po::options_description description("sqlite-server usage");
    description.add_options()
            ("help,h", "Display this help message")
            ("version,v", "Display the version number")
            ("config,c", po::value<std::string>(), "Config path")
            ("ip", po::value<std::string>()->default_value("localhost"), "Listen IP")
            ("port,p", po::value<uint16_t>()->default_value(3333), "Listen port")
            ("databases-folder,d", po::value<std::string>()->default_value("sqlite"), "Databases folder")
            ("workers,w", po::value<uint16_t>()->default_value((uint16_t)boost::thread::hardware_concurrency()), "Database workers")
            ("client-max-packet-size", po::value<uint32_t>()->default_value(16*1024*1024), "Max allowed packet size from client");

    po::variables_map vm;
    try {
        po::store(po::command_line_parser(argc, argv).options(description).run(), vm);
    } catch(const po::error& e) {
        Log.error("{}\n", e.what());
        exit(EXIT_FAILURE);
    }
    po::notify(vm);

    if(vm.count("help"))
    {
        fmt::print("{}\n", description);
        exit(EXIT_SUCCESS);
    }

    if(vm.count("version"))
    {
        fmt::print("GIT Branch: {}\nGIT Commit hash: {}\n", SQLITE_SERVER_GIT_BRANCH, SQLITE_SERVER_GIT_COMMIT_HASH);
        exit(EXIT_SUCCESS);
    }
    return vm;
}

int main(int argc, const char* argv[])
{
    //process cmd arguments
    const auto vm = process_program_options(argc, argv);
    try {
        Config::instance().init(vm);
        Log.debug("Config loaded with values:\n{}\n", Config::instance());
    } catch(const ConfigException& e) {
        Log.error("Config parse error:\n\t {}\n", e.what());
        exit(EXIT_FAILURE);
    }

    //register shutdown signal
    signal(SIGINT, signal_handler);

    //open sqlite socket and run forever
    auto sqliteSocketWorker = std::make_unique<NetworkWorker<SQLiteSocket>>(Config::instance().listen_endpoint, Config::instance().workers);
    shutdown_handler = [&](int signal) {
        Log.debug("Server shutdown...");
        sqliteSocketWorker->shutdown();
    };
    Log.debug("Server is running... [^C to stop]\n");
    sqliteSocketWorker->run();
    return 0;
}