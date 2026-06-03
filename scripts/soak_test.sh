#!/usr/bin/env bash
set -euo pipefail

# This script runs a 24-hour soak test against a tokoro server instance.
# It assumes 'wrk' or 'ab' is available, or falls back to using curl.

SERVER_BIN="./build/tokoro"
PORT=8080
DURATION_HOURS=24

if [ ! -f "$SERVER_BIN" ]; then
    echo "Error: Server binary not found at $SERVER_BIN"
    exit 1
fi

echo "Starting server..."
$SERVER_BIN --port $PORT &
SERVER_PID=$!

echo "Server started with PID: $SERVER_PID"

cleanup() {
    echo "Stopping server..."
    kill $SERVER_PID
    exit 0
}
trap cleanup SIGINT SIGTERM

# Give the server a second to bind
sleep 1

END_TIME=$(($(date +%s) + DURATION_HOURS * 3600))

echo "Running soak test for $DURATION_HOURS hours..."

while [ $(date +%s) -lt $END_TIME ]; do
    # Run a load generation tool for 1 minute
    if command -v wrk >/dev/null 2>&1; then
        wrk -t4 -c100 -d60s http://127.0.0.1:$PORT/ > /dev/null
    else
        # Fallback to curl in a loop if wrk is not available
        for i in {1..1000}; do
            curl -s http://127.0.0.1:$PORT/ > /dev/null
        done
        sleep 1
    fi
    
    # Check memory usage of the server
    if ps -p $SERVER_PID -o rss= > /dev/null; then
        MEM_KB=$(ps -p $SERVER_PID -o rss=)
        echo "$(date '+%Y-%m-%d %H:%M:%S') - Server Memory RSS: ${MEM_KB} KB"
    else
        echo "Server process died!"
        exit 1
    fi
done

echo "Soak test completed successfully."
cleanup
