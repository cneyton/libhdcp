#pragma once

#include <stdexcept>
#include <system_error>

namespace hdcp {

class hdcp_error: public std::runtime_error
{
public:
    hdcp_error(const std::error_code& errc):
        std::runtime_error(errc.message()), errc_(errc) {}
    hdcp_error(const std::error_code& errc, const std::string& what_arg):
        std::runtime_error(errc.message() + ":" + what_arg), errc_(errc) {}

    const std::error_code& code() const noexcept {return errc_;}

private:
    std::error_code errc_;
};

} /* namespace hdcp */

