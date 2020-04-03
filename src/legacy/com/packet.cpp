#include "packet.h"
#include <cstdlib>
#include <string>
#include <sstream>

#define SOP0            0x6E // n
#define SOP1            0x40 // @

constexpr uint8_t SOP_SIZE = 2;
constexpr uint8_t PTY_SIZE = 1;
constexpr uint8_t PCT_SIZE = 1;
constexpr uint8_t D_L_SIZE = 2;
constexpr uint8_t HDR_SIZE = SOP_SIZE + PTY_SIZE + PCT_SIZE + D_L_SIZE;
//#define SOP_SIZE        2
//#define PTY_SIZE        1
//#define PCT_SIZE        1
//#define D_L_SIZE        2
//#define HDR_SIZE        (SOP_SIZE + PTY_SIZE + PCT_SIZE + D_L_SIZE)

#define D_SIZE_MAX      2000
#define D_CRC_SIZE      2
#define P_SIZE_MAX      (HDR_SIZE + D_SIZE_MAX + D_CRC_SIZE)

Packet::Packet()
{
    clear();
}

Packet::Packet(common::Logger logger): logger_(logger)
{
    raw_data_.reserve(P_SIZE_MAX);
    clear();
}

Packet::Packet(uint8_t type, uint8_t cnt, uint16_t len, uint8_t * bytes)
    : type_(static_cast<packet::type>(type)), counter_(cnt), data_length_(len)
{
    raw_data_.reserve(len + HDR_SIZE + D_CRC_SIZE);
    raw_data_.push_back(SOP0);
    raw_data_.push_back(SOP1);
    raw_data_.push_back(type);
    raw_data_.push_back(counter_);
    raw_data_.push_back((uint8_t)(data_length_ >> 8));
    raw_data_.push_back((uint8_t)data_length_);
    if (bytes != NULL)
        std::copy(bytes, bytes + len, std::back_inserter(raw_data_));
    crc_ = compute_crc();
    raw_data_.push_back((uint8_t)(crc_ >> 8));
    raw_data_.push_back((uint8_t)crc_);
}

Packet::Packet(packet::type type, uint8_t cnt, RawBuffer data)
    : type_(type), counter_(cnt), data_length_(data.size())
{
    raw_data_.reserve(P_SIZE_MAX);
    raw_data_.push_back(SOP0);
    raw_data_.push_back(SOP1);
    raw_data_.push_back(static_cast<uint8_t>(type_));
    raw_data_.push_back(counter_);
    raw_data_.push_back((uint8_t)(data_length_ >> 8));
    raw_data_.push_back((uint8_t)data_length_);
    std::copy(data.begin(), data.end(), std::back_inserter(raw_data_));
    crc_ = compute_crc();
    raw_data_.push_back((uint8_t)(crc_ >> 8));
    raw_data_.push_back((uint8_t)crc_);
}

RawBuffer::const_iterator Packet::find_sop0(const RawBuffer& buffer, RawBuffer::const_iterator it)
{
    auto found = std::find(it, buffer.end(), SOP0);

    if (found != buffer.end()){
        raw_data_.push_back(*found);
        reading_state_ = packet::reading_state::sop1;
        return found + 1;
    } else {
        return found;
    }
}

RawBuffer::const_iterator Packet::find_sop1(const RawBuffer& buffer, RawBuffer::const_iterator it)
{
    if (*it == SOP1) {
        raw_data_.push_back(*it);
        reading_state_ = packet::reading_state::header;
    } else {
        clear();
    }
    return it + 1;
}

RawBuffer::const_iterator Packet::fill_header(const RawBuffer& buffer, RawBuffer::const_iterator it)
{
    uint16_t lenght_header_left = HDR_SIZE - raw_data_.size();

    if (it + lenght_header_left > buffer.end()) {
        std::copy(it, buffer.end(), std::back_inserter(raw_data_));
        it = buffer.end();
    } else {
        std::copy(it, it + lenght_header_left, std::back_inserter(raw_data_));
        it = it + lenght_header_left;
    }

    if (raw_data_.size() == static_cast<size_t>(HDR_SIZE)) {
        reading_state_ = packet::reading_state::data;
    }

    return it;
}

