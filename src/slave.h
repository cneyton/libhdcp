#pragma once

#include "common/log.h"
#include "common/statemachine.h"
#include "common/thread.h"

#include "request.h"
#include "transport.h"
#include "application.h"

namespace hdcp {
namespace appli {

class Slave: public common::Log, private common::Thread
{
public:
    using CmdCallback   = std::function<void(const Packet::BlockView&)>;
    enum class State {
        init,
        disconnected,
        connecting,
        connected
    };
    using StatusCallback = std::function<void(State, const std::error_code&)>;

    Slave(common::Logger logger, const Identification& id, std::unique_ptr<Transport> transport);
    ~Slave();

    State state() const {return statemachine_.get_state();};
    const Identification& master_id() const {return master_id_;}
    const Identification& slave_id()  const {return slave_id_;}

    void start();
    void stop() override;
    void set_cmd_cb(CmdCallback&& cb)       {cmd_cb_   = std::forward<CmdCallback>(cb);}
    void set_status_cb(StatusCallback&& cb) {status_cb_ = std::forward<StatusCallback>(cb);}
    /// Synchronous connect
    const Identification& connect();
    /// Synchronous disconnect
    void disconnect();

    void send_data(std::vector<Packet::BlockView>&);
    void send_data(std::vector<Packet::Block>&);

private:
    using common::Thread::start;

    const common::StatesList<State> states_ {
        {"init", State::init,
            {{std::bind(&Slave::handler_state_init, this),         State::init},
             {std::bind(&Slave::check_true, this),                 State::disconnected}}
        },
        {"disconnected", State::disconnected,
            {{std::bind(&Slave::handler_state_disconnected, this), State::disconnected},
             {std::bind(&Slave::check_connection_requested, this), State::connecting}}
        },
        {"connecting", State::connecting,
            {{std::bind(&Slave::handler_state_connecting, this),   State::connecting},
             {std::bind(&Slave::check_disconnected, this),         State::init},
             {std::bind(&Slave::check_connected, this),            State::connected}}
        },
        {"connected", State::connected,
            {{std::bind(&Slave::handler_state_connected, this),    State::connected},
             {std::bind(&Slave::check_disconnected, this),         State::init},
             {std::bind(&Slave::check_connection_requested, this), State::connecting}}
        }
    };

    int handler_state_init();
    int handler_state_disconnected();
    int handler_state_connecting();
    int handler_state_connected();

    int check_true();
    int check_connection_requested();
    int check_connected();
    int check_disconnected();


    // flags
    bool hip_received_ = false;
    bool ka_received_  = false;
    std::atomic_bool disconnection_requested_ = true;

    common::Statemachine<State>   statemachine_;
    std::unique_ptr<Transport>    transport_;
    slave::RequestManager         request_manager_;
    Identification                slave_id_;
    Identification                master_id_;
    CmdCallback                   cmd_cb_;
    Packet::Id                    received_packet_id_;
    std::error_code               errc_;

    StatusCallback                status_cb_;

    void run() override;
    void set_master_id(const Packet& p);
    void timeout_cb();
    void transport_error_cb(const std::error_code&);
};

} /* namespace appli  */
} /* namespace hdcp */
