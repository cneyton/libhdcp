#include "ComHwLinuxSerial.h"

#include <spdlog/spdlog.h>
#include <unistd.h>


ComHwLinuxSerial::ComHwLinuxSerial(int fd):m_fd(fd)
{

}

int32_t ComHwLinuxSerial::open()
{
    spdlog::get("console")->info("Serial open on fd {}", m_fd);

    std::stringstream res;

    spdlog::get("console")->info("Create device...");

    m_ctx = usb_host_init();
    assert(m_ctx != nullptr);
    m_device = usb_device_new("test",m_fd);
    assert(m_device != nullptr);

    spdlog::get("console")->info("Serial open on fd {}", usb_device_get_fd(m_device));

    spdlog::get("console")->info("Device R/W {}", usb_device_is_writeable(m_device));

    spdlog::get("console")->info("Get general info...");

    res << toString(usb_device_get_manufacturer_name(m_device)) << " "
        << toString(usb_device_get_product_name(m_device))
        << std::endl;

    spdlog::get("console")->info("iterate through descriptors...");

    usb_descriptor_iter it;
    usb_descriptor_iter_init(m_device, &it);

    usb_descriptor_header* h = usb_descriptor_iter_next(&it);
    res << "Descriptors:\n\n";

    while (h != nullptr) {
        switch (h->bDescriptorType) {
        case USB_DT_DEVICE:
        {
            usb_device_descriptor* devDesc = (usb_device_descriptor*) h;
            printDeviceDescriptor(m_device,devDesc,res);
            break;
        }
        case USB_DT_CONFIG:
        {
            usb_config_descriptor* configDesc = (usb_config_descriptor*) h;
            // TODO: log
        }
        case USB_DT_INTERFACE:
        {
            usb_interface_descriptor* itfDesc = (usb_interface_descriptor*) h;
            // TODO: log
        }
        case USB_DT_ENDPOINT:
        {
            usb_endpoint_descriptor* epDesc = (usb_endpoint_descriptor*) h;
            // TODO: log
            printEndpointDescriptor(m_device, epDesc, res);

            // Read endpoint
            if ((usb_endpoint_direction(epDesc) == USB_DIR_IN) &&
                (usb_endpoint_type(epDesc) == USB_ENDPOINT_XFER_BULK)) {
                readEndpoint = epDesc->bEndpointAddress;
                m_readEndpointDesc = epDesc;
            }

            // Write endpoint
            if ((usb_endpoint_direction(epDesc) == USB_DIR_OUT) &&
                (usb_endpoint_type(epDesc) == USB_ENDPOINT_XFER_BULK)) {
                writeEndpoint = epDesc->bEndpointAddress;
                m_writeEndpointDesc = epDesc;
            }

            spdlog::get("console")->info("Read endpoint {}, Write endpoint {}",
                                         readEndpoint, writeEndpoint);
        }
        case USB_DT_STRING:
        {
            usb_string_descriptor* strDesc = (usb_string_descriptor*) h;
            // TODO: log
        }
        default: // other types (including class specific ones)
        {
            res << "Type: " << ((int)h->bDescriptorType)
                << " (" << descriptorTypeToString(h->bDescriptorType) << ")\n"
                << "   Length: " << ((int)h->bLength) << "\n\n";
        }
        }
        h = usb_descriptor_iter_next(&it);
    }

//    usb_host_cleanup(m_ctx);

    spdlog::get("console")->info("Done enumerating USB device properties");
    spdlog::get("console")->info(res.str().c_str());

    return 0;
}

int32_t ComHwLinuxSerial::read(uint8_t * bytes, uint32_t length)
{
    //Bulk transfer
    int len = usb_device_bulk_transfer(m_device, readEndpoint, bytes, length, 2000);

    //Timeout
    if (errno == 110)
        return 0;

    if(len > 0) spdlog::get("console")->info("Read on fd {}, {} bytes", m_fd, len);

    if (len < 0){
        spdlog::get("console")->info("Read Error {}, Endpoint : {}", std::strerror(errno), readEndpoint);
        return -1;
    }

    bytesReceived(len);

    return len;
}

int32_t ComHwLinuxSerial::write(const uint8_t * bytes, uint32_t length)
{

    //Bulk transfer
    int len = usb_device_bulk_transfer(m_device, writeEndpoint, (void*)bytes, length, 2000);

    //Timeout
    if (errno == 110)
        return 0;

    if(len > 0) spdlog::get("console")->info("Write on fd {}, {} bytes", m_fd, len);

    if (length!=len){
        spdlog::get("console")->info("Write Error {}, Endpoint : {}", std::strerror(errno), writeEndpoint);
        return -1;
    }

    bytesSent(len);

    return len;
}

void ComHwLinuxSerial::close()
{
    usb_host_cleanup(m_ctx);
    usb_device_release_interface(m_device, 1);
    usb_device_close(m_device);
}

void ComHwLinuxSerial::setDataToRead(uint8_t * bytes, uint32_t length)
{

}

void ComHwLinuxSerial::getLastDataWritten(uint8_t * bytes, uint32_t length)
{

}

std::string ComHwLinuxSerial::toString(char* cstr)
{
    if (cstr == nullptr)
        return "null";
    std::string str(cstr);
    free(cstr);
    return str;
}

