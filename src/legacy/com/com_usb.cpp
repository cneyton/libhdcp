#include "com_usb.h"

#define ITFC_NB                     1
#define MAX_BULK_LENGTH             2048
#define TIMEOUT_READ                14000
#define TIMEOUT_WRITE               14000
#define KERNEL_AUTO_DETACH_DRIVER   1
#define VENDOR_ID                   0x0483
#define PRODUCT_ID                  0x5740
#define IN                          0x81
#define OUT                         0x01

ComUSB::ComUSB(common::Logger logger): BaseComHW(logger)
{

}

ComUSB::~ComUSB()
{
    if (ctx_) libusb_exit(ctx_);
}

int ComUSB::open()
{
    int ret;

    ret = libusb_init(&ctx_);
    common_die_zero(logger_, ret, -1, "failed to init libusb ctx");

    device_handle_ = libusb_open_device_with_vid_pid(ctx_, VENDOR_ID, PRODUCT_ID);
    common_die_null(logger_, device_handle_, -2, "device not found");

    ret = libusb_set_auto_detach_kernel_driver(device_handle_, KERNEL_AUTO_DETACH_DRIVER);
    common_die_zero(logger_, ret, -3, "auto detach kernel not supported");

    ret = libusb_claim_interface(device_handle_, ITFC_NB);
    common_die_zero(logger_, ret, -3, "failed to claim interface, libusb error {}",
                    libusb_error_name(ret));

    return 0;
}

int ComUSB::close()
{
    int ret;

    if (device_handle_) {
        ret = libusb_release_interface(device_handle_, ITFC_NB);
        common_die_zero(logger_, ret, -1, "failed to release interface, libusb error {}",
                        libusb_error_name(ret));

        libusb_close(device_handle_);
    }
    return 0;
}

int ComUSB::read(RawBuffer& buffer)
{
    int ret, transferred;

    ret = libusb_bulk_transfer(device_handle_, IN, buffer.data(), buffer.size(),
                               &transferred, TIMEOUT_READ);
    common_die_zero(logger_, ret, -1, "libusb error : {}", libusb_error_name(ret));

    buffer.resize(transferred);

    bytes_received_inc(transferred);

    return 0;
}

int ComUSB::write(const RawBuffer& buffer)
{
    int ret, transferred;

    ret = libusb_bulk_transfer(device_handle_, OUT, RawBuffer(buffer).data(), buffer.size(),
                               &transferred, TIMEOUT_WRITE);
    common_die_zero(logger_, ret, -1, "libusb error : {}", libusb_error_name(ret));

    //TODO: check transferred

    bytes_sent_inc(transferred);

    return 0;
}


