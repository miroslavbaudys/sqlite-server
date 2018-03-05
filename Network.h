//
// Created by Miroslav Baudys on 02/03/2018.
//

#ifndef SQLITE_SERVER_NETWORK_H
#define SQLITE_SERVER_NETWORK_H

#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>
#include "ListenSocket.h"

template<class T>
class NetworkWorker final
{
public:
    explicit NetworkWorker(const uint16_t listen_port, const uint16_t threads = 1) :
            m_listen_port(listen_port),
            m_threads(threads)
    {
    }
    
    void run()
    {
        ListenSocket<T> listenSocket(m_service, m_listen_port);
        boost::thread_group thread_pool;
        for(uint16_t i = 0;i < m_threads;++i)
        {
            thread_pool.create_thread(boost::bind(&boost::asio::io_service::run, &m_service));
        }
        thread_pool.join_all();
    }
    
    void shutdown()
    {
        m_service.stop();
    }
    
private:
    boost::asio::io_service m_service;
    const uint16_t          m_listen_port;
    const uint16_t          m_threads;
};

#endif /* SQLITE_SERVER_NETWORK_H */
