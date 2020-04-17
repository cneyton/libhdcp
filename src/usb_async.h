#pragma once

#include <libusb-1.0/libusb.h>

#include "common/log.h"
#include "common/readerwriterqueue.h"
#include "common/thread.h"

#include "hdcp/transport.h"

namespace hdcp
{

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
    virtual void submit();

    bool in_progress() const {return in_progress_;};
    void put_on_hold()       {in_progress_ = false;};

private:
    std::atomic_bool in_progress_ = false;
};


class UsbAsync: public common::Log, public common::Thread, public Transport
{
public:
    UsbAsync(common::Logger logger, int itfc_nb,
             uint16_t vendor_id, uint16_t product_id,
             uint8_t in_endoint, uint8_t out_endpoint);
    virtual ~UsbAsync();

    virtual void write(const std::string& buf);
    virtual void write(std::string&& buf);
    virtual bool read(std::string& buf);
    virtual void stop();
    virtual void start();

private:
    libusb_context       * ctx_ = nullptr;
    libusb_device_handle * device_handle_ = nullptr;

    uint16_t vendor_id_;
    uint16_t product_id_;
    int      itfc_nb_;
    uint8_t  in_endoint_;
    uint8_t  out_endpoit_;

    common::ReaderWriterQueue<std::string>         write_queue_;
    common::BlockingReaderWriterQueue<std::string> read_queue_;

    RTransfer * rtransfer_curr_ = nullptr;
    RTransfer * rtransfer_prev_ = nullptr;
    WTransfer * wtransfer_      = nullptr;

    void fill_transfer(WTransfer& transfer, std::string&& buf);
    void fill_transfer(RTransfer& transfer);
    static void write_cb(libusb_transfer * transfer);
    static void read_cb(libusb_transfer * transfer);

    void open();
    void close();

    virtual void run();
};

} /* namespace hdcp */
