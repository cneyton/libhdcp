#pragma once

#include <libusb-1.0/libusb.h>
#include "boost/asio.hpp"

#include "exception.h"

namespace hdcp {
namespace transport {

enum class Errc {
    write_queue_full = 1,
    read_queue_full,
    write_while_closed,
    internal,
    other,
};

struct ErrorCategory: public std::error_category
{
    const char * name() const noexcept override {return "transport";};
    std::string message(int e) const   override;
};

struct UsbErrCategory: public std::error_category
{
    const char * name() const noexcept override {return "usb";}
    std::string message(int e) const   override {return libusb_error_name(e);}
};

std::error_code make_error_code(Errc e);

} /* namespace transport */

struct transport_error: public hdcp_error
{
    transport_error(transport::Errc e): hdcp_error(make_error_code(e)) {}
};

struct libusb_error: public hdcp_error
{
    libusb_error(int code);
};

struct asio_error: public hdcp_error
{
    asio_error(const boost::system::error_code& ec): hdcp_error(std::error_code(ec)) {}
};

} /* namespace hdcp */

namespace std {

template <>
struct is_error_code_enum<hdcp::transport::Errc> : true_type {};

} /* namespace std */
