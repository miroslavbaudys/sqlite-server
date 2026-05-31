//
// Created by Miroslav Baudys on 02/03/2018.
//

#include "RequestHandler.h"
#include "Logger.h"
#include "Config.h"
#include "SQLiteSocket.h"

SQLiteSocket::SQLiteSocket(
    boost::asio::io_context &service,
    boost::asio::ip::tcp::socket socket
) : Socket(service, std::move(socket)),
    m_packet_size(0),
    m_handler(std::make_unique<RequestHandler>()) {
    LogDebug("SQLiteSocket create\n");
}

SQLiteSocket::~SQLiteSocket() {
    LogDebug("SQLiteSocket destroy\n");
}

void SQLiteSocket::do_read() {
    /*
     * Packet structure
     * 4 bytes data len
     * xx bytes data
     */
    auto self(shared_from_this());
    boost::asio::async_read(
        m_socket,
        boost::asio::buffer(&m_packet_size, sizeof(m_packet_size)),
        [self](const boost::system::error_code &ec, std::size_t) {
            if (!ec) {
                if (self->m_packet_size > Config::instance().client_max_packet_size) {
                    LogError("Client max allowed packet size reached: {}\n", self->m_packet_size);
                    self->m_socket.close();
                    return;
                }

                self->m_request.resize(self->m_packet_size);
                boost::asio::async_read(
                    self->m_socket,
                    boost::asio::buffer(self->m_request),
                    [self](const boost::system::error_code &ec_inner, std::size_t) {
                        if (!ec_inner) {
                            LogDebug("{:<10}{}\n", "REQUEST:", self->m_request);
                            self->send_response(self->m_handler->handle_request(self->m_request));
                            self->do_read();
                        }
                    }
                );
            }
        }
    );
}

void SQLiteSocket::send_response(std::unique_ptr<IResponse> response) {
    LogDebug("{:<10}{}\n", "RESPONSE:", response->data_repr());
    const auto write_in_progress = !m_out_packets.empty();
    m_out_packets.push(std::move(response));
    if (!write_in_progress) {
        do_write(m_out_packets.front());
    }
}

void SQLiteSocket::do_write(const std::unique_ptr<IResponse> &response) {
    auto self(shared_from_this());
    boost::asio::async_write(
        m_socket,
        boost::asio::buffer(response->data()),
        [self](const boost::system::error_code &ec, std::size_t) {
            if (!ec) {
                self->m_out_packets.pop();
                if (!self->m_out_packets.empty()) {
                    self->do_write(self->m_out_packets.front());
                }
            }
        }
    );
}
