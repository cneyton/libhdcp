#include "application_error.h"
#include "slave_request.h"

namespace hdcp {
namespace appli {
namespace slave {

void RequestManager::run()
{
    notify_running();
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
    keepalive_mngt_ = false;
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
            throw application_error(appli::Errc::data_too_big);
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
    keepalive_mngt_ = true;
    // set timeout
    timeout_keepalive_ = keepalive_timeout.count()/time_base_ms.count();
    keepalive_id_ = timeout_queue_.add(now_, timeout_keepalive_,
                                       std::bind(&RequestManager::ka_timeout_cb, this,
                                                 std::placeholders::_1, std::placeholders::_2));
}

void RequestManager::stop_keepalive_management()
{
    keepalive_mngt_ = false;
    timeout_queue_.erase(keepalive_id_);
}

void RequestManager::keepalive()
{
    if (keepalive_mngt_) {
        timeout_queue_.erase(keepalive_id_);
        keepalive_id_ = timeout_queue_.add(now_, timeout_keepalive_,
                                           std::bind(&RequestManager::ka_timeout_cb, this,
                                                     std::placeholders::_1, std::placeholders::_2));
    }

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
