#include "slave.h"
#include "application_error.h"

namespace hdcp {
namespace appli {

Slave::Slave(common::Logger logger, const Identification& id,
             std::unique_ptr<Transport> transport):
    common::Log(logger),
    statemachine_("com_slave", states_, State::init),
    transport_(std::move(transport)),
    request_manager_(logger, transport_.get(), std::bind(&Slave::timeout_cb, this)),
    slave_id_(id)
{
    statemachine_.set_transition_handler(
        [this] (const common::Statemachine<State>::State * p,
                const common::Statemachine<State>::State * c)
        {
            log_info(logger_, "slave protocol: {} -> {}", p->name, c->name);
            if (status_cb_)
                status_cb_(c->id, errc_);
            // reset error code once it has been sent
            errc_ = std::error_code();
        });
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
    transport_->start();
    statemachine_.reinit();
    common::Thread::start(true);
    log_debug(logger_, "application started");
}

void Slave::stop()
{
    if (!is_running())
        return;
    log_debug(logger_, "stopping application...");
    common::Thread::stop();
    if (joinable())
        join();
    request_manager_.stop();
    transport_->stop();
    log_debug(logger_, "application stopped");
}

void Slave::send_data(std::vector<Packet::BlockView>& blocks)
{
    if (state() != State::connected)
        throw application_error(Errc::write_while_disconnected);

    request_manager_.send_data(blocks);
}

void Slave::send_data(std::vector<Packet::Block>& blocks)
{
    if (state() != State::connected)
        throw application_error(Errc::write_while_disconnected);

    request_manager_.send_data(blocks);
}

common::transition_status Slave::handler_state_init()
{
    evt_mngr_.clear();
    master_id_ = Identification();
    errc_ = std::error_code();
    notify_running();
    return common::transition_status::stay_curr_state;
}

common::transition_status Slave::handler_state_disconnected()
{
    if (statemachine_.nb_loop_in_current_state() == 1) {
        request_manager_.stop();
        transport_->clear_queues();
    }

    Packet p;
    if (!transport_->read(p))
        return common::transition_status::stay_curr_state;

    log_trace(logger_, "{}", p);
    switch (p.type()) {
    case Packet::Type::hip:
        if (p.id() != 1)
            log_warn(logger_, "hip id should be 0");
        received_packet_id_ = p.id();
        set_master_id(p);
        evt_mngr_.notify(Event::hip_received);
        break;
    default:
        log_warn(logger_, "you should only receive hip in disconnected state");
        break;
    }

    return common::transition_status::stay_curr_state;
}

common::transition_status Slave::handler_state_connecting()
{
    if (statemachine_.nb_loop_in_current_state() == 1) {
        request_manager_.start();
        request_manager_.send_dip(slave_id_);
        request_manager_.start_keepalive_management(keepalive_timeout);
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
    case Packet::Type::ka:
        request_manager_.keepalive();
        evt_mngr_.notify(Event::first_ka_received);
        break;
    default:
        log_warn(logger_, "you should only receive ka in connecting state");
        break;
    }

    return common::transition_status::stay_curr_state;
}

common::transition_status Slave::handler_state_connected()
{
    Packet p;
    if (!transport_->read(p))
        return common::transition_status::stay_curr_state;

    log_trace(logger_, "{}", p);
    if (++received_packet_id_ != p.id()) {
        log_warn(logger_, "packet loss: received {}, expected {}", p.id(), received_packet_id_);
        received_packet_id_ = p.id();
    }

    switch (p.type()) {
    case Packet::Type::hip:
        master_id_ = Identification();
        request_manager_.stop_keepalive_management();
        request_manager_.stop();
        evt_mngr_.notify(Event::hip_received);
        break;
    case Packet::Type::cmd:
    {
        request_manager_.keepalive();
        request_manager_.send_cmd_ack(p);
        auto blocks = p.blocks();
        if (blocks.size() != 1) {
            log_warn(logger_, "you should receive exactly one block in cmds (received {})",
                     blocks.size());
            break;
        }
        try {
            if (cmd_cb_)
                cmd_cb_(blocks[0]);
        } catch (std::exception& e) {
            log_error(logger_, "failed to process command {}", p.id());
        }
        break;
    }
    case Packet::Type::ka:
        request_manager_.keepalive();
        break;
    default:
        log_warn(logger_,
                 "you should not receive this packet type ({:#x}) while connected", p.type());
        break;
    }

    return common::transition_status::stay_curr_state;
}

common::transition_status Slave::check_true()
{
    return common::transition_status::goto_next_state;
}

common::transition_status Slave::check_hip_received()
{
    if (evt_mngr_.erase(Event::hip_received))
        return common::transition_status::goto_next_state;
    return common::transition_status::stay_curr_state;
}

common::transition_status Slave::check_connected()
{
    if (evt_mngr_.erase(Event::first_ka_received))
        return common::transition_status::goto_next_state;
    return common::transition_status::stay_curr_state;
}

common::transition_status Slave::check_ka_timeout()
{
    if (evt_mngr_.erase(Event::ka_timeout)) {
        errc_ = Errc::ka_timeout;
        return common::transition_status::goto_next_state;
    }
    return common::transition_status::stay_curr_state;
}

common::transition_status Slave::check_transport_closed()
{
    errc_ = transport_->error_code();
    if (!transport_->is_open())
        return common::transition_status::goto_next_state;
    return common::transition_status::stay_curr_state;
}

void Slave::run()
{
    while (is_running()) {
        try {
            statemachine_.wakeup();
        } catch (hdcp_error& e) {
            log_error(logger_, e.what());
        } catch (std::exception& e) {
            log_error(logger_, e.what());
        } catch (...) {
            log_error(logger_, "error during thread execution");
        }
    }
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
            log_warn(logger_, "you shouldn't received non-id block type ({:#x})", b.type);
        }
    }
    log_debug(logger_, "master {}", master_id_);
}

void Slave::timeout_cb()
{
    evt_mngr_.notify(Event::ka_timeout);
}

} /* namespace appli  */
} /* namespace hdcp */
