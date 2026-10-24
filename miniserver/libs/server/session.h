#pragma once
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <functional>
#include <atomic>
#include <memory>
#include <string>
#include "server_config.h"
#include "http_types.h"

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
namespace ssl = asio::ssl;
class Session : public std::enable_shared_from_this<Session> {
public:
    explicit Session(asio::ssl::stream<asio::ip::tcp::socket> socket,
        AsyncContextRequestHandler request_handler, SessionConfig config, std::function<void()> on_close);
    ~Session();
    void start();
    void force_close();

private:
    void do_handshake(); //
    void do_read(); //
    void handle_request();
    void send_response(HttpResponse response);
    template <class Body>
    void write_response(http::response<Body> response);
    asio::ssl::stream<asio::ip::tcp::socket> socket_;
    asio::steady_timer deadline_;
    beast::flat_buffer buffer_;
	http::request_parser<http::string_body> parser_;
    AsyncContextRequestHandler request_handler_;
    SessionConfig config_;
    std::string client_address_;
    std::function<void()> on_close_;
    bool closed_ = false;
    void arm_deadline(std::chrono::seconds timeout);
    void cancel_deadline();
    void fail(beast::error_code ec, char const* what);
    void do_shutdown();
};
