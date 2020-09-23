#include "hdcp/exception.h"
#include "usb_async.h"

using namespace hdcp;

constexpr int kernel_auto_detach_driver = 1;

Transfer::Transfer()
{
    if (!(transfer_ = libusb_alloc_transfer(0)))
        throw std::bad_alloc();
}

Transfer::~Transfer()
{
    if (transfer_)
        libusb_free_transfer(transfer_);
}

void Transfer::submit()
{
    int ret;
    if ((ret = libusb_submit_transfer(transfer_)) < 0)
        throw libusb_error(ret);
}

int Transfer::async_cancel()
{
    int ret = libusb_cancel_transfer(transfer_);
    /*
     * no error if the cancel occurred or if the transfer is not in progress,
     * already complete, or already cancelled.
     */
    if (ret != 0 && ret != LIBUSB_ERROR_NOT_FOUND)
        throw libusb_error(ret);
    return ret;
}

void Transfer::cancel()
{
    if (async_cancel() != LIBUSB_ERROR_NOT_FOUND)
        wait_cancel();
}

void Transfer::wait_cancel()
{
    std::unique_lock<std::mutex> lk(mutex_cancel_);
    cv_cancel_.wait(lk, [this]{return cancelled_;});
}

void Transfer::notify_cancelled()
{
    std::lock_guard<std::mutex> lk(mutex_cancel_);
    cancelled_ = true;
    cv_cancel_.notify_all();
}

RTransfer::RTransfer(libusb_device_handle * device_handle): device_handle_(device_handle)
{
    if (!device_handle)
        throw::transport_error("device handle nullptr");

    if (!(buf_ = libusb_dev_mem_alloc(device_handle, Packet::max_size)))
        throw std::bad_alloc();
}

RTransfer::~RTransfer()
{
    libusb_dev_mem_free(device_handle_, buf_, Packet::max_size);
}

void WTransfer::submit()
{
    Transfer::submit();
    in_progress_ = true;
}

UsbAsync::UsbAsync(common::Logger logger, int itfc_nb,
                   uint16_t vendor_id, uint16_t product_id,
                   uint8_t in_endoint, uint8_t out_endpoint):
    common::Log(logger), itfc_nb_(itfc_nb), vendor_id_(vendor_id), product_id_(product_id),
    in_endoint_(in_endoint), out_endpoit_(out_endpoint),
    usb_logger_(logger->clone("usb_logger"))
{
    int ret;
    if ((ret = libusb_init(&ctx_)) < 0)
        throw hdcp::libusb_error(ret);

    spdlog::register_logger(usb_logger_);
    libusb_set_log_cb(ctx_, &UsbAsync::log_cb, LIBUSB_LOG_CB_GLOBAL);
    libusb_set_option(ctx_, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_WARNING);
}

UsbAsync::~UsbAsync()
{
    stop();
    libusb_exit(ctx_);
}

bool UsbAsync::is_open()
{
    return open_;
}

void UsbAsync::open()
{
    if (is_open())
        return;

    log_debug(logger_, "opening transport...");

    if (!(device_handle_ = libusb_open_device_with_vid_pid(ctx_, vendor_id_, product_id_)))
        throw hdcp::transport_error("device not found");

    int ret = libusb_set_auto_detach_kernel_driver(device_handle_, kernel_auto_detach_driver);
    if (ret < 0)
        throw hdcp::libusb_error(ret);

    if ((ret = libusb_claim_interface(device_handle_, itfc_nb_)) < 0)
        throw hdcp::libusb_error(ret);

    wtransfer_      = new WTransfer();
    rtransfer_curr_ = new RTransfer(device_handle_);
    rtransfer_prev_ = new RTransfer(device_handle_);
    fill_transfer(rtransfer_curr_);
    fill_transfer(rtransfer_prev_);

    open_ = true;
    log_debug(logger_, "transport opened");
}

void UsbAsync::close()
{
    if (!is_open())
        return;

    log_debug(logger_, "closing transport...");
    // delete transfers
    delete wtransfer_;
    delete rtransfer_curr_;
    delete rtransfer_prev_;
    wtransfer_      = nullptr;
    rtransfer_curr_ = nullptr;
    rtransfer_prev_ = nullptr;
    // close
    int ret;
    if ((ret = libusb_release_interface(device_handle_, itfc_nb_)) < 0)
        throw hdcp::libusb_error(ret);
    libusb_close(device_handle_);
    device_handle_ = nullptr;

    open_ = false;
    log_debug(logger_, "transport closed");
}

void UsbAsync::write(Packet&& p)
{
    if (!is_open())
        throw hdcp::transport_error("can't write while transport is closed");

    std::lock_guard<std::mutex> lk(mutex_wprogress_);
    if (wtransfer_->in_progress()) {
        if (!write_queue_.try_enqueue(std::forward<Packet>(p)))
            throw hdcp::transport_error("write queue full");
    } else {
        wtransfer_->packet() = std::forward<Packet>(p);
        fill_transfer(wtransfer_);
        wtransfer_->submit();
    }
}

void UsbAsync::fill_transfer(WTransfer * transfer)
{
    if (!transfer)
        throw transport_error("transfer null pointer");

    libusb_fill_bulk_transfer(transfer->libusb_transfer_ptr(), device_handle_, out_endpoit_,
                              (uint8_t*)transfer->packet().data(), transfer->packet().size(),
                              &UsbAsync::write_cb, this, timeout_write);
}

