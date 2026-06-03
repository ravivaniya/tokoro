# Phase 1 Benchmark Criteria

This document defines the benchmark criteria for tokoro Phase 1 after the correctness, safety, and operability improvements described in `docs/phase1_improvements.md` are implemented.

The goal is to replace a single headline throughput number with a reproducible benchmark suite that measures performance, tail latency, correctness under load, and resistance to basic failure modes.

## 1. Benchmark Rules

Every published run must include the following environment details:

- Hardware model, core/thread count, RAM, and network interface
- Operating system, kernel version, compiler version, and CMake build type
- Release build settings, sanitizer state, and link-time optimization state
- `ulimit -n`, listen backlog, and any TCP tuning used during the run
- tokoro runtime settings: bind address, port, worker count, docroot, request size caps, and timeout values
- Load generator name, version, exact flags, duration, warmup, and number of repeated runs
- Keep-alive state for each scenario
- Whether the test ran on loopback or across two hosts

A benchmark report is incomplete unless it can be reproduced from the published configuration.

## 2. Required Scenarios

Phase 1 benchmark coverage should include all of the following scenarios.

### 2.1 Static File Throughput

Measure a small static file response, such as a 100-byte asset.

- Load generator: `wrk`
- Expected state: keep-alive enabled
- Suggested shape: `wrk -t8 -c256 -d30s`
- Report:
  - Requests per second
  - Average latency
  - p50, p95, and p99 latency
  - Max latency

### 2.2 Static File Throughput Without Keep-Alive

Measure the same file with connection close semantics.

- Load generator: `wrk`
- Expected state: `Connection: close`
- Suggested shape: `wrk -t8 -c256 -d30s -H 'Connection: close'`
- Report the same latency and throughput fields as above.

### 2.3 Medium Static File

Measure a medium response, such as an 8 KB file, to ensure throughput remains stable when responses are no longer tiny.

- Load generator: `wrk`
- Expected state: keep-alive enabled
- Suggested shape: `wrk -t8 -c256 -d30s`

### 2.4 Large Static File

Measure a large file, such as 1 MB, to confirm the server remains stable under larger response bodies.

- Load generator: `wrk`
- Suggested shape: `wrk -t8 -c64 -d30s`
- Report CPU utilization, peak RSS, and throughput in addition to latency.

### 2.5 Pipelined Requests

Validate that multiple requests in one TCP receive buffer are processed correctly after the parser consumption fix.

- Load generator: a pipelined client or a custom script
- Required assertion: every request receives a response in order
- Required metric: no dropped or merged requests

### 2.6 Error Path Benchmark

Measure the hot path for common errors.

- Scenarios:
  - Missing file returns 404
  - Unsupported method returns 405
  - Invalid request returns 400
- Report throughput and latency for each error case.

## 3. Acceptance Criteria

The Phase 1 benchmark suite passes only if all of the following are true:

- No requests return 5xx during the benchmark suite
- No process crash, uncaught exception, or worker-thread death occurs
- No pipelined request is lost or reordered
- No benchmark scenario exceeds the configured request, header, or body size limits
- No benchmark run exhibits unbounded memory growth
- `Content-Length` parsing failures return a client error instead of terminating the process

If any of these conditions fail, the build is not benchmark-clean, even if the throughput number is high.

## 4. Safety and Abuse Checks

In addition to the throughput scenarios, Phase 1 should include a small abuse-oriented set of checks.

### 4.1 Oversized Request Rejection

Send headers or bodies larger than the configured limits.

- Expected result: request rejected with 413 or 400, depending on the failure mode
- Expected server behavior: no crash, no memory spike, connection closed or drained safely

### 4.2 Invalid Content-Length

Send `Content-Length: abc` and similar malformed values.

- Expected result: 400
- Expected server behavior: no exception escapes the worker thread

### 4.3 Path Traversal Attempts

Try plain traversal, encoded traversal, and absolute-path inputs.

- Examples:
  - `/../../etc/passwd`
  - `/%2e%2e/`
  - absolute paths and embedded NUL bytes
- Expected result: request rejected and confined to the docroot

### 4.4 Slowloris-Style Header Dribble

Open many connections and send headers slowly enough to trigger the header timeout.

- Expected result: connections are evicted by timeout
- Expected server behavior: healthy traffic continues to complete with minimal impact

## 5. Stress and Soak Checks

These are required before treating Phase 1 as stable.

- 10k concurrent keep-alive connections with repeated requests
- Several thousand task submissions to the thread pool under load
- One-hour soak test at a representative steady-state request rate

