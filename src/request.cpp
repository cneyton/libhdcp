#include "hdcp/exception.h"
#include "request.h"
#include "slave.h"
#include "master.h"

namespace hdcp {

void MasterRequestManager::start()
{
    if (is_running())
        return;
    common::Thread::start(true);
}

void MasterRequestManager::stop()
{
    if (!is_running())
        return;
    common::Thread::stop();
    if (joinable())
        join();
}

void MasterRequestManager::send_command(Packet::BlockType type, const std::string& data,
                                        Request::Callback request_cb,
                                        std::chrono::milliseconds timeout)
{
    common::TimeoutQueue::Id id =
        timeout_queue_.add_repeating(now_, timeout.count()/time_base_ms.count(),
                                     std::bind(&MasterRequestManager::cmd_timeout_cb, this,
                                               std::placeholders::_1, std::placeholders::_2));


    // reset keepalive mngt
    timeout_queue_.erase(keepalive_mngt_id_);
    keepalive_mngt_id_ =
        timeout_queue_.add_repeating(now_, keepalive_interval.count()/time_base_ms.count(),
                                     std::bind(&MasterRequestManager::ka_mngt_timeout_cb, this,
                                               std::placeholders::_1, std::placeholders::_2));

    // send command
    Packet cmd = Packet::make_command(++packet_id_, type, data);
    if (transport_)
        transport_->write(cmd);

    // add request to the set
    {
        std::unique_lock<std::mutex> lk(requests_mutex_);
        if (!requests_.insert({id, cmd, std::move(request_cb)}).second)
            throw hdcp::application_error(fmt::format("a request with the same packet id {} is pending", cmd.id()));
    }
}

void MasterRequestManager::send_hip(const Identification& id, std::chrono::milliseconds timeout)
{
    if (transport_)
        transport_->write(Packet::make_hip(++packet_id_, id));
    // set timeout
    dip_id_ = timeout_queue_.add(now_, timeout.count()/time_base_ms.count(),
                                 std::bind(&MasterRequestManager::dip_timeout_cb, this,
                                           std::placeholders::_1, std::placeholders::_2));
}

void MasterRequestManager::start_keepalive_management(std::chrono::milliseconds keepalive_interval,
                                                      std::chrono::milliseconds keepalive_timeout)
{
    keepalive_mngt_id_ =
        timeout_queue_.add_repeating(now_, keepalive_interval.count()/time_base_ms.count(),
                                     std::bind(&MasterRequestManager::ka_mngt_timeout_cb, this,
                                               std::placeholders::_1, std::placeholders::_2));
    if (transport_)
        transport_->write(Packet::make_keepalive(++packet_id_));
    // set timeout
    timeout_keepalive_ = keepalive_timeout.count()/time_base_ms.count();
    keepalive_id_ = timeout_queue_.add(now_, timeout_keepalive_,
                                       std::bind(&MasterRequestManager::ka_timeout_cb, this,
                                                 std::placeholders::_1, std::placeholders::_2));
}

void MasterRequestManager::stop_keepalive_management()
{
    timeout_queue_.erase(keepalive_mngt_id_);
}

void MasterRequestManager::ack_command(Packet& packet)
{
    // get the first block, it should have the same block type of cmd sent
    // and the data should correspond to its packet id
    auto block = packet.blocks().at(0);
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
    set_by_command.erase(id);
}

void MasterRequestManager::ack_keepalive()
{
    timeout_queue_.erase(keepalive_id_);
}

void MasterRequestManager::ack_dip()
{
    timeout_queue_.erase(dip_id_);
}

void inc_retry(Request& r)
{
    r.retry_++;
}

void MasterRequestManager::cmd_timeout_cb(common::TimeoutQueue::Id id, int64_t)
{
    std::unique_lock<std::mutex> lk(requests_mutex_);
    auto& set_by_request = requests_.get<by_request>();
    auto search = set_by_request.find(id);
    if (search == set_by_request.end())
        throw hdcp::application_error("request id not found, you should not be here");

    if (search->get_retry() < max_retry_) {
        log_warn(logger_, "command {} timeout, try = {}", search->get_command().id(),
                 search->get_retry());
        set_by_request.modify(search, &inc_retry);
        if (!transport_)
            throw hdcp::application_error("transport null pointer");
        transport_->write(search->get_command());

        // reset keepalive mngt
        timeout_queue_.erase(keepalive_mngt_id_);
        keepalive_mngt_id_ =
            timeout_queue_.add_repeating(now_, keepalive_interval.count()/time_base_ms.count(),
                                         std::bind(&MasterRequestManager::ka_mngt_timeout_cb, this,
                                                   std::placeholders::_1, std::placeholders::_2));
    } else {
        log_error(logger_, "command {} failed", search->get_command().id());
        Request r(*search);
        r.set_status(Request::Status::timeout);
        r.call_callback();
        timeout_queue_.erase(r.get_id());
        set_by_request.erase(id);
    }
}

void MasterRequestManager::ka_timeout_cb(common::TimeoutQueue::Id, int64_t)
{
    if (master_)
        master_->keepalive_timed_out();
}

void MasterRequestManager::dip_timeout_cb(common::TimeoutQueue::Id, int64_t)
{
    if (master_)
        master_->dip_timed_out();
}

void MasterRequestManager::ka_mngt_timeout_cb(common::TimeoutQueue::Id, int64_t)
{
    // send keepalive
    if (transport_)
        transport_->write(Packet::make_keepalive(++packet_id_));

    keepalive_id_ = timeout_queue_.add(now_, timeout_keepalive_,
                                       std::bind(&MasterRequestManager::ka_timeout_cb, this,
                                                 std::placeholders::_1, std::placeholders::_2));
}

void MasterRequestManager::run()
{
    clear();
    notify_running(0);
    while (is_running()) {
        timeout_queue_.run_once(now_++);
        std::this_thread::sleep_for(time_base_ms);
    }
    clear();
}

void MasterRequestManager::clear()
{
    {
        std::unique_lock<std::mutex> lk(requests_mutex_);
        requests_.clear();
    }
    timeout_queue_.clear();
    now_              = 0;
    packet_id_        = 0;
}

void SlaveRequestManager::start()
{
    if (is_running())
        return;
    common::Thread::start(true);
}

void SlaveRequestManager::stop()
{
    if (!is_running())
        return;
    common::Thread::stop();
    if (joinable())
        join();
}

void SlaveRequestManager::send_cmd_ack(const Packet& packet)
{
    if (transport_)
        transport_->write(Packet::make_cmd_ack(++packet_id_,
                                               packet.blocks().at(0).type, packet.id()));
}

void SlaveRequestManager::send_data(std::vector<Packet::Block>& blocks)
{
    if (transport_)
        transport_->write(Packet::make_data(++packet_id_, blocks));
}

void SlaveRequestManager::send_dip(const Identification& id)
{
    if (transport_)
        transport_->write(Packet::make_dip(++packet_id_, id));
}

void SlaveRequestManager::start_keepalive_management(std::chrono::milliseconds keepalive_timeout)
{
    // set timeout
    timeout_keepalive_ = keepalive_timeout.count()/time_base_ms.count();
    keepalive_id_ = timeout_queue_.add(now_, timeout_keepalive_,
                                       std::bind(&SlaveRequestManager::ka_timeout_cb, this,
                                                 std::placeholders::_1, std::placeholders::_2));
}

void SlaveRequestManager::stop_keepalive_management()
{
    timeout_queue_.erase(keepalive_id_);
}

void SlaveRequestManager::keepalive()
{
    timeout_queue_.erase(keepalive_id_);
    keepalive_id_ = timeout_queue_.add(now_, timeout_keepalive_,
                                       std::bind(&SlaveRequestManager::ka_timeout_cb, this,
                                                 std::placeholders::_1, std::placeholders::_2));

    if (transport_)
        transport_->write(Packet::make_keepalive_ack(++packet_id_));
}


void SlaveRequestManager::clear()
{
    timeout_queue_.clear();
    now_              = 0;
    packet_id_        = 0;
}

void SlaveRequestManager::ka_timeout_cb(common::TimeoutQueue::Id, int64_t)
{
    if (slave_)
        slave_->keepalive_timed_out();
}

} /* namespace hdcp */
