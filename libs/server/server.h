#pragma once
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast.hpp>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include "server_config.h"
#include "http_types.h"

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
namespace ssl = asio::ssl;

class Session;

class Server {
public:
	Server(ServerConfig server_config, SessionConfig session_config, RequestHandler handler);
	Server(ServerConfig server_config, SessionConfig session_config, ContextRequestHandler handler);
	void run();
	void stop();
private:
	void do_accept();
	void do_stop();
	void run_worker() noexcept;
	std::atomic<std::size_t> active_connections_{0};
	std::atomic<bool> stop_requested_{false};
	ServerConfig server_config_;
	SessionConfig session_config_;
	boost::asio::io_context io_context_;
	asio::strand<asio::io_context::executor_type> control_strand_;
	boost::asio::ssl::context ssl_context_;
	boost::asio::ip::tcp::acceptor acceptor_;
	asio::signal_set signals_;
	ContextRequestHandler request_handler_;
	std::vector<std::weak_ptr<Session>> sessions_;
	std::mutex failure_mutex_;
	std::exception_ptr failure_;
};
