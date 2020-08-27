#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "common/log.h"

#include "transport.h"
#include "application.h"

namespace hdcp {

class Packet
{
public:
    using Id        = uint16_t;
    using BlockType = uint16_t;

    // reserved block types
    static const BlockType id_name          = 0x0001;
    static const BlockType id_serial_number = 0x0002;
    static const BlockType id_hw_version    = 0x0003;
    static const BlockType id_sw_version    = 0x0004;

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
    Packet& operator=(const Packet&);
    virtual ~Packet();

    Id      get_id()       const {return get_header()->id;};
    uint8_t get_ver()      const {return get_header()->ver;};
    Type    get_type()     const {return get_header()->type;};
    uint8_t get_nb_block() const {return get_header()->n_block;}

    std::vector<Block> get_blocks() const;
    const std::string& get_data() const {return data_;};

    static Packet make_command(Id, BlockType, const std::string&);
    static Packet make_cmd_ack(Id, BlockType, Id);
    static Packet make_data(Id, std::vector<Block>& blocks);
    static Packet make_keepalive(Id id);
    static Packet make_keepalive_ack(Id id);
    static Packet make_hip(Id id, const hdcp::Identification& host_id);
    static Packet make_dip(Id id, const hdcp::Identification& dev_id);

private:
    friend std::ostream& operator<<(std::ostream& out, const Packet& p);
    using Crc = uint16_t;

    static const uint16_t sop = 0xCAFE;
    static const uint8_t  ver = 0x01;

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

    static std::string make_header(Id id, Type type, uint8_t n_block, const std::string& payload);
    static std::string make_block(BlockType type, const std::string& data);
    static std::string make_block(Block& b);

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
