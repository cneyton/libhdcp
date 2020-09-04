#include "slave.h"
#include "hdcp/exception.h"

using namespace hdcp;

Slave::Slave(common::Logger logger, const Identification& id,
             std::unique_ptr<Transport> transport, CmdCallback cmd_cb):
    common::Log(logger),
    statemachine_(logger, "com_slave", states_, State::init),
    transport_(std::move(transport)), request_manager_(logger, transport_.get()),
    id_(id), cmd_cb_(cmd_cb)
{
    statemachine_.display_trace();
}

Slave::~Slave()
{
    stop();
}

void Slave::start()
{
    if (is_running())
        return;
    common::Thread::start(true);
}

void Slave::stop()
{
    if (!is_running())
        return;
    common::Thread::stop();
    if (joinable())
        join();
    transport_->stop();
    request_manager_.stop();
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

void Slave::wait_connected()
{
    std::unique_lock<std::mutex> lk(mutex_connecting_);
    cv_connecting_.wait(lk, [&]{return statemachine_.get_state() == State::connected;});
}

void Slave::disconnect()
{
    disconnection_requested_ = true;
}

int Slave::handler_state_init()
{
    notify_running(0);
    return 0;
}

int Slave::handler_state_disconnected()
{
    if (statemachine_.get_nb_loop_in_current_state() == 1) {
        disconnection_requested_ = false;
        request_manager_.stop_slave_keepalive_management();
        request_manager_.stop();
        transport_->clear_queues();
        transport_->start();
    }

    Packet p;
    if (!transport_->read(p))
        return 0;

    log_trace(logger_, "{}", p);
    switch (p.type()) {
    case Packet::Type::hip:
        connection_requested_ = true;
        set_master_id(p);
        request_manager_.start();
        request_manager_.send_dip(id_);
        request_manager_.start_slave_keepalive_management(keepalive_timeout);
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

    if (!transport_)
        throw hdcp::application_error("transport null pointer");

    Packet p;
    if (!transport_->read(p))
        return 0;

    log_trace(logger_, "{}", p);
    switch (p.type()) {
    case Packet::Type::hip:
        connection_requested_ = true;
        break;
    case Packet::Type::cmd:
        // The cb should send an ack if the cmd is well formated
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
    return connection_requested_ ? common::statemachine::goto_next_state:
                                   common::statemachine::stay_curr_state;
}

int Slave::check_connected()
{
    return ka_received_ ? common::statemachine::goto_next_state:
                           common::statemachine::stay_curr_state;
}

int Slave::check_disconnected()
{
    if (disconnection_requested_ || request_manager_.keepalive_timeout()
        || !transport_->is_open())
        return common::statemachine::goto_next_state;
    else
        return common::statemachine::stay_curr_state;
}

void Slave::run()
{
    while (is_running()) {
        try {
            statemachine_.wakeup();
        } catch (hdcp::packet_error& e) {
            log_warn(logger_, e.what());
        }
    }
}

void Slave::set_master_id(const Packet& p)
{
    for (auto& b: p.blocks()) {
        switch (b.type) {
        case Packet::id_name:
            master_id_.name = b.data;
            break;
        case Packet::id_serial_number:
            master_id_.serial_number = b.data;
            break;
        case Packet::id_hw_version:
            master_id_.hw_version = b.data;
            break;
        case Packet::id_sw_version:
            master_id_.sw_version = b.data;
            break;
        default:
            log_warn(logger_, "you should no received non-id block type ({:#x})", b.type);
        }
    }
    log_debug(logger_, "master {}", master_id_);
}
