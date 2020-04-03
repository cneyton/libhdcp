#ifndef PACKET_H
#define PACKET_H

#include <cstdint>
#include <vector>

#include <gsl/gsl>

#include "common/log.h"


using RawBuffer = std::vector<uint8_t>;

namespace packet
{
    enum class reading_state {
        sop0,
        sop1,
        header,
        data,
        crc,
        filled,
    };

    enum class type: uint8_t {
        unknown         = 0x00,
        ack             = 0x01,
        nack            = 0x02,
        wave            = 0x10,
        text            = 0x12,

        seq_config      = 0x13,
        dev_config      = 0x14,
        dev_info        = 0x15,
        dev_status      = 0x16,

        fe_tgc          = 0x17,
        fe_profile      = 0x18,
        fe_coef         = 0x19,
        fe_config       = 0x1A,

        reg_config      = 0x20,

        ping            = 0x30,
        pong            = 0x32,

        cmd_poll        = 0x82,

        cmd_seq_config  = 0x83,
        cmd_control     = 0x84,
        cmd_dev_config  = 0x86,

        cmd_read_reg    = 0x87,
        cmd_write_reg   = 0x88,

        cmd_fe_tgc      = 0x90,
        cmd_fe_profile  = 0x91,
        cmd_fe_coef     = 0x92,
        cmd_fe_config   = 0x93,

        cmd_ping        = 0xA0,
        cmd_pong        = 0xA1,
    };
}

class Packet
{
public:
    Packet();
    Packet(common::Logger logger);
    Packet(uint8_t type, uint8_t identifier, uint16_t length, uint8_t * bytes);
    Packet(packet::type type, uint8_t cnt, RawBuffer data);
    //Packet(Packet&&) = delete;

    bool is_header_valid();
    bool is_data_valid();
    int read_header();
    int clear();
    packet::reading_state    get_state()  const;
    packet::type             get_type()   const;
    uint16_t                 get_length() const;
    const RawBuffer&         get_raw()    const;
    gsl::span<const uint8_t> get_data()   const;

    RawBuffer::const_iterator find_sop0  (const RawBuffer& buffer, RawBuffer::const_iterator it);
    RawBuffer::const_iterator find_sop1  (const RawBuffer& buffer, RawBuffer::const_iterator it);
    RawBuffer::const_iterator fill_header(const RawBuffer& buffer, RawBuffer::const_iterator it);
    RawBuffer::const_iterator fill_data  (const RawBuffer& buffer, RawBuffer::const_iterator it);
    RawBuffer::const_iterator fill_crc   (const RawBuffer& buffer, RawBuffer::const_iterator it);


private:
    uint16_t compute_crc();

    packet::reading_state reading_state_;

    packet::type  type_;
    uint8_t  counter_;
    uint16_t data_length_;
    uint16_t crc_;

    RawBuffer raw_data_;

    common::Logger logger_ = nullptr;
};

#endif /* PACKET_H */
