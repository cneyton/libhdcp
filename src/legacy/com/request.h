#ifndef REQUEST_H
#define REQUEST_H

#include <cstdint>
#include "packet.h"

class Request
{
public:
    Request(const Packet& packet, packet::type response_type)
        : packet_(packet), response_type_(response_type) {};

    Request(const Packet& packet, uint8_t response_type)
        : packet_(packet), response_type_(static_cast<packet::type>(response_type)) {};

    packet::type get_response_type() const
    {
        return response_type_;
    }

    const Packet get_packet() const
    {
        return packet_;
    }

private:
    Packet       packet_;
    packet::type response_type_;
};

#endif // REQUEST_H
