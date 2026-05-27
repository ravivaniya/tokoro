# Tokoro Design Document

## 1. Goals & Non-Goals

### Goals
- Implement a from-scratch, multi-threaded HTTP/1.1 server using POSIX sockets in C++20.
- Evolve it into an inference-aware reverse proxy optimized for LLM workloads.
- Provide custom application-layer routing (prompt-prefix hashing).
- Implement specialized features: token-budget LB, SSE passthrough + cancellation, request coalescing, and per-tenant rate limiting.
- Outperform or match general proxies (like nginx) in LLM-specific workloads (cache-hit rate, TTFT).
- Maximize learning of C++ memory models, threading, and raw sockets.

### Non-Goals
- Building a full production-grade replacement for nginx/Envoy.
- Automatic certificate management (Let's Encrypt).
- Implementing HTTP/2 or HTTP/3 (initially).
- Abstracting away the OS with libraries like Boost.Asio.

## 2. Threading Model (Phase 1)
In Phase 1, the server employs a standard blocking accept-loop with a thread pool worker model.

- **Accept Loop (Main Thread):** The primary thread calls `accept()` in a blocking manner. Upon a successful connection, it receives a new file descriptor (fd), wraps it (or manages its lifecycle), and enqueues it into a shared task queue.
- **Task Queue:** A `std::queue` protected by a `std::mutex` and signaled by a `std::condition_variable`.
- **Worker Threads:** A fixed pool of workers (`std::thread`) equal to the hardware concurrency count. They wait on the condition variable. When a task (fd) is available, a worker dequeues it, parses the HTTP request, and dispatches the response.

*Note: In Phase 3, this model will be replaced by an `epoll`-based event loop (reactor pattern) for non-blocking I/O.*

## 3. HTTP Parser State Machine (Phase 1)
*(To be written)*

## 4. Keep-Alive & Timeouts (Phase 1)
*(To be written)*

## 5. Phase 2: Routing — prefix-hash, token estimation, hybrid policy
*(To be written)*

## 6. Phase 2: SSE Streaming & Cancellation
*(To be written)*

## 7. Phase 2: Upstream Connection Pool
*(To be written)*

## 8. Phase 3: epoll Event Loop
*(To be written)*

## 9. Phase 3: Coalescing — subscriber state machine, late-joiner policy, failure fan-out
*(To be written)*

## 10. Phase 3: Per-Tenant Token-Rate Limiting
*(To be written)*

## 11. Phase 3: Prefix-Cache Hint Table
*(To be written)*

## 12. Phase 3: Mid-Stream Upstream Failover
*(To be written)*

## 13. Benchmarking Methodology
See `benchmarks.md`.

## 14. Open Questions
- Late-joiner policy in Phase 3 coalescing (refuse vs replay).
