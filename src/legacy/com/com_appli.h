#ifndef COM_APPLI_H
#define COM_APPLI_H

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <map>
#include <memory>

#include "common/thread.h"
#include "common/wait_queue.h"
#include "common/statemachine.h"
#include "common/log.h"

#include "packet.h"
#include "com_transport.h"

#include "data/BE8RawData.h"

class Request;
class PacketHandlerInterface;
class BaseComHW;

// TODO disable copy constructor
class ComAppli: public common::Thread
{
public:
    ComAppli(std::unique_ptr<BaseComHW> com_hw, PacketHandlerInterface * packet_handler,
             common::Logger logger);

    virtual ~ComAppli();

    enum class state {
        disconnected,
        connecting,
        connected
    };

    int SendRequestSync(const Request& packet, int msTimeout, Packet& returnPacket);

    virtual void run();

    // Communication handler --------------------------------------------------
    int   connect();
    int   disconnect();
    state get_state() const;
    // ------------------------------------------------------------------------

    int process_packet(const Packet& packet);

private:
    int handler_state_disconnected_();
    int handler_state_connecting_();
    int handler_state_connected_();

    int check_connection_requested_();
    int check_connected_();
    int check_disconnected_();

    const common::StatesList<state> states_ {
        {"disconnected", state::disconnected,
            {{std::bind(&ComAppli::handler_state_disconnected_, this), state::disconnected},
             {std::bind(&ComAppli::check_connection_requested_, this), state::connecting}}
        },
        {"connecting", state::connecting,
            {{std::bind(&ComAppli::handler_state_connecting_, this),   state::connecting},
             {std::bind(&ComAppli::check_disconnected_, this),         state::disconnected},
             {std::bind(&ComAppli::check_connected_, this),            state::connected}}
        },
        {"connected", state::connected,
            {{std::bind(&ComAppli::handler_state_connected_, this),    state::connected},
             {std::bind(&ComAppli::check_disconnected_, this),         state::disconnected}}
        }
    };

    common::WaitQueue<Packet>      packet_queue_;
    common::Logger                 logger_;
    common::Statemachine<state>    statemachine_;

    ComTransport               transport_;
    PacketHandlerInterface   * packet_handler_;

    bool disconnect_             = false;
    bool connection_requested_   = false;
    bool connection_established_ = false;

    int queue_packet_(const Packet& packet);
    int process_queued_packets_();

    std::mutex              mutex_;
    std::mutex              mutex_run_;
    std::condition_variable cv_;
    std::condition_variable cv_run_;

    std::map<packet::type, Packet> received_map_;
    packet::type                   response_type_;

    int  wait_response(int ms, packet::type type, Packet& packetAnswer);
    void signal_packet(packet::type type, const Packet& packet);
};



#endif // COM_APPLI_H
