#include "tcp_client.h"

namespace hdcp {
namespace transport {
namespace tcp {

Client::Client(common::Logger logger, std::string_view host, std::string_view service):
    Log(logger),
    io_context_(), socket_(io_context_), host_(host), service_(service)
{
}

Client::~Client()
{
    stop();
}

void Client::write(Packet&& p)
{
    if (!is_open())
        throw transport_error(TransportErrc::not_permitted);

    boost::asio::post(io_context_,
        [this, p] ()
        {
            if (!write_queue_.try_enqueue(p))
                throw transport_error(TransportErrc::write_queue_full);
            if (!write_in_progress_)
                do_write();
        });
}

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

void Client::start()
{
    if (is_running())
        return;
    log_debug(logger_, "starting transport...");
    clear_queues();
    write_in_progress_ = false;
    open();
    read_header();
    io_context_.restart();
    common::Thread::start(true);
    log_debug(logger_, "transport started");
};

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
    notify_running(0);
    while (is_running()) {
        try {
            io_context_.run();
            break; // run exited normally
        } catch (packet_error& e) {
            log_warn(logger_, e.what());
        } catch (asio_error& e) {
            log_error(logger_, e.what());
            close();
            if (error_cb_)
                error_cb_(e.code());
        } catch (std::exception& e) {
            log_error(logger_, e.what());
            close();
        }
    }
}

void Client::read_header()
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

void Client::read_payload()
{
    try {
        read_packet_.parse_header();
    } catch (hdcp::packet_error& e) {
        log_error(logger_, "{}", e.what());
        read_header();
        return;
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
                        throw transport_error(TransportErrc::read_queue_full);
                } catch (std::exception& e) {
                    log_error(logger_, "{}", e.what());
                }
                read_header();
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
