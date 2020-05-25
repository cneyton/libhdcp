#include <iostream>

#include "common/log.h"
#include "hdcp/hdcp.h"
#include "spdlog/sinks/stdout_color_sinks.h"

using namespace hdcp;

class TransportTest: public Transport
{
public:
    TransportTest() {};
    virtual ~TransportTest() {};

    virtual void write(const std::string& buf)
    {
        std::cout << "write1: " << buf << std::endl;
    }

    virtual void write(std::string&& buf)
    {
        std::cout << "write1: " << buf << std::endl;
    }

    virtual bool read(std::string& buf)
    {
        Identification dev_id("dev", "00001", "0.1.01", "0.1.02");
        Packet dip = Packet::make_dip(1, dev_id);
        buf = dip.get_data();
        return true;
    }

    virtual void start()
    {
        std::cout << "start" << std::endl;
    }

    virtual void stop()
    {
        std::cout << "stop" << std::endl;
    }
};

void data_cb(const Packet&)
{
}

int main()
{
    common::Logger logger(spdlog::stdout_color_mt("hdcp"));
    logger->set_level(spdlog::level::trace);

    TransportTest transport;
    Identification host_id("host", "00001", "0.1.01", "0.1.02");
    Master m(logger, &transport, data_cb, host_id);
    m.start();
    m.stop();
}
