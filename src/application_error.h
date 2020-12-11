#pragma once

#include "exception.h"

namespace hdcp {
namespace appli {

enum class Errc {
    ka_timeout = 1,
    dip_timeout,
    request_overrun,
    request_not_found,
    invalid_cmd_ack_format,
    data_too_big,
    write_while_disconnected,
    connection_failed
};

struct ErrorCategory: public std::error_category
{
    const char * name() const noexcept override {return "application";}
    std::string message(int e) const   override;
};

std::error_code make_error_code(Errc e);

} /* namespace appli */

struct application_error: public hdcp_error
{
    application_error(appli::Errc errc): hdcp_error(make_error_code(errc)) {}
    application_error(appli::Errc errc, const std::string& what_arg):
        hdcp_error(make_error_code(errc), what_arg) {}
};

} /* namespace hdcp */

namespace std {

template <>
struct is_error_code_enum<hdcp::appli::Errc> : true_type {};

} /* namespace std */
