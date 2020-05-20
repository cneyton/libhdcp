#pragma once

#include "common/log.h"
#include "common/readerwriterqueue.h"
#include "common/thread.h"
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include "transport.h"

namespace hdcp
{

class TcpAsync: public common::Log, private common::Thread, public Transport, public std::enable_shared_from_this<TcpAsync>
{
public:
    explicit TcpAsync(common::Logger logger,
             const std::string& remote_ip,
             uint16_t remote_port);
    virtual ~TcpAsync();
    // Non-copyable
    TcpAsync(const TcpAsync&) = delete;
    const TcpAsync& operator=(const TcpAsync&) = delete;

    virtual void write(const std::string& buf);
    virtual void write(std::string&& buf);
    virtual bool read(std::string& buf);
    virtual void stop();
    virtual void start();
    
    inline bool is_open()
    {
        return (socket_ptr_) ? socket_ptr_->is_open() : false;
    }

private:
    using common::Thread::start;
    
    std::recursive_mutex mutex_;
    
    std::string remote_ip_ = {};
    
    uint16_t remote_port_number_;
    
    boost::asio::io_service io_service_;
    std::unique_ptr<boost::asio::io_service::work> io_work_;
    std::thread io_thread_;
    boost::asio::io_service::strand strand_;
    boost::thread async_thread_;

    // Socket for the connection.
    std::shared_ptr<boost::asio::ip::tcp::socket> socket_ptr_;

    // The io_context used to perform asynchronous operations.
//    boost::asio::io_context& io_context_;

    // The signal_set is used to register for process termination notifications.
    boost::asio::signal_set signals_;

    // Acceptor used to listen for incoming connections.
    boost::asio::ip::tcp::acceptor acceptor_;

    common::ReaderWriterQueue<std::string>         write_queue_;
    common::BlockingReaderWriterQueue<std::string> read_queue_;
    
    enum { read_buffer_length = 1024 };
    char buffer_[read_buffer_length];
    
    // Perform an asynchronous accept operation.
    void do_accept();

    // Wait for a request to stop the server.
    void do_await_stop();
    
    // Perform an asynchronous read operation.
    void do_read();
    
    // Send the data to the client
    void handle_read(const boost::system::error_code& error,
                     size_t bytes_transferred);

    // Perform an asynchronous write operation.
    void do_write(const char *data, size_t size);

    void open();
    void close();

    virtual void run();
    
    void writeImpl(const std::string& message);

    void write();

    void writeHandler(const boost::system::error_code& error, const size_t bytesTransferred);

};

} /* namespace hdcp */
