#include "request.h"

Request::Request(const Packet & packet, uint8_t responseId):
    m_packet(packet), m_expected_response(responseId)
{

}
