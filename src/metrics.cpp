#include "metrics.hpp"
#include <sstream>

namespace tokoro {

Metrics& Metrics::instance() {
    static Metrics inst;
    return inst;
}

Metrics::Metrics() {}

void Metrics::inc_requests_total() {
    http_requests_total_.fetch_add(1, std::memory_order_relaxed);
}

void Metrics::inc_parse_errors_total() {
    tokoro_parse_errors_total_.fetch_add(1, std::memory_order_relaxed);
}

void Metrics::add_request_size_bytes(uint64_t bytes) {
    http_request_size_bytes_.fetch_add(bytes, std::memory_order_relaxed);
}

void Metrics::add_response_size_bytes(uint64_t bytes) {
    http_response_size_bytes_.fetch_add(bytes, std::memory_order_relaxed);
}

void Metrics::inc_active_connections() {
    tokoro_active_connections_.fetch_add(1, std::memory_order_relaxed);
}

void Metrics::dec_active_connections() {
    tokoro_active_connections_.fetch_sub(1, std::memory_order_relaxed);
}

void Metrics::observe_request_duration(double duration_seconds) {
    // Note: atomic<double> fetch_add is C++20, but since this project requires C++20, it should be fine.
    // If not supported by older compilers, we'd need a CAS loop. Assuming C++20.
    // Let's use a CAS loop just to be safe if std::atomic<double> fetch_add isn't fully implemented in the stdlib.
    double expected = duration_sum_.load(std::memory_order_relaxed);
    while (!duration_sum_.compare_exchange_weak(expected, expected + duration_seconds, std::memory_order_relaxed)) {
        // loop
    }
    
    duration_count_.fetch_add(1, std::memory_order_relaxed);

    if (duration_seconds <= 0.01) duration_bucket_0_01_.fetch_add(1, std::memory_order_relaxed);
    if (duration_seconds <= 0.05) duration_bucket_0_05_.fetch_add(1, std::memory_order_relaxed);
    if (duration_seconds <= 0.1) duration_bucket_0_1_.fetch_add(1, std::memory_order_relaxed);
    if (duration_seconds <= 0.5) duration_bucket_0_5_.fetch_add(1, std::memory_order_relaxed);
    if (duration_seconds <= 1.0) duration_bucket_1_0_.fetch_add(1, std::memory_order_relaxed);
    duration_bucket_inf_.fetch_add(1, std::memory_order_relaxed);
}

void Metrics::set_queue_depth_callback(std::function<size_t()> cb) {
    queue_depth_cb_ = std::move(cb);
}

std::string Metrics::to_prometheus_string() const {
    std::ostringstream oss;
    
    oss << "# HELP http_requests_total Total number of HTTP requests processed.\n";
    oss << "# TYPE http_requests_total counter\n";
    oss << "http_requests_total " << http_requests_total_.load(std::memory_order_relaxed) << "\n\n";

    oss << "# HELP tokoro_parse_errors_total Total number of HTTP parse errors.\n";
    oss << "# TYPE tokoro_parse_errors_total counter\n";
    oss << "tokoro_parse_errors_total " << tokoro_parse_errors_total_.load(std::memory_order_relaxed) << "\n\n";

    oss << "# HELP http_request_size_bytes Total size of received requests.\n";
    oss << "# TYPE http_request_size_bytes counter\n";
    oss << "http_request_size_bytes " << http_request_size_bytes_.load(std::memory_order_relaxed) << "\n\n";

    oss << "# HELP http_response_size_bytes Total size of sent responses.\n";
    oss << "# TYPE http_response_size_bytes counter\n";
    oss << "http_response_size_bytes " << http_response_size_bytes_.load(std::memory_order_relaxed) << "\n\n";

    oss << "# HELP tokoro_active_connections Current number of active connections.\n";
    oss << "# TYPE tokoro_active_connections gauge\n";
    oss << "tokoro_active_connections " << tokoro_active_connections_.load(std::memory_order_relaxed) << "\n\n";

    if (queue_depth_cb_) {
        oss << "# HELP tokoro_worker_queue_depth Current depth of the thread pool task queue.\n";
        oss << "# TYPE tokoro_worker_queue_depth gauge\n";
        oss << "tokoro_worker_queue_depth " << queue_depth_cb_() << "\n\n";
    }

    oss << "# HELP http_request_duration_seconds Histogram of request processing durations.\n";
    oss << "# TYPE http_request_duration_seconds histogram\n";
    oss << "http_request_duration_seconds_bucket{le=\"0.01\"} " << duration_bucket_0_01_.load(std::memory_order_relaxed) << "\n";
    oss << "http_request_duration_seconds_bucket{le=\"0.05\"} " << duration_bucket_0_05_.load(std::memory_order_relaxed) << "\n";
    oss << "http_request_duration_seconds_bucket{le=\"0.1\"} " << duration_bucket_0_1_.load(std::memory_order_relaxed) << "\n";
    oss << "http_request_duration_seconds_bucket{le=\"0.5\"} " << duration_bucket_0_5_.load(std::memory_order_relaxed) << "\n";
    oss << "http_request_duration_seconds_bucket{le=\"1.0\"} " << duration_bucket_1_0_.load(std::memory_order_relaxed) << "\n";
    oss << "http_request_duration_seconds_bucket{le=\"+Inf\"} " << duration_bucket_inf_.load(std::memory_order_relaxed) << "\n";
    oss << "http_request_duration_seconds_sum " << duration_sum_.load(std::memory_order_relaxed) << "\n";
    oss << "http_request_duration_seconds_count " << duration_count_.load(std::memory_order_relaxed) << "\n";

    return oss.str();
}

} // namespace tokoro
