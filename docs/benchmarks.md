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
