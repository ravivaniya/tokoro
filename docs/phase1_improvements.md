# Phase 1 — Improvements & Production-Grade Plan

This document consolidates a code review of tokoro Phase 1 and a roadmap for taking it from a working learning project to a production-grade HTTP/1.1 server. It is organized in two parts:

- **Part A — Phase 1 Improvements** (correctness, safety, quality, build/CI hygiene gaps in the *current* code).
- **Part B — Production-Grade Hardening** (everything else needed before tokoro could sit on a real network path).

File references use workspace-relative paths.

---

## Part A — Phase 1 Improvements

### A1. Critical Correctness Bugs

1. **Parser does not report bytes consumed → pipelined requests are lost.**
   `Server::handle_client` admits this in a comment. A second request in the same `recv()` buffer is dropped.
   - Files: `src/http_parser.hpp`, `src/http_parser.cpp`, `src/server.cpp`.
   - Fix: change `HttpParser::parse` to return `{ParseResult, size_t bytes_consumed}` (or take an out-param), then re-enter parse on the remaining slice until `Pending`.

2. **Header lookups are case-sensitive — violates RFC 7230 §3.2.**
   `req.headers.find("Connection")` / `"Content-Length"` / `"Transfer-Encoding"` will miss `connection:` etc.
   - Fix: normalize header field names to lowercase on insert in the parser; look them up in lowercase everywhere.

3. **`std::stoull` on untrusted `Content-Length` throws.**
   `Content-Length: abc` throws `std::invalid_argument` from inside the worker → uncaught → `std::terminate`.
   - Fix: parse digits manually with overflow check, or wrap in `try/catch` and transition to `Error` (return 400).

4. **No upper bound on request size — trivial DoS.**
   Header name/value accumulators grow without limit; `Content-Length` is trusted; chunked bodies are unbounded.
   - Fix: introduce `max_request_line_bytes`, `max_header_bytes`, `max_headers`, `max_body_bytes`; transition to `Error` (413/400) on overflow.

5. **`SIGPIPE` will kill the process on client disconnect mid-`send()`.**
   - Fix: `signal(SIGPIPE, SIG_IGN)` once in `main`, or pass `MSG_NOSIGNAL` to every `send()`.

6. **`send()` is not looped — partial sends drop response bytes.**
   - Fix: wrap in `send_all()` that loops until all bytes are sent or an error occurs.

7. **Path traversal check is too weak.**
   `src/file_handler.cpp` rejects only literal `..`. Misses `%2e%2e`, absolute paths, NUL bytes, Windows separators, symlinks.
   - Fix: URL-decode first, reject NUL/control chars, resolve with `fs::weakly_canonical`, require the canonical path starts with the canonical docroot.

8. **Worker swallows exceptions → silent thread death.**
   `task();` in `worker_loop` is not wrapped. One throwing task kills the thread (and eventually the process).
   - Fix: `try { task(); } catch (const std::exception& e) { log error; } catch (...) { log unknown; }`.

9. **`ThreadPool::enqueue` ignores `stop_`.**
   It happily pushes tasks during destruction.
   - Fix: under the lock, if `stop_` is true, reject (throw `std::runtime_error` or no-op + log).

10. **Tab in header value is rejected.**
    `HeaderValue` state currently treats `\t` as a control char. RFC 7230 allows HTAB in OWS.
    - Fix: explicitly allow `c == '\t'` in `HeaderValue`.

11. **HTTP version string is not validated.**
    Anything between the second space and CRLF is accepted.
    - Fix: validate format `HTTP/\d\.\d`, reject otherwise (400 or 505).

12. **Duplicate sources in test target in `CMakeLists.txt`.**
    Source lists drift; refactor to a shared `tokoro_core` static library consumed by both `tokoro` and `tokoro_tests`.

### A2. Resource & Lifecycle

1. **`Server::run()` has no shutdown path** — `while (true)` with no signal handling.
   - Fix: `std::atomic<bool> running_` toggled by SIGINT/SIGTERM; break out of accept loop (self-pipe / `EINTR`).

