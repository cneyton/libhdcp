#include "common/log.h"
#include "common/timeout_queue.h"
#include "common/thread.h"

#include "transport.h"
#include "packet.h"

namespace hdcp {
namespace appli {
namespace slave {

class RequestManager: public common::Log, public common::Thread
{
public:
    using TimeoutCallback = std::function<void()>;

    RequestManager(common::Log logger, Transport * transport, TimeoutCallback cb):
        Log(logger), transport_(transport), timeout_cb_(cb) {}

    void send_cmd_ack(const Packet& packet);
    void send_data(std::vector<Packet::BlockView>& blocks);
    void send_data(std::vector<Packet::Block>& blocks);
    void send_dip(const Identification& id);
    void start_keepalive_management(std::chrono::milliseconds keepalive_timeout);
    void stop_keepalive_management();
    void keepalive();

    void start();
    void stop() override;

private:
    using common::Thread::start;

    common::TimeoutQueue     timeout_queue_;
    common::TimeoutQueue::Id keepalive_id_;

    int64_t                 now_ = 0;
    std::atomic<Packet::Id> packet_id_ = 0;

    bool keepalive_mngt_ = false;

    int64_t timeout_keepalive_;

    Transport * transport_;

    TimeoutCallback timeout_cb_;

    void ka_timeout_cb(common::TimeoutQueue::Id id, int64_t now);

    void run() override;
    void clear();
};

} /* namespace slave */
} /* namespace appli */
} /* namespace hdcp */
