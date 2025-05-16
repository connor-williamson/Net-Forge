#!/bin/bash
set -euo pipefail

PORT=8080
SERVER=build/http_server
LOG_FILE="logs/http_access.log"
UA="ThreadTestClient/1.0"
CONCURRENCY=10
URLS=("/index.html" "/style.css" "/script.js" "/info.txt")

# Ensure log file and shutdown flag are clean
: > "$LOG_FILE"
rm -f /tmp/shutdown.flag

# Start the HTTP server in the background
$SERVER &
SERVER_PID=$!
echo "Started HTTP server [PID=$SERVER_PID]"
sleep 1  # Give the server a moment to start

echo "Running multithreaded request test with $CONCURRENCY concurrent requests..."

# Launch concurrent curl requests
for i in $(seq 1 "$CONCURRENCY"); do
    url="${URLS[$((i % ${#URLS[@]}))]}"
   curl --max-time 2 -s -A "$UA" -H "Connection: close" "http://localhost:$PORT${url}" > /dev/null && echo "[$i] Done" &
done

wait  # Wait for all curl jobs to complete

echo "All requests completed"

# Trigger server shutdown
touch /tmp/shutdown.flag

# Wait for server process to exit gracefully
wait "$SERVER_PID" 2>/dev/null || true
rm -f /tmp/shutdown.flag

# Validate log entries
echo "Verifying log entries..."
entries=$(grep -c "requested" "$LOG_FILE" || true)

if [ "$entries" -lt "$CONCURRENCY" ]; then
    echo "Only $entries of $CONCURRENCY requests logged"
    exit 1
else
    echo "$entries log entries found"
fi