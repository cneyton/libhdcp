#ifndef COM_USB
#define COM_USB

#include <libusb-1.0/libusb.h>
#include "base_com_hw.h"
#include "common/log.h"

class ComUSB: public BaseComHW
{
public:
    ComUSB(common::Logger logger);
    virtual ~ComUSB();

    virtual int open();
    virtual int close();
    int write(const RawBuffer& buffer);
    int read(RawBuffer& buffer);

private:
    libusb_context       * ctx_ = nullptr;
    libusb_device_handle * device_handle_ = nullptr;
};

#endif /* COM_USB */
