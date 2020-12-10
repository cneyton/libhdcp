#include "boost/asio.hpp"

#include "common/thread.h"

#include "transport.h"

namespace hdcp {
namespace transport {
namespace tcp {

class Client: public common::Log, private common::Thread, public Transport
{
public:
    Client(common::Logger logger, std::string_view host, std::string_view service);
    ~Client();

    void write(Packet&& p) override;
    void start()   override;
    void stop()    override;
    bool is_open() override;
    void open()    override;
    void close()   override;

private:
    using common::Thread::start;

    boost::asio::io_context      io_context_;
    boost::asio::ip::tcp::socket socket_;
    std::string                  host_;
    std::string                  service_;

    Packet read_packet_;
    Packet write_packet_;
    std::atomic_bool write_in_progress_ = false;

    void do_write();
    void read_header();
    void read_payload();

    void run() override;
};

} /* namespace tcp  */
} /* namespace transport  */
} /* namespace hdcp */
