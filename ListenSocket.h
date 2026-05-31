//
// Created by Miroslav Baudys on 02/03/2018.
//

#ifndef SQLITE_SERVER_LISTENSOCKET_H
#define SQLITE_SERVER_LISTENSOCKET_H

#include <boost/asio.hpp>

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
                std::make_shared<T>(m_service, std::move(m_socket))->do_read();
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
