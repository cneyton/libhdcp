#include "tcp_client.h"

using namespace hdcp;

TcpClient::TcpClient(common::Logger logger, std::string_view host, std::string_view service):
    Log(logger),
    io_context_(), socket_(io_context_), host_(host), service_(service)
{
}

TcpClient::~TcpClient()
{
    stop();
}

void TcpClient::write(Packet&& p)
{
    if (!is_running())
        throw hdcp::transport_error("not allowed to write when transport is stopped");

    boost::asio::post(io_context_,
        [this, p] ()
        {
            if (!write_queue_.try_enqueue(p))
                throw hdcp::transport_error("write queue full");
            if (!write_in_progress_)
                do_write();
        });
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
               read_header();
           else
               throw asio_error(ec);
       });
}

void TcpClient::close()
{
    boost::asio::post(io_context_,
        [this] ()
        {
            clear_queues();
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

void TcpClient::read_header()
{
    auto h = read_packet_.header_view();
    boost::asio::async_read(socket_,
                            boost::asio::buffer(const_cast<char*>(h.data()), h.size()),
        [this](const boost::system::error_code& ec, size_t)
        {
            if (!ec) {

                read_payload();
            } else {
                throw asio_error(ec);
            }
        });
}

void TcpClient::read_payload()
{
    try {
        read_packet_.parse_header();
    } catch (hdcp::packet_error& e) {
        log_error(logger_, "{}", e.what());
        read_header();
    }

    auto pl = read_packet_.payload();
    boost::asio::async_read(socket_,
                            boost::asio::buffer(const_cast<char*>(pl.data()), pl.size()),
        [this](const boost::system::error_code& ec, size_t)
        {
            if (!ec) {
                try {
                    read_packet_.parse_payload();
                    if (!read_queue_.try_enqueue(read_packet_))
                        throw hdcp::transport_error("read queue full");
                } catch (std::exception& e) {
                    log_error(logger_, "{}", e.what());
                }
                read_header();
            } else {
                throw asio_error(ec);
            }
        });
}

void TcpClient::do_write()
{
    if (!write_queue_.try_dequeue(write_packet_))
        return;
    write_in_progress_ = true;
    boost::asio::async_write(socket_,
                             boost::asio::buffer(write_packet_.data(), write_packet_.size()),
        [this](const boost::system::error_code& ec, size_t)
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
