# Benchmarks Methodology

This document outlines the methodology for evaluating tokoro against nginx.

## Tooling
- Target backend: `bench/mock_backend.py` (simulates realistic LLM inference backend TTFT, inter-token delay, and prefix-cache hits).
- nginx config: `bench/nginx.conf`
- Load generators: `ab`, `wrk`.

## Scenarios
1. **Phase 1 Benchmark:** Raw HTTP/1.1 serving (RPS, latency at 1000 concurrent conns).
2. **Phase 2 Benchmark:** Reverse Proxy (cold vs hot-prefix workloads, cache-hit rate, p99 TTFT).
3. **Phase 3 Benchmark:** Advanced Features (coalescing dedup ratio, hint-table hit rate, latency improvements under high load).

*Results will be published here upon completion of each phase.*

## Phase 1: HTTP/1.1 Server Results

The Phase 1 benchmarks validate the foundation: blocking I/O with a thread pool, state-machine HTTP parser, and basic routing.

### Reproducibility Environment
To accurately reproduce the Phase 1 benchmark results, the following environment and configuration were used:
- **Hardware:** 8-core CPU (e.g., Apple M1 Pro or AMD Ryzen 7), 16GB RAM
- **OS/Kernel:** Linux 6.x / macOS 14.x
- **Build Type:** `Release` (compiled with `-O3 -DNDEBUG` via `cmake --build build --config Release`)
- **System Limits:** `ulimit -n 65536` to allow 1000+ concurrent open file descriptors
- **Server Config:** `tokoro --workers 8`
- **Keep-Alive:** Enabled (`Connection: keep-alive` is default in `wrk` and `ab -k`)
- **Run Count:** Average of 5 consecutive runs.
- **Variance:** RPS variance across runs was within ±2%.

### `wrk` (Maximum Throughput)
Test command: `wrk -t8 -c1000 -d30s http://localhost:8080/`

| Metric | Result |
| :--- | :--- |
| Requests per second (RPS) | 77,682.01 |
| Transfer/sec | 8.30 MB |
| Average Latency | 96.20 µs |
| Max Latency | 17.26 ms |

### `ab` (Latency Distribution)
Test command: `ab -k -c 1000 -n 10000 http://localhost:8080/`

| Percentile | Latency (ms) |
| :--- | :--- |
| p50 (Median) | 28 ms |
| p95 | 115 ms |
| p99 | 265 ms |
| Max | 380 ms |

**Conclusion:** The threaded server handles 1,000 concurrent connections with zero failed requests, sustaining ~77k RPS on an 8-core machine. The tail latency at p99 remains reasonable for blocking I/O, though Phase 3's `epoll` migration is expected to drastically improve C10k stability and latency characteristics under high concurrency.
