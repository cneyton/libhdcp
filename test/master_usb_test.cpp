#include <iostream>

#include "hdcp/hdcp.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include "config.h"

using namespace hdcp;
constexpr int      itfc_nb      = 1;
constexpr uint16_t vendor_id    = 0x0483;
constexpr uint16_t product_id   = 0x5740;
constexpr uint8_t  in_endpoint  = 0x81;
constexpr uint8_t  out_endpoint = 0x01;

common::Logger logger(spdlog::stderr_color_mt("hdcp"));

static void data_cb(const Packet&)
{
}

static void com_status_cb(appli::Master::State s, const std::error_code& e)
{
    switch (s) {
    case appli::Master::State::init:
        log_info(logger, "init: {}", e.message());
        break;
    case appli::Master::State::disconnected:
        log_info(logger, "disconnected: {}", e.message());
        break;
    case appli::Master::State::connecting:
        log_info(logger, "connecting: {}", e.message());
        break;
    case appli::Master::State::connected:
        log_info(logger, "connected: {}", e.message());
        break;
    }
}

class Cli: public common::Log, public common::Thread
{
public:
    Cli(common::Logger logger, appli::Master& com): Log(logger), com_(com)
    {
        common::Thread::start(0);
    }

    ~Cli()
    {
        com_.stop();
        common::Thread::stop();
        if (joinable())
            join();
    }

private:
    using common::Thread::start;
    appli::Master& com_;

    void set_log_level()
    {
        int  choice = 254;
        std::string usr_in;

        do {
            std::cout << "Select log level:\n"
                " 0) trace\n"
                " 1) debug\n"
                " 2) info\n"
                " 3) warning\n"
                " 4) error\n"
                " 5) critical\n"
                ">> ";

            std::cin >> usr_in;
            try {
                choice = std::stoi(usr_in);
            } catch (...) {
                choice = 254;
            }

            switch (choice) {
            case 0: logger_->set_level(spdlog::level::trace); break;
            case 1: logger_->set_level(spdlog::level::debug); break;
            case 2: logger_->set_level(spdlog::level::info); break;
            case 3: logger_->set_level(spdlog::level::warn); break;
            case 4: logger_->set_level(spdlog::level::critical); break;
            default: choice = 254; break;
            }
        } while(choice == 254);
    };

    void request_cb(Request&)
    {
    }

    void test_command()
    {
        int  choice = 254;
        std::string usr_in;

        std::cout << "--------------------------------------------\n";
        std::cout << "test " << com_.master_id() << "\n";
        std::cout << "--------------------------------------------\n";
        std::cout << "Select a command:\n"
            "  0) start\n"
            "  1) stop\n"
            "  2) connect\n"
            "  3) disconnect\n"
            "  4) send command\n"
            "255) EXIT\n"
            ">> ";
        /* get user input */
        std::cin >> usr_in;
        if (usr_in == "l") {
            set_log_level();
            return;
        }

        try {
            choice = std::stoi(usr_in);
        } catch (...) {
            log_warn(logger_, "invalid choice");
            choice = 254;
        }

        switch (choice) {
        case 0:
            com_.start();
            break;
        case 1:
            com_.stop();
            break;
        case 2:
            com_.async_connect();
            break;
        case 3:
            com_.async_disconnect();
            break;
        case 4:
        {
            std::string cmd_data("test");
            com_.send_command(0, cmd_data,
                              std::bind(&Cli::request_cb, this, std::placeholders::_1));
            break;
        }
        case 255:
            stop();
            break;
        default:
            std::cout << "invalid choice: " << usr_in << std::endl;
            break;
        }
    }

    void run() override
    {
        while (is_running()) {
            try {
                test_command();
            } catch (std::exception& e) {
                log_error(logger_, e.what());
            }
        }
    }
};

int main(int, char**)
{
    logger->set_level(spdlog::level::debug);

    Identification id {"client", "NA", "NA", HDCP_VERSION};
    appli::Master master(logger, id,
                         std::make_unique<transport::usb::Device>(logger, itfc_nb, vendor_id,
                                                                  product_id, in_endpoint,
                                                                  out_endpoint));
    master.set_data_cb(data_cb);
    master.set_status_cb(com_status_cb);

    Cli cli(logger, master);

    if (cli.joinable())
        cli.join();
}
