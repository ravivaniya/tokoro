# Project: tokoro — An Inference-Aware HTTP/1.1 Server in C++

## 🎯 Mission

Build **tokoro**, a from-scratch, multi-threaded HTTP/1.1 server in modern C++ (C++17/20) using **only raw POSIX sockets** — no HTTP libraries, no Boost.Asio, no framework. Then extend it into an **inference-aware reverse proxy** that does things nginx structurally cannot: prompt-prefix-hash routing for KV-cache affinity, token-budget load balancing, SSE streaming with cancellation, request coalescing, per-tenant token-rate limiting, and a non-blocking `epoll`-based I/O core.

The project is delivered in **three phases**:

1. **Phase 1 — HTTP/1.1 Server (Weeks 1–6):** the foundation; ship publicly.
2. **Phase 2 — Inference-Aware Reverse Proxy, Core (Weeks 7–8):** the differentiating thesis; ship a benchmark vs nginx.
3. **Phase 3 — AI-Infra-Legible Advanced Features (Weeks 9–12):** the features that make tokoro genuinely *unlike* nginx and legible to AI infrastructure engineers.

The end result is a project **immediately legible to AI infrastructure engineers** that demonstrates real systems-level C++ skill — and produces a resume line stronger than "I built an HTTP server."

---

## 🧠 Why This Project

Every inference server (vLLM, TGI, Triton, llama.cpp server) is a networked C++/C process. This project teaches what sits **below** frameworks like NestJS, Express, or FastAPI — the actual byte-level protocol, the socket lifecycle, and the concurrency primitives. The Phase 2 and Phase 3 work then teaches the **inference-serving-specific** problems that general-purpose proxies (nginx, HAProxy, Envoy in its default config) don't solve well.

---

## 📚 Learning Goals (Non-Negotiable)

I want to master, by completing this project:

1. **Modern C++ (C++17/20)** — memory ownership, RAII, smart pointers (`unique_ptr`, `shared_ptr`), move semantics, rule of five, `std::optional`, `std::variant`, `std::string_view`.
   - *Resource:* "A Tour of C++" by Bjarne Stroustrup — 2 chapters/week.
2. **CMake build system** — how to structure, build, link, and test a real C++ project.
3. **Multithreading** — `std::thread`, `std::mutex`, `std::condition_variable`, `std::atomic`, thread pools, lock-free counters on hot paths.
   - *Resource:* "C++ Concurrency in Action" by Anthony Williams — ch 2–4.
4. **BSD sockets & HTTP from scratch** — `socket()`, `bind()`, `listen()`, `accept()`, `recv()`, `send()`, `setsockopt()`, raw byte-level protocol parsing.
   - *Resource:* Beej's Guide to Network Programming + RFC 7230.
5. **GitHub shipping discipline** — clean repo, CI with GitHub Actions, issues/milestones/labels/project board, design docs, README with architecture diagram and benchmarks, public release.

Phase 2 adds:

6. **Streaming I/O** — bidirectional SSE / chunked transfer.
7. **Connection pooling** to upstream backends with keep-alive on both sides.
8. **Backpressure & cancellation** — propagating client disconnects to upstream work.

Phase 3 adds:

9. **Non-blocking I/O with `epoll`** (Linux) — edge-triggered event loop, per-connection state machines.
10. **Lock-free / wait-free data structures** for the hot path (atomic counters, RCU-style read paths).
11. **Distributed-systems thinking in a single process** — subscriber sets, fan-out streams, coalescing, fair-queueing.
12. **Observability** — Prometheus-format metrics, structured logging, request tracing.

---

## 🏗 Engineering Practices (Enforce From Day 1)

- Compile with `-Wall -Wextra -Wpedantic` and `-std=c++20`.
- Debug builds run with `-fsanitize=address,undefined -g`.
- `clang-format` + `clang-tidy` enforced in CI.
- **Design-doc-first** for any non-trivial component (parser, thread pool, router, coalescing, epoll loop).
- **TDD the HTTP parser** — it's the hardest component.
- `signal(SIGPIPE, SIG_IGN)` or use `MSG_NOSIGNAL` on `send()`.
- **No premature optimization:** correct → measured → optimized, in that order.
- Every PR runs CI; `main` is always green.
- Every checklist item is a GitHub Issue with a label and a milestone.

---

