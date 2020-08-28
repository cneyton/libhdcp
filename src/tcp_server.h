#pragma once

#include <boost/asio.hpp>

#include "common/thread.h"
#include "common/concurrentqueue.h"
#include "common/readerwriterqueue.h"

#include "transport.h"
#include "hdcp/exception.h"
#include "packet.h"

namespace hdcp {

class TcpServer: public common::Log, private common::Thread, public Transport
{
public:
    TcpServer(common::Logger logger, uint16_t port);
    virtual ~TcpServer();

    virtual void write(Packet&&);
    virtual void stop();
    virtual void start();
    bool is_open() {return socket_.is_open();};

private:
    using common::Thread::start;

    // Non-copyable
    TcpServer(const TcpServer&) = delete;
    const TcpServer& operator=(const TcpServer&) = delete;

    boost::asio::io_context        io_context_;
    boost::asio::ip::tcp::socket   socket_;
    boost::asio::ip::tcp::acceptor acceptor_;

    Packet read_packet_;
    Packet write_packet_;
    std::atomic_bool write_in_progress_ = false;

    void do_write();
    void read_header();
    void read_payload();

    void open();
    void close();

    virtual void run();
};

} /* namespace hdcp */