void UsbAsync::fill_transfer(RTransfer * transfer)
{
    if (!transfer)
        throw transport_error("transfer null pointer");

    libusb_fill_bulk_transfer(transfer->libusb_transfer_ptr(), device_handle_,
                              in_endoint_, transfer->get_buffer(), Packet::max_size,
                              &UsbAsync::read_cb, this, timeout_read);
}

void UsbAsync::write_cb(libusb_transfer * transfer) noexcept
{
    UsbAsync * usb = (UsbAsync*)transfer->user_data;

    try {
        switch (transfer->status) {
            case LIBUSB_TRANSFER_COMPLETED:
            {
                log_trace(usb->get_logger(), "write cb: {:#x}",
                          fmt::join((uint8_t*)transfer->buffer,
                                    (uint8_t*)transfer->buffer + transfer->actual_length, "|"));
                std::lock_guard<std::mutex> lk(usb->mutex_wprogress_);
                if (usb->write_queue_.try_dequeue(usb->wtransfer_->packet())) {
                    usb->fill_transfer(usb->wtransfer_);
                    usb->wtransfer_->submit();
                } else {
                    usb->wtransfer_->put_on_hold();
                }
                break;
            }
            case LIBUSB_TRANSFER_CANCELLED:
            {
                usb->wtransfer_->notify_cancelled();
                break;
            }
            case LIBUSB_TRANSFER_TIMED_OUT:
            case LIBUSB_TRANSFER_STALL:
            case LIBUSB_TRANSFER_NO_DEVICE:
            case LIBUSB_TRANSFER_OVERFLOW:
            case LIBUSB_TRANSFER_ERROR:
                throw libusb_error(transfer->status);
            default:
                log_error(usb->get_logger(), "unknown transfer status, you should not be here");
        }
    } catch (std::exception& e) {
        log_error(usb->get_logger(), e.what());
    }
}

void UsbAsync::read_cb(libusb_transfer * transfer) noexcept
{
    UsbAsync * usb = (UsbAsync*)transfer->user_data;

    try {
        switch (transfer->status) {
            case LIBUSB_TRANSFER_COMPLETED:
            {
                RTransfer * tmp = usb->rtransfer_curr_;
                usb->rtransfer_curr_ = usb->rtransfer_prev_;
                usb->rtransfer_curr_->submit();
                usb->rtransfer_prev_ = tmp;

                log_trace(usb->get_logger(), "read cb: {:#x}",
                          fmt::join((uint8_t*)transfer->buffer,
                                    (uint8_t*)transfer->buffer + transfer->actual_length, "|"));

                if (!usb->read_queue_.try_enqueue(std::string_view((char*)transfer->buffer,
                                                                   transfer->actual_length)))
                    throw hdcp::transport_error("read queue full");
                break;
            }
            case LIBUSB_TRANSFER_CANCELLED:
            {
                usb->rtransfer_curr_->notify_cancelled();
                break;
            }
            case LIBUSB_TRANSFER_STALL:
            case LIBUSB_TRANSFER_NO_DEVICE:
            case LIBUSB_TRANSFER_OVERFLOW:
            case LIBUSB_TRANSFER_TIMED_OUT:
            case LIBUSB_TRANSFER_ERROR:
                throw libusb_error(transfer->status);
            default:
                log_error(usb->get_logger(), "unknown transfer status, you should not be here");
        }
    } catch (std::exception& e) {
        log_error(usb->get_logger(), e.what());
    }
}

void UsbAsync::run()
{
    rtransfer_curr_->submit();
    notify_running(0);
    while (is_running()) {
        libusb_handle_events(ctx_);
    }
}

void UsbAsync::stop()
{
    if (!is_running())
        return;
    log_debug(logger_, "stopping transport...");
    cancel_transfers();
    common::Thread::stop();
    close();
    if (joinable())
        join();
    log_debug(logger_, "transport stopped");
}

void UsbAsync::start()
{
    if (is_running())
        return;
    log_debug(logger_, "starting transport...");
    open();
    common::Thread::start(true);
    log_debug(logger_, "transport started");
}

void UsbAsync::cancel_transfers()
{
    log_debug(logger_, "cancelling transfers...");
    if (wtransfer_)      wtransfer_->cancel();
    if (rtransfer_curr_) rtransfer_curr_->cancel();
    if (rtransfer_prev_) rtransfer_prev_->cancel();
    log_debug(logger_, "transfers cancelled");
}

void UsbAsync::log_cb(libusb_context*, libusb_log_level lvl, const char* str) noexcept
{
    auto logger = spdlog::get("usb_logger");
    if (!logger)
        return;

    switch (lvl) {
    case LIBUSB_LOG_LEVEL_NONE:
        break;
    case LIBUSB_LOG_LEVEL_ERROR:
        log_error(logger, str);
        break;
    case LIBUSB_LOG_LEVEL_WARNING:
        log_warn(logger, str);
        break;
    case LIBUSB_LOG_LEVEL_INFO:
        log_info(logger, str);
        break;
    case LIBUSB_LOG_LEVEL_DEBUG:
        log_debug(logger, str);
        break;
    default:
        break;
    }
}
