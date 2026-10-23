#include "server.h"
#include "logger.h"
#include <iostream> 
#include <boost/asio.hpp>
#include "session.h"
#include <algorithm>
#include <csignal>
#include <exception>

Server::Server(ServerConfig server_config, SessionConfig session_config,
            RequestHandler request_handler)
            : server_config_(std::move(server_config)),
              session_config_(std::move(session_config)),
              io_context_(boost::asio::io_context()),
              ssl_context_(boost::asio::ssl::context::tlsv13),
              acceptor_(io_context_),
              signals_(io_context_, SIGINT, SIGTERM),
            request_handler_(std::move(request_handler))
         {
    try {  
        const auto address = boost::asio::ip::make_address(server_config_.address);
        const boost::asio::ip::tcp::endpoint endpoint(address, server_config_.port);
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen(boost::asio::socket_base::max_listen_connections);
        
        ssl_context_.set_options(
            boost::asio::ssl::context::default_workarounds |
            boost::asio::ssl::context::no_sslv2 |
            boost::asio::ssl::context::no_sslv3
        );
        ssl_context_.use_certificate_chain_file(server_config_.certificate_file);
        ssl_context_.use_private_key_file(server_config_.private_key_file, boost::asio::ssl::context::pem);
        log_debug("Server: SSL Context initialized");
        do_accept();
    }
    catch (const std::exception& e) {
        log_error(std::string("Error initializing server: ") +  e.what());
		throw;
	}
};
void Server::do_accept() {
    if (stop_requested_) return;
    log_debug("Server: Waiting for new connections...");
    acceptor_.async_accept([this](const boost::system::error_code& ec,
        boost::asio::ip::tcp::socket socket) {
            if (!ec) {
                if (stop_requested_) {
                    boost::system::error_code close_ec;
                    socket.close(close_ec);
                } else if (active_connections_.fetch_add(1, std::memory_order_relaxed) >= server_config_.max_connections) {
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
                    auto session = std::make_shared<Session>(std::move(*ssl_socket),
                        request_handler_, session_config_, std::move(release_connection));
                    sessions_.erase(std::remove_if(sessions_.begin(), sessions_.end(),
                        [](const auto& item) { return item.expired(); }), sessions_.end());
                    sessions_.push_back(session);
                    session->start();
                }
            }
            if (ec && !stop_requested_ && ec != asio::error::operation_aborted) {
                log_error(std::string("Server: Accept error: ") + ec.message());
            }
            if (!stop_requested_) do_accept();
        });
}

void Server::run() {
    if (!stop_requested_) {
        signals_.async_wait([this](const boost::system::error_code& ec, int signal) {
            if (ec) return;
            log_info("Server: Received signal " + std::to_string(signal) + ", shutting down");
            stop();
        });
    }

    log_debug("Server: Server is running");
    std::exception_ptr failure;
    for (;;) {
        try {
            io_context_.run();
            break;
        } catch (...) {
            if (!failure) failure = std::current_exception();
            stop_requested_.store(true, std::memory_order_relaxed);
            do_stop();
        }
    }
    if (failure) std::rethrow_exception(failure);
}

void Server::stop() {
    if (stop_requested_.exchange(true)) return;
    asio::dispatch(io_context_, [this]() { do_stop(); });
}

void Server::do_stop() {
    boost::system::error_code error;
    acceptor_.cancel(error);
    acceptor_.close(error);
    signals_.cancel(error);
    for (const auto& weak_session : sessions_) {
        if (auto session = weak_session.lock()) session->force_close();
    }
    sessions_.clear();
    log_info("Server: Stopped");
}
