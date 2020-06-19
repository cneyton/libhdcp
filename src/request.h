#pragma once

#include <chrono>
#include <atomic>

#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include "common/log.h"
#include "common/timeout_queue.h"
#include "common/thread.h"

#include "transport.h"
#include "packet.h"

namespace hdcp
{

class Request
{
public:
    using Callback = std::function<void(Request&)>;

    Request(common::TimeoutQueue::Id id, Packet& cmd, Callback callback):
        id_(id), command_(cmd), cb_(callback) {}

    virtual ~Request() {}

    enum class Status {
        pending,
        timeout,
        fulfilled,
    };

    Status                   get_status()     const {return status_;}
    common::TimeoutQueue::Id get_id()         const {return id_;}
    Packet::Id               get_command_id() const {return command_.get_id();}
    const Packet&            get_command()    const {return command_;}
    Packet*                  get_ack()        const {return ack_;}
    uint                     get_retry()      const {return retry_;}

    void set_status(Status s) {status_ = s;}
    void set_ack(Packet& ack) {ack_ = &ack;}
    void inc_retry()          {retry_++;}

    void call_callback()      {cb_(*this);}

private:
    common::TimeoutQueue::Id  id_;
    Packet                    command_;
    Callback                  cb_;
    Packet                  * ack_    = nullptr;
    Status                    status_ = Status::pending;
    uint                      retry_  = 0;
};

class RequestManager: public common::Log, public common::Thread
{
public:
    RequestManager(common::Log logger, Transport* transport):
        Log(logger), transport_(transport) {}
    virtual ~RequestManager() {}

    void send_command(Packet::BlockType type, const std::string& data,
                      Request::Callback request_cb, std::chrono::milliseconds timeout);
    void send_cmd_ack(Packet& packet);
    void send_data(std::vector<Packet::Block>& blocks);
    void send_hip(const Identification& id, std::chrono::milliseconds timeout);
    void send_dip(const Identification& id);
    void start_master_keepalive_management(std::chrono::milliseconds keepalive_interval,
                                           std::chrono::milliseconds keepalive_timeout);
    void start_slave_keepalive_management(std::chrono::milliseconds keepalive_timeout);
    void stop_master_keepalive_management();
    void stop_slave_keepalive_management();
    void ack_command(Packet& packet);
    void ack_dip();
    void ack_keepalive();
    void keepalive();

    bool keepalive_timeout() const {return ka_timeout_flag_;}
    bool dip_timeout()       const {return dip_timeout_flag_;}

private:
    static const uint max_retry_    = 4;

    struct by_request {};
    struct by_command {};
    typedef boost::multi_index_container<
        Request,
        boost::multi_index::indexed_by<
            boost::multi_index::ordered_unique<
            boost::multi_index::tag<by_request>,
            boost::multi_index::const_mem_fun<Request, common::TimeoutQueue::Id, &Request::get_id>>,
        boost::multi_index::ordered_unique<
            boost::multi_index::tag<by_command>,
            boost::multi_index::const_mem_fun<Request, Packet::Id, &Request::get_command_id>>>>
            Set;

    int64_t                 now_ = 0;
    std::atomic<Packet::Id> packet_id_ = 0;

    int64_t timeout_keepalive_;

    Set requests_;
    std::mutex requests_mutex_;

    common::TimeoutQueue     timeout_queue_;
    common::TimeoutQueue::Id keepalive_id_;
    common::TimeoutQueue::Id keepalive_mngt_id_;
    common::TimeoutQueue::Id dip_id_;
    std::atomic_bool ka_timeout_flag_  = false;
    std::atomic_bool dip_timeout_flag_ = false;

    Transport* transport_;

    /*
     * This cb resend a keep-alive every timeout_keepalive
     */
    void ka_mngt_timeout_cb(common::TimeoutQueue::Id id, int64_t now);

    void cmd_timeout_cb(common::TimeoutQueue::Id id, int64_t now);
    void ka_timeout_cb(common::TimeoutQueue::Id id, int64_t now);
    void dip_timeout_cb(common::TimeoutQueue::Id id, int64_t now);

    virtual void run();
    void clear();
};

} /* namespace hdcp */
