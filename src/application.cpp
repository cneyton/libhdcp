#include "application.h"

using namespace hdcp;

Application::Application(common::Logger logger, Transport& transport, const Identification& host_id):
    common::Log(logger),
    statemachine_(logger, "com", states_, State::disconnected),
    transport_(transport), request_manager_(logger, transport),
    host_id_(host_id)
{
}

void Application::send_command(Packet::BlockType id, const std::string& data, Request::Callback cb)
{
    request_manager_.send_command(id, data, cb, command_timeout_);
}

int Application::handler_state_disconnected_()
{
    return common::statemachine::goto_next_state;
}

int Application::handler_state_connecting_()
{
    if (statemachine_.get_nb_loop_in_current_state() == 1) {
        request_manager_.send_hip(host_id_, connecting_timeout_);
    }

    Packet p(transport_.read());

    switch (p.get_type()) {
    case Packet::Type::dip:
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
    Packet p(transport_.read());

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
    return common::statemachine::goto_next_state;
}

int Application::check_connected_()
{
    return common::statemachine::goto_next_state;
}

int Application::check_disconnected_()
{
    return common::statemachine::goto_next_state;
}

void Application::run()
{
    while (is_running()) {
        statemachine_.wakeup();
    }
}
