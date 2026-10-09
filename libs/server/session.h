#pragma once
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <functional>
#include <memory>

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
namespace ssl = asio::ssl;
using HttpRequest = http::request<http::string_body>;
using HttpResponse = http::response<http::string_body>;

using RequestHandler = std::function<HttpResponse(const HttpRequest&)>;

class Session : public std::enable_shared_from_this<Session> {
public:
    explicit Session(asio::ssl::stream<asio::ip::tcp::socket> socket,
        RequestHandler request_handler, std::function<void()> on_close);
    ~Session();
    void start();

private:
    void do_handshake(); //
    void do_read(); //
    void handle_request();
    template <class Body>
	void send_response(http::response<Body> res); //
    asio::ssl::stream<asio::ip::tcp::socket> socket_;
    asio::steady_timer deadline_;
    beast::flat_buffer buffer_;
    http::request_parser<http::string_body> parser_;
	RequestHandler request_handler_;
    std::function<void()> on_close_;
    bool closed_ = false;
    void arm_deadline(std::chrono::seconds timeout);
    void cancel_deadline();
    void fail(beast::error_code ec, char const* what);
    void do_shutdown();
};