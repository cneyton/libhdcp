#include "hdcp/hdcp.h"
#include <iostream>

using namespace hdcp;

int main(int argc, char* argv[])
{
    if (argc != 3) {
        std::cerr << "usage: " << argv[0] << " <host> <port>" << std::endl;
        exit(EXIT_FAILURE);
    }
    common::Logger logger(spdlog::stdout_color_mt("hdcp"));
    logger->set_level(spdlog::level::trace);

    TcpClient client(logger, argv[1], argv[2]);

    client.start();
    client.write("toto\n");
    client.write("titi\n");
    std::this_thread::sleep_for(std::chrono::seconds(10));
    client.stop();
}
