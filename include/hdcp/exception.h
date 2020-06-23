#pragma once

#include <stdexcept>
#include <libusb-1.0/libusb.h>
#include "boost/asio.hpp"

#include "common/log.h"

namespace hdcp
{

class libusb_error: public std::runtime_error
{
public:
    libusb_error(int error_code): std::runtime_error(libusb_error_name(error_code)) {}
};

class transport_error: public std::runtime_error
{
public:
    transport_error(const std::string& what_arg): std::runtime_error(what_arg) {}
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

class asio_error: public std::runtime_error
{
public:
    asio_error(const boost::system::error_code& ec):
        std::runtime_error(fmt::format("{} ({})", ec.message(), ec.value())) {}
};

} /* namespace hdcp */
