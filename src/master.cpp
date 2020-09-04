#include "master.h"
#include "hdcp/exception.h"

using namespace hdcp;

Master::Master(common::Logger logger, const Identification& host_id,
               std::unique_ptr<Transport> transport, DataCallback data_cb):
    common::Log(logger),
    statemachine_(logger, "com_master", states_, State::init),
    transport_(std::move(transport)), request_manager_(logger, transport_.get()),
    host_id_(host_id), data_cb_(data_cb)
{
    statemachine_.display_trace();
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
    log_debug(logger_, "application started...");
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
        connection_requested_ = true;
        cv_connection_.notify_all();
    }
    if (joinable())
        join();
    transport_->stop();
    request_manager_.stop();
    log_debug(logger_, "application stopped...");
}

void Master::send_command(Packet::BlockType id, const std::string& data, Request::Callback cb)
{
    if (get_state() != State::connected)
        throw hdcp::application_error("can't send command while disconnected");

    request_manager_.send_command(id, data, cb, command_timeout_);
}

void Master::connect()
{
    /* TODO: check if already connected  <01-09-20, cneyton> */
    std::unique_lock<std::mutex> lk(mutex_connection_);
    disconnection_requested_ = false;
    connection_requested_ = true;
    cv_connection_.notify_all();
}

bool Master::wait_connected()
{
    std::unique_lock<std::mutex> lk(mutex_connecting_);
    cv_connecting_.wait(lk);
    return statemachine_.get_state() == State::connected;
}

void Master::disconnect()
{
    disconnection_requested_ = true;
}

int Master::handler_state_init()
{
    notify_running(0);
    return 0;
}

int Master::handler_state_disconnected()
{
    if (statemachine_.get_nb_loop_in_current_state() == 1) {
        disconnection_requested_ = false;
        transport_->stop();
        request_manager_.stop_master_keepalive_management();
        request_manager_.stop();
        if (connecting_) {
            // notify connection attempt failed
            std::unique_lock<std::mutex> lk(mutex_connecting_);
            connecting_ = false;
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
        connecting_ = true;
        transport_->clear_queues();
        transport_->start();
        request_manager_.start();
        request_manager_.send_hip(host_id_, connecting_timeout_);
    }

    Packet p;
    if (!transport_->read(p))
        return 0;

    log_trace(logger_, "{}", p);
    switch (p.type()) {
    case Packet::Type::dip:
        set_device_id(p);
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
        request_manager_.start_master_keepalive_management(keepalive_interval, keepalive_timeout);

        std::unique_lock<std::mutex> lk(mutex_connecting_);
        connecting_ = false;
        cv_connecting_.notify_all();
    }

    Packet p;
    if (!transport_->read(p))
        return 0;

    log_trace(logger_, "{}", p);
    switch (p.type()) {
    case Packet::Type::cmd_ack:
        request_manager_.ack_command(p);
        break;
    case Packet::Type::data:
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
    if (disconnection_requested_ || request_manager_.dip_timeout() ||
        request_manager_.keepalive_timeout() || !transport_->is_open())
        return common::statemachine::goto_next_state;
    else
        return common::statemachine::stay_curr_state;
}

void Master::run()
{
    while (is_running()) {
        try {
            statemachine_.wakeup();
        } catch (hdcp::packet_error& e) {
            log_warn(logger_, e.what());
        }
    }
}

void Master::wait_connection_request()
{
    std::unique_lock<std::mutex> lk(mutex_connection_);
    cv_connection_.wait(lk, [this]{return connection_requested_ ? true:false;});
}

void Master::set_device_id(const Packet& p)
{
    for (auto& b: p.blocks()) {
        switch (b.type) {
        case Packet::id_name:
            device_id_.name = b.data;
            break;
        case Packet::id_serial_number:
            device_id_.serial_number = b.data;
            break;
        case Packet::id_hw_version:
            device_id_.hw_version = b.data;
            break;
        case Packet::id_sw_version:
            device_id_.sw_version = b.data;
            break;
        default:
            log_warn(logger_,
                     "you should not received this block type ({:#x}) in an identifation packet",
                     b.type);
        }
    }
    log_debug(logger_, "device {}", device_id_);
}