2. **`shared_ptr<Socket>` per connection is overkill.**
   - Use a moved-into-lambda `Socket` value or `unique_ptr<Socket>`.

3. **Single 5s `SO_RCVTIMEO` conflates header read, body read, and idle keep-alive.**
   - Separate: `header_timeout` (e.g. 5s), `body_timeout` (e.g. 30s), `keepalive_idle_timeout` (e.g. 60s).

4. **Static files are loaded entirely into RAM.**
   - Cap with `max_file_size`; stream large files with `sendfile()` on Linux.

5. **Missing required response headers.**
   - Add `Date` (RFC 7231 §7.1.1.2), `Server`, and echo `Connection` on close.

6. **405 response missing `Allow` header** (RFC 7231 §6.5.5).

### A3. Logging & Observability

1. **`std::cout` / `std::cerr` from many threads is unsynchronized** — log lines interleave.
   - Add a tiny thread-safe logger with levels, or pull in `spdlog`.

2. **No per-request access log line** — add `ts method uri status bytes duration_ms`.

3. **No metric counters** — add `std::atomic<uint64_t>` counters for `requests_total`, `bytes_sent_total`, `parse_errors_total`, `active_connections`. Wires up Phase 3 `/metrics` later.

### A4. Test Coverage Gaps

Add Catch2 tests for:

1. Header name case-insensitivity (after fix).
2. Pipelined requests in one buffer.
3. Oversized header line / oversized body → `Error`.
4. `Content-Length: abc` → `Error` (no throw).
5. `Content-Length: 0` POST → `Complete` immediately.
6. Chunked body split across many `parse()` calls.
7. Tab in header value (RFC OWS).
8. Invalid HTTP version string.
9. `FileHandler`: 405 for POST, 404 for missing, traversal rejection (`/../../etc/passwd`, `/%2e%2e/`), MIME types.
10. End-to-end test: bind `Server` on port 0, perform a real TCP exchange.
11. Thread pool stress (thousands of tasks) and shutdown-drains-all-tasks test.

Also tighten `tests/test_socket.cpp` move-assignment test: assert the previous fd is actually closed (e.g. `fcntl(old_fd, F_GETFD)` returns -1).

### A5. Build / CI Discipline (Phase 1 milestone items still open)

1. **No `.github/workflows/ci.yml`** — Phase 1 milestone requires CI on every PR.
2. **No `.clang-format` / `.clang-tidy`** — listed as Day-1 practice.
3. **No `LICENSE`** — Phase 1 deliverable.
4. **`CMakeLists.txt` applies warnings + sanitizers globally** (including to Catch2). Scope per-target with `target_compile_options(tokoro PRIVATE ...)`.
5. **No Release build guidance** — published benchmark numbers must state build type.
6. **Refactor into `tokoro_core` library** to remove duplicated source lists.
7. **`-Werror` in CI** (not local).
8. **Pin `Catch2` with `GIT_SHALLOW TRUE`** to speed clean builds.

### A6. Code Quality / Modern C++ Polish

1. Bound `current_chunk_size_` against `max_body_bytes`; check for overflow in the `*16` accumulation.
2. `HttpRequest::headers` → `unordered_map` with transparent hasher so `string_view` lookup is allocation-free.
3. `HttpResponse::serialize()` builds via repeated `+=`; pre-`reserve()` and serialize once.
4. Helper `make_error_response(int code, std::string_view msg)` replaces the `vector<uint8_t>{'4','0','4', ...}` literals in `file_handler.cpp`.
5. Add `Socket::shutdown()`, `Socket::recv()`, `Socket::send_all()` so `server.cpp` doesn’t mix raw `::recv`/`::send` with the RAII type.
6. `Socket::accept()` should use `accept4(..., SOCK_CLOEXEC)` on Linux.
7. Wrap `HttpParser`, `HttpRequest`, `FileHandler` in `namespace tokoro` for consistency.
8. Unify header guard style (`#pragma once` everywhere).
9. Add `[[nodiscard]]` to `HttpParser::parse` and other error-returning functions.

