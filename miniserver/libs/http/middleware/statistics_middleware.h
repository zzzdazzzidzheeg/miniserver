#pragma once

#include "middleware.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>

struct RequestStatisticsSnapshot {
    std::uint64_t total_requests = 0;
    std::uint64_t active_requests = 0;
    std::uint64_t responses_2xx = 0;
    std::uint64_t responses_3xx = 0;
    std::uint64_t responses_4xx = 0;
    std::uint64_t responses_5xx = 0;
    std::uint64_t rate_limited_requests = 0;
};

struct RateLimitConfig;

class RequestStatistics {
public:
    RequestStatisticsSnapshot snapshot() const noexcept;

private:
    friend Middleware make_request_statistics_middleware(std::shared_ptr<RequestStatistics>);
    friend Middleware make_rate_limit_middleware(std::shared_ptr<RequestStatistics>,
        RateLimitConfig);
    friend AsyncMiddleware make_async_request_statistics_middleware(
        std::shared_ptr<RequestStatistics>);
    friend AsyncMiddleware make_async_rate_limit_middleware(
        std::shared_ptr<RequestStatistics>, RateLimitConfig);

    std::atomic<std::uint64_t> total_requests_{0};
    std::atomic<std::uint64_t> active_requests_{0};
    std::atomic<std::uint64_t> responses_2xx_{0};
    std::atomic<std::uint64_t> responses_3xx_{0};
    std::atomic<std::uint64_t> responses_4xx_{0};
    std::atomic<std::uint64_t> responses_5xx_{0};
    std::atomic<std::uint64_t> rate_limited_requests_{0};
};

struct RateLimitConfig {
    std::size_t requests = 60;
    std::chrono::seconds window{60};
    std::size_t max_tracked_clients = 10000;
    bool group_by_principal = true;
    bool enabled = true;
};

Middleware make_request_statistics_middleware(std::shared_ptr<RequestStatistics> statistics);

Middleware make_rate_limit_middleware(std::shared_ptr<RequestStatistics> statistics,
    RateLimitConfig config);
AsyncMiddleware make_async_request_statistics_middleware(
    std::shared_ptr<RequestStatistics> statistics);
AsyncMiddleware make_async_rate_limit_middleware(std::shared_ptr<RequestStatistics> statistics,
    RateLimitConfig config);
