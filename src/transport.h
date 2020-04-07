#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <string>

constexpr std::size_t max_transfer_size = 2048;
constexpr uint timeout_read  = 4000;
constexpr uint timeout_write = 4000;
constexpr uint write_retry   = 5;

class Transport
{
public:
    virtual void open()  = 0;
    virtual void close() = 0;
    virtual void write(const std::string& buf)  = 0;
    virtual void write(const std::string&& buf) = 0;
    virtual std::string read()  = 0;
    bool is_open() const {return open_;};

protected:
    bool open_ = false;
};

#endif /* TRANSPORT_H */