RawBuffer::const_iterator Packet::fill_data(const RawBuffer& buffer, RawBuffer::const_iterator it)
{
    uint16_t lenght_data_left = data_length_ - (raw_data_.size() - HDR_SIZE);

    if (it + lenght_data_left > buffer.end()) {
        std::copy(it, buffer.end(), std::back_inserter(raw_data_));
        it = buffer.end();
    } else {
        std::copy(it, it + lenght_data_left, std::back_inserter(raw_data_));
        it = it + lenght_data_left;
    }

    if (raw_data_.size() == static_cast<size_t>(HDR_SIZE + data_length_)) {
        crc_ = compute_crc();
        reading_state_ = packet::reading_state::crc;
    }

    return it;
}

RawBuffer::const_iterator Packet::fill_crc(const RawBuffer& buffer, RawBuffer::const_iterator it)
{
    uint16_t lenght_crc_left = D_CRC_SIZE - (raw_data_.size() - (HDR_SIZE + data_length_));

    if (it + lenght_crc_left > buffer.end()) {
        std::copy(it, buffer.end(), std::back_inserter(raw_data_));
        it = buffer.end();
    } else {
        std::copy(it, it + lenght_crc_left, std::back_inserter(raw_data_));
        it = it + lenght_crc_left;
    }

    if (raw_data_.size() == static_cast<size_t>(HDR_SIZE + data_length_ + D_CRC_SIZE)) {
        reading_state_ = packet::reading_state::filled;
    }

    return it;
}


int Packet::clear()
{
    type_          = packet::type::unknown;
    counter_       = 0;
    data_length_   = 0;
    crc_           = 0;
    reading_state_ = packet::reading_state::sop0;
    raw_data_.clear();

    return 0;
}

packet::reading_state Packet::get_state() const
{
    return reading_state_;
}

packet::type Packet::get_type() const
{
    return type_;
}

uint16_t Packet::get_length() const
{
    return data_length_;
}

int Packet::read_header()
{
    if (raw_data_.size() < HDR_SIZE)
        common_die(logger_, -1, "raw data size too small to read header");

    type_    = static_cast<packet::type>(raw_data_[SOP_SIZE]);
    counter_ = raw_data_[SOP_SIZE + PTY_SIZE];
    data_length_ = ((uint16_t)raw_data_[SOP_SIZE + PTY_SIZE + PCT_SIZE] << 8)
                 +  (uint16_t)raw_data_[SOP_SIZE + PTY_SIZE + PCT_SIZE + 1];

    return 0;
}

bool Packet::is_header_valid()
{
    if (data_length_ > D_SIZE_MAX) {
        log_warn(logger_, "data length too big ({}), clearing packet", data_length_);
        return false;
    }
    return true;
}

bool Packet::is_data_valid()
{
    uint16_t crc_received = ((*(raw_data_.end() - 1)) << 8) | *(raw_data_.end());

    if (crc_ == crc_received)
        return true;
    return true; //TODO check crc
}

uint16_t Packet::compute_crc()
{
    uint8_t CRCA = 0, CRCB = 0;

    // skip sop to compute crc
    for (auto it = raw_data_.begin() + SOP_SIZE; it != raw_data_.end(); ++it) {
        CRCA = CRCA + *it;
        CRCB = CRCB + CRCA;
    }

    return CRCA << 8 | CRCB;
}

const RawBuffer& Packet::get_raw() const
{
    return raw_data_;
}

gsl::span<const uint8_t> Packet::get_data() const
{
    gsl::span<const uint8_t> span(raw_data_.data() + HDR_SIZE, data_length_);
    return span;
}

