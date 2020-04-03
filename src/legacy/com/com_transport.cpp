#include "com_transport.h"

#include "com_appli.h"

ComTransport::ComTransport(ComAppli* parent, std::unique_ptr<BaseComHW> com_hw,
                           common::Logger logger):
   com_hw_(std::move(com_hw)), parent_(parent), logger_(logger), packet_(logger)
{
}

ComTransport::~ComTransport()
{
    stop();
}

int ComTransport::start()
{
    common_die_null(logger_, com_hw_, -1, "com_hw null ptr");

    int ret;
    ret = com_hw_->open();
    common_die_zero(logger_, ret, -2, "failed to opend hardware layer");

    active_ = true;
    return 0;
}


int ComTransport::run()
{
    if (!active_)
        common_die(logger_, -1, "transport layer not active");

    int ret;
    std::vector<uint8_t> buffer;

    ret = com_hw_->read(buffer);
    common_die_zero(logger_, ret, -2, "failed to read");

    RawBuffer::const_iterator it = buffer.begin();

    while (it != buffer.end()) {
        switch(packet_.get_state()) {
        case packet::reading_state::sop0:
            it = packet_.find_sop0(buffer, it);
            break;
        case packet::reading_state::sop1:
            it = packet_.find_sop1(buffer, it);
            break;
        case packet::reading_state::header:
            it = packet_.fill_header(buffer, it);
            break;
        case packet::reading_state::data:
            ret = packet_.read_header();
            if (packet_.is_header_valid()) {
                it = packet_.fill_data(buffer, it);
            } else {
                ret = packet_.clear();
                it = it + 1;
            }
            break;
        case packet::reading_state::crc:
            it = packet_.fill_crc(buffer, it);
            break;
        case packet::reading_state::filled:
            if (packet_.is_data_valid()) {
                ret = parent_->process_packet(packet_);
            } else {
                log_warn(logger_, "invalid packet");
            }
            ret = packet_.clear();
            break;
        default:
            ret = packet_.clear();
            common_die(logger_, -1, "unknown packet state, you should not be here");
            break;
        }
    }

    return 0;
}

int ComTransport::stop()
{
    if (!active_)
        return 0;

    active_ = false;
    common_die_null(logger_, com_hw_, -1, "com_hw null ptr");

    int ret = com_hw_->close();
    common_die_zero(logger_, ret, -2, "failed to close com_hw");

    return 0;
}

bool ComTransport::active() const
{
    return active_;
}

int ComTransport::write_packet(Packet * packet)
{
    common_die_null(logger_, com_hw_, -1, "com_hw null ptr");

    int ret = com_hw_->write(packet->get_raw());
    common_die_zero(logger_, ret, -2, "failed to write buffer");

    return 0;
}
