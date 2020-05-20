#include "hdcp/exception.h"
#include "tcp_async.h"

using namespace hdcp;

TcpAsync::TcpAsync(common::Logger logger,
                   const std::string& remote_ip,
                   uint16_t remote_port):
    common::Log(logger),
    remote_ip_(remote_ip),
    remote_port_number_(remote_port),
    io_service_(),
    io_work_(new boost::asio::io_service::work(io_service_)),
    strand_(io_service_),
    async_thread_(boost::bind(&boost::asio::io_service::run, &io_service_)),
    signals_(io_service_),
    acceptor_(io_service_)
{
//    open();
}

TcpAsync::~TcpAsync()
{
    stop();
    close();
}

void TcpAsync::open()
{
    try {
        
        // Register to handle the signals that indicate when the server should exit.
        // It is safe to register for the same signal multiple times in a program,
        // provided all registration for the specified signal is made through Asio.
        signals_.add(SIGINT);
        signals_.add(SIGTERM);
        #if defined(SIGQUIT)
            signals_.add(SIGQUIT);
        #endif // defined(SIGQUIT)

        do_await_stop();

        // Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
        boost::asio::ip::tcp::resolver resolver(io_service_);
        boost::asio::ip::tcp::endpoint endpoint =
          *resolver.resolve(remote_ip_, std::to_string(remote_port_number_)).begin();
        acceptor_.open(boost::asio::ip::tcp::v4());
        acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
//        acceptor_.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), remote_port_number_));
        log_info(logger_, "open connection success with address: {}", endpoint.address().to_string());
        acceptor_.bind(endpoint);
        acceptor_.listen();
    }
    catch (std::exception& e)
    {
        throw hdcp::transport_error("error open socket");
    }
        
    log_info(logger_, "open connection success with address: {}", acceptor_.local_endpoint().address().to_string());
    
    // give some work to io_service before start
    io_service_.post(std::bind(&TcpAsync::do_accept, this));

    // run io_service for async io
//    io_thread_ = std::thread([this] () {
//                io_service_.run();
//            });
    
}

void TcpAsync::do_accept()
{
    acceptor_.async_accept(
          [this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket)
          {
            // Check whether the server was stopped by a signal before this
            // completion handler had a chance to run.
            if (!acceptor_.is_open())
            {
                log_error(logger_, "Acceptor error");
              return;
            }

            if (!ec)
            {
                log_info(logger_, "Start  connection");
                
                socket_ptr_ = std::make_shared<boost::asio::ip::tcp::socket>(std::move(socket));
                
                do_await_stop();
                
                do_read();
            }
          });
}

void TcpAsync::do_await_stop()
{
  signals_.async_wait(
      [this](boost::system::error_code /*ec*/, int /*signo*/)
      {
        // The server is stopped by cancelling all outstanding asynchronous
        // operations. Once all operations have finished the io_context::run()
        // call will exit.
        acceptor_.close();
      });
}

void TcpAsync::close()
{
    // empty queues
    while (read_queue_.pop()) {}
    while (write_queue_.pop()) {}
    
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!is_open())
        return;

    socket_ptr_->shutdown(boost::asio::ip::tcp::socket::shutdown_send);
    socket_ptr_->cancel();
    socket_ptr_->close();

    io_work_.reset();
    io_service_.stop();

    if (io_thread_.joinable())
        io_thread_.join();

    io_service_.reset();
}

void TcpAsync::write(const std::string& buf)
{
    log_info(logger_, "write : {}", buf);
    write(std::string(buf));
}

void TcpAsync::write(std::string&& buf)
{
    log_info(logger_, "write : {}", buf);
    if (!is_running())
        throw hdcp::transport_error("not allowed to write when transport is stopped");
    
    log_info(logger_, "post ");
    strand_.post(boost::bind(&TcpAsync::writeImpl,this, buf));
}

void TcpAsync::writeImpl(const std::string& message)
{
    if (!write_queue_.try_enqueue(message))
        throw hdcp::transport_error("write queue full");
    if ( write_queue_.size_approx() > 1 ) {
        // outstanding async_write
        log_info(logger_, "outstanding async_write");
        return;
    }

    this->write();
}

void TcpAsync::write()
{
    log_info(logger_, "concrete async_write");
    std::string message;
    if (write_queue_.try_dequeue(message)) {
        log_info(logger_, "try_dequeue ok");
        boost::asio::async_write(
        *socket_ptr_,
        boost::asio::buffer( message.c_str(), message.size() ),
        strand_.wrap(
            boost::bind(
                &TcpAsync::writeHandler,
                this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred
                )
            )
        );
    }
}

void TcpAsync::writeHandler(const boost::system::error_code& error, const size_t bytesTransferred)
{
    log_info(logger_, "writeHandler , pop");
    write_queue_.pop();

    if ( error ) {
        log_error(logger_, "could not write : {}" , boost::system::system_error(error).what());
        
        return;
    }

    if ( write_queue_.size_approx() > 0 ) {
        // more messages to send
        log_info(logger_, "more messages to send");
        this->write();
    }
}

bool TcpAsync::read(std::string& buf)
{
    if (!is_running())
        throw hdcp::transport_error("not allowed to read when transport is stopped");
    
    log_info(logger_, "waiting for read ");

    return read_queue_.wait_dequeue_timed(buf, timeout_read);
}

void TcpAsync::do_read()
{
    if (!is_running())
        throw hdcp::transport_error("not allowed to read when transport is stopped");
    
    socket_ptr_->async_read_some(boost::asio::buffer(buffer_),
        [this](boost::system::error_code ec, std::size_t bytes_transferred)
        {
          if (!ec)
          {
              log_info(logger_, "read : {} bytes" , bytes_transferred);
              // Enqueue readed buffer
              read_queue_.try_enqueue(buffer_);
          }

          else if (ec != boost::asio::error::operation_aborted)
          {
               log_error(logger_, "could read : {}" , boost::system::system_error(ec).what());
          }
        
        do_read();
    });
}

void TcpAsync::handle_read(const boost::system::error_code& error,
                                 size_t bytes_transferred) {
    log_info(logger_, "Read : {}, bytes.",  bytes_transferred);
    
    // handle the error
    if (error) {
        log_error(logger_, "Read Error : {}", boost::system::system_error(error).what());
        //TODO: check this
//        do_accept();
        return;
    }
    
    if (bytes_transferred == 0) {
        // This can happen when shutdown is called on the socket,
        // therefore we check _should_exit again.
//        do_accept();
        return;
    }

    if (bytes_transferred < 0) {
        // LogErr() << "recvfrom error: " << GET_ERROR(errno);
        return;
    }

    // Enqueue readed buffer
    read_queue_.try_enqueue(buffer_);
}

void TcpAsync::run()
{
    while (is_running()) {
        try {
            io_service_.run();
        } catch (std::exception& e) {
            log_error(logger_, e.what());
        }
    }
}

void TcpAsync::stop()
{
    close();
    
    common::Thread::stop();
    
    if (joinable())
        join();
}

void TcpAsync::start()
{
    log_info(logger_, "Start thread");
    if (is_running())
        return;
    open();
    common::Thread::start(0);
}