std::string ComHwLinuxSerial::descriptorTypeToString(uint8_t dt)
{
    switch (dt) {
    case USB_DT_DEVICE:                 return "Device";
    case USB_DT_CONFIG:                 return "Config";
    case USB_DT_INTERFACE:              return "Interface";
    case USB_DT_ENDPOINT:               return "Endpoint";
    case USB_DT_STRING:                 return "String";
    case USB_DT_CS_DEVICE:              return "Class-specific device";
    case USB_DT_CS_CONFIG:              return "Class-specific config";
    case USB_DT_CS_INTERFACE:           return "Class-specific interface";
    case USB_DT_CS_ENDPOINT:            return "Class-specific endpoint";
    case USB_DT_CS_STRING:              return "Class-specific string";
    case USB_DT_DEVICE_QUALIFIER:       return "Device qualifier";
    case USB_DT_OTHER_SPEED_CONFIG:     return "Other speed config";
    case USB_DT_INTERFACE_POWER:        return "Interface power";
    case USB_DT_OTG:                    return "OTG";
    case USB_DT_DEBUG:                  return "Debug";
    case USB_DT_INTERFACE_ASSOCIATION:  return "Interface association";
    case USB_DT_SECURITY:               return "Security";
    case USB_DT_KEY:                    return "Key";
    case USB_DT_ENCRYPTION_TYPE:        return "Enrcyption type";
    case USB_DT_BOS:                    return "BOS";
    case USB_DT_DEVICE_CAPABILITY:      return "Capability";
    case USB_DT_WIRELESS_ENDPOINT_COMP: return "Wireless endpoint comp";
    case USB_DT_SS_ENDPOINT_COMP:       return "Endpoint comp";
    default:                            return "Other/Unknown";
    }
}

std::string ComHwLinuxSerial::classToString(uint8_t cls)
{
    switch(cls) {
    case USB_CLASS_APP_SPEC:            return "App. specific";
    case USB_CLASS_AUDIO:               return "Audio";
    case USB_CLASS_CDC_DATA:            return "CDC Data";
    case USB_CLASS_COMM:                return "Comm";
    case USB_CLASS_CONTENT_SEC:         return "Content Sec.";
    case USB_CLASS_CSCID:               return "CSCID";
    case USB_CLASS_HID:                 return "HID";
    case USB_CLASS_HUB:                 return "HUB";
    case USB_CLASS_MASS_STORAGE:        return "Mass storage";
    case USB_CLASS_MISC:                return "Misc";
    case USB_CLASS_PER_INTERFACE:       return "Per interface";
    case USB_CLASS_PHYSICAL:            return "Physical";
    case USB_CLASS_PRINTER:             return "Printer";
    case USB_CLASS_STILL_IMAGE:         return "Still image";
    case USB_CLASS_VENDOR_SPEC:         return "Vendor specific";
    case USB_CLASS_VIDEO:               return "Video";
    case USB_CLASS_WIRELESS_CONTROLLER: return "Wireless controller";
    default:                            return "Other/Unknown";
    }
}

void ComHwLinuxSerial::printDeviceDescriptor(usb_device* dev, usb_device_descriptor* desc,
                                             std::stringstream& ss)
{
    ss  << "Type: " << (int)desc->bDescriptorType << " (" << descriptorTypeToString(desc->bDescriptorType) << ")\n"
        << "   Length: "         << (int)desc->bLength << std::endl
        << "   USB version: "    << std::hex << (int)__le16_to_cpu(desc->bcdUSB) << std::dec << std::endl
        << "   Device version: " << (int)__le16_to_cpu(desc->bcdDevice) << std::endl
        << "   Class: "          << (int)desc->bDeviceClass << " (" << classToString(desc->bDeviceClass) << ")\n"
        << "   Subclass: "       << (int)desc->bDeviceSubClass << std::endl
        << "   Protocol: "       << std::hex << (int)desc->bDeviceProtocol << std::dec << std::endl
        << "   Num configs: "    << (int)desc->bNumConfigurations << std::endl
        << "   Product ID: "     << (int)__le16_to_cpu(desc->idProduct) << std::endl
        << "   Vendor ID: "      << (int)__le16_to_cpu(desc->idVendor) << std::endl
        << "   Product: "        << (int)desc->iProduct << " ("
        << toString(usb_device_get_string(dev,desc->iProduct)) << ")\n"
        << "   Manufacturer: "   << (int)desc->iManufacturer << " ("
        << toString(usb_device_get_string(dev,desc->iManufacturer)) << ")\n"
        << "   Serial num: "    << (int)desc->iSerialNumber << " ("
        << toString(usb_device_get_string(dev,desc->iSerialNumber)) << ")\n\n";
}

void ComHwLinuxSerial::printEndpointDescriptor(usb_device* dev, usb_endpoint_descriptor* desc,
                                               std::stringstream& ss)
{
    ss  << "Type: " << (int)desc->bDescriptorType << " (" << descriptorTypeToString(desc->bDescriptorType) << ")\n"
        << "   Length: "         << (int)desc->bLength << std::endl
        << "   Endpoint: "    << std::hex << (int)__le16_to_cpu(desc->bEndpointAddress) << std::dec << std::endl
        << "   IN/OUT: "         << usb_endpoint_direction(desc) << ")\n\n";
}
