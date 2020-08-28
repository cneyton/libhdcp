#pragma once

#include <chrono>
#include <string>

#include "common/readerwriterqueue.h"
#include "common/concurrentqueue.h"

#include "packet.h"

namespace hdcp {

constexpr std::size_t max_queue_size    = 100;
constexpr std::chrono::milliseconds time_base_ms(100);
constexpr uint64_t timeout_read  = 0; // no timeout when reading
constexpr uint64_t timeout_write = time_base_ms.count();

class Transport
{
public:
    Transport(): write_queue_(max_queue_size), read_queue_(max_queue_size)
    {
    }
    virtual ~Transport()                       = default;
    Transport(const Transport&)                = delete;
    Transport& operator=(const Transport&)     = delete;
    Transport(Transport&&)                     = delete;
    Transport& operator=(Transport&&)          = delete;
    virtual void write(Packet&&) = 0;
    virtual void write(const Packet& p) {write(Packet(p));};
    bool read(Packet& p)
    {
        //if (!is_running())
            //throw hdcp::transport_error("not allowed to read when transport is stopped");
        return read_queue_.wait_dequeue_timed(p, time_base_ms);
    }
    virtual void start()                       = 0;
    virtual void stop()                        = 0;
    bool is_open() {return open_;};

    void clear_queues()
    {
        while (read_queue_.pop()) {}
        Packet p;
        while (write_queue_.try_dequeue(p)) {}
    }

protected:
    bool open_ = false;

    common::ConcurrentQueue<Packet>           write_queue_;
    common::BlockingReaderWriterQueue<Packet> read_queue_;
};

} /* namespace hdcp */