## 📐 Repository Layout

```
tokoro/
├── CMakeLists.txt
├── README.md
├── LICENSE
├── .clang-format
├── .clang-tidy
├── .github/workflows/ci.yml
├── docs/
│   ├── design.md          # threading, parser, routing, coalescing, epoll
│   ├── roadmap.md         # living roadmap
│   └── benchmarks.md      # methodology + results vs nginx
├── src/
│   ├── main.cpp
│   ├── socket.hpp / .cpp        # RAII fd wrapper
│   ├── server.hpp / .cpp        # accept loop
│   ├── http_parser.hpp / .cpp   # state-machine parser
│   ├── thread_pool.hpp / .cpp   # worker pool
│   ├── file_handler.hpp / .cpp  # static file serving
│   ├── upstream.hpp / .cpp      # upstream conn pool (Phase 2)
│   ├── router.hpp / .cpp        # prefix-hash + token-budget LB (Phase 2)
│   ├── sse_stream.hpp / .cpp    # SSE passthrough + cancellation (Phase 2)
│   ├── coalescer.hpp / .cpp     # request coalescing (Phase 3)
│   ├── rate_limiter.hpp / .cpp  # per-tenant token-rate limiter (Phase 3)
│   ├── event_loop.hpp / .cpp    # epoll-based reactor (Phase 3)
│   └── metrics.hpp / .cpp       # Prometheus metrics (Phase 3)
├── tests/
├── bench/
│   ├── mock_backend.py    # realistic LLM mock; lands in Week 6
│   ├── nginx.conf         # nginx config used for the comparison
│   └── scenarios/         # cold, hot-prefix, mixed workloads
└── www/                   # static files
```

---

## ✅ Phase 1 Milestones — HTTP/1.1 Server (Weeks 1–6)

- [ ] Handles **1000 concurrent connections** (`wrk -t8 -c1000 -d30s`).
- [ ] HTTP parser handles **chunked transfer-encoding**.
- [ ] **Keep-alive** connection reuse works (verified with `curl -v`).
- [ ] Serves static files from `./www` with correct MIME types and path-traversal protection.
- [ ] Returns correct status codes: 200, 400, 404, 405, 500.
- [ ] No memory leaks (clean under `valgrind` and `-fsanitize=address,undefined`).
- [ ] **CI green** on every PR via GitHub Actions (configure → build → test → smoke test).
- [ ] **README** with architecture diagram (Mermaid/ASCII) and benchmark table (RPS, p50/p99 latency).
- [ ] **`docs/design.md` §1–4 written before code** (threading model, parser state machine, keep-alive/timeouts).
- [ ] **`bench/mock_backend.py` implemented** (moved from Phase 2 into Phase 1 — see *Benchmarking Methodology*).
- [ ] Public on GitHub with a license, badges, and clear build instructions.

### Phase 1 Architecture

```
Client ──► Accept Loop (main thread)
              │
              ▼
        Task Queue (mutex + condvar)
              │
     ┌────────┼────────┐
     ▼        ▼        ▼
  Worker 1  Worker 2  Worker N
     │        │        │
     ▼        ▼        ▼
  HTTP Parser → File Handler → Response Writer
```

---

## 🚀 Phase 2 Milestones — Inference-Aware Reverse Proxy, Core (Weeks 7–8)

The thesis: nginx routes by URL/IP — it has no idea what a request *costs* or what *state* lives on which backend. tokoro will.

- [ ] **Reverse proxy mode**: `tokoro --upstream http://b1:9001,http://b2:9001` forwards `POST /v1/completions` to a pool of backends.
- [ ] **Upstream connection pool** with keep-alive on both sides.
- [ ] **SSE streaming passthrough** for `text/event-stream` with zero buffering; client `close()` propagates as upstream cancellation.
- [ ] **Prefix-hash router** — hash the first 512 bytes of the JSON `prompt`/`messages` field (xxhash or FNV), mod N → pick backend. Identical-prefix requests land on the same worker → KV-cache reuse.
- [ ] **Token-aware least-loaded LB** — track `in_flight_tokens` per backend with `std::atomic`; route to the backend with the lowest pending token count (estimate `len(prompt)/4 + max_tokens`).
- [ ] Hybrid routing policy: **prefix-hash first, fall back to least-loaded on hot-spot detection.**
- [ ] **Benchmark vs nginx** in `docs/benchmarks.md` — same mock backends, same workload, graph **cache-hit rate** and **p99 TTFT**.
- [ ] **`docs/design.md` §5–7 written before code** (routing, token estimation, SSE/cancellation semantics).

