#include <iostream>

#include "hdcp/hdcp.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include "config.h"

using namespace hdcp;

static void data_cb(const Packet&)
{
    //std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

class Cli: public common::Log, public common::Thread
{
public:
    Cli(common::Logger logger, appli::Master& com): Log(logger), com_(com) {}
    ~Cli()
    {
        com_.stop();
        stop();
        if (joinable())
            join();
    }

    void start()
    {
        com_.start();
        common::Thread::start(0);
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
        std::cout << "test " << com_.get_master_id() << "\n";
        std::cout << "--------------------------------------------\n";
        std::cout << "Select a command:\n"
            "  0) connect\n"
            "  1) disconnect\n"
            "  2) send command\n"
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
            com_.connect();
            break;
        case 1:
            com_.disconnect();
            break;
        case 2:
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

int main(int argc, char* argv[])
{
    if (argc != 3) {
        std::cerr << "usage: " << argv[0] << " <host> <port>" << std::endl;
        exit(EXIT_FAILURE);
    }
    common::Logger logger(spdlog::stderr_color_mt("hdcp"));
    logger->set_level(spdlog::level::debug);

    Identification id {"client", "NA", "NA", HDCP_VERSION};
    appli::Master com(logger, id,
                  std::make_unique<transport::tcp::Client>(logger, argv[1], argv[2]));
    com.set_data_cb(data_cb);

    Cli cli(logger, com);

    cli.start();
    if (cli.joinable())
        cli.join();
}
