#ifndef USB_ASYNC_H
#define USB_ASYNC_H

#include <memory>
#include <queue>
#include <libusb-1.0/libusb.h>

#include "common/log.h"

#include "transport.h"

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
    RTransfer();
    virtual ~RTransfer();

    void      free_buffer(libusb_device_handle * device_handle);
    void      alloc_buffer(libusb_device_handle * device_handle);
    uint8_t * get_buffer() const {return buf_;};

private:
    uint8_t * buf_ = nullptr;
};

class WTransfer: public Transfer
{
public:
    virtual void submit();
    void resubmit();

    bool in_progress() const {return in_progress_;};
    void put_on_hold()       {in_progress_ = false;};

    int  n_retry() const     {return retry_;};

private:
    bool in_progress_ = false;
    int  retry_       = 0;
};


class UsbAsync: public common::Log, public Transport
{
public:
    UsbAsync(common::Logger logger, int itfc_nb,
             uint16_t vendor_id, uint16_t product_id,
             uint8_t in_endoint, uint8_t out_endpoint);
    virtual ~UsbAsync();

    virtual void open();
    virtual void close();
    virtual void write(const std::string& buf);
    virtual void write(const std::string&& buf);
    virtual std::string read();

private:
    libusb_context       * ctx_ = nullptr;
    libusb_device_handle * device_handle_ = nullptr;

    uint16_t vendor_id_;
    uint16_t product_id_;
    int      itfc_nb_;
    uint8_t  in_endoint_;
    uint8_t  out_endpoit_;

    std::queue<std::string> write_queue_;
    std::queue<std::string> read_queue_;
    RTransfer               rtransfer_curr_;
    RTransfer               rtransfer_prev_;
    WTransfer               wtransfer_;

    void fill_transfer(WTransfer& transfer);
    void fill_transfer(RTransfer& transfer);
    static void write_cb(libusb_transfer * transfer);
    static void read_cb(libusb_transfer * transfer);
};

#endif /* USB_ASYNC_H */
