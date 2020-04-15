#include "request.h"
#include "hdcp/exception.h"
#include <mutex>

using namespace hdcp;

void RequestManager::send_command(Packet::BlockType type, std::string& data,
                                  Request::Callback request_cb, std::chrono::milliseconds timeout)
{
    common::TimeoutQueue::Id id =
        timeout_queue_.add_repeating(now_, timeout.count()/time_base_ms_,
                                     std::bind(&RequestManager::cmd_timeout_cb, this,
                                               std::placeholders::_1, std::placeholders::_2));

    // send command
    Packet cmd = Packet::make_command(++packet_id_, type, data);
    transport_.write(cmd.get_data());

    // add request to the set
    {
        std::unique_lock<std::mutex> lk(requests_mutex_);
        const auto [it, success] = requests_.insert({id, cmd, std::move(request_cb)});
        if (!success)
            throw hdcp::application_error("a request with the same packet id is pending");
    }
}

void RequestManager::manage_hip(std::chrono::milliseconds timeout)
{
    dip_timeout_flag_ = false;
    dip_id_ = timeout_queue_.add(now_, timeout.count()/time_base_ms_,
                                 std::bind(&RequestManager::dip_timeout_cb, this,
                                           std::placeholders::_1, std::placeholders::_2));
}

void RequestManager::start_keepalive_management(std::chrono::milliseconds keepalive_interval,
                                std::chrono::milliseconds keepalive_timeout)
{
    ka_timeout_flag_ = false;
    keepalive_mngt_id_ =
        timeout_queue_.add_repeating(now_, keepalive_interval.count()/time_base_ms_,
                                     std::bind(&RequestManager::ka_mngt_timeout_cb, this,
                                               std::placeholders::_1, std::placeholders::_2));
    // send keepalive
    Packet ka = Packet::make_keepalive(++packet_id_);
    transport_.write(ka.get_data());
    // set timeout
    timeout_keepalive_ = keepalive_timeout.count()/time_base_ms_;
    keepalive_id_ = timeout_queue_.add(now_, timeout_keepalive_,
                                       std::bind(&RequestManager::ka_timeout_cb, this,
                                                 std::placeholders::_1, std::placeholders::_2));
}

void RequestManager::stop_keepalive_management()
{
    timeout_queue_.erase(keepalive_mngt_id_);
}

void RequestManager::ack_command(Packet& packet)
{
    std::unique_lock<std::mutex> lk(requests_mutex_);
    auto& set_by_command = requests_.get<by_command>();
    auto search = set_by_command.find(packet.get_id());
    if (search == set_by_command.end())
        throw hdcp::application_error(fmt::format("request with packet id {} not found",
                                                  packet.get_id()));
    Request r(*search);
    r.set_status(Request::Status::fulfilled);
    r.set_ack(packet);
    r.call_callback();

    timeout_queue_.erase(r.get_id());
    set_by_command.erase(packet.get_id());
}

void RequestManager::ack_keepalive()
{
    timeout_queue_.erase(keepalive_id_);
}

void RequestManager::ack_dip()
{
    timeout_queue_.erase(dip_id_);
}

void RequestManager::cmd_timeout_cb(common::TimeoutQueue::Id id, int64_t now)
{
    std::unique_lock<std::mutex> lk(requests_mutex_);
    auto& set_by_request = requests_.get<by_request>();
    auto search = set_by_request.find(id);
    if (search == set_by_request.end())
        throw hdcp::application_error("request id not found, you should not be here");

    if (search->get_retry() <= max_retry_) {
        log_warn(logger_, "");
        set_by_request.modify(search, std::bind(&Request::inc_retry, *search));
        transport_.write(search->get_command().get_data());
    } else {
        log_error(logger_, "");
        Request r(*search);
        r.set_status(Request::Status::timeout);
        r.call_callback();
        timeout_queue_.erase(r.get_id());
        set_by_request.erase(id);
    }
}

void RequestManager::ka_timeout_cb(common::TimeoutQueue::Id id, int64_t now)
{
    ka_timeout_flag_ = true;
}

void RequestManager::dip_timeout_cb(common::TimeoutQueue::Id id, int64_t now)
{
    dip_timeout_flag_ = true;
}

void RequestManager::ka_mngt_timeout_cb(common::TimeoutQueue::Id id, int64_t now)
{
    // send keepalive
    Packet ka = Packet::make_keepalive(++packet_id_);
    transport_.write(ka.get_data());
    keepalive_id_ = timeout_queue_.add(now_, timeout_keepalive_,
                                       std::bind(&RequestManager::ka_timeout_cb, this,
                                                 std::placeholders::_1, std::placeholders::_2));
}

void RequestManager::run()
{
    now_ = 0;
    packet_id_ = 0;
    while (is_running()) {
        timeout_queue_.run_once(now_++);
        std::this_thread::sleep_for(std::chrono::milliseconds(time_base_ms_));
    }
}
