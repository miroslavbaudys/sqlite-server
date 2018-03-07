//
// Created by Miroslav Baudys on 02/03/2018.
//

#include <fmt/printf.h>
#include "RequestHandler.h"
#include "Logger.h"
#include "Config.h"
#include "SQLiteSocket.h"

SQLiteSocket::SQLiteSocket(boost::asio::io_service& service,
                           boost::asio::ip::tcp::socket socket) :
	Socket(service, std::move(socket)),
	m_packet_size(0),
	m_handler(std::make_unique<RequestHandler>())
{

}

void SQLiteSocket::do_read()
{
    /*
     * Packet structure
     * 4 bytes data len
     * xx bytes data
     */
    auto self(shared_from_this());
    boost::asio::async_read(m_socket, boost::asio::buffer(&m_packet_size, sizeof(m_packet_size)), [this, self](boost::system::error_code ec, std::size_t length) {
        if(!ec)
        {
            if(m_packet_size > Config::instance().client_max_packet_size)
            {
                Log.error("Client max allowed packet size reached: {}\n", m_packet_size);
                return;
            }

            m_request.resize((size_t)m_packet_size);
            boost::asio::async_read(m_socket, boost::asio::buffer(m_request), [this, self](boost::system::error_code ec, std::size_t length) {
                if(!ec)
                {
                    Log.debug("Request - {}\n", m_request);
                    send_response(std::move(m_handler->handle_request(m_request)));
                    do_read();
                }
            });
        }
    });
}

void SQLiteSocket::send_response(std::unique_ptr<IResponse> response)
{
    Log.debug("Response - {}\n", response->data_repr());
    const auto write_in_progress = !m_out_packets.empty();
    m_out_packets.push(std::move(response));
    if(!write_in_progress)
    {
        do_write(m_out_packets.front());
    }
}

void SQLiteSocket::do_write(const std::unique_ptr<IResponse>& response)
{
    auto self(shared_from_this());
    boost::asio::async_write(m_socket, boost::asio::buffer(response->data()), [this, self](boost::system::error_code ec, std::size_t length)
    {
        if(!ec)
        {
            m_out_packets.pop();
            if(!m_out_packets.empty())
            {
                do_write(m_out_packets.front());
            }
        }
    });
}
