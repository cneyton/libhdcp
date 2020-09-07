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

    Identification id {"client", "0001", "0.0.01", "0.1.02"};
    Master master(logger, id,
                  std::make_unique<TcpClient>(logger, argv[1], argv[2]),
                  data_cb);

    master.start();
    auto [success, slave_id] = master.connect();
    if (!success)
        throw application_error("connection failed");

    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::string cmd_data("test");
    master.send_command(0, cmd_data,
                        [logger](Request& r)
                        {
                            switch (r.get_status()) {
                            case Request::Status::pending:
                                log_info(logger, "pending");
                                break;
                            case Request::Status::timeout:
                                log_info(logger, "timeout");
                                break;
                            case Request::Status::fulfilled:
                                log_info(logger, "fulfilled");
                                break;
                            }
                        });

    std::this_thread::sleep_for(std::chrono::seconds(30));
    master.stop();
}
