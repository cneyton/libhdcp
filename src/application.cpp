#include "application.h"
#include "hdcp/exception.h"

using namespace hdcp;

Application::Application(common::Logger logger, Transport* transport, const Identification& host_id):
    common::Log(logger),
    statemachine_(logger, "com", states_, State::disconnected),
    transport_(transport), request_manager_(logger, transport),
    host_id_(host_id)
{
}

void Application::send_command(Packet::BlockType id, const std::string& data, Request::Callback cb)
{
    if (get_state() == State::connected)
        request_manager_.send_command(id, data, cb, command_timeout_);
    else
        throw hdcp::application_error("can't send command while disconnected");
}

int Application::handler_state_disconnected_()
{
    if (statemachine_.get_nb_loop_in_current_state() == 1) {
        disconnection_requested_ = false;
        request_manager_.stop();
        if (request_manager_.joinable())
            request_manager_.join();
    }
    return common::statemachine::goto_next_state;
}

int Application::handler_state_connecting_()
{
    if (statemachine_.get_nb_loop_in_current_state() == 1) {
        connection_requested_ = false;
        request_manager_.start(1);
        request_manager_.send_hip(host_id_, connecting_timeout_);
    }

    if (!transport_)
        throw hdcp::application_error("transport null pointer");

    Packet p(transport_->read());

    switch (p.get_type()) {
    case Packet::Type::dip:
        dip_received_ = true;
        request_manager_.ack_dip();
        break;
    default:
        log_warn(logger_, "you should only receive dip in connecting state");
        break;
    }

    return common::statemachine::goto_next_state;
}

int Application::handler_state_connected_()
{
    if (statemachine_.get_nb_loop_in_current_state() == 1) {
        dip_received_ = false;
        request_manager_.start_keepalive_management(keepalive_interval, keepalive_timeout);
    }

    if (!transport_)
        throw hdcp::application_error("transport null pointer");

    Packet p(transport_->read());

    switch (p.get_type()) {
    case Packet::Type::cmd_ack:
        request_manager_.ack_command(p);
        break;
    case Packet::Type::data:
        break;
    case Packet::Type::ka_ack:
        request_manager_.ack_keepalive();
        break;
    default:
        log_warn(logger_, "you should not be here");
        break;
    }

    return common::statemachine::goto_next_state;
}

int Application::check_connection_requested_()
{
    return connection_requested_ ? common::statemachine::goto_next_state:
                                   common::statemachine::stay_curr_state;
}

int Application::check_connected_()
{
    return dip_received_ ? common::statemachine::goto_next_state:
                           common::statemachine::stay_curr_state;
}

int Application::check_disconnected_()
{
    if (disconnection_requested_ || request_manager_.dip_timeout() ||
        request_manager_.keepalive_timeout())
        return common::statemachine::goto_next_state;
    else
        return common::statemachine::stay_curr_state;
}

void Application::run()
{
    while (is_running()) {
        statemachine_.wakeup();
    }
}
