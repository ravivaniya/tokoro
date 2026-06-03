# Tokoro Operational Runbook

This runbook outlines standard operating procedures for deploying, monitoring, and managing Tokoro in a production environment.

## Deployment

### Prerequisites
- Build `tokoro` using the `Release` profile: `cmake --build build --config Release`.
- Ensure a TLS terminator (e.g., HAProxy, Envoy, or Nginx) is positioned in front of Tokoro, as Tokoro does not handle TLS termination natively.
- Set OS-level limits appropriately. `ulimit -n 65536` is recommended to support thousands of concurrent connections.

### Starting the Server
Start Tokoro using `systemd` or via a containerized environment (Docker/Kubernetes).
Example CLI invocation:
```bash
tokoro --port 8080 --workers 8 --docroot /var/www/html --log-level info
```

## Monitoring & Metrics

Tokoro exports Prometheus-compatible metrics at the `/metrics` endpoint.

### Key Metrics to Monitor
- `tokoro_http_requests_total{status="2xx|4xx|5xx"}`: Monitor the rate of requests. A spike in `5xx` indicates backend or internal server errors.
- `tokoro_http_request_duration_seconds`: Histogram of request latencies. Watch `p95` and `p99` closely.
- `tokoro_active_connections`: Current number of open connections. Nearing the configured limit may require scaling out.
- `tokoro_worker_queue_depth`: The number of requests waiting for a worker thread. If this grows unbounded, the server is overloaded.

## Log Analysis

Tokoro provides structured access logs for every request.
Format: `[TIMESTAMP] [METHOD] [URI] [STATUS] [BYTES_SENT] [DURATION_MS] [CLIENT_IP]`
Example: `2026-06-03T12:00:00Z GET /index.html 200 1024 12 127.00.1`

### Common Log Patterns
- **400 Bad Request**: Often due to malformed HTTP syntax or exceeding `max-header` / `max-body` size limits.
- **404 Not Found**: Client requested a missing file or unmapped route.
- **413 Payload Too Large**: Request body exceeded `--max-body` limit.
- **503 Service Unavailable**: Worker queue is full and rejecting new requests.

## Operations

### Graceful Restart / Reload
To reload configurations without dropping connections (assuming Phase 1 config-reload via signal is implemented):
```bash
kill -SIGHUP <tokoro_pid>
```

### Log Rotation
If writing directly to a log file, rotate the file and signal Tokoro to reopen it:
```bash
mv /var/log/tokoro/access.log /var/log/tokoro/access.log.old
kill -SIGHUP <tokoro_pid>
```

## Alerts & Responses

| Alert Condition | Meaning | Recommended Action |
| :--- | :--- | :--- |
| **5xx Rate > 1%** | Tokoro or its upstream backend is failing. | Check Tokoro application logs for uncaught exceptions. Verify the upstream service health if acting as a reverse proxy. |
| **p99 Latency > 500ms** | Requests are taking too long to serve. | Check `tokoro_worker_queue_depth`. If high, add more worker threads or scale horizontally. If serving static files, ensure disks are not bottlenecked. |
| **Worker Queue Depth > 100** | Thread pool is saturated. | Scale out Tokoro instances. Consider lowering `keepalive-timeout` to free up connections faster. |
| **Memory Usage Growth** | Potential memory leak. | Restart the service and capture a heap profile or run under ASan. |
