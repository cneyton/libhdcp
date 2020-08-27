#include <iostream>

#include "hdcp/hdcp.h"
#include "spdlog/sinks/stdout_color_sinks.h"

using namespace hdcp;

static void data_cb(const Packet&)
{
    //std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

int main(int argc, char* argv[])
{
    if (argc != 3) {
        std::cerr << "usage: " << argv[0] << " <host> <port>" << std::endl;
        exit(EXIT_FAILURE);
    }
    common::Logger logger(spdlog::stdout_color_mt("hdcp"));
    logger->set_level(spdlog::level::debug);

    Identification id ("client", "0001", "0.0.01", "0.1.02");
    TcpClient client(logger, argv[1], argv[2]);
    Master master(logger, &client, data_cb, id);

    client.start();
    master.start();
    master.connect();
    std::this_thread::sleep_for(std::chrono::seconds(1000));
    master.stop();
    client.stop();
}