---

## 🧠 Phase 3 Milestones — AI-Infra-Legible Advanced Features (Weeks 9–12)

This is where tokoro becomes genuinely *unlike* nginx. None of these features should be cut — they are the entire differentiating story. Each is gated by a design-doc section landing first.

- [ ] **`epoll`-based non-blocking event loop** — edge-triggered, one reactor thread per core, per-connection state machine. Replaces the blocking-I/O + thread-pool hot path. (Design doc: §8.)
- [ ] **Request coalescing** — identical in-flight prompts within a small window share one upstream response, fanned out to multiple clients. Includes:
  - explicit subscriber state machine (`Subscribing` / `Streaming` / `Draining` / `Cancelled` / `Failed`),
  - late-joiner policy (refuse vs replay-from-ring-buffer — pick one and document it),
  - "last subscriber leaves → cancel upstream" semantics,
  - upstream failure → consistent error to all subscribers. (Design doc: §9.)
- [ ] **Per-tenant token-rate limiting** — token-bucket keyed by API key / tenant header, denominated in **tokens/sec** (not requests/sec). (Design doc: §10.)
- [ ] **Prefix-cache hint table** — a small router-side LRU mapping prompt-prefix hash → last-served backend, so the router can prefer cache-warm backends even when prefix-hash mod-N would route elsewhere. (Design doc: §11.)
- [ ] **Mid-stream upstream failover** — on upstream 5xx mid-SSE, attempt one transparent retry to a different backend with prompt + already-emitted tokens forwarded as the new prefix. (Design doc: §12. Honest about correctness limits — document them.)
- [ ] **Observability**:
  - `/metrics` endpoint in Prometheus text format,
  - per-backend gauges: `in_flight_tokens`, `in_flight_streams`, `cache_hit_ratio`,
  - per-request histograms: `ttft_seconds`, `inter_token_seconds`, `total_tokens`.
- [ ] **Graceful shutdown** — drain in-flight streams up to a deadline before exiting; never kill mid-stream connections on SIGTERM.
- [ ] **Updated benchmark vs nginx** in `docs/benchmarks.md` showing the cumulative win from Phase 3 features (prefix-cache-hint hit rate, coalescing dedup ratio, p99 TTFT under load).

### Capability Comparison (for README)

| Capability                                                | nginx | tokoro (P2) | tokoro (P3) |
|-----------------------------------------------------------|:-----:|:-----------:|:-----------:|
| Round-robin / least-conn LB                               |   ✅   |      ✅      |      ✅      |
| Route by **request body content** (system prompt prefix)  |   ❌   |      ✅      |      ✅      |
| **Prefix-hash affinity** for KV-cache reuse               |   ❌   |      ✅      |      ✅      |
| **Prefix-cache hint table** across backends               |   ❌   |      ❌      |      ✅      |
| LB by **estimated tokens**, not request count             |   ❌   |      ✅      |      ✅      |
| **Cancel upstream** on SSE client disconnect              |   ⚠️   |      ✅      |      ✅      |
| **Request coalescing** for identical in-flight prompts    |   ❌   |      ❌      |      ✅      |
| **Mid-stream upstream failover**                          |   ❌   |      ❌      |      ✅      |
| Per-tenant **token-rate** limiting (not req/s)            |   ❌   |      ❌      |      ✅      |
| `epoll`-based non-blocking core                           |   ✅   |      ❌      |      ✅      |
| Prometheus metrics tailored to LLM serving                |   ⚠️   |      ❌      |      ✅      |

### README Pitch

> **tokoro** is a from-scratch C++20 HTTP/1.1 server *and* an experimental inference-aware reverse proxy. Unlike general-purpose proxies (nginx, HAProxy), tokoro routes requests using application-layer signals — prompt-prefix hashing for KV-cache affinity, token-budget accounting for load balancing, request coalescing, mid-stream failover, and per-tenant token-rate limiting — yielding higher cache-hit rates and lower tail latency for LLM inference workloads.

---

## 🧪 Benchmarking Methodology (binding)

