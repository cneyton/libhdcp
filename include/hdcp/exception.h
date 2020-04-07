#pragma once

#include <stdexcept>
#include <libusb-1.0/libusb.h>

namespace hdcp
{

class libusb_error: public std::runtime_error
{
public:
    libusb_error(int error_code): std::runtime_error(libusb_error_name(error_code)) {}
};

class device_error: public std::runtime_error
{
public:
    device_error(const std::string& what_arg): std::runtime_error(what_arg) {}
};

class transport_error: public std::runtime_error
{
public:
    transport_error(const std::string& what_arg): std::runtime_error(what_arg) {}
};

} /* namespace hdcp */
