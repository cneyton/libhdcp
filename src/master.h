#pragma once

#include "common/log.h"
#include "common/statemachine.h"
#include "common/thread.h"
#include "common/event_mngr.h"

#include "master_request.h"
#include "transport.h"
#include "application.h"

namespace hdcp {
namespace appli {

class Master: public common::Log, private common::Thread
{
public:
    enum class State {
        init,
        disconnected,
        connecting,
        connected
    };
    using DataCallback   = std::function<void(const Packet&)>;
    using StatusCallback = std::function<void(State, const std::error_code&)>;

    Master(common::Logger logger, const Identification& id, std::unique_ptr<Transport> transport);
    ~Master();

    State state() const {return statemachine_.curr_state();};
    const Identification& master_id() const {return master_id_;}
    const Identification& slave_id()  const {return slave_id_;}

    void start();
    void stop() override;
    void set_data_cb(DataCallback&& cb)     {data_cb_   = std::forward<DataCallback>(cb);}
    void set_status_cb(StatusCallback&& cb) {status_cb_ = std::forward<StatusCallback>(cb);}
    void async_connect();
    void async_disconnect();
    void connect();
    void send_command(Packet::BlockType id, const std::string& data, Request::Callback cb);

private:
    using common::Thread::start;

    const common::Statemachine<State>::StateList states_ {
        {"init", State::init,
            {{ State::init,         std::bind(&Master::handler_state_init, this)         },
             { State::disconnected, std::bind(&Master::check_true, this)                 }}
        },
        {"disconnected", State::disconnected,
            {{ State::disconnected, std::bind(&Master::handler_state_disconnected, this) },
             { State::connecting,   std::bind(&Master::check_connection_requested, this) }}
        },
        {"connecting", State::connecting,
            {{ State::connecting,   std::bind(&Master::handler_state_connecting, this)   },
             { State::disconnected, std::bind(&Master::check_transport_closed, this)     },
             { State::disconnected, std::bind(&Master::check_dip_timeout, this)          },
             { State::connected,    std::bind(&Master::check_connected, this)            }}
        },
        {"connected", State::connected,
            {{ State::connected,    std::bind(&Master::handler_state_connected, this)    },
             { State::disconnected, std::bind(&Master::check_transport_closed, this)     },
             { State::disconnected, std::bind(&Master::check_ka_timeout, this)           },
             { State::disconnected, std::bind(&Master::check_disconnected, this)         }}
        }
    };

    common::transition_status handler_state_init();
    common::transition_status handler_state_disconnected();
    common::transition_status handler_state_connecting();
    common::transition_status handler_state_connected();

    common::transition_status check_true();
    common::transition_status check_connection_requested();
    common::transition_status check_connected();
    common::transition_status check_disconnected();
    common::transition_status check_dip_timeout();
    common::transition_status check_ka_timeout();
    common::transition_status check_transport_closed();

    enum class Event {
        connection_requested,
        disconnection_requested,
        dip_received,
        dip_timeout,
        ka_timeout,
        stop,
        connection_attempt
    };
    common::EventMngr<Event> evt_mngr_;

    common::Statemachine<State>   statemachine_;

    std::unique_ptr<Transport>    transport_;
    master::RequestManager        request_manager_;
    Identification                master_id_;
    Identification                slave_id_;
    Packet::Id                    received_packet_id_;
    uint                          connection_attempts_;

    DataCallback                  data_cb_;
    StatusCallback                status_cb_;
    std::error_code               errc_;

    void run() override;
    void set_slave_id(const Packet& p);
    void timeout_cb(master::RequestManager::TimeoutType);
};

} /* namespace appli  */
} /* namespace hdcp */
