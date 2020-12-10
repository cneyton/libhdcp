#include "tcp_client.h"

namespace hdcp {
namespace transport {
namespace tcp {

Client::Client(common::Logger logger, std::string_view host, std::string_view service):
    Log(logger),
    io_context_(), socket_(io_context_),
    host_(host), service_(service)
{
}

Client::~Client()
{
    stop();
}

void Client::write(Packet&& p)
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

void Client::start()
{
    if (is_running())
        return;
    log_debug(logger_, "starting transport...");
    write_in_progress_ = false;
    open();
    common::Thread::start(true);
    log_debug(logger_, "transport started");
};

void Client::stop()
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

void Client::open()
{
    if (is_open())
        return;
    log_debug(logger_, "opening transport...");
    boost::asio::ip::tcp::resolver resolver(io_context_);
    auto endpoints = resolver.resolve(host_, service_);
    boost::asio::connect(socket_, endpoints);
    log_debug(logger_, "transport opened");
}

void Client::close()
{
    if (!is_open())
        return;
    log_debug(logger_, "closing transport...");
    socket_.shutdown(boost::asio::socket_base::shutdown_type::shutdown_both);
    socket_.close();
    log_debug(logger_, "transport closed");
}

bool Client::is_open()
{
    return socket_.is_open();
}

void Client::run()
{
    try {
        io_context_.restart();
        read_header();
        notify_running();
        while (is_running()) {
            try {
                io_context_.run();
                break; // run exited normally
            } catch (packet_error& e) {
                // discard ill-formed packets
                log_warn(logger_, e.what());
                read_header();
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

void Client::read_header()
{
    auto h = read_packet_.header_view();
    boost::asio::async_read(socket_,
                            boost::asio::buffer(const_cast<char*>(h.data()), h.size()),
        [this](const boost::system::error_code& ec, size_t)
        {
            errc_ = ec;
            if (!ec) {
                read_payload();
            } else if (ec == boost::asio::error::operation_aborted) {
                log_warn(logger_, "{}", ec.message());
            } else if (ec == boost::asio::error::eof) {
                log_info(logger_, "{}", ec.message());
                close();
                errc_ = ec;
            } else {
                throw asio_error(ec);
            }
        });
}

void Client::read_payload()
{
    read_packet_.parse_header();

    auto pl = read_packet_.payload();
    boost::asio::async_read(socket_,
                            boost::asio::buffer(const_cast<char*>(pl.data()), pl.size()),
        [this](const boost::system::error_code& ec, size_t)
        {
            errc_ = ec;
            if (!ec) {
                read_packet_.parse_payload();
                if (!read_queue_.try_enqueue(read_packet_)) {
                    // discard packet if queue is full
                    auto e = make_error_code(Errc::read_queue_full);
                    log_warn(logger_, e.message());
                }
                read_header();
            } else if (ec == boost::asio::error::operation_aborted) {
                log_warn(logger_, "{}", ec.message());
            } else if (ec == boost::asio::error::eof) {
                log_info(logger_, "{}", ec.message());
                close();
            } else {
                throw asio_error(ec);
            }
        });
}

void Client::do_write()
{
    if (!write_queue_.try_dequeue(write_packet_))
        return;
    write_in_progress_ = true;
    boost::asio::async_write(socket_,
                             boost::asio::buffer(write_packet_.data(), write_packet_.size()),
        [this](const boost::system::error_code& ec, size_t)
        {
            errc_ = ec;
            if (!ec) {
                if (write_queue_.size_approx() > 0)
                    do_write();
                else
                    write_in_progress_ = false;
            } else if (ec == boost::asio::error::operation_aborted) {
                log_warn(logger_, "{}", ec.message());
            } else if (ec == boost::asio::error::eof) {
                log_info(logger_, "{}", ec.message());
                close();
            } else {
                throw asio_error(ec);
            }
        });
}

} /* namespace tcp  */
} /* namespace transport  */
} /* namespace hdcp */
