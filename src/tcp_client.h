#include "boost/asio.hpp"

#include "common/thread.h"
#include "common/readerwriterqueue.h"

#include "transport.h"
#include "hdcp/exception.h"
#include "packet.h"

namespace hdcp
{

class TcpClient: public common::Log, private common::Thread, public Transport
{
public:
    TcpClient(common::Logger logger, std::string_view host, std::string_view service);

    virtual ~TcpClient();

    virtual void write(Packet&&);
    virtual void stop();
    virtual void start();
    bool is_open() {return socket_.is_open();};

private:
    using common::Thread::start;

    boost::asio::io_context      io_context_;
    boost::asio::ip::tcp::socket socket_;
    std::string                  host_;
    std::string                  service_;

    Packet read_packet_;
    Packet write_packet_;
    std::atomic_bool write_in_progress_ = false;

    void open();
    void close();

    void do_write();
    void read_header();
    void read_payload();

    virtual void run();
};

} /* namespace hdcp */
