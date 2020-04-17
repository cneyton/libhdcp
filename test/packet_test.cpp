#include <string>
#include <iostream>
#include  "hdcp/hdcp.h"

using namespace hdcp;

int main()
{
    Packet p = Packet::make_keepalive(15);

    // test copy-assigment
    Identification host_id("host", "00001", "0.1.01", "0.1.02");
    p = Packet::make_hip(17, host_id);
    std::vector<Packet::Block> blocks = p.get_blocks();
    for (auto& b: blocks) {
        std::cout << b.data.data() << std::endl;
    }

    // test move constructor
    std::cout << (void*)p.get_data().data() << std::endl;
    Packet p_copy(std::move(p));
    std::cout << (void*)p_copy.get_data().data() << std::endl;

    // test copy constructor
    Packet p_copy2(std::move(p));
    std::cout << (void*)p_copy2.get_data().data() << std::endl;

    // test command
    std::string a(1000, 'a');
    p = Packet::make_command(1, 1, a);
    blocks = p.get_blocks();
    for (auto& b: blocks) {
        std::cout << b.data.data() << std::endl;
    }
}
