#ifndef TOKORO_METRICS_HPP
#define TOKORO_METRICS_HPP

#include <atomic>
#include <cstdint>
#include <string>
#include <functional>

namespace tokoro {

class Metrics {
public:
    static Metrics& instance();

    Metrics(const Metrics&) = delete;
    Metrics& operator=(const Metrics&) = delete;

    // Counters
    void inc_requests_total();
    void inc_parse_errors_total();
    
    // Accumulators
    void add_request_size_bytes(uint64_t bytes);
    void add_response_size_bytes(uint64_t bytes);
    
    // Gauges
    void inc_active_connections();
    void dec_active_connections();
    
    // Histogram approximations (using atomic counters for buckets)
    void observe_request_duration(double duration_seconds);

    // Callbacks to external components
    void set_queue_depth_callback(std::function<size_t()> cb);

    // Formatter
    std::string to_prometheus_string() const;

private:
    Metrics();
    ~Metrics() = default;

    std::atomic<uint64_t> http_requests_total_{0};
    std::atomic<uint64_t> tokoro_parse_errors_total_{0};
    
    std::atomic<uint64_t> http_request_size_bytes_{0};
    std::atomic<uint64_t> http_response_size_bytes_{0};
    
    std::atomic<int64_t> tokoro_active_connections_{0};
    
    // Histogram for duration (buckets: 0.01, 0.05, 0.1, 0.5, 1.0, +Inf)
    std::atomic<uint64_t> duration_bucket_0_01_{0};
    std::atomic<uint64_t> duration_bucket_0_05_{0};
    std::atomic<uint64_t> duration_bucket_0_1_{0};
    std::atomic<uint64_t> duration_bucket_0_5_{0};
    std::atomic<uint64_t> duration_bucket_1_0_{0};
    std::atomic<uint64_t> duration_bucket_inf_{0};
    
    std::atomic<double> duration_sum_{0.0};
    std::atomic<uint64_t> duration_count_{0};

    std::function<size_t()> queue_depth_cb_;
};

} // namespace tokoro

#endif // TOKORO_METRICS_HPP
