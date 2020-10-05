#include <iostream>

#include "hdcp/hdcp.h"
#include "spdlog/sinks/stdout_color_sinks.h"

using namespace hdcp;
constexpr int      itfc_nb      = 1;
constexpr uint16_t vendor_id    = 0x0483;
constexpr uint16_t product_id   = 0x5740;
constexpr uint8_t  in_endpoint  = 0x81;
constexpr uint8_t  out_endpoint = 0x01;

static void data_cb(const Packet&)
{
    std::cout << "data received" << std::endl;
}

int main(int argc, char* argv[])
{
    if (argc != 1) {
        std::cerr << "usage: " << argv[0] << std::endl;
        exit(EXIT_FAILURE);
    }
    common::Logger logger(spdlog::stdout_color_mt("hdcp"));
    logger->set_level(spdlog::level::trace);

    Identification id {"client", "0001", "0.0.01", "0.1.02"};
    appli::Master master(logger, id,
                         std::make_unique<transport::usb::Device>(logger, itfc_nb, vendor_id,
                                                                  product_id, in_endpoint,
                                                                  out_endpoint));
    master.set_data_cb(data_cb);

    master.start();
    master.connect();
    std::this_thread::sleep_for(std::chrono::seconds(1000));
    master.stop();
}
