#pragma once
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <atomic>

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
namespace ssl = asio::ssl;

using HttpRequest = http::request<http::string_body>;
using HttpResponse = http::response<http::string_body>;

using RequestHandler = std::function<HttpResponse(const HttpRequest&)>;

class Server {
public:
	Server(unsigned short port,
		const std::string& cert_file,
		const std::string& key_file, 
		RequestHandler handler);
	void run();
	void stop();
private:
	void do_accept();
	static constexpr std::size_t max_connections_ = 1024;

	boost::asio::io_context io_context_;
	boost::asio::ssl::context ssl_context_;
	boost::asio::ip::tcp::acceptor acceptor_;
	std::atomic<std::size_t> active_connections_{0};
	std::atomic<bool> stopped_{false};
	RequestHandler request_handler_;
};
