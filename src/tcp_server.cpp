#include "tcp_server.h"

namespace hdcp {
namespace transport {
namespace tcp {

Server::Server(common::Logger logger, uint16_t port):
    common::Log(logger),
    io_context_(), socket_(io_context_),
    acceptor_(io_context_, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
{
    acceptor_.set_option(boost::asio::socket_base::keep_alive(true));
    acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
}

Server::~Server()
{
    stop();
}

void Server::start()
{
    if (is_running())
        return;
    log_debug(logger_, "starting transport...");
    write_in_progress_ = false;
    open();
    common::Thread::start(true);
    log_debug(logger_, "transport started");
}

void Server::stop()
{
    if (!is_running())
        return;
    log_debug(logger_, "stopping transport...");
    close();
    common::Thread::stop();
    if (joinable())
        join();
    log_debug(logger_, "transport stopped");
}

void Server::open()
{
    if (is_open())
        return;
    log_debug(logger_, "opening transport, begin accepting connection...");
    acceptor_.async_accept(socket_,
        [this](const boost::system::error_code& ec)
        {
            errc_ = ec;
            if (!ec) {
                log_debug(logger_, "connection accepted, transport opened");
                read_header();
            } else {
                log_debug(logger_, "accept error: {}", ec.message());
            }
        });
}

void Server::close()
{
    log_debug(logger_, "closing transport...");
    if (is_open())
        socket_.shutdown(boost::asio::socket_base::shutdown_type::shutdown_both);
    acceptor_.cancel();
    socket_.close();
    log_debug(logger_, "transport closed");
}

bool Server::is_open()
{
    return socket_.is_open();
}

void Server::write(Packet&& p)
{
    if (!is_open())
        throw transport_error(Errc::write_while_closed);

    boost::asio::post(io_context_,
        [this, p] ()
        {
            if (!write_queue_.try_enqueue(p))
                throw transport_error(Errc::write_queue_full);
            if (!write_in_progress_)
                do_write();
        });
}

void Server::run()
{
    try {
        io_context_.restart();
        notify_running();
        while (is_running()) {
            try {
                io_context_.run();
                break; // run exited normally
            } catch (packet_error& e) {
                log_warn(logger_, e.what());
            } catch (transport_error& e) {
                // only read queue full can occur here -> discard packet
                log_warn(logger_, e.what());
            } catch (std::exception& e) {
                log_error(logger_, e.what());
                close();
                break;
            } catch (...) {
                close();
                break;
            }
        }
    } catch (std::exception& e) {
        log_error(logger_, e.what());
    } catch (...) {
        log_error(logger_, "error during thread execution");
    }
}

void Server::read_header()
{
    auto h = read_packet_.header_view();
    boost::asio::async_read(socket_,
                            boost::asio::buffer(const_cast<char*>(h.data()), h.size()),
        [this](const boost::system::error_code& ec, size_t len)
        {
            log_trace(logger_, "read {} bytes", len);
            errc_ = ec;
            if (!ec) {
                read_payload();
            } else if (ec == boost::asio::error::operation_aborted) {
                log_warn(logger_, "{}", ec.message());
            } else if (ec == boost::asio::error::eof) {
                log_info(logger_, "{}", ec.message());
                close();
                open();
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
        [this](const boost::system::error_code& ec, size_t len)
        {
            log_trace(logger_, "read {} bytes", len);
            errc_ = ec;
            if (!ec) {
                try {
                    read_packet_.parse_payload();
                    if (!read_queue_.try_enqueue(read_packet_))
                        throw transport_error(Errc::read_queue_full);
                } catch (std::exception& e) {
                    log_error(logger_, "{}", e.what());
                }
                read_header();
            } else if (ec == boost::asio::error::operation_aborted) {
                log_warn(logger_, "{}", ec.message());
            } else if (ec == boost::asio::error::eof) {
                log_info(logger_, "{}", ec.message());
                close();
                errc_ = ec;
                open();
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
        [this](const boost::system::error_code& ec, size_t len)
        {
            log_trace(logger_, "write {} bytes", len);
            errc_ = ec;
            if (!ec) {
                if (write_queue_.size_approx() > 0)
                    do_write();
                else
                    write_in_progress_ = false;
            } else if (ec == boost::asio::error::operation_aborted) {
                log_warn(logger_, "{}", ec.message());
                write_in_progress_ = false;
            } else if (ec == boost::asio::error::eof) {
                log_info(logger_, "{}", ec.message());
                close();
                errc_ = ec;
                open();
                write_in_progress_ = false;
            } else {
                throw asio_error(ec);
            }
        });
}

} /* namespace tcp  */
} /* namespace transport  */
} /* namespace hdcp */
