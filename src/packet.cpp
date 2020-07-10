#include "packet.h"
#include "hdcp/exception.h"
#include "hdcp/hdcp.h"

using namespace hdcp;

Packet::Packet(const std::string& buf): data_(buf)
{
    validate_packet();
}

Packet::Packet(std::string&& buf): data_(std::forward<std::string>(buf))
{
    validate_packet();
}

Packet::Packet(const Packet& other): data_(other.data_)
{
}

Packet::Packet(Packet&& other): data_(std::move(other.data_))
{
}

Packet& Packet::operator=(const Packet& p)
{
    if (this == &p)
        return *this;
    data_ = p.get_data();
    return *this;
}

Packet::~Packet()
{
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
    payload += Packet::make_block(id_name, host_id.name);
    payload += Packet::make_block(id_serial_number, host_id.serial_number);
    payload += Packet::make_block(id_hw_version, host_id.hw_version);
    payload += Packet::make_block(id_sw_version, host_id.sw_version);
    std::string header(Packet::make_header(id, Packet::Type::hip, 4, payload));
    return Packet(header + payload);
}

Packet Packet::make_dip(Id id, const hdcp::Identification& dev_id)
{
    std::string payload;
    payload += Packet::make_block(id_name, dev_id.name);
    payload += Packet::make_block(id_serial_number, dev_id.serial_number);
    payload += Packet::make_block(id_hw_version, dev_id.hw_version);
    payload += Packet::make_block(id_sw_version, dev_id.sw_version);
    std::string header(Packet::make_header(id, Packet::Type::dip, 4, payload));
    return Packet(header + payload);
}

Packet::Crc Packet::compute_crc(std::string_view data)
{
    uint8_t CRCA = 0, CRCB = 0;

    for (const auto& c: data) {
        CRCA = CRCA + *reinterpret_cast<const uint8_t*>(&c);
        CRCB = CRCB + CRCA;
    }

    return CRCA << 8 | CRCB;
}

Packet::Crc Packet::compute_hcrc() const
{
    std::string_view h(data_.data(), sizeof(Packet::Header) - sizeof(Crc));
    return compute_crc(h);
}

Packet::Crc Packet::compute_pcrc() const
{
    return compute_crc(get_payload());
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
        .p_crc = Packet::compute_crc(std::string_view(payload.data(), payload.size())),
        .h_crc = 0
    };

    char * it = reinterpret_cast<char*>(&h);
    std::string_view header_view(it, sizeof(Packet::Header) - sizeof(Crc));
    h.h_crc = Packet::compute_crc(header_view);

    return std::string(it, it + sizeof(h));
}

std::string Packet::make_block(BlockType type, const std::string& data)
{
    Block b = {
        .type = type,
        .data = data
    };
    return Packet::make_block(b);
}

std::string Packet::make_block(Block& b)
{
    char * it = reinterpret_cast<char*>(&b.type);
    std::string type_str(it, it + sizeof(Block::type));

    uint16_t len = b.data.size();
    it = reinterpret_cast<char*>(&len);
    std::string len_str(it, it + sizeof(len));

    std::string ret = type_str + len_str;
    ret.append(b.data);

    return ret;
}

void Packet::validate_packet() const
{
    if (data_.size() < sizeof(Packet::Header) || data_.size() > max_transfer_size)
        throw hdcp::packet_error(fmt::format("invalid buffer size: {}", data_.size()));

    const Header * h = get_header();
    if (compute_hcrc() != h->h_crc)
        throw hdcp::packet_error("wrong header crc");
    if (h->sop != sop)
        throw hdcp::packet_error("wrong start of packet");
    if (h->ver != ver)
        throw hdcp::packet_error("wrong protocol version");
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
        throw hdcp::packet_error("wrong packet type");
    }

    uint16_t len = data_.size() - sizeof(Packet::Header);
    if (h->len != len)
        throw hdcp::packet_error(fmt::format("wrong payload length: {} != {}", h->len, len));

    if (h->p_crc != compute_pcrc())
        throw hdcp::packet_error("wrong payload crc");

    if (h->n_block != get_blocks().size())
        throw hdcp::packet_error("wrong number of blocks");
}

std::vector<Packet::Block> Packet::get_blocks() const
{
    std::string_view payload(get_payload());

    std::vector<Block> blocks;
    auto it = payload.begin();
    while (it + sizeof(BHeader) <= payload.end()) {
        const BHeader * header = reinterpret_cast<const BHeader*>(it);
        it += sizeof(Packet::BHeader);
        if (it + header->len > payload.end())
            throw hdcp::packet_error("wrong block length");
        Block b = {
            .type = header->type,
            .data = std::string_view(it, header->len),
        };
        it += header->len;
        blocks.push_back(b);
    }

    return blocks;
}
