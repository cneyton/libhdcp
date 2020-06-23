#pragma once

#include <string>
#include <chrono>

namespace hdcp
{

constexpr std::chrono::milliseconds connecting_timeout_(1000);
constexpr std::chrono::milliseconds command_timeout_(1000);
constexpr std::chrono::milliseconds keepalive_timeout(3000);
constexpr std::chrono::milliseconds keepalive_interval(1000);

struct Identification
{
    Identification() {}
    Identification(const std::string& name,       const std::string& serial_number,
                   const std::string& hw_version, const std::string& sw_version):
        name(name), serial_number(serial_number),
        hw_version(hw_version), sw_version(sw_version) {}

    std::string name;
    std::string serial_number;
    std::string hw_version;
    std::string sw_version;
};

} /* namespace hdcp */
