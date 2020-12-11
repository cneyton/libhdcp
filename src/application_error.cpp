#include "application_error.h"

namespace hdcp {
namespace appli {

const ErrorCategory appli_error_category {};

std::string ErrorCategory::message(int e) const
{
    switch (static_cast<Errc>(e)) {
    case Errc::ka_timeout:                return "keepalive timeout";
    case Errc::dip_timeout:               return "device identification packet timeout";
    case Errc::request_overrun:           return "request overrun";
    case Errc::request_not_found:         return "request not found";
    case Errc::invalid_cmd_ack_format:    return "invalid command ack format";
    case Errc::data_too_big:              return "data too big";
    case Errc::write_while_disconnected:  return "writing is not permitted while disconnected";
    case Errc::connection_failed:         return "connection failed";
    default:                              return "unknown error code";
    }
}

std::error_code make_error_code(Errc e)
{
    return {static_cast<int>(e), appli_error_category};
}

} /* namespace appli */
} /* namespace hdcp */
