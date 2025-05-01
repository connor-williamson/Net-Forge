#!/bin/bash
set -euo pipefail

PORT=8080
SERVER=build/http_server
BASE_URL="http://localhost:$PORT"
LOG_FILE="logs/http_access.log"

# Start HTTP server
$SERVER &
PID=$!
echo "Started HTTP server [PID=$PID]"
sleep 1

function cleanup {
    echo "Stopping HTTP server..."
    kill $PID
}
trap cleanup EXIT

# Ensure log is empty to start clean
: > $LOG_FILE

echo "Testing: GET /"
curl -s -o /tmp/index.html "$BASE_URL/"
grep -q "Net-Forge" /tmp/index.html && echo "/ index test passed"

echo "Testing: GET /style.css"
curl -s -o /tmp/style.css "$BASE_URL/style.css"
grep -q "background" /tmp/style.css && echo "/style.css served"

echo "Testing: GET /script.js"
curl -s -o /tmp/script.js "$BASE_URL/script.js"
grep -q "Net-Forge JS" /tmp/script.js && echo "/script.js served"

echo "Testing: GET /info.txt"
curl -s -o /tmp/info.txt "$BASE_URL/info.txt"
grep -q "info file" /tmp/info.txt && echo "/info.txt served"

echo "Testing: 404 response"
if ! curl -s "$BASE_URL/missing_page.html" | grep -q "200 OK"; then
    echo "404 error triggered correctly"
else
    echo "404 error test failed" && exit 1
fi

echo "Testing: Log entries"
if grep -q "/info.txt" "$LOG_FILE" && grep -q "/missing_page.html" "$LOG_FILE"; then
    echo "Logging working"
else
    echo "Logging test failed" && exit 1
fi

exit 0
