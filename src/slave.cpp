#include "slave.h"
#include "hdcp/exception.h"

namespace hdcp {
namespace appli {

Slave::Slave(common::Logger logger, const Identification& id,
             std::unique_ptr<Transport> transport):
    common::Log(logger), statemachine_(logger, "com_slave", states_, State::init),
    transport_(std::move(transport)),
    request_manager_(logger, transport_.get(), std::bind(&Slave::timeout_cb, this)),
    slave_id_(id)
{
    statemachine_.display_trace();
    transport_->set_error_cb(std::bind(&Slave::transport_error_cb, this, std::placeholders::_1));
}

Slave::~Slave()
{
    stop();
}

void Slave::start()
{
    if (is_running())
        return;
    log_debug(logger_, "starting application...");
    common::Thread::start(true);
    log_debug(logger_, "application started");
}

void Slave::stop()
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

void Slave::send_data(std::vector<Packet::Block>& blocks)
{
    if (get_state() != State::connected)
        throw hdcp::application_error("can't send data while disconnected");

    request_manager_.send_data(blocks);
}

void Slave::send_cmd_ack(const Packet& packet)
{
    request_manager_.send_cmd_ack(packet);
}

const Identification& Slave::connect()
{
    if (get_state() == State::connected)
        return master_id_;

    {
        // wake-up if we were waiting to connect
        std::unique_lock<std::mutex> lk(mutex_connection_);
        disconnection_requested_ = false;
        connection_requested_ = true;
        cv_connection_.notify_all();
    }

    std::unique_lock<std::mutex> lk(mutex_connecting_);
    transport_->start();
    cv_connecting_.wait(lk, [&]{return !connection_requested_ &&
                        statemachine_.get_state() != State::connecting;});

    if (statemachine_.get_state() != State::connected)
        throw application_error("connection failed");
    return master_id_;
}

void Slave::disconnect()
{
    if (get_state() == State::disconnected)
        return;
    std::unique_lock<std::mutex> lk(mutex_disconnection_);
    disconnection_requested_ = true;
    cv_disconnection_.wait(lk, [this]{return !disconnection_requested_;});
}

int Slave::handler_state_init()
{
    connection_requested_    = false;
    hip_received_            = false;
    ka_received_             = false;
    disconnection_requested_ = false;
    notify_running(0);
    return 0;
}

int Slave::handler_state_disconnected()
{
    if (statemachine_.get_nb_loop_in_current_state() == 1) {
        master_id_ = Identification();
        request_manager_.stop_keepalive_management();
        request_manager_.stop();
        transport_->stop();
        {
            // notify disconnected
            std::unique_lock<std::mutex> lk(mutex_disconnection_);
            disconnection_requested_ = false;
            cv_disconnection_.notify_all();
        }
        {
            // notify connection attempt failed
            std::unique_lock<std::mutex> lk(mutex_connecting_);
            cv_connecting_.notify_all();
        }
        log_debug(logger_, "waiting connection request...");
        wait_connection_request();
        // return here to not wait for packet when stopping
        return 0;
    }

    Packet p;
    if (!transport_->read(p))
        return 0;

    log_trace(logger_, "{}", p);
    switch (p.type()) {
    case Packet::Type::hip:
        hip_received_ = true;
        set_master_id(p);
        break;
    default:
        log_warn(logger_, "you should only receive hip in disconnected state");
        break;
    }

    return 0;
}

int Slave::handler_state_connecting()
{
    if (statemachine_.get_nb_loop_in_current_state() == 1) {
        connection_requested_ = false;
        hip_received_ = false;
        request_manager_.start();
        request_manager_.send_dip(slave_id_);
        request_manager_.start_keepalive_management(keepalive_timeout);
    }

    Packet p;
    if (!transport_->read(p))
        return 0;

    log_trace(logger_, "{}", p);
    switch (p.type()) {
    case Packet::Type::ka:
        request_manager_.keepalive();
        ka_received_ = true;
        break;
    default:
        log_warn(logger_, "you should only receive ka in connecting state");
        break;
    }

    return 0;
}

int Slave::handler_state_connected()
{
    if (statemachine_.get_nb_loop_in_current_state() == 1) {
        ka_received_ = false;
        std::unique_lock<std::mutex> lk(mutex_connecting_);
        cv_connecting_.notify_all();
    }

    Packet p;
    if (!transport_->read(p))
        return 0;

    log_trace(logger_, "{}", p);
    switch (p.type()) {
    case Packet::Type::hip:
        hip_received_ = true;
        master_id_ = Identification();
        request_manager_.stop_keepalive_management();
        request_manager_.stop();
        break;
    case Packet::Type::cmd:
        request_manager_.keepalive();
        // The cb should send an ack if the cmd is well formated
        if (cmd_cb_)
            cmd_cb_(p);
        break;
    case Packet::Type::ka:
        request_manager_.keepalive();
        break;
    default:
        log_warn(logger_,
                 "you should not receive this packet type ({:#x}) while connected", p.type());
        break;
    }

    return 0;
}

int Slave::check_true()
{
    return common::statemachine::goto_next_state;
}

int Slave::check_connection_requested()
{
    if (hip_received_)
        return common::statemachine::goto_next_state;
    return common::statemachine::stay_curr_state;
}

int Slave::check_connected()
{
    return ka_received_ ? common::statemachine::goto_next_state:
                           common::statemachine::stay_curr_state;
}

int Slave::check_disconnected()
{
    if (disconnection_requested_)
        return common::statemachine::goto_next_state;

    return common::statemachine::stay_curr_state;
}

void Slave::run()
{
    while (is_running()) {
        try {
            statemachine_.wakeup();
        } catch (std::exception& e) {
            log_error(logger_, e.what());
            disconnection_requested_ = true;
        }
    }
}

void Slave::wait_connection_request()
{
    std::unique_lock<std::mutex> lk(mutex_connection_);
    cv_connection_.wait(lk, [this]{return (connection_requested_ || !is_running());});
}

void Slave::set_master_id(const Packet& p)
{
    for (auto& b: p.blocks()) {
        switch (b.type) {
        case Packet::ReservedBlockType::name:
            master_id_.name = b.data;
            break;
        case Packet::ReservedBlockType::serial_number:
            master_id_.serial_number = b.data;
            break;
        case Packet::ReservedBlockType::hw_version:
            master_id_.hw_version = b.data;
            break;
        case Packet::ReservedBlockType::sw_version:
            master_id_.sw_version = b.data;
            break;
        default:
            log_warn(logger_, "you should no received non-id block type ({:#x})", b.type);
        }
    }
    log_debug(logger_, "master {}", master_id_);
}

void Slave::timeout_cb()
{
    disconnection_requested_ = true;
}

void Slave::transport_error_cb(std::exception_ptr eptr)
{
    try {
        std::rethrow_exception(eptr);
    } catch (std::exception& e) {
        disconnection_requested_ = true;
    }
}

} /* namespace appli  */
} /* namespace hdcp */
