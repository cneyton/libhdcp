#pragma once

#include <string>
#include <chrono>

#include "common/log.h"

namespace hdcp {

constexpr std::chrono::milliseconds connecting_timeout_(1000);
constexpr std::chrono::milliseconds command_timeout_(1000);
constexpr std::chrono::milliseconds keepalive_timeout(3000);
constexpr std::chrono::milliseconds keepalive_interval(1000);
constexpr uint max_connection_attempts = 3;

struct Identification
{
    std::string name;
    std::string serial_number;
    std::string hw_version;
    std::string sw_version;

    friend std::ostream& operator<<(std::ostream& os, const Identification& id)
    {
        return os << fmt::format("identification :\n"
                                 "\tname: {}\n"
                                 "\tserial number: {}\n"
                                 "\thardware version: {}\n"
                                 "\tsoftware version: {}",
                                 id.name, id.serial_number, id.hw_version, id.sw_version);
    }
};

/* TODO: add com status class by grouping master & slave states <11-12-20, cneyton> */

//class Status
//{
//public:
    //enum class State {
        //init,
        //disconnected,
        //connecting,
        //connected
    //};
    //State state()                 const {return s_;}
    //const std::error_code& errc() const {return errc_;}
//private:
    //State           s_;
    //std::error_code errc_;
//};

} /* namespace hdcp */
