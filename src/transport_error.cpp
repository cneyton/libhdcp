#include "transport_error.h"

namespace hdcp {
namespace transport {

const ErrorCategory  transport_error_category;
const UsbErrCategory usb_error_category;

std::string ErrorCategory::message(int e) const
{
    switch (static_cast<Errc>(e)) {
    case Errc::write_queue_full:   return "write queue full";
    case Errc::read_queue_full:    return "read queue full";
    case Errc::write_while_closed: return "write is not permitted while transport closed";
    case Errc::internal:           return "internal";
    case Errc::other:              return "other";
    default:                       return "unknown error code";
    }
};

std::error_code make_error_code(Errc e)
{
    return {static_cast<int>(e), transport_error_category};
}


} /* namespace transport */

libusb_error::libusb_error(int code):
    hdcp_error(std::error_code(code, transport::usb_error_category))
{
}

} /* namespace hdcp */