### A7. Documentation Debt

1. `docs/design.md` §4 “Keep-Alive & Timeouts” is `(To be written)` even though the code partially implements it — write before extending.
2. `docs/benchmarks.md` does not state hardware, kernel, build type (Release?), `ulimit`, keep-alive on/off, run count, variance. Without these the 77k RPS number is not reproducible.
3. `README.md` build instructions don’t mention `-DCMAKE_BUILD_TYPE=Release`, don’t mention `ctest`, and don’t mention that `./www` is resolved relative to CWD (bug — see §B6).

### A8. Recommended Order of Attack

1. Correctness on the happy path: A1.1, A1.2, A1.3, A1.5, A1.6, A1.8.
2. Safety caps: A1.4, A1.7.
3. Make the binary runnable from anywhere: CLI flags + signal-driven shutdown.
4. CMake refactor (`tokoro_core` library, per-target warnings/sanitizers).
5. CI + `clang-format` + `clang-tidy` + `LICENSE`.
6. Write `docs/design.md` §4 and tighten `docs/benchmarks.md` reproducibility.
7. Backfill the missing tests in A4.

---

## Part B — Production-Grade Hardening

Everything above gets Phase 1 to “correct and reasonably safe.” The following is what separates that from a server you would put behind a real load balancer.

### B1. Protocol Correctness (RFC 7230/7231/7232)

1. Case-insensitive header field names (RFC 7230 §3.2).
2. Multi-value header tokenization (`Connection: keep-alive, Upgrade`, `Transfer-Encoding: gzip, chunked`). `find("chunked") != npos` is not enough.
3. **Request smuggling defenses** — reject any request that has both `Content-Length` and `Transfer-Encoding`; reject duplicate `Content-Length` headers; reject bare `\n` line terminators.
4. Strict request-line validation: method is a token, URI length cap, version matches `HTTP/\d\.\d` exactly.
5. `Expect: 100-continue` support (or explicit 417).
6. `HEAD` method — behave like GET but omit body. Currently returns 405.
7. `OPTIONS *` — at minimum a 200 with `Allow`.
8. Conditional GET: `If-Modified-Since` / `If-None-Match` → 304.
9. `Range` requests / 206 Partial Content.
10. Correct `Date:` header on every response (IMF-fixdate).
11. `Server:` header with version string.
12. `Allow:` header on 405.
13. Echo `Connection: close` in the final response of any closing connection.
14. HTTP version negotiation — respond with highest version ≤ client’s; reject `HTTP/0.9` and unknown versions with 505.
15. Reject obsolete line folding in request headers (RFC 7230 §3.2.4 MUST).
16. Validate trailers in chunked encoding.

### B2. Security Hardening

1. Request size limits enforced at parse time (see A1.4).
2. Safe integer parsing for `Content-Length` and chunk sizes (see A1.3).
3. **Slowloris / slow-body defenses** — distinct deadlines for header read, body read, idle keep-alive.
4. Max concurrent connections per process and per source IP (cheap token bucket).
5. **Bounded work queue** in `ThreadPool` — under overload, reject with 503 instead of growing RAM.
6. Path traversal hardening (see A1.7).
7. `SIGPIPE` ignored (see A1.5).
8. Drop privileges if started as root (bind low port → `setuid`/`setgid`).
9. `SO_KEEPALIVE`, `TCP_NODELAY`, `TCP_USER_TIMEOUT` configured per role.
10. **fd-exhaustion handling**: `accept4(..., SOCK_CLOEXEC)`; reserve an emergency fd to gracefully close-and-reject on `EMFILE/ENFILE` instead of busy-looping.
11. Reject bare `\n` / stray CR (request smuggling vector).
12. CRLF-injection-safe header serialization (validate any user-controlled value placed in a response header).
13. No TLS in Phase 1 — document that tokoro must run behind a TLS terminator, and bind to localhost / unix socket by default.

