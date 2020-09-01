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
        throw hdcp::libusb_error(ret);
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
    in_endoint_(in_endoint), out_endpoit_(out_endpoint)
{
    int ret;
    if ((ret = libusb_init(&ctx_)) < 0)
        throw hdcp::libusb_error(ret);
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

    if (!(device_handle_ = libusb_open_device_with_vid_pid(ctx_, vendor_id_, product_id_)))
        throw hdcp::transport_error("device not found");

    libusb_set_auto_detach_kernel_driver(device_handle_, kernel_auto_detach_driver);

    int ret;
    if ((ret = libusb_claim_interface(device_handle_, itfc_nb_)) < 0)
        throw hdcp::libusb_error(ret);

    wtransfer_      = new WTransfer();
    rtransfer_curr_ = new RTransfer(device_handle_);
    rtransfer_prev_ = new RTransfer(device_handle_);
    fill_transfer(rtransfer_curr_);
    fill_transfer(rtransfer_prev_);

    open_ = true;
}

void UsbAsync::close()
{
    if (!is_open())
        return;

    clear_queues();
    // delete transfers
    delete wtransfer_;
    delete rtransfer_curr_;
    delete rtransfer_prev_;
    wtransfer_      = nullptr;
    rtransfer_curr_ = nullptr;
    rtransfer_prev_ = nullptr;
    // close
    libusb_close(device_handle_);
    device_handle_ = nullptr;

    open_ = false;
}

void UsbAsync::write(Packet&& p)
{
    if (!is_open())
        throw hdcp::transport_error("can't write while transport is closed");

    if (wtransfer_->in_progress()) {
        if (!write_queue_.try_enqueue(std::forward<Packet>(p)))
            throw hdcp::transport_error("write queue full");
    } else {
        wtransfer_->set_packet(std::forward<Packet>(p));
        fill_transfer(wtransfer_);
        wtransfer_->submit();
    }
}

void UsbAsync::fill_transfer(WTransfer * transfer)
{
    libusb_fill_bulk_transfer(transfer->get_libusb_transfer(), device_handle_, out_endpoit_,
                              (uint8_t*)transfer->get_packet().data(),
                              transfer->get_packet().size(),
                              &UsbAsync::write_cb, this, timeout_write);
}

void UsbAsync::fill_transfer(RTransfer * transfer)
{
    libusb_fill_bulk_transfer(transfer->get_libusb_transfer(), device_handle_,
                              in_endoint_, transfer->get_buffer(), Packet::max_size,
                              &UsbAsync::read_cb, this, timeout_read);
}

void UsbAsync::write_cb(libusb_transfer * transfer)
{
    if (transfer->status != LIBUSB_TRANSFER_COMPLETED)
        throw hdcp::libusb_error(transfer->status);

    UsbAsync * usb = (UsbAsync*)transfer->user_data;
    log_trace(usb->get_logger(), "write cb: {:#x}",
              fmt::join((uint8_t*)transfer->buffer,
                        (uint8_t*)transfer->buffer + transfer->actual_length, "|"));

    if (usb->write_queue_.try_dequeue(usb->wtransfer_->get_packet())) {
        usb->fill_transfer(usb->wtransfer_);
        usb->wtransfer_->submit();
    } else {
        usb->wtransfer_->put_on_hold();
    }
}

void UsbAsync::read_cb(libusb_transfer * transfer)
{
    UsbAsync * usb = (UsbAsync*)transfer->user_data;
    RTransfer * tmp = usb->rtransfer_curr_;
    usb->rtransfer_curr_ = usb->rtransfer_prev_;
    usb->rtransfer_curr_->submit();
    usb->rtransfer_prev_ = tmp;

    if (transfer->status != LIBUSB_TRANSFER_COMPLETED)
        throw hdcp::libusb_error(transfer->status);

    log_trace(usb->get_logger(), "read cb: {:#x}",
              fmt::join((uint8_t*)transfer->buffer,
                        (uint8_t*)transfer->buffer + transfer->actual_length, "|"));

    if (!usb->read_queue_.try_enqueue(std::string_view((char*)transfer->buffer,
                                                       transfer->actual_length)))
        throw hdcp::transport_error("read queue full");
}

void UsbAsync::run()
{
    rtransfer_curr_->submit();
    while (is_running()) {
        try {
            libusb_handle_events(ctx_);
        } catch (std::exception& e) {
            log_error(logger_, e.what());
        }
    }
}

void UsbAsync::stop()
{
    common::Thread::stop();
    close();
    if (joinable())
        join();
}

void UsbAsync::start()
{
    if (is_running())
        return;
    open();
    common::Thread::start(0);
}
