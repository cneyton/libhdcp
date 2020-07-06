#include "master.h"
#include "hdcp/exception.h"

using namespace hdcp;

Master::Master(common::Logger logger, Transport* transport, DataCallback data_cb,
                         const Identification& host_id):
    common::Log(logger),
    statemachine_(logger, "com_master", states_, State::disconnected),
    transport_(transport), request_manager_(logger, transport),
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
    common::Thread::start(0);
}

void Master::stop()
{
    if (!is_running())
        return;
    common::Thread::stop();
    // wake-up in case we were waiting to connect
    cv_connection_.notify_all();
    if (joinable())
        join();
    statemachine_.reinit();
    // one more wakeup to execute one loop of the disconnected state
    statemachine_.wakeup();
}

void Master::send_command(Packet::BlockType id, const std::string& data, Request::Callback cb)
{
    if (get_state() != State::connected)
        throw hdcp::application_error("can't send command while disconnected");

    request_manager_.send_command(id, data, cb, command_timeout_);
}

void Master::connect()
{
    std::unique_lock<std::mutex> lk(mutex_connection_);
    disconnection_requested_ = false;
    connection_requested_ = true;
    cv_connection_.notify_all();
}

bool Master::wait_connected()
{
    std::unique_lock<std::mutex> lk(mutex_connecting_);
    cv_connecting_.wait(lk, [&]{State s = statemachine_.get_state();
                        return (s == State::disconnected || s == State::connected) ? true:false;});
    return statemachine_.get_state() == State::connected;
}

void Master::disconnect()
{
    disconnection_requested_ = true;
}

int Master::handler_state_disconnected_()
{
    if (statemachine_.get_nb_loop_in_current_state() == 1) {
        disconnection_requested_ = false;
        request_manager_.stop_master_keepalive_management();
        request_manager_.stop();
        if (request_manager_.joinable())
            request_manager_.join();
        // notify connection attempt failed
        std::unique_lock<std::mutex> lk(mutex_connecting_);
        cv_connecting_.notify_all();
        // need to return here to not wait when stopping
        return 0;
    }

    wait_connection_request();
    return 0;
}

int Master::handler_state_connecting_()
{
    if (statemachine_.get_nb_loop_in_current_state() == 1) {
        connection_requested_ = false;
        request_manager_.start(1);
        request_manager_.send_hip(host_id_, connecting_timeout_);
    }

    if (!transport_)
        throw hdcp::application_error("transport null pointer");

    std::string buf;
    if (!transport_->read(buf))
        return 0;

    Packet p(std::move(buf));
    log_trace(logger_, "{}", p);
    switch (p.get_type()) {
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

int Master::handler_state_connected_()
{
    if (statemachine_.get_nb_loop_in_current_state() == 1) {
        dip_received_ = false;
        request_manager_.start_master_keepalive_management(keepalive_interval, keepalive_timeout);

        std::unique_lock<std::mutex> lk(mutex_connecting_);
        cv_connecting_.notify_all();
    }

    if (!transport_)
        throw hdcp::application_error("transport null pointer");

    std::string buf;
    if (!transport_->read(buf))
        return 0;

    Packet p(std::move(buf));
    log_trace(logger_, "{}", p);
    switch (p.get_type()) {
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
                 "you should not receive this packet type ({:#x}) while connected", p.get_type());
        break;
    }

    return 0;
}

int Master::check_connection_requested_()
{
    return connection_requested_ ? common::statemachine::goto_next_state:
                                   common::statemachine::stay_curr_state;
}

int Master::check_connected_()
{
    return dip_received_ ? common::statemachine::goto_next_state:
                           common::statemachine::stay_curr_state;
}

int Master::check_disconnected_()
{
    if (disconnection_requested_ || request_manager_.dip_timeout() ||
        request_manager_.keepalive_timeout())
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
    cv_connection_.wait(lk);
}

void Master::set_device_id(const Packet& p)
{
    for (auto& b: p.get_blocks()) {
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
