#include "hdcp/exception.h"
#include "request.h"

namespace hdcp {

void inc_retry(Request& r)
{
    r.retry_++;
}

namespace appli {
namespace master {

void RequestManager::start()
{
    if (is_running())
        return;
    log_debug(logger_, "starting request manager...");
    clear();
    common::Thread::start(true);
    log_debug(logger_, "request manager started");
}

void RequestManager::stop()
{
    if (!is_running())
        return;
    log_debug(logger_, "stopping request manager...");
    common::Thread::stop();
    if (joinable())
        join();
    log_debug(logger_, "request manager stopped");
}

void RequestManager::send_command(Packet::BlockType type, const std::string& data,
                                  Request::Callback request_cb,
                                  std::chrono::milliseconds timeout)
{
    common::TimeoutQueue::Id id =
        timeout_queue_.add_repeating(now_, timeout.count()/time_base_ms.count(),
                                     std::bind(&RequestManager::cmd_timeout_cb, this,
                                               std::placeholders::_1, std::placeholders::_2));

    // reset keepalive mngt
    timeout_queue_.erase(keepalive_mngt_id_);
    keepalive_mngt_id_ =
        timeout_queue_.add_repeating(now_, keepalive_interval.count()/time_base_ms.count(),
                                     std::bind(&RequestManager::ka_mngt_timeout_cb, this,
                                               std::placeholders::_1, std::placeholders::_2));

    // send command
    Packet cmd = Packet::make_command(++packet_id_, type, data);
    if (transport_)
        transport_->write(cmd);

    // add request to the set
    {
        std::unique_lock<std::mutex> lk(requests_mutex_);
        if (!requests_.insert({id, cmd, std::move(request_cb)}).second)
            throw application_error(ApplicationErrc::request_overrun,
                                    fmt::format("id {} is pending", cmd.id()));
    }
}

void RequestManager::send_hip(const Identification& id, std::chrono::milliseconds timeout)
{
    if (transport_ && transport_->is_open())
        transport_->write(Packet::make_hip(++packet_id_, id));
    // set timeout
    dip_id_ = timeout_queue_.add(now_, timeout.count()/time_base_ms.count(),
                                 std::bind(&RequestManager::dip_timeout_cb, this,
                                           std::placeholders::_1, std::placeholders::_2));
}

void RequestManager::start_keepalive_management(std::chrono::milliseconds keepalive_interval,
                                                std::chrono::milliseconds keepalive_timeout)
{
    keepalive_mngt_id_ =
        timeout_queue_.add_repeating(now_, keepalive_interval.count()/time_base_ms.count(),
                                     std::bind(&RequestManager::ka_mngt_timeout_cb, this,
                                               std::placeholders::_1, std::placeholders::_2));
    if (transport_ && transport_->is_open())
        transport_->write(Packet::make_keepalive(++packet_id_));
    // set timeout
    timeout_keepalive_ = keepalive_timeout.count()/time_base_ms.count();
    auto id = timeout_queue_.add(now_, timeout_keepalive_,
                                 std::bind(&RequestManager::ka_timeout_cb, this,
                                           std::placeholders::_1, std::placeholders::_2));
    std::unique_lock<std::mutex> lk(mutex_id_);
    ka_ids_.clear();
    ka_ids_.push_back(id);
}

void RequestManager::stop_keepalive_management()
{
    timeout_queue_.erase(keepalive_mngt_id_);
}

void RequestManager::ack_command(Packet& packet)
{
    // get the first block, it should have the same block type of cmd sent
    // and the data should correspond to its packet id
    auto block = packet.blocks().at(0);
    if (block.data.size() != sizeof(Packet::Id))
        throw application_error(ApplicationErrc::invalid_cmd_ack_format);
    const Packet::Id id = *reinterpret_cast<const Packet::Id*>(block.data.data());

    std::unique_lock<std::mutex> lk(requests_mutex_);
    auto& set_by_command = requests_.get<by_command>();
    auto search = set_by_command.find(id);
    if (search == set_by_command.end())
        throw application_error(ApplicationErrc::request_not_found,
                                fmt::format("id {} not found", id));
    Request r(*search);
    r.set_status(Request::Status::fulfilled);
    r.set_ack(packet);
    r.call_callback();

    timeout_queue_.erase(r.get_id());
    set_by_command.erase(id);
}

void RequestManager::ack_keepalive()
{
    std::unique_lock<std::mutex> lk(mutex_id_);
    for (auto id: ka_ids_)
        timeout_queue_.erase(id);
    ka_ids_.clear();
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
        throw hdcp::application_error(ApplicationErrc::request_not_found);

    if (search->get_retry() < max_retry_) {
        log_warn(logger_, "command {} timeout, try = {}", search->get_command().id(),
                 search->get_retry());
        set_by_request.modify(search, &inc_retry);
        if (transport_ && transport_->is_open())
            transport_->write(search->get_command());

        // reset keepalive mngt
        timeout_queue_.erase(keepalive_mngt_id_);
        keepalive_mngt_id_ =
            timeout_queue_.add_repeating(now_, keepalive_interval.count()/time_base_ms.count(),
                                         std::bind(&RequestManager::ka_mngt_timeout_cb, this,
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

void RequestManager::ka_timeout_cb(common::TimeoutQueue::Id, int64_t)
{
    log_error(logger_, "ka timeout");
    if (timeout_cb_)
        timeout_cb_(TimeoutType::ka_timeout);
}

void RequestManager::dip_timeout_cb(common::TimeoutQueue::Id, int64_t)
{
    log_error(logger_, "dip timeout");
    if (timeout_cb_)
        timeout_cb_(TimeoutType::dip_timeout);
}

void RequestManager::ka_mngt_timeout_cb(common::TimeoutQueue::Id, int64_t)
{
    // send keepalive
    if (transport_ && transport_->is_open())
        transport_->write(Packet::make_keepalive(++packet_id_));

    auto id = timeout_queue_.add(now_, timeout_keepalive_,
                                 std::bind(&RequestManager::ka_timeout_cb, this,
                                           std::placeholders::_1, std::placeholders::_2));
    std::unique_lock<std::mutex> lk(mutex_id_);
    ka_ids_.push_back(id);
}

void RequestManager::run()
{
    notify_running(0);
    while (is_running()) {
        timeout_queue_.run_once(now_++);
        std::this_thread::sleep_for(time_base_ms);
    }
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
}

} /* namespace master */

namespace slave {

void RequestManager::run()
{
    notify_running(0);
    while (is_running()) {
        timeout_queue_.run_once(now_++);
        std::this_thread::sleep_for(time_base_ms);
    }
}

void RequestManager::start()
{
    if (is_running())
        return;

    log_debug(logger_, "starting request manager...");
    clear();
    common::Thread::start(true);
    log_debug(logger_, "request manager started");
}

void RequestManager::stop()
{
    if (!is_running())
        return;
    log_debug(logger_, "stopping request manager...");
    common::Thread::stop();
    if (joinable())
        join();
    log_debug(logger_, "request manager stopped");
}

void RequestManager::send_cmd_ack(const Packet& packet)
{
    if (transport_ && transport_->is_open())
        transport_->write(Packet::make_cmd_ack(++packet_id_,
                                               packet.blocks().at(0).type, packet.id()));
}

void RequestManager::send_data(std::vector<Packet::BlockView>& blocks)
{
    std::vector<Packet::BlockView> payload;
    size_t payload_size = 0;
    for (auto& b: blocks) {
        if (b.size() > Packet::max_pl_size)
            throw application_error(ApplicationErrc::data_too_big);
        if (payload_size + b.size() > Packet::max_pl_size) {
            transport_->write(Packet::make_data(++packet_id_, payload));
            payload.clear();
            payload_size = 0;
        }
        payload.push_back(b);
        payload_size += b.size();
    }
    if (payload.size() != 0)
        transport_->write(Packet::make_data(++packet_id_, payload));
}

void RequestManager::send_data(std::vector<Packet::Block>& blocks)
{
    std::vector<Packet::BlockView> view;
    std::copy(blocks.begin(), blocks.end(), std::back_inserter(view));
    send_data(view);
}

void RequestManager::send_dip(const Identification& id)
{
    if (transport_ && transport_->is_open())
        transport_->write(Packet::make_dip(++packet_id_, id));
}

void RequestManager::start_keepalive_management(std::chrono::milliseconds keepalive_timeout)
{
    // set timeout
    timeout_keepalive_ = keepalive_timeout.count()/time_base_ms.count();
    keepalive_id_ = timeout_queue_.add(now_, timeout_keepalive_,
                                       std::bind(&RequestManager::ka_timeout_cb, this,
                                                 std::placeholders::_1, std::placeholders::_2));
}

void RequestManager::stop_keepalive_management()
{
    timeout_queue_.erase(keepalive_id_);
}

void RequestManager::keepalive()
{
    timeout_queue_.erase(keepalive_id_);
    keepalive_id_ = timeout_queue_.add(now_, timeout_keepalive_,
                                       std::bind(&RequestManager::ka_timeout_cb, this,
                                                 std::placeholders::_1, std::placeholders::_2));

    if (transport_ && transport_->is_open())
        transport_->write(Packet::make_keepalive_ack(++packet_id_));
}


void RequestManager::clear()
{
    timeout_queue_.clear();
    now_              = 0;
    packet_id_        = 0;
}

void RequestManager::ka_timeout_cb(common::TimeoutQueue::Id, int64_t)
{
    log_error(logger_, "ka timeout");
    if (timeout_cb_)
        timeout_cb_();
}

} /* namespace slave  */
} /* namespace appli  */
} /* namespace hdcp */
