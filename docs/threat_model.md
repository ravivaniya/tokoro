# Tokoro Threat Model (Phase 1)

This document outlines the threat model for the Tokoro HTTP server in its Phase 1 (blocking I/O, thread pool) implementation.

## In-Scope: What Tokoro Defends Against

Tokoro is designed to be robust against common application-layer HTTP attacks and basic network-level abuse.

1. **Slowloris and Slow Body Attacks:**
   - Mitigated via strict deadlines for reading headers (`header_timeout`) and bodies (`body_timeout`).
2. **Buffer Overflows & Memory Exhaustion:**
   - Mitigated via strict caps on `max-header-bytes`, `max-body-bytes`, and `max-uri-bytes`.
   - Use of modern C++ standard library containers (`std::string`, `std::vector`) prevents classical buffer overflows.
3. **HTTP Request Smuggling:**
   - Mitigated by rejecting ambiguous requests (e.g., both `Content-Length` and `Transfer-Encoding` present, duplicate headers, invalid spacing).
4. **Path Traversal:**
   - Mitigated via strict URI sanitization, URL decoding, resolving to absolute paths, and verifying the resolved path starts with the configured document root.
5. **Connection Exhaustion (Application Level):**
   - Mitigated via the `max-connections` limit. Connections exceeding this limit are rejected or queued to an emergency file descriptor.
   - Idle Keep-Alive timeouts gracefully reap inactive connections.

## Out-of-Scope: What Tokoro Delegates

Tokoro is designed to operate behind a Reverse Proxy / Load Balancer or TLS Terminator (e.g., Envoy, HAProxy, Nginx). It explicitly **does not** defend against:

1. **TLS / HTTPS Security:**
   - Tokoro speaks raw, unencrypted HTTP. All encryption, certificate management, and TLS downgrade protections must be handled upstream.
2. **Volumetric DDoS Attacks:**
   - SYN floods, UDP amplification, or massive HTTP floods will easily overwhelm Tokoro's thread pool. Network-level DDoS mitigation must be provided by the infrastructure (e.g., Cloudflare, AWS Shield).
3. **Rate Limiting & WAF:**
   - Advanced IP-based rate limiting, bot detection, or SQL injection/XSS filtering are currently out of scope for Phase 1.
4. **Privilege Escalation:**
   - Tokoro should be run as an unprivileged user. Dropping privileges from `root` is not handled internally by the application itself; it relies on `systemd` or Docker.
