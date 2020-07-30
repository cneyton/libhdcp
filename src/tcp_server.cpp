#include "tcp_server.h"

using namespace hdcp;

TcpServer::TcpServer(common::Logger logger, uint16_t port):
    common::Log(logger),
    io_context_(), socket_(io_context_),
    acceptor_(io_context_, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
    write_queue_(max_queue_size), read_queue_(max_queue_size)
{
    write_buf_.reserve(max_transfer_size);
    acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
}

TcpServer::~TcpServer()
{
    stop();
}

void TcpServer::stop()
{
    common::Thread::stop();
    close();
    if (joinable())
        join();
}

void TcpServer::start()
{
    if (is_running())
        return;
    open();
    common::Thread::start(0);
}

void TcpServer::open()
{
    acceptor_.async_accept(socket_,
        [this](const boost::system::error_code& ec)
        {
            if (!ec) {
                log_debug(logger_, "connection accepted");
                do_read();
            }
        });
}

void TcpServer::close()
{
    boost::asio::post(io_context_,
        [this] ()
        {
            std::string s;
            // empty queues
            while (read_queue_.pop()) {}
            while (write_queue_.try_dequeue(s)) {}
            // close socket
            socket_.close();
        });
}

void TcpServer::write(const std::string& buf)
{
    write(std::string(buf));
}

void TcpServer::write(std::string&& buf)
{
    if (!is_running())
        throw hdcp::transport_error("not allowed to write when transport is stopped");

    boost::asio::post(io_context_,
                      [this, buf] ()
                      {
                          if (!write_queue_.try_enqueue(buf))
                              throw hdcp::transport_error("write queue full");
                          if (!write_in_progress_)
                              do_write();
                      });
}

bool TcpServer::read(std::string& buf)
{
    if (!is_running())
        throw hdcp::transport_error("not allowed to read when transport is stopped");

    return read_queue_.wait_dequeue_timed(buf, time_base_ms);
}

void TcpServer::run()
{
    while (is_running()) {
        try {
            io_context_.run();
        } catch (std::exception& e) {
            log_error(logger_, e.what());
        }
    }
}

void TcpServer::do_read()
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

void TcpServer::do_write()
{
    if (!write_queue_.try_dequeue(write_buf_))
        return;
    write_in_progress_ = true;
    boost::asio::async_write(socket_, boost::asio::buffer(write_buf_),
        [this](const boost::system::error_code& ec, size_t /* length */)
        {
            if (!ec) {
                if (write_queue_.size_approx() > 0)
                    do_write();
                else
                    write_in_progress_ = false;
            } else {
                throw asio_error(ec);
            }
        });
}
