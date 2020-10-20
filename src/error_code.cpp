#include "hdcp/exception.h"

namespace hdcp {

const TransportErrCategory   transport_err_category {};
const UsbErrCategory         usb_err_category {};
const ApplicationErrCategory application_err_category {};

std::error_code make_error_code(TransportErrc e)
{
    return {static_cast<int>(e), transport_err_category};
}

std::error_code make_error_code(ApplicationErrc e)
{
    return {static_cast<int>(e), application_err_category};
}

libusb_error::libusb_error(int code):
        base_transport_error(std::error_code(code, usb_err_category))
{
}

} /* namespace hdcp */
