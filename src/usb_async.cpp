#include "usb_async.h"

namespace hdcp {
namespace transport {
namespace usb {

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

void Transfer::cancel()
{
    cancelled_ = false;
    int ret = libusb_cancel_transfer(transfer_);
    switch (ret) {
    case 0:
    {
        std::unique_lock<std::mutex> lk(mutex_cancel_);
        cv_cancel_.wait(lk, [this]{return cancelled_;});
        break;
    }
    case LIBUSB_ERROR_NOT_FOUND:
        // the transfer is not in progress, already complete, or already cancelled.
        return;
    default:
        throw libusb_error(ret);
    }
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
        throw transport_error(Errc::internal);

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

Device::Device(common::Logger logger, int itfc_nb,
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
    libusb_set_log_cb(ctx_, &Device::log_cb, LIBUSB_LOG_CB_GLOBAL);
    libusb_set_option(ctx_, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_WARNING);
}

Device::~Device()
{
    stop();
    libusb_exit(ctx_);
}

bool Device::is_open()
{
    std::lock_guard<std::mutex> lk(mutex_open_);
    return open_;
}

void Device::open()
{
    if (is_open())
        return;

    log_debug(logger_, "opening transport...");
    std::lock_guard<std::mutex> lk(mutex_open_);

    if (!(device_handle_ = libusb_open_device_with_vid_pid(ctx_, vendor_id_, product_id_)))
        throw libusb_error(LIBUSB_ERROR_NO_DEVICE);

    int ret = libusb_set_auto_detach_kernel_driver(device_handle_, kernel_auto_detach_driver);
    if (ret < 0)
        throw libusb_error(ret);

    if ((ret = libusb_claim_interface(device_handle_, itfc_nb_)) < 0)
        throw libusb_error(ret);

    wtransfer_      = new WTransfer();
    rtransfer_curr_ = new RTransfer(device_handle_);
    rtransfer_prev_ = new RTransfer(device_handle_);
    fill_transfer(rtransfer_curr_);
    fill_transfer(rtransfer_prev_);

    open_ = true;
    log_debug(logger_, "transport opened");
}

void Device::close()
{
    if (!is_open())
        return;

    log_debug(logger_, "closing transport...");
    std::lock_guard<std::mutex> lk(mutex_open_);
    // delete transfers
    delete wtransfer_;
    delete rtransfer_curr_;
    delete rtransfer_prev_;
    wtransfer_      = nullptr;
    rtransfer_curr_ = nullptr;
    rtransfer_prev_ = nullptr;
    // release interface
    int ret = libusb_release_interface(device_handle_, itfc_nb_);
    // even if we cannot release the itfc, the device is closed
    libusb_close(device_handle_);
    device_handle_ = nullptr;

    open_ = false;
    log_debug(logger_, "transport closed");
    if (ret < 0)
        throw libusb_error(ret);
}

void Device::write(Packet&& p)
{
    if (!is_open())
        throw transport_error(Errc::write_while_closed);

    std::lock_guard<std::mutex> lk(mutex_wprogress_);
    if (wtransfer_->in_progress()) {
        if (!write_queue_.try_enqueue(std::forward<Packet>(p)))
            throw transport_error(Errc::write_queue_full);
    } else {
        wtransfer_->packet() = std::forward<Packet>(p);
        fill_transfer(wtransfer_);
        wtransfer_->submit();
    }
}

void Device::fill_transfer(WTransfer * transfer)
{
    if (!transfer)
        throw transport_error(Errc::internal);

    libusb_fill_bulk_transfer(transfer->libusb_transfer_ptr(), device_handle_, out_endpoit_,
                              (uint8_t*)transfer->packet().data(), transfer->packet().size(),
                              &Device::write_cb, this, timeout_write);
}

void Device::fill_transfer(RTransfer * transfer)
{
    if (!transfer)
        throw transport_error(Errc::internal);

    libusb_fill_bulk_transfer(transfer->libusb_transfer_ptr(), device_handle_,
                              in_endoint_, transfer->get_buffer(), Packet::max_size,
                              &Device::read_cb, this, timeout_read);
}

void Device::write_cb(libusb_transfer * transfer) noexcept
{
    Device * usb = (Device*)transfer->user_data;

    try {
        switch (transfer->status) {
            case LIBUSB_TRANSFER_COMPLETED:
            {
                log_trace(usb->get_logger(), "write {} bytes", transfer->actual_length);

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
                log_debug(usb->get_logger(), "write transfer cancelled");
                usb->wtransfer_->notify_cancelled();
                break;
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
        usb->eptr_ = std::current_exception();
    }
}

void Device::read_cb(libusb_transfer * transfer) noexcept
{
    Device * usb = (Device*)transfer->user_data;

    try {
        switch (transfer->status) {
            case LIBUSB_TRANSFER_COMPLETED:
            {
                RTransfer * tmp = usb->rtransfer_curr_;
                usb->rtransfer_curr_ = usb->rtransfer_prev_;
                usb->rtransfer_curr_->submit();
                usb->rtransfer_prev_ = tmp;

                log_trace(usb->get_logger(), "read {} bytes", transfer->actual_length);

                if (!usb->read_queue_.try_enqueue(std::string_view((char*)transfer->buffer,
                                                                   transfer->actual_length))) {
                    // discard packet if queue is full
                    auto e = make_error_code(Errc::read_queue_full);
                    log_warn(usb->get_logger(), e.message());
                }
                break;
            }
            case LIBUSB_TRANSFER_CANCELLED:
                log_debug(usb->get_logger(), "read transfer cancelled");
                usb->rtransfer_curr_->notify_cancelled();
                break;
            case LIBUSB_TRANSFER_TIMED_OUT:
            {
                // resbmit transfer, if the problem persists the application will timeout
                RTransfer * tmp = usb->rtransfer_curr_;
                usb->rtransfer_curr_ = usb->rtransfer_prev_;
                usb->rtransfer_curr_->submit();
                usb->rtransfer_prev_ = tmp;
                break;
            }
            case LIBUSB_TRANSFER_NO_DEVICE:
            case LIBUSB_TRANSFER_STALL:
            case LIBUSB_TRANSFER_OVERFLOW:
            case LIBUSB_TRANSFER_ERROR:
                throw libusb_error(transfer->status);
            default:
                log_error(usb->get_logger(), "unknown transfer status, you should not be here");
        }
    } catch (std::exception& e) {
        usb->eptr_ = std::current_exception();
    }
}

void Device::run()
{
    try {
        rtransfer_curr_->submit();
        notify_running();
        while (is_running()) {
            try {
                eptr_ = nullptr;
                libusb_handle_events(ctx_);
                if (eptr_)
                    std::rethrow_exception(eptr_);
            } catch (packet_error& e) {
                // discard ill-formed packets
                log_warn(logger_, e.what());
            } catch (transport_error& e) {
                if (e.code() == Errc::read_queue_full) {
                    // discard packet if the queue is full
                    log_warn(logger_, e.what());
                } else {
                    log_error(logger_, e.what());
                    errc_ = e.code();
                    break;
                }
            } catch (std::exception& e) {
                log_error(logger_, e.what());
                errc_ = Errc::other;
                break;
            } catch (...) {
                errc_ = Errc::other;
                break;
            }
        }
        close();
    } catch (std::exception& e) {
        log_error(logger_, e.what());
        errc_ = Errc::other;
    } catch (...) {
        log_error(logger_, "error during thread execution");
        errc_ = Errc::other;
    }
}

void Device::stop()
{
    if (!is_running())
        return;
    log_debug(logger_, "stopping transport...");
    try {
        cancel_transfers();
        common::Thread::stop();
        close();
    } catch (libusb_error& e) {
        log_warn(logger_, e.what());
    }
    if (joinable())
        join();
    log_debug(logger_, "transport stopped");
}

void Device::start()
{
    if (is_running())
        return;
    log_debug(logger_, "starting transport...");
    open();
    common::Thread::start(true);
    log_debug(logger_, "transport started");
}

void Device::cancel_transfers()
{
    log_debug(logger_, "cancelling transfers...");
    if (wtransfer_)      wtransfer_->cancel();
    if (rtransfer_curr_) rtransfer_curr_->cancel();
    if (rtransfer_prev_) rtransfer_prev_->cancel();
    log_debug(logger_, "transfers cancelled");
}

void Device::log_cb(libusb_context*, libusb_log_level lvl, const char* str) noexcept
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

} /* namespace usb  */
} /* namespace transport  */
} /* namespace hdcp */
