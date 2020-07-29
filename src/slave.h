#pragma once

#include "common/log.h"
#include "common/statemachine.h"
#include "common/thread.h"

#include "request.h"
#include "transport.h"
#include "application.h"

namespace hdcp
{

class Slave: public common::Log, private common::Thread
{
public:
    using CmdCallback = std::function<void(const Packet&)>;
    enum class State {
        disconnected,
        connecting,
        connected
    };

    Slave(common::Logger logger, Transport* transport, CmdCallback cmd_cb,
          const Identification& id);
    virtual ~Slave();

    State get_state() const {return statemachine_.get_state();};
    const Identification& get_master_id() const {return master_id_;}

    void start();
    void stop();
    void wait_connected();
    void disconnect();
    void send_data(std::vector<Packet::Block>& blocks);
    void send_cmd_ack(const Packet& packet);

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
            {{std::bind(&Slave::handler_state_disconnected_, this), State::disconnected},
             {std::bind(&Slave::check_connection_requested_, this), State::connecting}}
        },
        {"connecting", State::connecting,
            {{std::bind(&Slave::handler_state_connecting_, this),   State::connecting},
             {std::bind(&Slave::check_disconnected_, this),         State::disconnected},
             {std::bind(&Slave::check_connected_, this),            State::connected}}
        },
        {"connected", State::connected,
            {{std::bind(&Slave::handler_state_connected_, this),    State::connected},
             {std::bind(&Slave::check_disconnected_, this),         State::disconnected},
             {std::bind(&Slave::check_connection_requested_, this), State::connecting}}
        }
    };

    // flags
    std::atomic_bool connection_requested_    = false;
    std::atomic_bool ka_received_             = false;
    std::atomic_bool disconnection_requested_ = false;

    std::mutex              mutex_connecting_;
    std::condition_variable cv_connecting_;

    common::Statemachine<State>    statemachine_;
    Transport*                     transport_;
    RequestManager                 request_manager_;
    Identification                 id_;
    Identification                 master_id_;
    CmdCallback                    cmd_cb_;

    virtual void run();
    void set_master_id(const Packet& p);
};

} /* namespace hdcp */
