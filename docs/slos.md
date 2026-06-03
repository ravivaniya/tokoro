# Tokoro Phase 1 Service Level Objectives (SLOs)

This document defines the latency and availability targets for Tokoro during Phase 1. These targets apply when running in a production-like environment with appropriate resources.

## Service Level Indicators (SLIs)

- **Availability:** The percentage of valid HTTP requests that return a successful status code (`2xx`, `3xx`, or valid `4xx` client errors). Unexpected `5xx` errors or dropped connections are considered failures.
- **Latency (p99):** The maximum duration taken to serve 99% of requests, measured from the time the TCP connection is fully established until the last byte of the response is dispatched to the socket.

## Service Level Objectives (SLOs)

### 1. Availability Objective
**Target:** 99.9% (Three Nines) over a rolling 30-day window.

**Conditions:**
- Excludes planned downtime or server restarts.
- Assumes the upstream inference backend (if proxying) is 100% available.
- Evaluated via load testing (e.g., `wrk` with 10k connections).

### 2. Latency Objective
**Target:** p99 latency ≤ 10ms for static file serving.

**Conditions:**
- Evaluated under moderate load (≤ 50% of maximum RPS capacity).
- Files are cached in memory (OS page cache) and fit entirely in RAM.
- Applies to small-to-medium files (< 1MB).
- Does not account for network latency between the client and the server.

### 3. Concurrency Objective
**Target:** Gracefully handle 1,000 concurrent active connections.

**Conditions:**
- No dropped connections or `std::terminate` under peak load.
- If the thread pool is saturated, Tokoro must correctly buffer requests or reject them cleanly with a `503 Service Unavailable`, rather than crashing.
