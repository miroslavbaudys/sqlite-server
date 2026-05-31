//
// Created by Miroslav Baudys on 02/03/2018.
//

#ifndef SQLITE_SERVER_NETWORK_H
#define SQLITE_SERVER_NETWORK_H

#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include "ListenSocket.h"

template<class T>
class NetworkWorker final {
public:
    explicit NetworkWorker(boost::asio::ip::tcp::endpoint listen_endpoint, const uint16_t workers = 1) :
            m_listen_endpoint(std::move(listen_endpoint)),
            m_workers(workers) {
    }

    void run() {
        ListenSocket<T> listenSocket(m_service, m_listen_endpoint);
        boost::thread_group thread_pool;
        for (auto i = 0; i < m_workers; ++i) {
            thread_pool.create_thread([&]() { m_service.run(); });
        }
        thread_pool.join_all();
    }

    void shutdown() {
        m_service.stop();
    }

private:
    boost::asio::io_context m_service;
    boost::asio::ip::tcp::endpoint m_listen_endpoint;
    const uint16_t m_workers;
};

#endif /* SQLITE_SERVER_NETWORK_H */
