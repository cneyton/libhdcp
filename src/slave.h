#pragma once

#include "common/log.h"
#include "common/statemachine.h"
#include "common/thread.h"
#include "common/event_mngr.h"

#include "slave_request.h"
#include "transport.h"
#include "application.h"

namespace hdcp {
namespace appli {

class Slave: public common::Log, private common::Thread
{
public:
    enum class State {
        init,
        disconnected,
        connecting,
        connected
    };
    using CmdCallback    = std::function<void(const Packet::BlockView&)>;
    using StatusCallback = std::function<void(State, const std::error_code&)>;

    Slave(common::Logger logger, const Identification& id, std::unique_ptr<Transport> transport);
    ~Slave();

    State state() const {return statemachine_.curr_state();};
    const Identification& master_id() const {return master_id_;}
    const Identification& slave_id()  const {return slave_id_;}

    void start();
    void stop() override;
    void set_cmd_cb(CmdCallback&& cb)       {cmd_cb_   = std::forward<CmdCallback>(cb);}
    void set_status_cb(StatusCallback&& cb) {status_cb_ = std::forward<StatusCallback>(cb);}

    void send_data(std::vector<Packet::BlockView>&);
    void send_data(std::vector<Packet::Block>&);

private:
    using common::Thread::start;

    const common::Statemachine<State>::StateList states_ {
        {"init", State::init,
            {{ State::init,         std::bind(&Slave::handler_state_init, this)         },
             { State::disconnected, std::bind(&Slave::check_true, this)                 }}
        },
        {"disconnected", State::disconnected,
            {{ State::disconnected, std::bind(&Slave::handler_state_disconnected, this) },
             { State::disconnected, std::bind(&Slave::check_transport_closed, this)     },
             { State::connecting,   std::bind(&Slave::check_hip_received, this)         }}
        },
        {"connecting", State::connecting,
            {{ State::connecting,   std::bind(&Slave::handler_state_connecting, this)   },
             { State::disconnected, std::bind(&Slave::check_transport_closed, this)     },
             { State::disconnected, std::bind(&Slave::check_ka_timeout, this)           },
             { State::connected,    std::bind(&Slave::check_connected, this)            }}
        },
        {"connected", State::connected,
            {{ State::connected,    std::bind(&Slave::handler_state_connected, this)    },
             { State::disconnected, std::bind(&Slave::check_transport_closed, this)     },
             { State::disconnected, std::bind(&Slave::check_ka_timeout, this)           },
             { State::connecting,   std::bind(&Slave::check_hip_received, this)         }}
        }
    };

    common::transition_status handler_state_init();
    common::transition_status handler_state_disconnected();
    common::transition_status handler_state_connecting();
    common::transition_status handler_state_connected();

    common::transition_status check_true();
    common::transition_status check_hip_received();
    common::transition_status check_connected();
    common::transition_status check_ka_timeout();
    common::transition_status check_transport_closed();

    enum class Event {
        hip_received,
        first_ka_received,
        ka_timeout,
    };
    common::EventMngr<Event> evt_mngr_;

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
};

} /* namespace appli  */
} /* namespace hdcp */
