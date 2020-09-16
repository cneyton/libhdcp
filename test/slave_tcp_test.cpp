#include <iostream>

#include "hdcp/hdcp.h"
#include "spdlog/sinks/stdout_color_sinks.h"

using namespace hdcp;

static void cmd_cb(const Packet&)
{
}

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <port>" << std::endl;
        exit(EXIT_FAILURE);
    }
    common::Logger logger(spdlog::stdout_color_mt("hdcp"));
    logger->set_level(spdlog::level::debug);

    Identification id {"server", "0001", "0.0.01", "0.1.02"};
    uint16_t port = std::stoi(argv[1]);
    Slave slave(logger, id, std::make_unique<TcpServer>(logger, port));
    slave.set_cmd_cb(cmd_cb);

    slave.start();
    slave.wait_connected();
    std::string data(1000, 'a');
    std::vector<hdcp::Packet::Block> blocks = {
        {
            .type = 0x2854,
            .data = data
        }
    };
    for (int i=0; i<20000; i++) {
        slave.send_data(blocks);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    slave.stop();
}
