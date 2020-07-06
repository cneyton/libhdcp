#include "request.h"
#include "hdcp/exception.h"

using namespace hdcp;

void RequestManager::send_command(Packet::BlockType type, const std::string& data,
                                  Request::Callback request_cb, std::chrono::milliseconds timeout)
{
    if (!transport_)
        throw hdcp::application_error("transport null pointer");

    common::TimeoutQueue::Id id =
        timeout_queue_.add_repeating(now_, timeout.count()/time_base_ms.count(),
                                     std::bind(&RequestManager::cmd_timeout_cb, this,
                                               std::placeholders::_1, std::placeholders::_2));

    // send command
    Packet cmd = Packet::make_command(++packet_id_, type, data);
    transport_->write(cmd.get_data());

    // add request to the set
    {
        std::unique_lock<std::mutex> lk(requests_mutex_);
        const auto [it, success] = requests_.insert({id, cmd, std::move(request_cb)});
        if (!success)
            throw hdcp::application_error("a request with the same packet id is pending");
    }
}

void RequestManager::send_cmd_ack(const Packet& packet)
{
    if (!transport_)
        throw hdcp::application_error("transport null pointer");

    // send command ack
    Packet cmd_ack = Packet::make_cmd_ack(++packet_id_, packet.get_blocks().at(0).type,
                                          packet.get_id());
    transport_->write(cmd_ack.get_data());

}

void RequestManager::send_data(std::vector<Packet::Block>& blocks)
{
    if (!transport_)
        throw hdcp::application_error("transport null pointer");

    // send data
    Packet data = Packet::make_data(++packet_id_, blocks);
    transport_->write(data.get_data());
}

void RequestManager::send_hip(const Identification& id, std::chrono::milliseconds timeout)
{
    if (!transport_)
        throw hdcp::application_error("transport null pointer");
    dip_timeout_flag_ = false;
    // send hip
    Packet hip = Packet::make_hip(++packet_id_, id);
    transport_->write(hip.get_data());
    // set timeout
    dip_id_ = timeout_queue_.add(now_, timeout.count()/time_base_ms.count(),
                                 std::bind(&RequestManager::dip_timeout_cb, this,
                                           std::placeholders::_1, std::placeholders::_2));
}

void RequestManager::send_dip(const Identification& id)
{
    if (!transport_)
        throw hdcp::application_error("transport null pointer");
    // send dip
    Packet dip = Packet::make_dip(++packet_id_, id);
    transport_->write(dip.get_data());
}

void RequestManager::start_master_keepalive_management(std::chrono::milliseconds keepalive_interval,
                                                       std::chrono::milliseconds keepalive_timeout)
{
    if (!transport_)
        throw hdcp::application_error("transport null pointer");
    ka_timeout_flag_ = false;
    keepalive_mngt_id_ =
        timeout_queue_.add_repeating(now_, keepalive_interval.count()/time_base_ms.count(),
                                     std::bind(&RequestManager::ka_mngt_timeout_cb, this,
                                               std::placeholders::_1, std::placeholders::_2));
    // send keepalive
    Packet ka = Packet::make_keepalive(++packet_id_);
    transport_->write(ka.get_data());
    // set timeout
    timeout_keepalive_ = keepalive_timeout.count()/time_base_ms.count();
    keepalive_id_ = timeout_queue_.add(now_, timeout_keepalive_,
                                       std::bind(&RequestManager::ka_timeout_cb, this,
                                                 std::placeholders::_1, std::placeholders::_2));
}

void RequestManager::start_slave_keepalive_management(std::chrono::milliseconds keepalive_timeout)
{
    if (!transport_)
        throw hdcp::application_error("transport null pointer");
    ka_timeout_flag_ = false;
    // set timeout
    timeout_keepalive_ = keepalive_timeout.count()/time_base_ms.count();
    keepalive_id_ = timeout_queue_.add(now_, timeout_keepalive_,
                                       std::bind(&RequestManager::ka_timeout_cb, this,
                                                 std::placeholders::_1, std::placeholders::_2));
}

