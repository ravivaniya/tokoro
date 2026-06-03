# Build stage
FROM debian:bookworm-slim AS builder

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --config Release -j$(nproc)

# Runtime stage
FROM gcr.io/distroless/cc-debian12

WORKDIR /app

# Copy the binary
COPY --from=builder /src/build/tokoro /app/tokoro

# Create a basic www structure
COPY --from=builder /src/www /app/www

# Expose the default port
EXPOSE 8080

# Run as nonroot user provided by distroless
USER nonroot:nonroot

# Distroless does not have curl/wget or a shell. 
# For a true HTTP healthcheck in a distroless container, typically an external 
# probe is used (e.g., Kubernetes livenessProbe).
# As a basic placeholder, we verify the binary can execute.
HEALTHCHECK --interval=30s --timeout=5s --start-period=5s --retries=3 \
  CMD ["/app/tokoro", "--version"]

ENTRYPOINT ["/app/tokoro"]
CMD ["--bind", "0.0.0.0", "--port", "8080", "--docroot", "/app/www"]
