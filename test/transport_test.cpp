#include "common/log.h"
#include "hdcp/hdcp.h"
#include <exception>

#define ITFC_NB                     1
#define VENDOR_ID                   0x0483
#define PRODUCT_ID                  0x5740
#define IN                          0x81
#define OUT                         0x01

using namespace hdcp;

int main()
{
    common::Logger logger(spdlog::stdout_color_mt("hdcp"));
    logger->set_level(spdlog::level::trace);

    transport::usb::Device t(logger, ITFC_NB, VENDOR_ID, PRODUCT_ID, IN, OUT);
    try {
        t.start();
    } catch (std::exception& e) {
        log_error(logger, e.what());
    }
    t.stop();
}
