#include "statistics_middleware.h"

#include <boost/beast/http.hpp>

#include <algorithm>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace {

struct ClientWindow {
    std::chrono::steady_clock::time_point started;
    std::size_t requests = 0;
};

HttpResponse too_many_requests(int version, std::chrono::seconds retry_after) {
    HttpStringResponse response{http::status::too_many_requests, version};
    response.set(http::field::content_type, "application/json");
    response.set(http::field::retry_after, std::to_string(std::max<std::int64_t>(1, retry_after.count())));
    response.body() = R"({"error":"Too many requests"})";
    response.prepare_payload();
    return response;
}

} // namespace

RequestStatisticsSnapshot RequestStatistics::snapshot() const noexcept {
    return {
        total_requests_.load(std::memory_order_relaxed),
        active_requests_.load(std::memory_order_relaxed),
        responses_2xx_.load(std::memory_order_relaxed),
        responses_3xx_.load(std::memory_order_relaxed),
        responses_4xx_.load(std::memory_order_relaxed),
        responses_5xx_.load(std::memory_order_relaxed),
        rate_limited_requests_.load(std::memory_order_relaxed)
    };
}

Middleware make_request_statistics_middleware(std::shared_ptr<RequestStatistics> statistics) {
    if (!statistics) throw std::invalid_argument("Request statistics cannot be null");
    return [statistics = std::move(statistics)](
            HttpRequestContext& context, const MiddlewareNext& next) -> HttpResponse {
        statistics->total_requests_.fetch_add(1, std::memory_order_relaxed);
        statistics->active_requests_.fetch_add(1, std::memory_order_relaxed);
        struct ActiveRequestGuard {
            std::atomic<std::uint64_t>& count;
            ~ActiveRequestGuard() { count.fetch_sub(1, std::memory_order_relaxed); }
        } active_guard{statistics->active_requests_};

        auto response = next(context);
        const unsigned status = std::visit(
            [](const auto& message) { return message.result_int(); }, response);
        if (status >= 200 && status < 300) {
            statistics->responses_2xx_.fetch_add(1, std::memory_order_relaxed);
        } else if (status >= 300 && status < 400) {
            statistics->responses_3xx_.fetch_add(1, std::memory_order_relaxed);
        } else if (status >= 400 && status < 500) {
            statistics->responses_4xx_.fetch_add(1, std::memory_order_relaxed);
        } else if (status >= 500 && status < 600) {
            statistics->responses_5xx_.fetch_add(1, std::memory_order_relaxed);
        }
        return response;
    };
}

Middleware make_rate_limit_middleware(std::shared_ptr<RequestStatistics> statistics,
        RateLimitConfig config) {
    if (!statistics) throw std::invalid_argument("Request statistics cannot be null");
    if (config.requests == 0 || config.window.count() <= 0 || config.max_tracked_clients == 0) {
        throw std::invalid_argument("Rate limit values must be positive");
    }

    auto clients = std::make_shared<std::unordered_map<std::string, ClientWindow>>();
    auto clients_mutex = std::make_shared<std::mutex>();
    auto calls = std::make_shared<std::uint64_t>(0);

    return [statistics = std::move(statistics), config, clients = std::move(clients),
            clients_mutex = std::move(clients_mutex), calls = std::move(calls)](
            HttpRequestContext& context, const MiddlewareNext& next) -> HttpResponse {
        const auto now = std::chrono::steady_clock::now();
        std::string client_key;
        if (config.group_by_principal && context.principal) {
            client_key = "principal:" + context.principal->subject;
        } else {
            client_key = "ip:" + context.client_address;
        }
        bool allowed = false;
        {
            std::lock_guard<std::mutex> lock(*clients_mutex);
            auto client = clients->find(client_key);
            if (client != clients->end() && now - client->second.started >= config.window) {
                client->second = ClientWindow{now, 0};
            }

            if (client == clients->end()) {
                if (clients->size() >= config.max_tracked_clients) {
                    if ((++*calls % 256) == 0) {
                        for (auto it = clients->begin(); it != clients->end();) {
                            if (now - it->second.started >= config.window) it = clients->erase(it);
                            else ++it;
                        }
                    }
                    if (clients->size() < config.max_tracked_clients) {
                        client = clients->emplace(client_key,
                            ClientWindow{now, 0}).first;
                    }
                } else {
                    client = clients->emplace(client_key,
                        ClientWindow{now, 0}).first;
                }
            }

            if (client != clients->end() && client->second.requests < config.requests) {
                ++client->second.requests;
                allowed = true;
            }
        }

        if (!allowed) {
            statistics->rate_limited_requests_.fetch_add(1, std::memory_order_relaxed);
            return too_many_requests(context.request.version(), config.window);
        }
        return next(context);
    };
}
