#ifndef PACKET_H
#define PACKET_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "hdcp/hdcp.h"

namespace hdcp
{


class Packet
{
public:
    using Id        = uint16_t;
    using BlockType = uint16_t;

    struct Block
    {
        BlockType        type;
        std::string_view data;
    };

    enum class Type: uint8_t
    {
        hip     = 0x01, // host identication packet
        dip     = 0x41, // device identification packet
        ka      = 0x02, // keep-alive
        ka_ack  = 0x42, // keep-alive ack
        cmd     = 0x03, // command
        cmd_ack = 0x43, // command ack
        data    = 0x44, // data
    };

    Packet(const std::string&);
    Packet(std::string&&);
    Packet(const Packet&);
    Packet(Packet&&);
    virtual ~Packet();

    Id      get_id()       const {return get_header()->id;};
    Type    get_type()     const {return get_header()->type;};
    uint8_t get_nb_block() const {return get_header()->n_block;}

    std::vector<Block> get_blocks() const;
    const std::string& get_data() const {return data_;};

    static Packet make_command(Id, BlockType, std::string&);
    static Packet make_keepalive(Id id);
    static Packet make_hip(Id id, hdcp::Identification);

private:
    using Crc = uint16_t;

    static const uint16_t sop = 0xCAFE;
    static const uint8_t  ver = 0x01;

    // reserved block types
    static const BlockType name          = 0x0001;
    static const BlockType serial_number = 0x0002;
    static const BlockType hw_version    = 0x0003;
    static const BlockType sw_version    = 0x0004;

    struct Header
    {
        uint16_t sop;

        uint8_t  ver;
        uint16_t len;
        Id       id;
        Type     type;
        uint8_t  n_block;
        Crc      p_crc;
        Crc      h_crc;
    }__attribute__((packed));

    struct BHeader
    {
        BlockType type;
        uint16_t  len;
    }__attribute__((packed));


    static Crc compute_crc(std::string_view data);
    Crc compute_hcrc() const;
    Crc compute_pcrc() const;

    static std::string make_header(Id id, Type type, uint8_t n_block, std::string& payload);
    static std::string make_block(BlockType type, std::string& data);

    std::string data_;

    const Header * get_header() const
    {
         return reinterpret_cast<const Header*>(data_.data());
    }

    std::string_view get_payload() const
    {
        return std::string_view(data_.data() + sizeof(Packet::Header),
                                data_.size() - sizeof(Packet::Header));
    }

    void validate_packet() const;
};

} /* namespace hdcp */

#endif /* PACKET_H */
