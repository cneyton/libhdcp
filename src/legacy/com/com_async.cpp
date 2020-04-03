#include "com_async.h"

#define ITFC_NB                     1
#define MAX_BULK_LENGTH             2048
#define TIMEOUT_READ                14000
#define TIMEOUT_WRITE               14000
#define KERNEL_AUTO_DETACH_DRIVER   1
#define VENDOR_ID                   0x0483
#define PRODUCT_ID                  0x5740
#define IN                          0x81
#define OUT                         0x01

//TODO: set to packet size
constexpr std::size_t max_transfer_size = 2048;
constexpr std::size_t stack_size        = 10;

Transfer::Transfer(ComAsync * com): com_(com), buffer_(max_transfer_size)
{
    transfer_ = libusb_alloc_transfer(0);
    if (transfer_ != nullptr)
        valid_ = true;
}

Transfer::Transfer(ComAsync * com, const RawBuffer& buffer): com_(com), buffer_(buffer)
{
    transfer_ = libusb_alloc_transfer(0);
    //TODO: better to throw
    if (transfer_ != nullptr)
        valid_ = true;
}

Transfer::~Transfer()
{
    if (transfer_ != nullptr)
        libusb_free_transfer(transfer_);
}

bool Transfer::is_valid() const
{
    return valid_;
}

ComAsync * Transfer::get_com() const
{
    return com_;
}

int Transfer::fill_write(libusb_device_handle * device_handle)
{
    libusb_fill_bulk_transfer(transfer_, device_handle, OUT, buffer_.data(), buffer_.size(),
                              &Transfer::write_cb, this, TIMEOUT_WRITE);
    return 0;
}

int Transfer::fill_read(libusb_device_handle * device_handle)
{
    libusb_fill_bulk_transfer(transfer_, device_handle, IN, buffer_.data(), buffer_.size(),
                              &Transfer::read_cb, this, TIMEOUT_READ);
    return 0;
}

int Transfer::submit()
{
    libusb_submit_transfer(transfer_);
    return 0;
}

void Transfer::write_cb(libusb_transfer * transfer)
{
    Transfer * obj = (Transfer*)transfer->user_data;
    delete obj;
}

void Transfer::read_cb(libusb_transfer * transfer)
{
    Transfer * obj = (Transfer*)transfer->user_data;
    ComAsync * com = obj->get_com();

    com->submit();
    RawBuffer buffer(transfer->buffer, transfer->buffer + transfer->actual_length);
    com->push_buffer(buffer);
    com->push(obj);
}

ComAsync::ComAsync(common::Logger logger): BaseComHW(logger)
{
}

ComAsync::~ComAsync()
{
    libusb_exit(ctx_);
}

libusb_device_handle * ComAsync::get_device_handle() const
{
    return device_handle_;
}

int ComAsync::open()
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

    std::deque<Transfer*> transfers{stack_size, new Transfer(this)};
    for (auto xfer: transfers)
        xfer->fill_read(device_handle_);

    transfer_stack_ = std::stack<Transfer*>(transfers);

    //TODO: for better perf, use DMA buffers with:
    //uint8_t * data = libusb_dev_mem_alloc(device_handle_, xxxx);
    //common_die_null(logger_, data, -4, "failed to allocate DMA");

    m_hw_activated = true;
    start_thread(0);

    return 0;
}

void ComAsync::run()
{
    // submit the first read transfert
    submit();
    submit();
    while (!is_canceled())
        libusb_handle_events(ctx_);

    return;
}

int ComAsync::submit()
{
    auto transfer = transfer_stack_.top();
    transfer_stack_.pop();
    //transfer->fill_read(device_handle_);
    transfer->submit();
    return 0;
}

int ComAsync::push(Transfer * transfer)
{
    transfer_stack_.push(transfer);
    return 0;
}

int ComAsync::write(const RawBuffer& buffer)
{
    Transfer * transfer = new Transfer(this, buffer);
    transfer->fill_write(device_handle_);
    transfer->submit();
    return 0;
}

int ComAsync::read(RawBuffer& buffer)
{
    buffer_queue_.pop(buffer);
    return 0;
}

int ComAsync::push_buffer(RawBuffer& buffer)
{
    buffer_queue_.push(std::move(buffer));
    return 0;
}

int ComAsync::close()
{
    if (!m_hw_activated) return 0;

    cancel();

    while(!transfer_stack_.empty()) {
        auto transfer = transfer_stack_.top();
        delete transfer;
        transfer_stack_.pop();
    }

    libusb_close(device_handle_);

    join();
    return 0;
}
