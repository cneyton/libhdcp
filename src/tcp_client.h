#include "boost/asio.hpp"

#include "common/thread.h"
#include "common/readerwriterqueue.h"

#include "transport.h"
#include "hdcp/exception.h"

namespace hdcp
{

class TcpClient: public common::Log, private common::Thread, public Transport
{
public:
    TcpClient(common::Logger logger, std::string_view host, std::string_view service);

    virtual ~TcpClient();

    virtual void write(const std::string& buf);
    virtual void write(std::string&& buf);
    virtual bool read(std::string& buf);
    virtual void stop();
    virtual void start();
    bool is_open() {return socket_.is_open();};

private:
    using common::Thread::start;

    boost::asio::io_context      io_context_;
    boost::asio::ip::tcp::socket socket_;
    std::string                  host_;
    std::string                  service_;
    std::array<char, max_transfer_size> read_buf_ = {0};
    common::ReaderWriterQueue<std::string>         write_queue_;
    common::BlockingReaderWriterQueue<std::string> read_queue_;

    void open();
    void close();

    void do_read();
    void do_write();

    virtual void run();
};

} /* namespace hdcp */
