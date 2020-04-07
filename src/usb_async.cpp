#include "hdcp/exception.h"
#include "usb_async.h"

constexpr int   kernel_auto_detach_driver = 1;

Transfer::Transfer()
{
    if (!(transfer_ = libusb_alloc_transfer(0)))
        throw std::bad_alloc();
}

Transfer::~Transfer()
{
    if (transfer_ != nullptr)
        libusb_free_transfer(transfer_);
}

void Transfer::submit()
{
    int ret;
    if ((ret = libusb_submit_transfer(transfer_)) < 0)
        throw hdcp::libusb_error(ret);
}

void WTransfer::submit()
{
    Transfer::submit();
    in_progress_ = true;
    retry_ = 0;
}

void WTransfer::resubmit()
{
    Transfer::submit();
    in_progress_ = true;
    retry_++;
}

void RTransfer::free_buffer(libusb_device_handle * device_handle)
{
    int ret;
    if (buf_ && device_handle) {
        if ((ret = libusb_dev_mem_free(device_handle, buf_, max_transfer_size)) < 0)
            throw hdcp::libusb_error(ret);
    }
}

void RTransfer::alloc_buffer(libusb_device_handle * device_handle)
{
    if (!(buf_ = libusb_dev_mem_alloc(device_handle, max_transfer_size)))
        throw std::bad_alloc();
}



UsbAsync::UsbAsync(common::Logger logger, int itfc_nb,
                   uint16_t vendor_id, uint16_t product_id,
                   uint8_t in_endoint, uint8_t out_endpoint):
    common::Log(logger), itfc_nb_(itfc_nb), vendor_id_(vendor_id), product_id_(product_id),
    in_endoint_(in_endoint), out_endpoit_(out_endpoint)
{
    int ret;
    if ((ret = libusb_init(&ctx_)) < 0)
        throw hdcp::libusb_error(ret);
}

UsbAsync::~UsbAsync()
{
    if (open_)
        close();
    libusb_exit(ctx_);
}

void UsbAsync::open()
{
    if (!(device_handle_ = libusb_open_device_with_vid_pid(ctx_, vendor_id_, product_id_)))
        throw hdcp::device_error("device not found");

    libusb_set_auto_detach_kernel_driver(device_handle_, kernel_auto_detach_driver);

    int ret;
    if ((ret = libusb_claim_interface(device_handle_, itfc_nb_)) < 0)
        throw hdcp::libusb_error(ret);

    fill_transfer(rtransfer_curr_);
    fill_transfer(rtransfer_prev_);

    open_ = true;
}

void UsbAsync::close()
{
    if (!open_)
        return;

    /* TODO: clear read & write queue  <07-04-20, cneyton> */
    rtransfer_curr_.free_buffer(device_handle_);
    rtransfer_prev_.free_buffer(device_handle_);
    libusb_close(device_handle_);
    device_handle_ = nullptr;

    open_ = false;
}

void UsbAsync::write(const std::string& buf)
{
    if (!open_)
        throw hdcp::transport_error("not allowed to write when transport is closed");

    write_queue_.push(buf);
    if (!wtransfer_.in_progress()) {
        fill_transfer(wtransfer_);
        wtransfer_.submit();
    }
}

void UsbAsync::write(const std::string&& buf)
{
    if (!open_)
        throw hdcp::transport_error("not allowed to write when transport is closed");

    write_queue_.push(std::move(buf));
    if (!wtransfer_.in_progress()) {
        fill_transfer(wtransfer_);
        wtransfer_.submit();
    }
}

std::string UsbAsync::read()
{
    if (!open_)
        throw hdcp::transport_error("not allowed to read  when transport is closed");

    auto front = read_queue_.front();
    read_queue_.pop();
    return front;
}

void UsbAsync::fill_transfer(WTransfer& transfer)
{
    std::string& buf = write_queue_.front();
    libusb_fill_bulk_transfer(transfer.get_libusb_transfer(), device_handle_,
                              out_endpoit_, reinterpret_cast<uint8_t*>(buf.data()), buf.size(),
                              &UsbAsync::write_cb, this, timeout_write);
}

void UsbAsync::fill_transfer(RTransfer& transfer)
{
    transfer.alloc_buffer(device_handle_);
    libusb_fill_bulk_transfer(transfer.get_libusb_transfer(), device_handle_,
                              in_endoint_, transfer.get_buffer(), max_transfer_size,
                              &UsbAsync::read_cb, this, timeout_read);
}

void UsbAsync::write_cb(libusb_transfer * transfer)
{
    UsbAsync * usb = (UsbAsync*)transfer->user_data;
    if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
        usb->write_queue_.pop();
        if (!usb->write_queue_.empty()) {
            usb->fill_transfer(usb->wtransfer_);
            usb->wtransfer_.submit();
        } else {
            usb->wtransfer_.put_on_hold();
        }
    } else {
        log_warn(usb->logger_, "write failed, libusb error {}",
                 libusb_error_name(transfer->status));
        if (usb->wtransfer_.n_retry() < write_retry)
            usb->wtransfer_.resubmit();
        else
            throw hdcp::libusb_error(transfer->status);
    }
}

void UsbAsync::read_cb(libusb_transfer * transfer)
{
    UsbAsync * usb = (UsbAsync*)transfer->user_data;
    usb->rtransfer_curr_ = usb->rtransfer_prev_;
    usb->rtransfer_curr_.submit();

    if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
        usb->read_queue_.push(std::string(transfer->buffer,
                                          transfer->buffer + transfer->actual_length));
    } else {
        throw hdcp::libusb_error(transfer->status);
    }
}
