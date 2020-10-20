#pragma once

#include <stdexcept>
#include <libusb-1.0/libusb.h>
#include "boost/asio.hpp"

#include "common/log.h"

namespace hdcp {

class hdcp_error: public std::runtime_error
{
public:
    hdcp_error(const std::string& what_arg): std::runtime_error(what_arg) {}
};

class base_transport_error: public hdcp_error
{
public:
    base_transport_error(const std::error_code& e):
        hdcp_error(e.message()), errc_(e) {}

    const std::error_code& code() const noexcept {return errc_;}

private:
    std::error_code errc_;
};


enum class TransportErrc {
    write_queue_full = 1,
    read_queue_full,
    not_permitted,
    internal,
    other,
};

struct TransportErrCategory: public std::error_category
{
    const char * name() const noexcept override {return "transport";}
    std::string message(int e) const override
    {
        switch (static_cast<TransportErrc>(e)) {
        case TransportErrc::write_queue_full: return "write queue full";
        case TransportErrc::read_queue_full:  return "read queue full";
        case TransportErrc::not_permitted:    return "not permitted";
        case TransportErrc::internal:         return "internal";
        case TransportErrc::other:            return "other";
        default: return "unknown error code";
        }
    }
};

std::error_code make_error_code(TransportErrc e);

class transport_error: public base_transport_error
{
public:
    transport_error(TransportErrc e):
        base_transport_error(make_error_code(e)) {}
};

struct UsbErrCategory: public std::error_category
{
    const char * name() const noexcept override {return "usb";}
    std::string message(int e) const   override {return libusb_error_name(e);}
};

class libusb_error: public base_transport_error
{
public:
    libusb_error(int code);
};

class asio_error: public base_transport_error
{
public:
    asio_error(const boost::system::error_code& ec):
        base_transport_error(std::error_code(ec)) {}
};

enum class ApplicationErrc {
    timeout = 1,
    connection_failed,
    request_overrun,
    request_not_found,
    invalid_cmd_ack_format,
    data_too_big,
    not_permitted,
};

struct ApplicationErrCategory: public std::error_category
{
    const char * name() const noexcept override {return "application";}
    std::string message(int e) const   override
    {
        switch (static_cast<ApplicationErrc>(e)) {
        case ApplicationErrc::timeout:                return "timeout";
        case ApplicationErrc::connection_failed:      return "connection failed";
        case ApplicationErrc::request_overrun:        return "request overrun";
        case ApplicationErrc::request_not_found:      return "request not found";
        case ApplicationErrc::invalid_cmd_ack_format: return "invalid command ack format";
        case ApplicationErrc::data_too_big:           return "data too big";
        case ApplicationErrc::not_permitted:          return "not permitted";
        default:                                      return "unknown error code";
        }
    }
};


std::error_code make_error_code(ApplicationErrc e);

class application_error: public hdcp_error
{
public:
    application_error(const std::error_code& e): hdcp_error(e.message()), errc_(e) {}
    application_error(const std::error_code& e, const std::string& what_arg):
        hdcp_error(e.message() + ":" + what_arg), errc_(e) {}
    application_error(ApplicationErrc e): application_error(make_error_code(e)) {}
    application_error(ApplicationErrc e, const std::string& what_arg):
        application_error(make_error_code(e), what_arg) {}

    const std::error_code& code() const noexcept {return errc_;}

private:
    std::error_code errc_;
};

class packet_error: public hdcp_error
{
public:
    packet_error(const std::string& what_arg): hdcp_error(what_arg) {}
};

} /* namespace hdcp */

namespace std {

template <>
struct is_error_code_enum<hdcp::TransportErrc> : true_type {};

template <>
struct is_error_code_enum<hdcp::ApplicationErrc> : true_type {};

} /* namespace std */
