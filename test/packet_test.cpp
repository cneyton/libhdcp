#include <string>
#include <iostream>
#include  "hdcp/hdcp.h"

using namespace hdcp;

int main()
{
    Packet p = Packet::make_keepalive(15);
    std::cout << "size: " << sizeof(p) << std::endl;
    std::cout << "address = " << (void*)p.data() << std::endl;

    // test copy-assignment
    Identification host_id {"host", "00001", "0.1.01", "0.1.02"};
    auto hip = Packet::make_hip(17, host_id);
    p = hip;
    std::cout << "copy assignment: address = " << (void*)p.data() << std::endl;

    // test move constructor
    Packet p_copy(std::move(p));
    std::cout << "move ctor: address = " << (void*)p_copy.data() << std::endl;

    // test move assignment
    Packet p_copy2;
    p_copy2 = std::move(p_copy);
    std::cout << "move assignment: address = " << (void*)p_copy2.data() << std::endl;
}
