#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <chrono>
#include <string>

namespace hdcp
{

constexpr std::size_t max_transfer_size = 2048;
constexpr std::chrono::milliseconds time_base_ms(100);
constexpr uint timeout_read  = 0; // no timeout when reading
constexpr uint timeout_write = time_base_ms.count();

class Transport
{
public:
    Transport()                                = default;
    virtual ~Transport()                       = default;
    Transport(const Transport&)                = delete;
    Transport& operator=(const Transport&)     = delete;
    Transport(Transport&&)                     = delete;
    Transport& operator=(Transport&&)          = delete;
    virtual void write(const std::string& buf) = 0;
    virtual void write(std::string&& buf)      = 0;
    virtual bool read(std::string& buf)        = 0;
    virtual void start()                       = 0;
    virtual void stop()                        = 0;
};

} /* namespace hdcp */

#endif /* TRANSPORT_H */
