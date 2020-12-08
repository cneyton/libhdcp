#include "packet_error.h"

namespace hdcp {
namespace packet {

const ErrorCategory packet_error_category {};

std::string ErrorCategory::message(int e) const
{
    switch (static_cast<Errc>(e)) {
    case Errc::sop_not_found:                return "start of packet not found in buffer";
    case Errc::invalid_buffer_size:          return "invalid buffer size";
    case Errc::invalid_header_crc:           return "invalid header crc";
    case Errc::invalid_payload_crc:          return "invalid payload crc";
    case Errc::invalid_protocol_version:     return "invalid protocol version";
    case Errc::invalid_packet_type:          return "invalid packet type";
    case Errc::invalid_number_of_block:      return "invalid number of blocks";
    case Errc::payload_exceed_max_size:      return "payload exceed max allowed size";
    default:                                 return "unknown error code";
    }
}

std::error_code make_error_code(Errc e)
{
    return {static_cast<int>(e), packet_error_category};
}

} /* namespace packet */
} /* namespace hdcp */
