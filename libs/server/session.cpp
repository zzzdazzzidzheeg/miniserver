#include "session.h"
#include "logger.h"
#include <iostream>

Session::Session(asio::ssl::stream<asio::ip::tcp::socket> socket,
    RequestHandler request_handler, std::function<void()> on_close)
    : socket_(std::move(socket)),
    deadline_(socket_.get_executor()),
    request_handler_(std::move(request_handler)),
    on_close_(std::move(on_close)) {
}

Session::~Session() {
    if (on_close_) {
        on_close_();
    }
}

void Session::arm_deadline(std::chrono::seconds timeout) {
    deadline_.expires_after(timeout);
    deadline_.async_wait([self = shared_from_this()](beast::error_code ec) {
        if (!ec) {
            log_warning("Session: Connection deadline exceeded");
            beast::error_code close_ec;
            self->socket_.lowest_layer().cancel(close_ec);
            self->socket_.lowest_layer().close(close_ec);
        }
    });
}

void Session::cancel_deadline() {
    deadline_.cancel();
}

void Session::do_handshake() {
    arm_deadline(std::chrono::seconds(10));
    log_debug("Session: " + socket_.lowest_layer().remote_endpoint().address().to_string() + ": Doing handshake...");
    socket_.async_handshake(
        ssl::stream_base::server,
        [self = shared_from_this()](beast::error_code ec) {
            if (!ec) {
                self->cancel_deadline();
                log_debug("Session: SSL handshake successful");
                self->do_read(); 
            }
            else {
                log_error(std::string("Session: SSL handshake error: ") + ec.message());
            }
        }
    );
}

void Session::start() {
	log_info("Session: New connection from: " + socket_.lowest_layer().remote_endpoint().address().to_string());
    do_handshake();
}

void Session::do_read() {
    log_info("Session: Reading...");
    parser_.header_limit(8192);
    parser_.body_limit(1024 * 1024);
    arm_deadline(std::chrono::seconds(15));
    http::async_read(
        socket_, buffer_, parser_,
        [self = shared_from_this()](beast::error_code ec, std::size_t) {
            self->cancel_deadline();
            if (!ec) self->handle_request();
            else self->fail(ec, "read");
        }
    );
}

void Session::handle_request() {
try {
        // 1. Логируем, что запрос получен (помогает при отладке)
        log_debug("Session: Received " + std::string(parser_.get().method_string()) +
                  " " + std::string(parser_.get().target()));

        // 2. Создаем объект ответа с типом тела string_body
        auto request = parser_.release();

        auto response = request_handler_(std::move(request));
        log_info("Session: Sending HTTP response " + std::to_string(response.result_int()));
        send_response(std::move(response));
        // 3. Устанавливаем статус 200 OK и версию HTTP из запроса
    }
    catch (const std::exception& e) {
        log_error("Session: Critical error: " + std::string(e.what()));
        // Фолбэк ответ генерируем прямо здесь, так как это уровень сети
        HttpResponse err_res{http::status::internal_server_error, parser_.get().version()};
        err_res.body() = "Internal Server Error";
        err_res.prepare_payload();
        send_response(std::move(err_res));
    }
}

template <class Body>
void Session::send_response(http::response<Body> res) {
    auto resp = std::make_shared<http::response<Body>>(std::move(res));
    arm_deadline(std::chrono::seconds(15));
    http::async_write(
        socket_,
        *resp,
        [self = shared_from_this(), resp](beast::error_code ec, std::size_t) {
            if (!ec) {
                self->cancel_deadline();
                self->do_shutdown();
            }
            else {
                log_error("Session: Response write failed: " + ec.message());
                self->fail(ec, "write");
            }
        }
    );
}

void Session::fail(beast::error_code ec, char const* what) {
    // Игнорируем штатные ситуации
    if (ec == beast::http::error::body_limit) {
        log_warning("Session: Request body too large");
        // Можно отправить 413 Payload Too Large
        HttpResponse err_res{http::status::payload_too_large, parser_.get().version()};
        err_res.body() = "Request body too large";
        err_res.prepare_payload();
        send_response(std::move(err_res));
        return;
    }
    if (ec == asio::error::operation_aborted) {
        log_debug("Session: Operation aborted");
         return; // Операция отменена
    }
    if (ec == beast::http::error::end_of_stream) {
        log_debug("Session: End of stream");
        return; // Клиент закрыл соединение
    }
    if (ec == asio::error::eof) {
        log_debug("Session: End of file");
        return; // Конец потока
    }
    
    // Логируем только реальные ошибки
    log_error(std::string("Session: ") + what + ": " + ec.message());
    do_shutdown();
}

void Session::do_shutdown() {
    if (closed_) {
        return;
    }
    closed_ = true;
    cancel_deadline();
    log_debug("Session: Shutting down SSL connection...");
    
    socket_.async_shutdown(
        [self = shared_from_this()](beast::error_code ec) {
            if (ec) {
                // Игнорируем ошибки при shutdown (клиент мог уже закрыть соединение)
                if (ec != asio::error::eof) {
                    log_error("Session: Shutdown error: " + ec.message());
                }
            }
        });
}
