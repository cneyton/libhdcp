#ifndef COM_ASYNC
#define COM_ASYNC

#include <vector>
#include <stack>

#include <libusb-1.0/libusb.h>
#include "common/log.h"
#include "common/wait_queue.h"
#include "common/thread.h"

#include "com/base_com_hw.h"
#include "com/packet.h"

class ComAsync;

class Transfer
{
public:
    Transfer(ComAsync * com);
    //Transfer(Transfer&& other);
    Transfer(ComAsync * com, const RawBuffer& buffer);
    ~Transfer();

    bool is_valid() const;
    ComAsync * get_com() const;

    int fill_write(libusb_device_handle * device_handle);
    int fill_read(libusb_device_handle * device_handle);
    int submit();

private:
    libusb_transfer * transfer_ = nullptr;

    ComAsync * com_ = nullptr;

    std::vector<uint8_t> buffer_;

    static void write_cb(libusb_transfer * transfer);
    static void read_cb(libusb_transfer * transfer);

    bool valid_ = false;
};

class ComAsync: public BaseComHW, public common::Thread
{
public:
    ComAsync(common::Logger logger);
    virtual ~ComAsync();

    int open();
    int close();
    int write(const RawBuffer& buffer);
    int read(RawBuffer& buffer);
    void run();
    int submit();

    int push_buffer(RawBuffer& buffer);
    int push(Transfer * transfer);

    libusb_device_handle * get_device_handle() const;

private:
    libusb_context       * ctx_ = nullptr;
    libusb_device_handle * device_handle_ = nullptr;

    std::stack<Transfer*> transfer_stack_;
    common::WaitQueue<RawBuffer> buffer_queue_;
};

#endif /* COM_ASYNC */
