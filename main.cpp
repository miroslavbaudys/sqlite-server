#include <thread>
#include <fmt/printf.h>
#include "Network.h"
#include "SQLiteSocket.h"

namespace {
    std::function<void(int)> shutdown_handler;
    void signal_handler(int signal)
    {
        shutdown_handler(signal);
    }
}

int main()
{
    //register shutdown signal
    signal(SIGINT, signal_handler);

    //open sqlite socket and run forever
    auto sqliteSocketWorker = std::make_unique<NetworkWorker<SQLiteSocket>>(3333, 4);
    shutdown_handler = [&](int signal) {
        fmt::print("Server shutdown...\n");
        sqliteSocketWorker->shutdown();
    };
    sqliteSocketWorker->run();
    return 0;
}