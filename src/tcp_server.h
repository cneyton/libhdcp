#pragma once

#include <boost/asio.hpp>

#include "common/thread.h"
#include "common/readerwriterqueue.h"

#include "transport.h"
#include "hdcp/exception.h"

namespace hdcp
{

class TcpServer: public common::Log, private common::Thread, public Transport
{
public:
    TcpServer(common::Logger logger, uint16_t port);
    virtual ~TcpServer();

    virtual void write(const std::string& buf);
    virtual void write(std::string&& buf);
    virtual bool read(std::string& buf);
    virtual void stop();
    virtual void start();
    bool is_open() {return socket_.is_open();};

private:
    using common::Thread::start;

    // Non-copyable
    TcpServer(const TcpServer&) = delete;
    const TcpServer& operator=(const TcpServer&) = delete;

    std::recursive_mutex mutex_;

    boost::asio::io_context        io_context_;
    boost::asio::ip::tcp::socket   socket_;
    boost::asio::ip::tcp::acceptor acceptor_;

    std::array<char, max_transfer_size> read_buf_ = {0};
    common::ReaderWriterQueue<std::string>         write_queue_;
    common::BlockingReaderWriterQueue<std::string> read_queue_;

    void do_read();
    void do_write();

    void open();
    void close();

    virtual void run();
};

} /* namespace hdcp */
