//
// Created by Miroslav Baudys on 11/04/2018.
//

#include "Socket.h"
#include "Logger.h"

Socket::Socket(
    boost::asio::io_context &service,
    boost::asio::ip::tcp::socket socket
) : m_service(service),
    m_socket(std::move(socket)) {
    LogDebug("Socket create\n");
}

Socket::~Socket() {
    LogDebug("Socket destroy\n");
}