void RequestManager::stop_master_keepalive_management()
{
    timeout_queue_.erase(keepalive_mngt_id_);
}

void RequestManager::stop_slave_keepalive_management()
{
    timeout_queue_.erase(keepalive_id_);
}

void RequestManager::ack_command(Packet& packet)
{
    // get the first block, it should have the same block type of cmd sent
    // and the data should correspond to its packet id
    auto block = packet.get_blocks().at(0);
    if (block.data.size() != sizeof(Packet::Id))
        throw hdcp::application_error("first block of cmd ack should contain a packet id");
    const Packet::Id id = *reinterpret_cast<const Packet::Id*>(block.data.data());

    std::unique_lock<std::mutex> lk(requests_mutex_);
    auto& set_by_command = requests_.get<by_command>();
    auto search = set_by_command.find(id);
    if (search == set_by_command.end())
        throw hdcp::application_error(fmt::format("request with packet id {} not found", id));
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

void RequestManager::keepalive()
{
    timeout_queue_.erase(keepalive_id_);
    keepalive_id_ = timeout_queue_.add(now_, timeout_keepalive_,
                                       std::bind(&RequestManager::ka_timeout_cb, this,
                                                 std::placeholders::_1, std::placeholders::_2));
    // send keepalive ack
    Packet p = Packet::make_keepalive_ack(++packet_id_);
    transport_->write(p.get_data());
}

void RequestManager::ack_dip()
{
    timeout_queue_.erase(dip_id_);
}

void RequestManager::cmd_timeout_cb(common::TimeoutQueue::Id id, int64_t)
{
    std::unique_lock<std::mutex> lk(requests_mutex_);
    auto& set_by_request = requests_.get<by_request>();
    auto search = set_by_request.find(id);
    if (search == set_by_request.end())
        throw hdcp::application_error("request id not found, you should not be here");

    if (search->get_retry() <= max_retry_) {
        log_warn(logger_, "command {} timeout, try = {}", search->get_command().get_id(),
                 search->get_retry());
        set_by_request.modify(search, std::bind(&Request::inc_retry, *search));
        if (!transport_)
            throw hdcp::application_error("transport null pointer");
        transport_->write(search->get_command().get_data());
    } else {
        log_error(logger_, "command {} failed", search->get_command().get_id());
        Request r(*search);
        r.set_status(Request::Status::timeout);
        r.call_callback();
        timeout_queue_.erase(r.get_id());
        set_by_request.erase(id);
    }
}

void RequestManager::ka_timeout_cb(common::TimeoutQueue::Id, int64_t)
{
    log_warn(logger_, "keepalive timeout");
    ka_timeout_flag_ = true;
}

void RequestManager::dip_timeout_cb(common::TimeoutQueue::Id, int64_t)
{
    log_warn(logger_, "dip timeout");
    dip_timeout_flag_ = true;
}

void RequestManager::ka_mngt_timeout_cb(common::TimeoutQueue::Id, int64_t)
{
    if (!transport_)
        throw hdcp::application_error("transport null pointer");

    // send keepalive
    Packet ka = Packet::make_keepalive(++packet_id_);
    transport_->write(ka.get_data());
    keepalive_id_ = timeout_queue_.add(now_, timeout_keepalive_,
                                       std::bind(&RequestManager::ka_timeout_cb, this,
                                                 std::placeholders::_1, std::placeholders::_2));
}

void RequestManager::run()
{
    clear();
    notify_running(0);
    while (is_running()) {
        timeout_queue_.run_once(now_++);
        std::this_thread::sleep_for(time_base_ms);
    }
    clear();
}

void RequestManager::clear()
{
    {
        std::unique_lock<std::mutex> lk(requests_mutex_);
        requests_.clear();
    }
    timeout_queue_.clear();
    now_              = 0;
    packet_id_        = 0;
    ka_timeout_flag_  = false;
    dip_timeout_flag_ = false;
}
