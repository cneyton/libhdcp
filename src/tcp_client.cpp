#include "tcp_client.h"

using namespace hdcp;

TcpClient::TcpClient(common::Logger logger, std::string_view host, std::string_view service):
    Log(logger),
    io_context_(), socket_(io_context_), host_(host), service_(service),
    write_queue_(max_queue_size), read_queue_(max_queue_size)
{
}

TcpClient::~TcpClient()
{
    stop();
}

void TcpClient::write(const std::string& buf)
{
    write(std::string(buf));
}

void TcpClient::write(std::string&& buf)
{
    if (!is_running())
        throw hdcp::transport_error("not allowed to write when transport is stopped");

    boost::asio::post(io_context_,
                      [this, buf] ()
                      {
                          bool write_in_progress = false;
                          if (write_queue_.peek())
                              write_in_progress = true;
                          if (!write_queue_.try_enqueue(buf))
                              throw hdcp::transport_error("write queue full");
                          if (!write_in_progress)
                              do_write();
                      });
}

bool TcpClient::read(std::string& buf)
{
    if (!is_running())
        throw hdcp::transport_error("not allowed to read when transport is stopped");

    return read_queue_.wait_dequeue_timed(buf, time_base_ms);
}

void TcpClient::stop()
{
    common::Thread::stop();
    close();
    if (joinable())
        join();
}

void TcpClient::start()
{
    if (is_running())
        return;
    open();
    common::Thread::start(0);
};

void TcpClient::open()
{
    boost::asio::ip::tcp::resolver resolver(io_context_);
    auto endpoints = resolver.resolve(host_, service_);
    boost::asio::async_connect(socket_, endpoints,
       [this](const boost::system::error_code& ec, const boost::asio::ip::tcp::endpoint&)
       {
           if (!ec)
               do_read();
           else
               throw asio_error(ec);
       });
}

void TcpClient::close()
{
    boost::asio::post(io_context_,
        [this] ()
        {
            // empty queues
            while (read_queue_.pop()) {}
            while (write_queue_.pop()) {}
            // close socket
            socket_.close();
        });
}

void TcpClient::run()
{
    while (is_running()) {
        try {
            io_context_.run();
        } catch (std::exception& e) {
            log_error(logger_, e.what());
        }
    }
}

void TcpClient::do_read()
{
    socket_.async_receive(boost::asio::buffer(read_buf_),
        [this](const boost::system::error_code& ec, size_t len)
        {
            if (!ec) {
                read_queue_.enqueue(std::string(read_buf_.begin(), len));
                do_read();
            } else {
                throw asio_error(ec);
            }
        });
}

void TcpClient::do_write()
{
    if (!write_queue_.peek())
        return;
    boost::asio::async_write(socket_, boost::asio::buffer(*write_queue_.peek()),
        [this](const boost::system::error_code& ec, size_t /* length */)
        {
            if (!ec) {
                if (!write_queue_.pop())
                    throw hdcp::transport_error("queue shouldn't be empty");
                if (write_queue_.peek())
                    do_write();
            } else {
                throw asio_error(ec);
            }
        });
}
