#pragma once

#include <chrono>

#include "common/log.h"
#include "common/statemachine.h"
#include "common/thread.h"

#include "request.h"
#include "hdcp/transport.h"

namespace hdcp
{

constexpr std::chrono::milliseconds connecting_timeout_(1000);
constexpr std::chrono::milliseconds command_timeout_(1000);
constexpr std::chrono::milliseconds keepalive_timeout(1000);
constexpr std::chrono::milliseconds keepalive_interval(3000);

class Application: public common::Log, public common::Thread
{
public:
    Application(common::Logger logger, Transport* transport, const Identification& host_id);
    virtual ~Application();

    enum class State {
        disconnected,
        connecting,
        connected
    };

    State get_state() const {return statemachine_.get_state();};
    const Identification& get_device_id() const {return device_id_;}

    void send_command(Packet::BlockType id, const std::string& data, Request::Callback cb);

private:
    int handler_state_disconnected_();
    int handler_state_connecting_();
    int handler_state_connected_();

    int check_connection_requested_();
    int check_connected_();
    int check_disconnected_();

    const common::StatesList<State> states_ {
        {"disconnected", State::disconnected,
            {{std::bind(&Application::handler_state_disconnected_, this), State::disconnected},
             {std::bind(&Application::check_connection_requested_, this), State::connecting}}
        },
        {"connecting", State::connecting,
            {{std::bind(&Application::handler_state_connecting_, this),   State::connecting},
             {std::bind(&Application::check_disconnected_, this),         State::disconnected},
             {std::bind(&Application::check_connected_, this),            State::connected}}
        },
        {"connected", State::connected,
            {{std::bind(&Application::handler_state_connected_, this),    State::connected},
             {std::bind(&Application::check_disconnected_, this),         State::disconnected}}
        }
    };

    // flags
    std::atomic_bool connection_requested_    = false;
    std::atomic_bool dip_received_            = false;
    std::atomic_bool disconnection_requested_ = false;

    common::Statemachine<State>    statemachine_;
    Transport*                     transport_;
    RequestManager                 request_manager_;
    Identification                 host_id_;
    Identification                 device_id_;

    virtual void run();
};

} /* namespace hdcp */
