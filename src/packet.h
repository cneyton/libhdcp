#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "common/log.h"

#include "packet_error.h"
#include "application.h"

namespace hdcp {

class Packet
{
public:
    using Id        = uint16_t;
    using BlockType = uint16_t;

    enum ReservedBlockType: BlockType {
        name          = 0x0001,
        serial_number = 0x0002,
        hw_version    = 0x0003,
        sw_version    = 0x0004
    };

    struct BlockView;
    struct Block
    {
        explicit operator BlockView() const noexcept {return BlockView();};
        size_t size() {return data.size() + sizeof(BHeader);};
        BlockType        type;
        std::string      data;
    };

    /// Non-owning view of a block inside a packet
    struct BlockView
    {
        BlockView() = default;
        BlockView(const Block& from): type(from.type), data(from.data) {};
        size_t size() {return data.size() + sizeof(BHeader);};

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

    Packet(std::string_view);
    Packet()                         = default;
    Packet(const Packet&)            = default;
    Packet& operator=(const Packet&) = default;
    Packet(Packet&&)                 = default;
    Packet& operator=(Packet&&)      = default;

    Id       id()       const {return header()->id;}
    uint8_t  version()  const {return header()->ver;}
    Type     type()     const {return header()->type;}
    uint8_t  nb_block() const {return header()->n_block;}
    size_t   size()     const {return payload().size() + sizeof(Header);}
    char *   data()           {return data_.data();}
    const char * data() const {return data_.data();}
    std::vector<BlockView> blocks() const {return blocks(payload());};
    std::string_view header_view()  const {return std::string_view(data_.data(), sizeof(Header));};
    std::string_view payload() const
    {
        return std::string_view(data_.data() + sizeof(Packet::Header), header()->len);
    }

    void parse_header() const;
    void parse_payload() const;

    static Packet make_command(Id, BlockType, const std::string&);
    static Packet make_cmd_ack(Id, BlockType, Id);
    static Packet make_data(Id, std::vector<BlockView>& blocks);
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

public:
    static const size_t max_size    = 2048;
    static const size_t max_pl_size = max_size - sizeof(Header);

private:
    static Crc compute_crc(std::string_view);
    static std::string make_header(Id id, Type type, uint8_t n_block, const std::string& payload);
    static std::string make_block(BlockType type, const std::string& data);
    static std::string make_block(BlockView b);
    static std::vector<BlockView> blocks(std::string_view);
    static const Header * parse_header(std::string_view);
    static void parse_payload(std::string_view, const Header *);

    const Header * header() const
    {
        return reinterpret_cast<const Header*>(data_.data());
    }

    std::array<char, max_size> data_;
};

} /* namespace hdcp */