`bench/mock_backend.py` is **built in Week 6** and is the measurement instrument used for every benchmark from that point onward. The mock must simulate:

| Behavior | Target value |
|---|---|
| Time-to-first-token (cold)            | 200–500 ms |
| Time-to-first-token (prefix-cache hit) | 20–50 ms ← **the entire point of prefix-hash routing** |
| Inter-token delay                     | 20–40 ms (~30 tok/s) |
| Per-backend prefix-cache state        | LRU keyed by first N bytes of prompt |
| Failure injection                     | optional 5xx and mid-stream disconnects |

Benchmarks must report: **RPS, p50/p99 TTFT, p99 inter-token, cache-hit rate, coalescing dedup ratio (P3)**. nginx is configured fairly — round-robin or `ip_hash` — and that config lives at `bench/nginx.conf` in-repo so the comparison is reproducible.

---

## 🗓 Week-by-Week Plan

### Week 1 — C++ & CMake foundations
- *Tour of C++* ch 1–3.
- Project compiles via CMake; "Hello World" runs.
- Write the `Socket` RAII wrapper (closes fd in destructor, non-copyable, movable).
- **`docs/design.md` §1–2 written.**

### Week 2 — Raw sockets & TCP accept loop
- Master `socket`/`bind`/`listen`/`accept`/`recv`/`send`/`close`/`SO_REUSEADDR`.
- Single-threaded echo server, tested with `nc` and `curl -v`.
- **File all Phase 1 GitHub issues + milestone + labels + project board.**

### Week 3 — HTTP/1.1 parser
- Read RFC 7230 §3 and §4.
- **`docs/design.md` §3 (parser state machine) written first.**
- State-machine parser: request line → headers → body, TDD.
- Handle `Content-Length`; stub `Transfer-Encoding: chunked`.

### Week 4 — Thread pool
- `std::thread` + `std::mutex` + `std::condition_variable` + `std::queue` + `std::atomic<bool>` shutdown.
- Pool size ≈ `std::thread::hardware_concurrency()`.
- Replace one-thread-per-connection with the pool.

### Week 5 — Keep-alive, chunked, static files
- Loop on the same socket until `Connection: close` or `SO_RCVTIMEO` fires.
- Full chunked-encoding parser.
- Static file serving from `./www`, MIME types, `..` rejection.

### Week 6 — Benchmarks, CI, `mock_backend.py`, **ship Phase 1**
- Implement `bench/mock_backend.py` with realistic TTFT, inter-token delay, and prefix-cache speedup.
- Benchmark Phase 1 with `ab` and `wrk` at concurrency 1000.
- GitHub Actions CI: configure → build → test → smoke test.
- README + `docs/benchmarks.md` first cut.
- **Public release of Phase 1.**

### Week 7 — Reverse proxy core
- **`docs/design.md` §5–6 written first** (routing + SSE/cancellation).
- Upstream connection pool (keep-alive both sides).
- `POST /v1/completions` forwarding.
- SSE streaming passthrough with cancellation on client disconnect.
- **File all Phase 2 GitHub issues + milestone.**

### Week 8 — Inference-aware routing + nginx benchmark
- **`docs/design.md` §7 written first** (token estimation + hybrid routing).
- Prefix-hash router.
- Token-budget least-loaded LB with `std::atomic` counters.
- Hybrid policy: prefix-hash → fall back to least-loaded.
- **Benchmark vs nginx published in `docs/benchmarks.md`.**
- **Public release of Phase 2.**

### Week 9 — `epoll` migration
- **`docs/design.md` §8 written first.**
- Edge-triggered `epoll` reactor; per-connection state machine.
- Migrate accept loop, parser feed, and SSE passthrough off blocking I/O.
- Re-run Phase 2 benchmarks; expect lower thread count for the same RPS.
- **File all Phase 3 GitHub issues + milestone.**

### Week 10 — Coalescing + prefix-cache hint table
- **`docs/design.md` §9 + §11 written first.**
- Implement `coalescer` with explicit state machine and documented late-joiner policy.
- Implement router-side prefix-cache hint LRU.
- Tests: client A disconnects mid-stream while B is attached; late joiner; upstream failure fan-out.

