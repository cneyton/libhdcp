#include <iostream>

#include "common/log.h"
#include "hdcp/hdcp.h"
#include "spdlog/sinks/stdout_color_sinks.h"

using namespace hdcp;

int main()
{
    common::Logger logger(spdlog::stdout_color_mt("hdcp"));
    logger->set_level(spdlog::level::trace);

    TcpAsync transport(logger, "192.168.1.14", 55555);
    Identification host_id("host", "00001", "0.1.01", "0.1.02");
    
    ApplicationSlave::DataCallback dc {
    };
    ApplicationSlave app(logger, &transport, dc, host_id);
    app.start();
    
    
//    app.connect();
//    if (app.joinable())
//        app.join();
    std::cout << "Wait for connection" << std::endl;
    char c[1];
    std::cin.getline(c,1); //It takes 1 charcters as input;
    std::cout<<c<<std::endl;
    
    std::cout << "Try connection" << std::endl;
    app.connect();
    
    std::cout << "connected" << std::endl;
    
    app.send_command(1, "test", nullptr);
    
    char d[1];
    std::cin.getline(d,1); //It takes 1 charcters as input;
    std::cout<<c<<std::endl;
}
