//
// Created by Miroslav Baudys on 02/03/2018.
//

#ifndef SQLITE_SERVER_SQLITESOCKET_H
#define SQLITE_SERVER_SQLITESOCKET_H

#include <queue>
#include "Socket.h"
#include "IRequestHandler.h"

class SQLiteSocket final : public Socket
{
    using OutPackets = std::queue<std::unique_ptr<IResponse>>;

public:
    explicit SQLiteSocket(boost::asio::io_service& service, boost::asio::ip::tcp::socket socket);

    //MARK: Socket
    void do_read() override;

private:
    void send_response(std::unique_ptr<IResponse> response);
    void do_write(const std::unique_ptr<IResponse>& response);

private:
    uint32_t							m_packet_size;
    std::string							m_request;
    std::unique_ptr<IRequestHandler>	m_handler;
    OutPackets							m_out_packets;
};


#endif //SQLITE_SERVER_SQLITESOCKET_H
