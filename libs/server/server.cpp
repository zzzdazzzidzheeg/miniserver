#include "server.h"
#include "logger.h"
#include <iostream> 
#include <boost/asio.hpp>
#include "session.h"

Server::Server(unsigned short port,
		    const std::string& cert_file,
            const std::string& key_file,
            RequestHandler request_handler) 
            : io_context_(boost::asio::io_context()),
              ssl_context_(boost::asio::ssl::context::tlsv13),
            acceptor_(io_context_),
            request_handler_(std::move(request_handler))
         {
    try {  
        acceptor_.open(boost::asio::ip::tcp::v4());
        acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
        acceptor_.bind(boost::asio::ip::tcp::endpoint(
            boost::asio::ip::tcp::v4(), port));
        acceptor_.listen(boost::asio::socket_base::max_listen_connections);
        
        ssl_context_.set_options(
            boost::asio::ssl::context::default_workarounds |
            boost::asio::ssl::context::no_sslv2 |
            boost::asio::ssl::context::no_sslv3
        );
        ssl_context_.use_certificate_chain_file(cert_file);
        ssl_context_.use_private_key_file(key_file, boost::asio::ssl::context::pem);
        log_debug("Server: SSL Context initialized");
        do_accept();
    }
    catch (const std::exception& e) {
        log_error(std::string("Error initializing server: ") +  e.what());
		throw;
	}
};
void Server::do_accept() {
    if (stopped_) return;
    try {
		log_debug("Server: Waiting for new connections...");
        acceptor_.async_accept([this](const boost::system::error_code& ec,
            boost::asio::ip::tcp::socket socket) {
                if (!ec) {
                    if (active_connections_.fetch_add(1, std::memory_order_relaxed) >= max_connections_) {
                        active_connections_.fetch_sub(1, std::memory_order_relaxed);
                        boost::system::error_code close_ec;
                        socket.close(close_ec);
                        log_warning("Server: Connection limit reached");
                    } else {
                        auto ssl_socket = std::make_unique<
                            boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>(
                                std::move(socket), ssl_context_);

                        auto release_connection = [this]() {
                            active_connections_.fetch_sub(1, std::memory_order_relaxed);
                        };
                        std::make_shared<Session>(std::move(*ssl_socket), request_handler_,
                            std::move(release_connection))->start();
                    }
                }
                if (ec && !stopped_) {
                    log_error(std::string("Server: Accept error: ") + ec.message());
                }
                if (!stopped_) do_accept();
                return;
            });
    }
    catch (const std::exception& e) {
        log_error(std::string("Server: Error accepting connection: ") + e.what());
    }
}

void Server::run() {
    try {
        log_debug("Server: Server is running");
        // async_accept starting with this
        io_context_.run();
    }
    catch (const std::exception& e) {
        log_error(std::string("Server: Error running server: ") + e.what());
	}
}

void Server::stop() {
    if (stopped_.exchange(true)) return;
    boost::system::error_code error;
    acceptor_.cancel(error);
    acceptor_.close(error);
    io_context_.stop();
    log_info("Server: Stopped");
}