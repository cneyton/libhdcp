#include <iostream>

#include "hdcp/hdcp.h"
#include "spdlog/sinks/stdout_color_sinks.h"

using namespace hdcp;

static void cmd_cb(const Packet&)
{
    std::cout << "data received" << std::endl;
}

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <port>" << std::endl;
        exit(EXIT_FAILURE);
    }
    common::Logger logger(spdlog::stdout_color_mt("hdcp"));
    logger->set_level(spdlog::level::trace);

    Identification id ("server", "0001", "0.0.01", "0.1.02");
    uint16_t port = std::stoi(argv[1]);
    TcpServer server(logger, port);
    Slave slave(logger, &server, cmd_cb, id);

    server.start();
    slave.start();
    std::this_thread::sleep_for(std::chrono::seconds(1000));
    slave.stop();
    server.stop();
}