### B3. Reliability & Lifecycle

1. **Graceful shutdown** on SIGINT/SIGTERM: stop accepting, drain in-flight up to a deadline, then close.
2. Exception-safe worker (A1.8).
3. Reject `enqueue` after shutdown (A1.9).
4. `send_all()` loop (A1.6).
5. Pipelining fix (A1.1).
6. Health-check endpoints: `/healthz`, `/readyz`.
7. Process supervision: ship a `systemd` unit (`Restart=on-failure`, `LimitNOFILE=`, `MemoryMax=`, `User=`) or a non-root `Dockerfile`.
8. All client-input errors map to 4xx; only allocator/OS errors are fatal.

### B4. Observability

1. Structured logging with levels; per-request access log: `ts method uri status bytes duration_ms client_ip ua`.
2. Request IDs — generate or read `X-Request-Id`, include in every log line and echo in response.
3. Prometheus-style metrics:
   - `http_requests_total{method,status}`
   - `http_request_duration_seconds` histogram
   - `http_request_size_bytes`, `http_response_size_bytes`
   - `tokoro_active_connections`
   - `tokoro_worker_queue_depth`
   - `tokoro_parse_errors_total`
4. Per-connection state dump via admin endpoint or `SIGUSR1`.
5. Crash diagnostics: signal handler for `SIGSEGV`/`SIGABRT` printing a backtrace.
6. Split debug info (`-g -gsplit-dwarf`); ship stripped binary + separate `.dwp`.

### B5. Performance & Resource Discipline

1. `CMAKE_BUILD_TYPE=Release` for benchmark/prod builds.
2. Document and raise `ulimit -n` (e.g. 65536); set `LimitNOFILE=` in systemd.
3. `SO_REUSEPORT` + one accept loop per core (still useful pre-epoll).
4. Eliminate per-request heap allocations on hot path: thread-local serialization buffer; reuse parser state.
5. Static file LRU cache keyed by `(path, mtime)`.
6. `sendfile()` / `writev()` for large static responses.
7. Bounded recv per request (size + deadline).
8. Drop `shared_ptr<Socket>` per connection (A2.2).
9. Document TCP tuning: `net.core.somaxconn`, listen backlog (currently default 128), `net.ipv4.tcp_tw_reuse`.

### B6. Configuration & Operability

1. CLI flags / config file: `--port`, `--bind`, `--docroot`, `--workers`, `--max-body`, `--header-timeout`, `--body-timeout`, `--keepalive-timeout`, `--max-connections`, `--log-level`, `--log-file`.
2. **Docroot bug**: `./www` is resolved relative to CWD. Resolve to absolute at startup and validate it exists and is a directory.
3. Validate config at startup with clear errors.
4. `--version`, `--help`, proper exit codes.
5. Reload-without-restart: `SIGHUP` re-reads config / reopens log file (for log rotation).

### B7. Testing Discipline

1. End-to-end / integration tests that bind real `Server` on port 0 and talk over a real socket.
2. **Fuzz testing** of `HttpParser::parse` via libFuzzer.
3. Adversarial corpus: slowloris, huge headers, huge body, CL+TE, duplicate CL, absolute-URI requests, embedded NULs, malformed chunk sizes.
4. Concurrency stress: 10k connections × 100 requests with keep-alive; zero errors and bounded memory.
5. 24h soak test with memory-growth assertion.
6. Sanitizer matrix in CI: ASan+UBSan; TSan separately.
7. Coverage gate (e.g. ≥ 85% on `src/`) via `gcovr` / `llvm-cov`.
8. Static analysis in CI: `clang-tidy`, `cppcheck`, `include-what-you-use`.
9. Nightly `valgrind --leak-check=full`.

### B8. Build, Release, Packaging

