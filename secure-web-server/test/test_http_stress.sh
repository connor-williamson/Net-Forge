#!/bin/bash

SERVER_BIN="./build/http_server"
LOG_FILE="logs/http_access.log"
NUM_REQUESTS=100
URL="http://localhost:8080/"
REQUEST_TIMEOUT=5

# Start the server in the background
$SERVER_BIN &
server_pid=$!
echo "Started HTTP server [PID=$server_pid]"

# Wait for server to start
sleep 1

# Ensure log file is clean
: > "$LOG_FILE"

echo "Launching $NUM_REQUESTS concurrent requests..."
pids=()
for i in $(seq 1 "$NUM_REQUESTS"); do
    curl -s --max-time "$REQUEST_TIMEOUT" "$URL" > /dev/null &
    pids+=($!)
done

# Wait for all curl processes to complete
echo "Waiting for all requests to complete..."
for pid in "${pids[@]}"; do
    if ! wait "$pid"; then
        echo "Request PID $pid failed or timed out"
    fi
done

# Verify log entries
expected="$NUM_REQUESTS"
actual=$(wc -l < "$LOG_FILE")
echo "Expected: $expected log entries"
echo "Actual:   $actual log entries"

if [ "$actual" -eq "$expected" ]; then
    echo "Stress test passed: $actual requests successfully handled"
else
    echo "Stress test failed: mismatch in log count"
fi

# Gracefully shut down the server
echo "Shutting down server [PID=$server_pid]..."
kill -INT "$server_pid"
wait "$server_pid"
echo "Server terminated."

# Confirm port is released
if sudo lsof -i :8080 > /dev/null; then
    echo "Port 8080 is still in use after shutdown"
else
    echo "Port 8080 is free"
fi