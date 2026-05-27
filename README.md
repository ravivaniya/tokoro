# tokoro

> **tokoro** is a from-scratch C++20 HTTP/1.1 server *and* an experimental inference-aware reverse proxy. Unlike general-purpose proxies (nginx, HAProxy), tokoro routes requests using application-layer signals — prompt-prefix hashing for KV-cache affinity, token-budget accounting for load balancing, request coalescing, mid-stream failover, and per-tenant token-rate limiting — yielding higher cache-hit rates and lower tail latency for LLM inference workloads.

## Architecture (Phase 1)

```mermaid
graph TD
    Client[Client 1...N] -->|TCP Connection| AL(Accept Loop - Main Thread)
    AL -->|Enqueue Socket fd| TQ[(Task Queue - Mutex + CondVar)]
    
    TQ -->|Dequeue| W1(Worker Thread 1)
    TQ -->|Dequeue| W2(Worker Thread 2)
    TQ -->|Dequeue| WN(Worker Thread N)
    
    W1 --> HP1(HTTP Parser)
    W2 --> HP2(HTTP Parser)
    
    HP1 --> FH1{File Handler / Router}
    HP2 --> FH2{File Handler / Router}
    
    FH1 -->|Static File| RW1(Response Writer)
    FH2 -->|Reverse Proxy| RW2(Response Writer)
    RW1 -->|HTTP Response| Client
    RW2 -->|HTTP Response| Client
```

## Capability Comparison vs nginx

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

## Building & Running

### Requirements
- CMake 3.20+
- Modern C++ compiler (C++20 support)
- macOS / Linux (POSIX APIs)

### Build Instructions
```bash
mkdir build
cd build
cmake ..
cmake --build .
./tokoro
```
