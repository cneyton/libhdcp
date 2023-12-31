#pragma once

#include <mutex>
#include <condition_variable>

#include <libusb-1.0/libusb.h>

#include "common/log.h"
#include "common/thread.h"

#include "transport.h"

namespace hdcp {
namespace transport {
namespace usb {

class Transfer
{
public:
    Transfer();
    virtual ~Transfer();

    virtual void submit();
    void cancel();
    void notify_cancelled();
    libusb_transfer * libusb_transfer_ptr() const {return transfer_;};

private:
    libusb_transfer *       transfer_ = nullptr;
    std::mutex              mutex_cancel_;
    std::condition_variable cv_cancel_;
    bool                    cancelled_ = false;
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

    Packet& packet() {return p_;};

    bool in_progress() const {return in_progress_;};
    void put_on_hold()       {in_progress_ = false;};

private:
    Packet p_;
    std::atomic_bool in_progress_ = false;
};


class Device: public common::Log, private common::Thread, public Transport
{
public:
    Device(common::Logger logger, int itfc_nb,
             uint16_t vendor_id, uint16_t product_id,
             uint8_t in_endpoint, uint8_t out_endpoint);
    virtual ~Device();

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
    std::mutex  mutex_wprogress_;

    bool open_ = false;
    std::mutex mutex_open_;

    int      itfc_nb_;
    uint16_t vendor_id_, product_id_;
    uint8_t  in_endoint_, out_endpoit_;

    void fill_transfer(WTransfer * transfer);
    void fill_transfer(RTransfer * transfer);
    void cancel_transfers();
    /*
     * NB:
     * - never propagate exception through external module
     * - don't call close in cb -> deadlocks
     */
    static void write_cb(libusb_transfer * transfer) noexcept;
    static void read_cb(libusb_transfer * transfer)  noexcept;

    void run() override;
    std::exception_ptr eptr_;

    /*
     * we use a different logger for libusb since we need to get his name in
     * the log cb
     */
    static void log_cb(libusb_context*, libusb_log_level, const char*) noexcept;
    common::Logger usb_logger_;
};

} /* namespace usb  */
} /* namespace transport  */
} /* namespace hdcp */
