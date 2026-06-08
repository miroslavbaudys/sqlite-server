//
// Created by Miroslav Baudys on 02/03/2018.
//

#ifndef SQLITE_SERVER_LISTENSOCKET_H
#define SQLITE_SERVER_LISTENSOCKET_H

#include <boost/asio.hpp>
#include "Config.h"
#include "Logger.h"

template<class T>
class ListenSocket final {
public:
    explicit ListenSocket(
        boost::asio::io_context &service,
        const boost::asio::ip::tcp::endpoint &listen_endpoint
    ) : m_service(service),
        m_acceptor(service, listen_endpoint),
        m_socket(service) {
        do_accept();
    }

private:
    void do_accept() {
        m_acceptor.async_accept(m_socket, [this](const boost::system::error_code &ec) {
            //Check whether the server was stopped by a signal before this
            //completion handler had a chance to run.
            if (!m_acceptor.is_open()) {
                return;
            }

            if (!ec) {
                // Reject clients outside the configured whitelist before building any
                // per-connection state: close the socket and move on to the next accept.
                boost::system::error_code addr_ec;
                const auto remote = m_socket.remote_endpoint(addr_ec);
                if (!addr_ec && Config::instance().is_ip_allowed(remote.address())) {
                    std::make_shared<T>(m_service, std::move(m_socket))->do_read();
                } else {
                    LogError("Connection rejected, not in ip whitelist: {}\n",
                             addr_ec ? std::string("unknown") : remote.address().to_string());
                    m_socket.close(addr_ec);
                }
            }

            do_accept();
        });
    }

private:
    boost::asio::io_context &m_service;
    boost::asio::ip::tcp::acceptor m_acceptor;
    boost::asio::ip::tcp::socket m_socket;
};

#endif /* SQLITE_SERVER_LISTENSOCKET_H */
