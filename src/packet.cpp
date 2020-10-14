#include "packet.h"
#include "hdcp/exception.h"

namespace hdcp {

Packet::Packet(std::string_view v)
{
    // find start of packet
    auto pos = v.find(reinterpret_cast<const char*>(&sop), 0, sizeof(sop));
    if (pos == std::string::npos)
        throw hdcp::packet_error("sop not found");

    if (pos + sizeof(Header) > v.size())
        throw hdcp::packet_error("buffer too small for header");

    const Header * header = parse_header(std::string_view(v.data() + pos, sizeof(Header)));
    if (pos + sizeof(Header) + header->len > v.size())
        throw hdcp::packet_error("buffer too small for payload");

    std::string_view payload(v.data() + pos + sizeof(Header), header->len);
    parse_payload(payload, header);

    // buffer is valid, copy data
    std::copy(v.begin(), v.begin() + sizeof(Header) + header->len, data_.begin());
}

Packet Packet::make_command(Id id, BlockType type, const std::string& data)
{
    std::string payload(Packet::make_block(type, data));
    std::string header(Packet::make_header(id, Packet::Type::cmd, 1, payload));
    return Packet(header + payload);
}

Packet Packet::make_cmd_ack(Id id, BlockType type, Id cmd_id)
{
    std::string data(reinterpret_cast<char*>(&cmd_id), sizeof(Id));
    std::string payload(Packet::make_block(type, data));
    std::string header(Packet::make_header(id, Packet::Type::cmd_ack, 1, payload));
    return Packet(header + payload);
}

Packet Packet::make_data(Id id, std::vector<BlockView>& blocks)
{
    std::string payload;
    for (auto& b: blocks) {
        payload += Packet::make_block(b);
    }
    std::string header(Packet::make_header(id, Packet::Type::data, blocks.size(), payload));
    return Packet(header + payload);
}

Packet Packet::make_data(Id id, std::vector<Block>& blocks)
{
    std::string payload;
    for (auto& b: blocks) {
        payload += Packet::make_block(b);
    }
    std::string header(Packet::make_header(id, Packet::Type::data, blocks.size(), payload));
    return Packet(header + payload);
}

Packet Packet::make_keepalive(Id id)
{
    std::string payload;
    std::string header(Packet::make_header(id, Packet::Type::ka, 0, payload));
    return Packet(header + payload);
}

Packet Packet::make_keepalive_ack(Id id)
{
    std::string payload;
    std::string header(Packet::make_header(id, Packet::Type::ka_ack, 0, payload));
    return Packet(header + payload);
}

Packet Packet::make_hip(Id id, const hdcp::Identification& host_id)
{
    std::string payload;
    payload += Packet::make_block(ReservedBlockType::name, host_id.name);
    payload += Packet::make_block(ReservedBlockType::serial_number, host_id.serial_number);
    payload += Packet::make_block(ReservedBlockType::hw_version, host_id.hw_version);
    payload += Packet::make_block(ReservedBlockType::sw_version, host_id.sw_version);
    std::string header(Packet::make_header(id, Packet::Type::hip, 4, payload));
    return Packet(header + payload);
}

Packet Packet::make_dip(Id id, const hdcp::Identification& dev_id)
{
    std::string payload;
    payload += Packet::make_block(ReservedBlockType::name, dev_id.name);
    payload += Packet::make_block(ReservedBlockType::serial_number, dev_id.serial_number);
    payload += Packet::make_block(ReservedBlockType::hw_version, dev_id.hw_version);
    payload += Packet::make_block(ReservedBlockType::sw_version, dev_id.sw_version);
    std::string header(Packet::make_header(id, Packet::Type::dip, 4, payload));
    return Packet(header + payload);
}

Packet::Crc Packet::compute_crc(std::string_view v)
{
    uint8_t CRCA = 0, CRCB = 0;

    for (const auto& c: v) {
        CRCA = CRCA + *reinterpret_cast<const uint8_t*>(&c);
        CRCB = CRCB + CRCA;
    }

    return CRCA << 8 | CRCB;
}

std::string Packet::make_header(Id id, Type type, uint8_t n_block, const std::string& payload)
{
    Header h = {
        .sop  = sop,
        .ver  = ver,
        .len  = static_cast<uint16_t>(payload.size()),
        .id   = id,
        .type = type,
        .n_block = n_block,
        .p_crc = compute_crc(std::string_view(payload.data(), payload.size())),
        .h_crc = 0
    };

    char * it = reinterpret_cast<char*>(&h);
    std::string_view header_view(it, sizeof(Packet::Header) - sizeof(Crc));
    h.h_crc = compute_crc(header_view);

    return std::string(it, it + sizeof(h));
}

std::string Packet::make_block(BlockType type, const std::string& data)
{
    BlockView b;
    b.type = type;
    b.data = data;
    return Packet::make_block(b);
}

std::string Packet::make_block(BlockView b)
{
    char * it = reinterpret_cast<char*>(&b.type);
    std::string type_str(it, it + sizeof(BlockType));

    uint16_t len = b.data.size();
    it = reinterpret_cast<char*>(&len);
    std::string len_str(it, it + sizeof(len));

    std::string ret = type_str + len_str;
    ret.append(b.data);

    return ret;
}

std::vector<Packet::BlockView> Packet::blocks(std::string_view payload)
{
    std::vector<BlockView> blocks;
    auto it = payload.begin();
    while (it + sizeof(BHeader) <= payload.end()) {
        const BHeader * header = reinterpret_cast<const BHeader*>(it);
        it += sizeof(Packet::BHeader);
        if (it + header->len > payload.end())
            break;
        BlockView b;
        b.type = header->type;
        b.data = std::string_view(it, header->len);
        it += header->len;
        blocks.push_back(b);
    }

    return blocks;
}

const Packet::Header * Packet::parse_header(std::string_view v)
{
    if (v.size() != sizeof(Packet::Header))
        throw hdcp::packet_error(fmt::format("buffer size != header size: {}", v.size()));

    const Header * h = reinterpret_cast<const Header*>(v.data());
    if (compute_crc(std::string_view(v.data(), v.size()-sizeof(Crc))) != h->h_crc)
        throw hdcp::packet_error("wrong header crc");

    if (h->sop != sop)
        throw hdcp::packet_error("sop not at beginning of buffer");

    if (h->ver != ver)
        throw hdcp::packet_error(fmt::format("wrong protocol version: {}", h->ver));

    switch (h->type) {
    case Packet::Type::hip:
    case Packet::Type::dip:
    case Packet::Type::ka:
    case Packet::Type::ka_ack:
    case Packet::Type::cmd:
    case Packet::Type::cmd_ack:
    case Packet::Type::data:
        break;
    default:
        throw hdcp::packet_error(fmt::format("unknown packet type: {:#x}", h->type));
    }

    if (h->len > max_size - sizeof(Header))
        throw hdcp::packet_error(fmt::format("payload length too big {}", h->len));

    return h;
}

void Packet::parse_header() const
{
    parse_header(std::string_view(data_.data(), sizeof(Packet::Header)));
}

void Packet::parse_payload(std::string_view v, const Header * h)
{
    if (h->len != v.size())
        throw hdcp::packet_error(fmt::format("wrong payload length: {} should be {}",
                                             h->len, v.size()));

    if (h->p_crc != compute_crc(v))
        throw hdcp::packet_error("wrong payload crc");

    auto b = blocks(v);
    if (h->n_block != b.size())
        throw hdcp::packet_error(fmt::format("wrong number of blocks: {} should be {}",
                                 b.size(), h->n_block));
}

void Packet::parse_payload() const
{
    parse_payload(payload(), header());
}

std::ostream& operator<<(std::ostream& out, const Packet& p)
{
    out << fmt::format("\npacket {}: type={:#x}, with {} block(s) (prot ver:{})\n",
                       p.id(), p.type(), p.nb_block(), p.version());
    int i = 0;
    for (auto& b: p.blocks()) {
        out << fmt::format("\tblock {}: type {:#x}, len {} -> {:#x}\n",
                           i++, b.type, b.data.length(),
                           fmt::join((uint8_t*)b.data.data(),
                                     (uint8_t*)b.data.data() + b.data.length(), "|"));
    }
    return out;
}

} /* namespace hdcp */
