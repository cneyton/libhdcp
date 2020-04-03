#ifndef COM_TRANSPORT_H
#define COM_TRANSPORT_H

#include <memory>

#include "base_com_hw.h"
#include "packet.h"
#include "common/log.h"


class ComAppli;

class ComTransport
{
public:
    ComTransport(ComAppli * parent, std::unique_ptr<BaseComHW> com_hw, common::Logger logger);
    ~ComTransport();

    int start();
    int run();
    int stop();
    bool active() const;


    int write_packet(Packet * packet);

private:
    common::Logger             logger_;
    Packet                     packet_;
    std::unique_ptr<BaseComHW> com_hw_;
    ComAppli  * parent_;

    bool active_ = false;

    std::vector<uint8_t> buffer_;
};

#endif // COM_TRANSPORT_H
