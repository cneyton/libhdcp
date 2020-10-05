#pragma once

#include "common/log.h"
#include "common/statemachine.h"
#include "common/thread.h"

#include "request.h"
#include "transport.h"
#include "application.h"

namespace hdcp {
namespace appli {

class Master: public common::Log, private common::Thread
{
public:
    using DataCallback  = std::function<void(const Packet&)>;
    using ErrorCallback = std::function<void(int)>;
    enum class State {
        init,
        disconnected,
        connecting,
        connected
    };

    Master(common::Logger logger, const Identification& id, std::unique_ptr<Transport> transport);
    ~Master();

    State get_state() const {return statemachine_.get_state();};
    const Identification& get_slave_id()  const {return slave_id_;}
    const Identification& get_master_id() const {return master_id_;}

    void start();
    void stop() override;
    void set_data_cb(DataCallback&& cb)   {data_cb_  = std::forward<DataCallback>(cb);}
    void set_error_cb(ErrorCallback&& cb) {error_cb_ = std::forward<ErrorCallback>(cb);}
    /// Synchronous connect
    const Identification& connect();
    /// Synchronous disconnect
    void disconnect();
    /// Send command asynchronously
    void send_command(Packet::BlockType id, const std::string& data, Request::Callback cb);

private:
    using common::Thread::start;

    int handler_state_init();
    int handler_state_disconnected();
    int handler_state_connecting();
    int handler_state_connected();

    int check_true();
    int check_connection_requested();
    int check_connected();
    int check_disconnected();

    const common::StatesList<State> states_ {
        {"init", State::init,
            {{std::bind(&Master::handler_state_init, this),         State::init},
             {std::bind(&Master::check_true, this),                 State::disconnected}}
        },
        {"disconnected", State::disconnected,
            {{std::bind(&Master::handler_state_disconnected, this), State::disconnected},
             {std::bind(&Master::check_connection_requested, this), State::connecting}}
        },
        {"connecting", State::connecting,
            {{std::bind(&Master::handler_state_connecting, this),   State::connecting},
             {std::bind(&Master::check_disconnected, this),         State::disconnected},
             {std::bind(&Master::check_connected, this),            State::connected}}
        },
        {"connected", State::connected,
            {{std::bind(&Master::handler_state_connected, this),    State::connected},
             {std::bind(&Master::check_disconnected, this),         State::disconnected}}
        }
    };

    // flags
    std::atomic_bool connection_requested_    = false;
    std::atomic_bool dip_received_            = false;
    std::atomic_bool disconnection_requested_ = false;

    std::mutex              mutex_disconnection_;
    std::condition_variable cv_disconnection_;
    std::mutex              mutex_connection_;
    std::condition_variable cv_connection_;
    std::mutex              mutex_connecting_;
    std::condition_variable cv_connecting_;

    common::Statemachine<State>   statemachine_;
    std::unique_ptr<Transport>    transport_;
    master::RequestManager        request_manager_;
    Identification                master_id_;
    Identification                slave_id_;
    DataCallback                  data_cb_;
    ErrorCallback                 error_cb_;

    void run() override;
    void wait_connection_request();
    void set_slave_id(const Packet& p);
    void timeout_cb(master::RequestManager::TimeoutType);
    void transport_error_cb(std::exception_ptr);
};

} /* namespace appli  */
} /* namespace hdcp */