1. CI: Debug + Release builds on Linux + macOS, unit + integration tests, `clang-format` check, `clang-tidy`, ASan/UBSan, TSan.
2. `-Werror` in CI only.
3. `tokoro_core` static library shared by app and tests.
4. Per-target warnings/sanitizers (not global).
5. Reproducible builds: pinned `Catch2`, pinned toolchain in CI.
6. Versioning: embed `git describe` into a `version.hpp` generated by CMake; expose in `Server:` header and `--version`.
7. Commit `.clang-format`, `.clang-tidy`, `.editorconfig`.
8. `LICENSE` file.
9. `Dockerfile` — multi-stage, non-root user, distroless base, `HEALTHCHECK`.
10. `SECURITY.md` with vulnerability reporting process.

### B9. Documentation

1. Finish `docs/design.md` §4 (“Keep-Alive & Timeouts”) before extending behavior.
2. Rewrite `docs/benchmarks.md` with full reproducibility: hardware, kernel, ulimit, build type, exact `wrk` flags, keep-alive on/off, run count, variance.
3. Operational runbook: deploy, read metrics, common log lines, log rotation, graceful restart, alerts (`5xx rate`, `p99 > X`, `queue_depth > Y`).
4. Threat model: what tokoro defends against vs delegates to upstream LB / TLS terminator.
5. SLO doc: latency / availability targets Phase 1 commits to.

### B10. Polish

1. Unify namespaces — `HttpParser`, `HttpRequest`, `FileHandler` are global today while `Socket`/`Server`/`ThreadPool` are in `tokoro::`. Unify under `tokoro::`.
2. Header guard style — pick one (prefer `#pragma once`).
3. `[[nodiscard]]` on error-returning functions.
4. Keep “no `using namespace std`” discipline (currently clean).
5. Const-correctness sweep.

---

## Minimal Path to “Production-Grade Phase 1”

| # | Item | Why |
|---|------|-----|
| 1 | Bounded sizes + safe int parse + slowloris timeouts | Prevents trivial RAM/CPU DoS |
| 2 | Case-insensitive headers, CL/TE smuggling rejection, version validation, partial-send loop, pipelining fix | Correctness vs real clients & attackers |
| 3 | SIGPIPE handling, exception-safe worker, graceful shutdown | Server stays up |
| 4 | Path-traversal hardening + docroot canonicalization | Static-file safety |
| 5 | CLI/config (port, docroot, timeouts, workers, max-conn) | Operable in any environment |
| 6 | Structured access log + Prometheus `/metrics` + `/healthz` | Operable in real infra |
| 7 | CI (Linux+macOS, ASan/UBSan/TSan, clang-tidy, clang-format) + Release builds + Dockerfile + systemd unit | Shippable artifact |
| 8 | Integration tests + parser fuzzing + 24h soak | Confidence the above actually holds |

Everything else from Parts A and B is upside.

---

## Suggested PR Grouping

1. **PR 1 — Parser & server hardening:** A1.1, A1.2, A1.3, A1.4, A1.5, A1.6, A1.7, A1.8, A1.10, A1.11 + matching tests from A4.
2. **PR 2 — Lifecycle & config:** A2 items + B3.1 (graceful shutdown) + B6 (CLI flags, docroot fix, `--version`/`--help`).
3. **PR 3 — Observability:** B4.1–B4.3 (structured logging, request IDs, metrics) + `/healthz`, `/readyz`.
4. **PR 4 — Build & release:** A5 + B8 (CI, `tokoro_core` library, `LICENSE`, `clang-format`/`clang-tidy`, Dockerfile, systemd unit).
5. **PR 5 — Protocol completeness:** B1 items (HEAD, OPTIONS *, 304, Range, Date/Server headers, version negotiation).
6. **PR 6 — Test discipline:** B7 (integration, fuzz, soak, sanitizer matrix, coverage gate).
7. **PR 7 — Docs:** B9 (design §4, benchmarks rewrite, runbook, threat model, SLOs).
