#include "common/log.h"
#include "hdcp/hdcp.h"
#include <exception>
#include <iostream>

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
    
    TcpAsync t(logger, "192.168.1.14", 55555);
    try {
        t.start();
    } catch (std::exception& e) {
        log_error(logger, e.what());
    }
    
    
    char c[1];
    std::cin.getline(c,1); //It takes 1 charcters as input;
    std::cout<<c<<std::endl;
    
    std::string read_buf;
    while (!t.read(read_buf)) {
        log_info(logger, "Reading : {}", read_buf);
    }
    
    t.write("ok");
    t.write("bye");
    
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    log_info(logger, "Stop transport");
    
    t.stop();
}
