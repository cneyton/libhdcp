#pragma once

#include "exception.h"

namespace hdcp {
namespace packet {

enum class Errc {
    sop_not_found = 1,
    invalid_buffer_size,
    invalid_header_crc,
    invalid_payload_crc,
    invalid_protocol_version,
    invalid_packet_type,
    invalid_number_of_block,
    payload_exceed_max_size,
};

struct ErrorCategory: public std::error_category
{
    const char * name() const noexcept override {return "packet";}
    std::string message(int e) const   override;
};

std::error_code make_error_code(Errc e);

} /* namespace packet */

struct packet_error: public hdcp_error
{
    packet_error(packet::Errc errc): hdcp_error(make_error_code(errc)) {}
    packet_error(packet::Errc errc, const std::string& what_arg):
        hdcp_error(make_error_code(errc), what_arg) {}
};

} /* namespace hdcp */

namespace std {

template <>
struct is_error_code_enum<hdcp::packet::Errc> : true_type {};

} /* namespace std */

