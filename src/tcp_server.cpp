#include "tcp_server.h"

using namespace hdcp;

TcpServer::TcpServer(common::Logger logger, uint16_t port):
    common::Log(logger),
    io_context_(), socket_(io_context_),
    acceptor_(io_context_, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
{
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
                read_header();
            }
        });
}

void TcpServer::close()
{
    boost::asio::post(io_context_,
        [this] ()
        {
            clear_queues();
            // close socket
            socket_.close();
        });
}

bool TcpServer::is_open()
{
    return socket_.is_open();
}

void TcpServer::write(Packet&& p)
{
    if (!is_open())
        throw hdcp::transport_error("can't write while transport is closed");

    boost::asio::post(io_context_,
        [this, p] ()
        {
            if (!write_queue_.try_enqueue(p))
                throw hdcp::transport_error("write queue full");
            if (!write_in_progress_)
                do_write();
        });
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

void TcpServer::read_header()
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

void TcpServer::read_payload()
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

void TcpServer::do_write()
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
