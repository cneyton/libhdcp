#ifndef BASECOMHW_H
#define BASECOMHW_H

#include <cstdint>

#include "common/log.h"
#include "packet.h"

class BaseComHW
{
public:
    BaseComHW(common::Logger logger): logger_(logger) {}
    virtual ~BaseComHW(){}

    virtual int open() = 0;
    virtual int close() = 0;

    virtual int read(RawBuffer& buffer) = 0;
    virtual int write(const RawBuffer& buffer) = 0;

    uint32_t get_bytes_sent()
    {
        return m_bytes_sent;
    }

    uint32_t get_bytes_received()
    {
        return m_bytes_received;
    }

    bool is_active()
    {
        return m_hw_activated;
    }

    int reset_stats()
    {
        m_bytes_received = 0;
        m_bytes_sent     = 0;
        return 0;
    }

protected:
    void bytes_received_inc(uint32_t val)
    {
        m_bytes_received += val;
    }

    void bytes_sent_inc(uint32_t val)
    {
        m_bytes_sent += val;
    }

    bool    m_hw_activated = false;
    common::Logger  logger_;

private:
    uint32_t m_bytes_received = 0;
    uint32_t m_bytes_sent     = 0;
};

#endif // BASECOMHW_H
