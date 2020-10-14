#include <iostream>

#include "hdcp/hdcp.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include "config.h"

using namespace hdcp;

static void cmd_cb(const Packet&)
{
}

class Cli: public common::Log, public common::Thread
{
public:
    Cli(common::Logger logger, appli::Slave& com): Log(logger), com_(com) {}
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
    appli::Slave& com_;

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

    void test_command()
    {
        int  choice = 254;
        std::string usr_in;

        std::cout << "--------------------------------------------\n";
        std::cout << "test " << com_.get_slave_id() << "\n";
        std::cout << "--------------------------------------------\n";
        std::cout << "Select a command:\n"
            "  0) connect\n"
            "  1) disconnect\n"
            "  2) send data\n"
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
                std::vector<hdcp::Packet::Block> blocks = {
                    {
                        .type = 0x2854,
                        .data = std::string(1000, 'a')
                    }
                };
                log_info(logger_, "start sending data...");
                for (int i=0; i<20000; i++) {
                    com_.send_data(blocks);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                log_info(logger_, "20000 data sent...");
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
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <port>" << std::endl;
        exit(EXIT_FAILURE);
    }
    common::Logger logger(spdlog::stdout_color_mt("hdcp"));
    logger->set_level(spdlog::level::debug);

    Identification id {"server", "NA", "NA", HDCP_VERSION};
    uint16_t port = std::stoi(argv[1]);
    appli::Slave com(logger, id, std::make_unique<transport::tcp::Server>(logger, port));
    com.set_cmd_cb(cmd_cb);

    Cli cli(logger, com);

    cli.start();
    if (cli.joinable())
        cli.join();
}
