//
// Created by Miroslav Baudys on 02/03/2018.
//

#ifndef SQLITE_SERVER_SOCKET_H
#define SQLITE_SERVER_SOCKET_H

#include <memory>
#include <boost/asio.hpp>

class Socket
{
public:
    explicit Socket(boost::asio::io_service& service, boost::asio::ip::tcp::socket socket);
	virtual ~Socket();

    virtual void do_read() = 0;
    
protected:
    boost::asio::io_service&       m_service;
    boost::asio::ip::tcp::socket   m_socket;
};

#endif /* SQLITE_SERVER_SOCKET_H */
