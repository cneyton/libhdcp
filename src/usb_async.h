#pragma once

#include <libusb-1.0/libusb.h>

#include "common/log.h"
#include "common/thread.h"

#include "transport.h"

namespace hdcp {

class Transfer
{
public:
    Transfer();
    virtual ~Transfer();

    virtual void submit();
    libusb_transfer * get_libusb_transfer() const {return transfer_;};

private:
    libusb_transfer * transfer_ = nullptr;
};

class RTransfer: public Transfer
{
public:
    RTransfer(libusb_device_handle * device_handle);
    virtual ~RTransfer();

    uint8_t * get_buffer() const {return buf_;};

private:
    uint8_t * buf_ = nullptr;
    libusb_device_handle * device_handle_ = nullptr;
};

class WTransfer: public Transfer
{
public:
    void submit() override;

    void    set_packet(Packet&& p) {p_ = std::forward<Packet>(p);};
    Packet& get_packet() {return p_;};

    bool in_progress() const {return in_progress_;};
    void put_on_hold()       {in_progress_ = false;};

private:
    Packet p_;
    std::atomic_bool in_progress_ = false;
};


class UsbAsync: public common::Log, private common::Thread, public Transport
{
public:
    UsbAsync(common::Logger logger, int itfc_nb,
             uint16_t vendor_id, uint16_t product_id,
             uint8_t in_endpoint, uint8_t out_endpoint);
    virtual ~UsbAsync();

    void write(Packet&& p) override;
    void stop()    override;
    void start()   override;
    bool is_open() override;
    void open()    override;
    void close()   override;

private:
    using common::Thread::start;

    libusb_context       * ctx_ = nullptr;
    libusb_device_handle * device_handle_ = nullptr;

    RTransfer * rtransfer_curr_ = nullptr;
    RTransfer * rtransfer_prev_ = nullptr;
    WTransfer * wtransfer_      = nullptr;

    bool open_ = false;

    int      itfc_nb_;
    uint16_t vendor_id_, product_id_;
    uint8_t  in_endoint_, out_endpoit_;

    void fill_transfer(WTransfer * transfer);
    void fill_transfer(RTransfer * transfer);
    static void write_cb(libusb_transfer * transfer);
    static void read_cb(libusb_transfer * transfer);

    void run() override;
};

} /* namespace hdcp */
