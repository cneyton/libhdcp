#include "tcp_server.h"

namespace hdcp {
namespace transport {
namespace tcp {

Server::Server(common::Logger logger, uint16_t port):
    common::Log(logger),
    io_context_(), socket_(io_context_),
    acceptor_(io_context_, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
{
    acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
}

Server::~Server()
{
    stop();
}

void Server::stop()
{
    if (!is_running())
        return;
    log_debug(logger_, "stopping transport...");
    common::Thread::stop();
    close();
    if (joinable())
        join();
    log_debug(logger_, "transport stopped");
}

void Server::start()
{
    if (is_running())
        return;
    log_debug(logger_, "starting transport...");
    open();
    common::Thread::start(0);
    log_debug(logger_, "transport started");
}

void Server::open()
{
    if (is_open())
        return;
    log_debug(logger_, "opening transport, begin accepting connection...");
    acceptor_.async_accept(socket_,
        [this](const boost::system::error_code& ec)
        {
            if (!ec) {
                log_debug(logger_, "connection accepted, transport opened");
                read_header();
            } else {
                throw asio_error(ec);
            }
        });
}

void Server::close()
{
    if (!is_open())
        return;
    log_debug(logger_, "closing transport...");
    boost::asio::post(io_context_,
        [this] ()
        {
            socket_.close();
            log_debug(logger_, "transport closed");
        });
}

bool Server::is_open()
{
    return socket_.is_open();
}

void Server::write(Packet&& p)
{
    if (!is_open())
        throw transport_error("can't write while transport is closed",
                              transport_error::Code::not_permitted);

    boost::asio::post(io_context_,
        [this, p] ()
        {
            if (!write_queue_.try_enqueue(p))
                throw transport_error("write queue full",
                                      transport_error::Code::write_queue_full);
            if (!write_in_progress_)
                do_write();
        });
}

void Server::run()
{
    while (is_running()) {
        try {
            io_context_.run();
            break; // run exited normally
        } catch (std::exception& e) {
            log_error(logger_, e.what());
            if (error_cb_)
                error_cb_(std::current_exception());
        }
    }
}

void Server::read_header()
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

void Server::read_payload()
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
                        throw transport_error("read queue full",
                                              transport_error::Code::read_queue_full);
                } catch (std::exception& e) {
                    log_error(logger_, "{}", e.what());
                }
                read_header();
            } else {
                throw asio_error(ec);
            }
        });
}

void Server::do_write()
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

} /* namespace tcp  */
} /* namespace transport  */
} /* namespace hdcp */