### Week 11 — Token-rate limiting + mid-stream failover + observability
- **`docs/design.md` §10 + §12 written first.**
- Token-bucket rate limiter keyed by tenant.
- Mid-stream failover with one transparent retry; document correctness limits.
- `/metrics` Prometheus endpoint; histograms for TTFT and inter-token.
- Graceful shutdown.

### Week 12 — Final benchmark, polish, ship Phase 3
- Updated `docs/benchmarks.md` showing Phase 3 wins: coalescing dedup ratio, hint-table hit rate, p99 TTFT under load.
- README refresh: capability comparison table, architecture diagrams for all three phases.
- `docs/roadmap.md` updated with stretch goals (HTTP/2, TLS, etc.).
- **Public release of Phase 3.**

---

## 📄 `docs/design.md` Skeleton (Lives in Repo)

```
1. Goals & Non-Goals
2. Threading Model (Phase 1)
3. HTTP Parser State Machine (Phase 1)
4. Keep-Alive & Timeouts (Phase 1)
5. Phase 2: Routing — prefix-hash, token estimation, hybrid policy
6. Phase 2: SSE Streaming & Cancellation
7. Phase 2: Upstream Connection Pool
8. Phase 3: epoll Event Loop
9. Phase 3: Coalescing — subscriber state machine, late-joiner policy, failure fan-out
10. Phase 3: Per-Tenant Token-Rate Limiting
11. Phase 3: Prefix-Cache Hint Table
12. Phase 3: Mid-Stream Upstream Failover (with correctness limits)
13. Benchmarking Methodology
14. Open Questions
```

## 📄 `docs/roadmap.md` Skeleton (Lives in Repo)

```
Now    — Phase 1: HTTP/1.1 Server          (link to milestone)
Next   — Phase 2: Reverse Proxy Core       (link to milestone)
After  — Phase 3: AI-Infra-Legible Features (link to milestone)
Later  — HTTP/2 / h2c, TLS termination, HTTP/3 (explicitly stretch)
Out of scope — production-grade nginx replacement, cert management
```

---

## 🗂 GitHub Project Management (Set Up in Week 2)

- **Milestones:** `Phase 1: HTTP/1.1 Server`, `Phase 2: Reverse Proxy Core`, `Phase 3: AI-Infra-Legible Features`.
- **Labels:** `phase-1`, `phase-2`, `phase-3`, `area:parser`, `area:threading`, `area:proxy`, `area:routing`, `area:streaming`, `area:epoll`, `area:coalescing`, `area:rate-limit`, `area:observability`, `area:ci`, `area:docs`, `design-needed`, `good-first-step`.
- **Project board:** columns `Backlog / Designing / In Progress / In Review / Done`.
- **Rule:** any issue tagged `design-needed` cannot move past `Designing` until the relevant `docs/design.md` section is merged on `main`.

---

## 🎓 Definition of Mastery

When tokoro is done, I should be able to confidently explain, in an interview:

- What `accept()` actually returns and the full fd lifecycle.
- Why RAII matters for socket/fd ownership and how move semantics enable it.
- How a thread pool differs from one-thread-per-connection, and when each wins.
- The exact byte sequence of a chunked-encoded HTTP/1.1 response.
- Why `epoll` beats blocking I/O at high concurrency (the C10k problem), and how edge-triggered mode forces per-connection state machines.
- Why nginx is a bad fit for LLM inference routing — and what tokoro does differently (prefix-hash, token-budget, coalescing, hint table, mid-stream failover).
- How `std::atomic` enables lock-free token accounting on the hot path.
- How to propagate a TCP-level cancellation from client → proxy → upstream.
- How to design a subscriber state machine that handles late joiners, partial disconnects, and upstream failure consistently.
- How to design a benchmark that is honest (and what makes a benchmark dishonest).

---

## 📦 Final Deliverable

A public GitHub repository named `tokoro` containing:

1. C++20 source for **all three phases** — HTTP server, inference-aware proxy core, and AI-infra-legible advanced features.
2. A GitHub Actions workflow that builds and tests on every push.
3. `docs/design.md`, `docs/roadmap.md`, and `docs/benchmarks.md` — kept current.
4. `bench/mock_backend.py`, `bench/nginx.conf`, and reproducible benchmark scenarios.
5. A README with: pitch, three-phase architecture, capability comparison vs nginx, benchmark headlines, build instructions, design rationale, license, and CI badge.
6. Public issues, milestones, labels, and a project board that tell the story of how the project was built.