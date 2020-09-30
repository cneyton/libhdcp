#pragma once

#include <stdexcept>
#include <libusb-1.0/libusb.h>
#include "boost/asio.hpp"

#include "common/log.h"

namespace hdcp {

class transport_error: public std::runtime_error
{
public:
    enum Code {
        write_queue_full,
        read_queue_full,
        not_permitted,
        internal,
        other,
    };

    transport_error(const std::string& what_arg, int code):
        std::runtime_error(what_arg), code_(code) {}
    int code() const {return code_;}

private:
    int code_;
};

class libusb_error: public transport_error
{
public:
    libusb_error(int code): transport_error(libusb_error_name(code), code) {}
};

class asio_error: public transport_error
{
public:
    asio_error(const boost::system::error_code& ec):
        transport_error(fmt::format("{} ({})", ec.message(), ec.value()), ec.value()) {}
};

class application_error: public std::runtime_error
{
public:
    application_error(const std::string& what_arg): std::runtime_error(what_arg) {}
};

class packet_error: public std::runtime_error
{
public:
    packet_error(const std::string& what_arg): std::runtime_error(what_arg) {}
};

} /* namespace hdcp */
