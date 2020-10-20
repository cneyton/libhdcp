#include "master.h"
#include "hdcp/exception.h"

namespace hdcp {
namespace appli {

Master::Master(common::Logger logger, const Identification& master_id,
               std::unique_ptr<Transport> transport):
    common::Log(logger), statemachine_(logger, "com_master", states_, State::init),
    transport_(std::move(transport)),
    request_manager_(logger, transport_.get(),
                     std::bind(&Master::timeout_cb, this, std::placeholders::_1)),
    master_id_(master_id)
{
    statemachine_.display_trace();
    if (transport_)
        transport_->set_error_cb(std::bind(&Master::transport_error_cb,
                                           this, std::placeholders::_1));
}

Master::~Master()
{
    stop();
}

void Master::start()
{
    if (is_running())
        return;
    log_debug(logger_, "starting application...");
    common::Thread::start(true);
    log_debug(logger_, "application started");
}

void Master::stop()
{
    if (!is_running())
        return;
    log_debug(logger_, "stopping application...");
    common::Thread::stop();
    {
        // wake-up in case we were waiting to connect
        std::unique_lock<std::mutex> lk(mutex_connection_);
        cv_connection_.notify_all();
    }
    if (joinable())
        join();
    request_manager_.stop();
    transport_->stop();
    log_debug(logger_, "application stopped");
}

void Master::send_command(Packet::BlockType id, const std::string& data, Request::Callback cb)
{
    if (state() != State::connected)
        throw hdcp::application_error(ApplicationErrc::not_permitted);

    request_manager_.send_command(id, data, cb, command_timeout_);
}

const Identification& Master::connect()
{
    if (state() == State::connected)
        return slave_id_;

    {
        // wake-up if we were waiting to connect
        std::unique_lock<std::mutex> lk(mutex_connection_);
        disconnection_requested_ = false;
        connection_requested_ = true;
        cv_connection_.notify_all();
    }
    std::unique_lock<std::mutex> lk(mutex_connecting_);
    cv_connecting_.wait(lk, [this]{return !connection_requested_ &&
                                          state() != State::connecting;});
    if (state() != State::connected)
        throw application_error(ApplicationErrc::connection_failed);
    return slave_id_;
}

void Master::disconnect()
{
    if (state() == State::disconnected)
        return;
    std::unique_lock<std::mutex> lk(mutex_disconnection_);
    disconnection_requested_ = true;
    cv_disconnection_.wait(lk, [this]{return !disconnection_requested_;});
}

void Master::async_disconnect(std::function<void>(int))
{
    if (state() == State::disconnected)
        return;
}

int Master::handler_state_init()
{
    connection_requested_    = false;
    dip_received_            = false;
    disconnection_requested_ = false;
    errc_ = std::error_code();
    notify_running(0);
    return 0;
}

int Master::handler_state_disconnected()
{
    if (statemachine_.get_nb_loop_in_current_state() == 1) {
        slave_id_ = Identification();
        request_manager_.stop();
        transport_->stop();
        {
            // notify disconnected
            std::unique_lock<std::mutex> lk(mutex_disconnection_);
            disconnection_requested_ = false;
            if (error_cb_) {
                if (errc_)
                    error_cb_(errc_);
            }
            cv_disconnection_.notify_all();
        }
        {
            // notify connection attempt failed
            std::unique_lock<std::mutex> lk(mutex_connecting_);
            cv_connecting_.notify_all();
        }
    }

    log_debug(logger_, "waiting connection request...");
    wait_connection_request();
    return 0;
}

int Master::handler_state_connecting()
{
    if (statemachine_.get_nb_loop_in_current_state() == 1) {
        connection_requested_ = false;
        transport_->start();
        request_manager_.start();
        request_manager_.send_hip(master_id_, connecting_timeout_);
    }

    Packet p;
    if (!transport_->read(p))
        return 0;

    log_trace(logger_, "{}", p);
    switch (p.type()) {
    case Packet::Type::dip:
        if (p.id() != 1)
            log_warn(logger_, "dip id should be 0");
        received_packet_id_ = p.id();
        set_slave_id(p);
        dip_received_ = true;
        request_manager_.ack_dip();
        break;
    default:
        log_warn(logger_, "you should only receive dip in connecting state");
        break;
    }

    return 0;
}

int Master::handler_state_connected()
{
    if (statemachine_.get_nb_loop_in_current_state() == 1) {
        dip_received_ = false;
        request_manager_.start_keepalive_management(keepalive_interval, keepalive_timeout);
        std::unique_lock<std::mutex> lk(mutex_connecting_);
        cv_connecting_.notify_all();
    }

    Packet p;
    if (!transport_->read(p))
        return 0;

    log_trace(logger_, "{}", p);
    if (++received_packet_id_ != p.id()) {
        log_warn(logger_, "packet loss: received {}, expected {}", p.id(), received_packet_id_);
        received_packet_id_ = p.id();
    }
    switch (p.type()) {
    case Packet::Type::cmd_ack:
        request_manager_.ack_command(p);
        break;
    case Packet::Type::data:
        if (data_cb_)
            data_cb_(p);
        break;
    case Packet::Type::ka_ack:
        request_manager_.ack_keepalive();
        break;
    default:
        log_warn(logger_,
                 "you should not receive this packet type ({:#x}) while connected", p.type());
        break;
    }

    return 0;
}

int Master::check_true()
{
    return common::statemachine::goto_next_state;
}

int Master::check_connection_requested()
{
    return connection_requested_ ? common::statemachine::goto_next_state:
                                   common::statemachine::stay_curr_state;
}

int Master::check_connected()
{
    return dip_received_ ? common::statemachine::goto_next_state:
                           common::statemachine::stay_curr_state;
}

int Master::check_disconnected()
{
    if (disconnection_requested_ || !transport_->is_open())
        return common::statemachine::goto_next_state;

    return common::statemachine::stay_curr_state;
}

void Master::run()
{
    while (is_running()) {
        try {
            statemachine_.wakeup();
        } catch (base_transport_error& e) {
            log_error(logger_, e.what());
            errc_ = e.code();
            disconnection_requested_ = true;
        } catch (application_error& e) {
            log_error(logger_, e.what());
            errc_ = e.code();
            disconnection_requested_ = true;
        } catch (std::exception& e) {
            log_error(logger_, e.what());
            disconnection_requested_ = true;
        }
    }
}

void Master::wait_connection_request()
{
    std::unique_lock<std::mutex> lk(mutex_connection_);
    cv_connection_.wait(lk, [this]{return (connection_requested_ || !is_running());});
}

void Master::set_slave_id(const Packet& p)
{
    for (auto& b: p.blocks()) {
        switch (b.type) {
        case Packet::ReservedBlockType::name:
            slave_id_.name = b.data;
            break;
        case Packet::ReservedBlockType::serial_number:
            slave_id_.serial_number = b.data;
            break;
        case Packet::ReservedBlockType::hw_version:
            slave_id_.hw_version = b.data;
            break;
        case Packet::ReservedBlockType::sw_version:
            slave_id_.sw_version = b.data;
            break;
        default:
            log_warn(logger_,
                     "you should not received this block type ({:#x}) in an identification packet",
                     b.type);
        }
    }
    log_debug(logger_, "device {}", slave_id_);
}

void Master::timeout_cb(master::RequestManager::TimeoutType)
{
    std::unique_lock<std::mutex> lk(mutex_disconnection_);
    errc_ = ApplicationErrc::timeout;
    disconnection_requested_ = true;
}

void Master::connected_cb()
{
    // notify connection attempt failed
    std::unique_lock<std::mutex> lk(mutex_connecting_);
    cv_connecting_.notify_all();
}

void Master::disconnected_cb()
{
    // notify disconnected
    std::unique_lock<std::mutex> lk(mutex_disconnection_);
    disconnection_requested_ = false;
    cv_disconnection_.notify_all();
}

void Master::transport_error_cb(const std::error_code& errc)
{
    if (!errc)
        return;
    errc_ = errc;
    disconnection_requested_ = true;
}

} /* namespace appli */
} /* namespace hdcp */
