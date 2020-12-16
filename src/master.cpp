#include "master.h"
#include "application_error.h"

namespace hdcp {
namespace appli {

Master::Master(common::Logger logger, const Identification& master_id,
               std::unique_ptr<Transport> transport):
    common::Log(logger),
    statemachine_("com_master", states_, State::init),
    transport_(std::move(transport)),
    request_manager_(logger, transport_.get(),
                     std::bind(&Master::timeout_cb, this, std::placeholders::_1)),
    master_id_(master_id)
{
    statemachine_.set_transition_handler(
        [this] (const common::Statemachine<State>::State * p,
                const common::Statemachine<State>::State * c)
        {
            log_info(logger_, "master protocol: {} -> {}", p->name, c->name);
            if (status_cb_)
                 status_cb_(c->id, errc_);
            // reset error code once it has been sent
            errc_ = std::error_code();
            // notify for synchronous connect
            if (p->id == State::connecting)
                evt_mngr_.notify(Event::connection_attempt);
        });
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
    transport_->start();
    statemachine_.reinit();
    common::Thread::start(true);
    log_debug(logger_, "application started");
}

void Master::stop()
{
    if (!is_running())
        return;
    log_debug(logger_, "stopping application...");
    common::Thread::stop();
    evt_mngr_.notify(Event::stop);
    if (joinable())
        join();
    request_manager_.stop();
    transport_->stop();
    log_debug(logger_, "application stopped");
}

void Master::send_command(Packet::BlockType id, const std::string& data, Request::Callback cb)
{
    if (state() != State::connected)
        throw hdcp::application_error(Errc::write_while_disconnected);

    request_manager_.send_command(id, data, cb, command_timeout_);
}

void Master::async_connect()
{
    if (state() == State::connected)
        return;

    evt_mngr_.notify(Event::connection_requested);
}

void Master::async_disconnect()
{
    if (state() == State::disconnected)
        return;

    evt_mngr_.notify(Event::disconnection_requested);
}

void Master::connect()
{
    if (state() == State::connected)
        return;

    evt_mngr_.notify(Event::connection_requested);
    evt_mngr_.wait(Event::connection_attempt);
    evt_mngr_.erase(Event::connection_attempt);
    if (state() != State::connected)
        throw application_error(Errc::connection_failed);
}

common::transition_status Master::handler_state_init()
{
    errc_ = std::error_code();
    evt_mngr_.clear();
    notify_running();
    return common::transition_status::stay_curr_state;
}

common::transition_status Master::handler_state_disconnected()
{
    if (statemachine_.nb_loop_in_current_state() == 1) {
        slave_id_ = Identification();
        request_manager_.stop();
    }

    log_debug(logger_, "waiting for event...");
    evt_mngr_.wait();
    return common::transition_status::stay_curr_state;
}

common::transition_status Master::handler_state_connecting()
{
    if (statemachine_.nb_loop_in_current_state() == 1) {
        transport_->clear_queues();
        request_manager_.start();
        evt_mngr_.erase(Event::dip_received);
        connection_attempts_ = 0;
    }

    if (connection_attempts_== 0 || evt_mngr_.erase(Event::dip_timeout)) {
        connection_attempts_++;
        log_info(logger_, "connection attempt {}", connection_attempts_);
        request_manager_.send_hip(master_id_, connecting_timeout_);
    }

    Packet p;
    if (!transport_->read(p))
        return common::transition_status::stay_curr_state;

    log_trace(logger_, "{}", p);
    switch (p.type()) {
    case Packet::Type::dip:
        if (p.id() != 1)
            log_warn(logger_, "dip id should be 1");
        received_packet_id_ = p.id();
        set_slave_id(p);
        request_manager_.ack_dip();
        evt_mngr_.notify(Event::dip_received);
        break;
    default:
        log_warn(logger_, "you should only receive dip in connecting state");
        break;
    }

    return common::transition_status::stay_curr_state;
}

common::transition_status Master::handler_state_connected()
{
    if (statemachine_.nb_loop_in_current_state() == 1) {
        request_manager_.start_keepalive_management(keepalive_interval, keepalive_timeout);
    }

    Packet p;
    if (!transport_->read(p))
        return common::transition_status::stay_curr_state;

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

    return common::transition_status::stay_curr_state;
}

common::transition_status Master::check_true()
{
    return common::transition_status::goto_next_state;
}

common::transition_status Master::check_connection_requested()
{
    if (evt_mngr_.erase(Event::connection_requested))
        return common::transition_status::goto_next_state;
    return common::transition_status::stay_curr_state;
}

common::transition_status Master::check_connected()
{
    if (evt_mngr_.erase(Event::dip_received))
        return common::transition_status::goto_next_state;
    return common::transition_status::stay_curr_state;
}

common::transition_status Master::check_disconnected()
{
    if (evt_mngr_.erase(Event::disconnection_requested))
        return common::transition_status::goto_next_state;

    return common::transition_status::stay_curr_state;
}

common::transition_status Master::check_dip_timeout()
{
    if (evt_mngr_.contains(Event::dip_timeout) &&
        connection_attempts_ >= max_connection_attempts) {
        evt_mngr_.erase(Event::dip_timeout);
        errc_ = Errc::dip_timeout;
        return common::transition_status::goto_next_state;
    }
    return common::transition_status::stay_curr_state;
}

common::transition_status Master::check_ka_timeout()
{
    if (evt_mngr_.erase(Event::ka_timeout)) {
        errc_ = Errc::ka_timeout;
        return common::transition_status::goto_next_state;
    }

    return common::transition_status::stay_curr_state;
}

common::transition_status Master::check_transport_closed()
{
    if (!transport_->is_open()) {
        errc_ = transport_->error_code();
        return common::transition_status::goto_next_state;
    }
    return common::transition_status::stay_curr_state;
}

void Master::run()
{
    while (is_running()) {
        try {
            statemachine_.wakeup();
        } catch (hdcp_error& e) {
            log_error(logger_, e.what());
            errc_ = e.code();
        } catch (std::exception& e) {
            log_error(logger_, e.what());
        } catch (...) {
            log_error(logger_, "error during thread execution");
        }
    }
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

void Master::timeout_cb(master::RequestManager::TimeoutType timeout_type)
{
    switch (timeout_type) {
    case master::RequestManager::TimeoutType::ka_timeout:
        evt_mngr_.notify(Event::ka_timeout);
        break;
    case master::RequestManager::TimeoutType::dip_timeout:
        evt_mngr_.notify(Event::dip_timeout);
        break;
    }
}

} /* namespace appli */
} /* namespace hdcp */
