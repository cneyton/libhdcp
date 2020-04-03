#ifndef COMHWLINUX_H
#define COMHWLINUX_H

#include "com/hw/base_com_hw.h"
#include "ch9_custom.h"
#include <string>
#include <vector>
#include <cstdint>
#include <queue>
#include <string>
#include <sstream>
#include "usbhost.h"
#include <cassert>

class ComHwLinuxSerial: public BaseComHW
{
public:
    ComHwLinuxSerial(int fd);

    virtual ~ComHwLinuxSerial(){ if (m_device) delete(m_device);};

    // base com hw -------------------------------------------------------------
    virtual int32_t open();
    virtual void close();
    virtual int32_t read(uint8_t * bytes, uint32_t length);
    virtual int32_t write(const uint8_t * bytes, uint32_t length);
    // -------------------------------------------------------------------------

    void setDataToRead(uint8_t * bytes, uint32_t length);
    void getLastDataWritten(uint8_t * bytes, uint32_t length);

private:
    int m_fd;
    int readEndpoint;
    int writeEndpoint;

    usb_device              * m_device;
    usb_endpoint_descriptor * m_readEndpointDesc;
    usb_endpoint_descriptor * m_writeEndpointDesc;
    usb_host_context        * m_ctx;

    std::string toString(char* cstr);
    std::string descriptorTypeToString(uint8_t dt);
    std::string classToString(uint8_t cls);
    void printDeviceDescriptor(usb_device* dev, usb_device_descriptor* desc, std::stringstream& ss);

    void printEndpointDescriptor(usb_device* dev, usb_endpoint_descriptor* desc, std::stringstream& ss);

};


#endif // COMHWSOFTWARE_H
