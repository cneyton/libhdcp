#pragma once

#include <boost/asio.hpp>

#include "common/thread.h"

#include "transport.h"
#include "hdcp/exception.h"

namespace hdcp {
namespace transport {
namespace tcp {

class Server: public common::Log, private common::Thread, public Transport
{
public:
    Server(common::Logger logger, uint16_t port);
    ~Server();

    void write(Packet&& p) override;
    void stop()    override;
    void start()   override;
    bool is_open() override;
    void open()    override;
    void close()   override;

private:
    using common::Thread::start;

    boost::asio::io_context        io_context_;
    boost::asio::ip::tcp::socket   socket_;
    boost::asio::ip::tcp::acceptor acceptor_;

    Packet read_packet_;
    Packet write_packet_;
    std::atomic_bool write_in_progress_ = false;

    void do_write();
    void read_header();
    void read_payload();

    void run() override;
};

} /* namespace tcp */
} /* namespace transport */
} /* namespace hdcp */
