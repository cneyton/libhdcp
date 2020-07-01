#pragma once

#include "common/log.h"
#include "common/statemachine.h"
#include "common/thread.h"

#include "request.h"
#include "transport.h"
#include "application.h"

namespace hdcp
{

class Master: public common::Log, private common::Thread
{
public:
    using DataCallback = std::function<void(const Packet&)>;
    enum class State {
        disconnected,
        connecting,
        connected
    };

    Master(common::Logger logger, Transport* transport, DataCallback data_cb,
           const Identification& host_id);
    virtual ~Master();

    State get_state() const {return statemachine_.get_state();};
    const Identification& get_device_id() const {return device_id_;}

    void start();
    void stop();
    void connect();
    bool wait_connected();
    void disconnect();
    void send_command(Packet::BlockType id, const std::string& data, Request::Callback cb);

private:
    using common::Thread::start;

    int handler_state_disconnected_();
    int handler_state_connecting_();
    int handler_state_connected_();

    int check_connection_requested_();
    int check_connected_();
    int check_disconnected_();

    const common::StatesList<State> states_ {
        {"disconnected", State::disconnected,
            {{std::bind(&Master::handler_state_disconnected_, this), State::disconnected},
             {std::bind(&Master::check_connection_requested_, this), State::connecting}}
        },
        {"connecting", State::connecting,
            {{std::bind(&Master::handler_state_connecting_, this),   State::connecting},
             {std::bind(&Master::check_disconnected_, this),         State::disconnected},
             {std::bind(&Master::check_connected_, this),            State::connected}}
        },
        {"connected", State::connected,
            {{std::bind(&Master::handler_state_connected_, this),    State::connected},
             {std::bind(&Master::check_disconnected_, this),         State::disconnected}}
        }
    };

    // flags
    std::atomic_bool connection_requested_    = false;
    std::atomic_bool dip_received_            = false;
    std::atomic_bool disconnection_requested_ = false;

    std::mutex              mutex_connection_;
    std::condition_variable cv_connection_;
    std::mutex              mutex_connecting_;
    std::condition_variable cv_connecting_;

    common::Statemachine<State>    statemachine_;
    Transport*                     transport_;
    RequestManager                 request_manager_;
    Identification                 host_id_;
    Identification                 device_id_;
    DataCallback                   data_cb_;

    virtual void run();
    void wait_connection_request();
    void set_device_id(const Packet& p);
};

} /* namespace hdcp */
