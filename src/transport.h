#pragma once

#include <chrono>
#include <string>
#include <bitset>

#include "common/readerwriterqueue.h"
#include "common/concurrentqueue.h"

#include "packet.h"

namespace hdcp {
namespace transport {

constexpr std::chrono::milliseconds time_base_ms(100);
constexpr uint64_t timeout_read  = 0; // no timeout when reading
constexpr uint64_t timeout_write = time_base_ms.count();

class Transport
{
public:
    Transport(): write_queue_(max_queue_size), read_queue_(max_queue_size) {};
    virtual ~Transport() = default;

    virtual void write(const Packet& p) {write(Packet(p));};
    bool read(Packet& p)
    {
        return read_queue_.wait_dequeue_timed(p, time_base_ms);
    }

    virtual void write(Packet&&) = 0;
    virtual void start()   = 0;
    virtual void stop()    = 0;
    virtual bool is_open() = 0;
    virtual void open()    = 0;
    virtual void close()   = 0;

protected:
    common::ConcurrentQueue<Packet>           write_queue_;
    common::BlockingReaderWriterQueue<Packet> read_queue_;

    void clear_queues()
    {
        while (read_queue_.pop()) {}
        Packet p;
        while (write_queue_.try_dequeue(p)) {}
    }

private:
    static constexpr std::size_t max_queue_size = 100;

    // disable copy ctor, copy assignment, move ctor & move assignment
    /* TODO: enable move <30-09-20, cneyton> */
    Transport(const Transport&)            = delete;
    Transport& operator=(const Transport&) = delete;
    Transport(Transport&&)                 = delete;
    Transport& operator=(Transport&&)      = delete;
};

} /* namespace transport */
} /* namespace hdcp */