For each stress run, report:

- Peak RSS
- Peak file descriptor usage
- Peak worker queue depth
- Error count
- Any connection resets or timeout-related failures

## 6. Expected Reporting Format

Publish each benchmark run as a table with the following sections:

1. Environment
2. tokoro configuration
3. Load generator command
4. Throughput and latency results
5. Correctness checks
6. Resource usage
7. Notes and anomalies

## 7. Relationship to the Main Benchmark Doc

`docs/benchmarks.md` can continue to serve as the top-level methodology page, but this file is the Phase 1 acceptance document.

Use this file when deciding whether a Phase 1 change is benchmark-complete.

## 8. Human Verification and Reproduction Steps

This section is a practical runbook that a human reviewer can follow to verify Phase 1 results independently.

Scope: this runbook currently targets macOS (Apple Silicon) and Linux only.

### 8.1 What to Verify

A reviewer should be able to verify all of the following from logs, command output, and observed server behavior:

- Build is a Release build and runs without crashes
- Throughput and latency results can be reproduced within a reasonable variance window
- No unexpected 5xx responses occur during normal benchmark scenarios
- Pipelined requests are answered correctly and in-order
- Malformed and oversized requests are rejected without process termination
- Reported environment details match the actual machine and runtime settings

### 8.2 Prerequisites

Install required tools on the benchmark host:

- CMake 3.20+
- C++20-capable compiler
- `wrk`
- `ab` (optional but recommended for percentile cross-check)
- `curl`

Platform scope:

- Supported for this runbook: macOS (Apple Silicon) and Linux
- Other operating systems are out of scope for this version

Ensure the host is in a stable state before running:

- No heavy background workloads
- Consistent CPU power mode
- Known `ulimit -n` value

### 8.3 Build and Start tokoro

From repository root:

```bash
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
```

Start the server in a separate terminal:

```bash
./tokoro
```

If your server supports CLI flags for docroot, workers, and timeouts, include the exact flags in your run notes.

Note: the commands below assume a POSIX shell environment.

### 8.4 Capture Environment Metadata

Record these values before running load tests:

```bash
uname -a
ulimit -n
cmake --version
clang++ --version || g++ --version
wrk --version
ab -V
```

Also record:

- CPU model and core count
- RAM size
- Whether traffic is loopback or cross-host

### 8.5 Reproduce Core Throughput Scenarios

Run each scenario at least 3 times and keep all outputs.

Small static file, keep-alive:

```bash
wrk -t8 -c256 -d30s http://127.0.0.1:8080/
```

Small static file, connection close:

```bash
wrk -t8 -c256 -d30s -H "Connection: close" http://127.0.0.1:8080/
```

Latency cross-check with `ab`:

```bash
ab -c 1000 -n 10000 http://127.0.0.1:8080/
```

For each run, record:

- RPS
- Avg latency
- p50, p95, p99
- Max latency
- Non-2xx/3xx counts

### 8.6 Reproduce Correctness and Safety Checks

Invalid `Content-Length` should not crash the server:

```bash
printf 'POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: abc\r\n\r\n' | nc 127.0.0.1 8080
```

Oversized body/header should be rejected:

- Send a request exceeding configured limits (script or manual generator)
- Verify response is 413/400 per failure mode
- Verify server remains healthy by immediately rerunning a short `wrk` test

Path traversal should be rejected:

```bash
curl -i "http://127.0.0.1:8080/../../etc/passwd"
curl -i "http://127.0.0.1:8080/%2e%2e/"
```

Pipelined request handling check:

- Use a simple client script that sends two HTTP requests in one TCP write
- Verify two valid responses are received in order

### 8.7 Reproducibility Decision Rule

Treat Phase 1 results as reproducible only if:

- All required scenarios complete without crashes or hangs
- Tail latency and throughput are consistent across repeated runs
- Variation across runs stays within a documented tolerance (for example, within 10 to 15 percent for RPS on the same host)
- Safety checks produce expected client errors and do not degrade subsequent healthy traffic

### 8.8 Reviewer Sign-Off Template

Use this checklist for final human sign-off:

- [ ] Environment metadata captured and attached
- [ ] Release build confirmed
- [ ] Throughput scenarios rerun at least 3 times each
- [ ] Latency percentiles recorded
- [ ] No unexpected 5xx in healthy scenarios
- [ ] Invalid input checks passed (malformed length, oversized input, traversal)
- [ ] Pipelining behavior verified
- [ ] Reproducibility tolerance met
- [ ] Final benchmark report published with raw command outputs