#include <string>
#include <iostream>
#include  "packet.h"

using namespace hdcp;

int main()
{
    Packet p = Packet::make_keepalive(15);
    std::string data("toto");

    Packet p2 = Packet::make_command(16, 1, data);
    std::vector<Packet::Block> blocks = p2.get_blocks();
    std::cout << blocks.size() << std::endl;
    std::cout << blocks[0].data << std::endl;

    Identification host_id("host", "00001", "0.1.01", "0.1.02");

    Packet p3 = Packet::make_hip(17, host_id);
    std::vector<Packet::Block> blocks3 = p3.get_blocks();
    for (auto& b: blocks3) {
        std::cout << b.data << std::endl;
    }
}
