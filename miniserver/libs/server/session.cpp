#include "session.h"
#include "logger.h"
#include <iostream>
#include <string_view>

Session::Session(asio::ssl::stream<asio::ip::tcp::socket> socket,
    AsyncContextRequestHandler request_handler, SessionConfig config, std::function<void()> on_close)
    : socket_(std::move(socket)),
    deadline_(socket_.get_executor()),
    request_handler_(std::move(request_handler)),
    config_(std::move(config)),
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
            log_warning("Session", "Connection deadline exceeded");
            self->closed_ = true;
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
    arm_deadline(config_.handshake_timeout);
    log_debug("Session", socket_.lowest_layer().remote_endpoint().address().to_string() + ": Doing handshake...");
    socket_.async_handshake(
        ssl::stream_base::server,
        [self = shared_from_this()](beast::error_code ec) {
            if (!ec) {
                self->cancel_deadline();
                log_debug("Session", "SSL handshake successful");
                self->do_read(); 
            }
            else {
                log_error("Session", std::string("SSL handshake error: ") + ec.message());
            }
        }
    );
}

void Session::start() {
	const auto self = shared_from_this();
	asio::dispatch(socket_.get_executor(), [self] {
		self->client_address_ = self->socket_.lowest_layer().remote_endpoint().address().to_string();
		log_info("Session", "New connection from: " + self->client_address_);
		self->do_handshake();
	});
}

void Session::force_close() {
	const auto self = shared_from_this();
	asio::dispatch(socket_.get_executor(), [self] {
		self->closed_ = true;
		beast::error_code error;
		self->deadline_.cancel();
		self->socket_.lowest_layer().cancel(error);
		self->socket_.lowest_layer().close(error);
	});
}

void Session::do_read() {
    log_debug("Session", "Reading request");
    parser_.header_limit(config_.header_limit);
    parser_.body_limit(config_.body_limit);
    arm_deadline(config_.read_timeout);
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
    const unsigned request_version = parser_.get().version();
try {
        // 1. Логируем, что запрос получен (помогает при отладке)
        const auto target = parser_.get().target();
        const std::string_view target_view(target.data(), target.size());
        const auto query_start = target_view.find('?');
        log_debug("Session", "Received " + std::string(parser_.get().method_string()) +
                  " " + std::string(target_view.substr(0, query_start)));

        // 2. Создаем объект ответа с типом тела string_body
        auto request = parser_.release();
        auto completed = std::make_shared<std::atomic<bool>>(false);
        std::weak_ptr<Session> weak_self = shared_from_this();
        arm_deadline(config_.handler_timeout);
        request_handler_(std::move(request), client_address_,
            [weak_self, completed](HttpResponse response) mutable {
                if (completed->exchange(true, std::memory_order_relaxed)) return;
                if (auto self = weak_self.lock()) {
                    asio::dispatch(self->socket_.get_executor(),
                        [self, response = std::move(response)]() mutable {
                            self->cancel_deadline();
                            if (self->closed_) return;
                            const auto status = std::visit(
                                [](const auto& message) { return message.result_int(); }, response);
                            log_info("Session", "Sending HTTP response " + std::to_string(status));
                            self->send_response(std::move(response));
                        });
                }
            });
        // 3. Устанавливаем статус 200 OK и версию HTTP из запроса
    }
    catch (const std::exception& e) {
        log_error("Session", "Request handling exception: " + std::string(e.what()));
        // Фолбэк ответ генерируем прямо здесь, так как это уровень сети
        cancel_deadline();
        HttpStringResponse err_res{http::status::internal_server_error, request_version};
        err_res.body() = "Internal Server Error";
        err_res.prepare_payload();
        send_response(std::move(err_res));
    }
}

void Session::send_response(HttpResponse response) {
    std::visit([this](auto&& message) {
        write_response(std::move(message));
    }, std::move(response));
}

template <class Body>
void Session::write_response(http::response<Body> res) {
    // This session handles one request and then shuts down the connection.
    res.keep_alive(false);
    auto resp = std::make_shared<http::response<Body>>(std::move(res));
    arm_deadline(config_.write_timeout);
    http::async_write(
        socket_,
        *resp,
        [self = shared_from_this(), resp](beast::error_code ec, std::size_t) {
            if (!ec) {
                self->cancel_deadline();
                self->do_shutdown();
            }
            else {
                log_error("Session", "Response write failed: " + ec.message());
                self->fail(ec, "write");
            }
        }
    );
}

void Session::fail(beast::error_code ec, char const* what) {
    // Игнорируем штатные ситуации
    if (ec == beast::http::error::body_limit) {
        log_warning("Session", "Request body too large");
        // Можно отправить 413 Payload Too Large
        HttpStringResponse err_res{http::status::payload_too_large, parser_.get().version()};
        err_res.body() = "Request body too large";
        err_res.prepare_payload();
        send_response(std::move(err_res));
        return;
    }
    if (ec == asio::error::operation_aborted) {
        log_debug("Session", "Operation aborted");
         return; // Операция отменена
    }
    if (ec == beast::http::error::end_of_stream) {
        log_debug("Session", "End of stream");
        return; // Клиент закрыл соединение
    }
    if (ec == asio::error::eof) {
        log_debug("Session", "End of file");
        return; // Конец потока
    }
    
    // Логируем только реальные ошибки
    log_error("Session", std::string(what) + " failed: " + ec.message());
    do_shutdown();
}

void Session::do_shutdown() {
    if (closed_) {
        return;
    }
    closed_ = true;
    cancel_deadline();
    log_debug("Session", "Shutting down SSL connection...");

    arm_deadline(config_.shutdown_timeout);
    socket_.async_shutdown(
        [self = shared_from_this()](beast::error_code ec) {
            self->cancel_deadline();
            if (ec) {
                // Игнорируем ошибки при shutdown (клиент мог уже закрыть соединение)
                if (ec != asio::error::eof) {
                    log_error("Session", "Shutdown error: " + ec.message());
                }
            }
        });
}
