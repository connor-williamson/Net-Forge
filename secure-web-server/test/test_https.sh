#!/bin/bash
set -euo pipefail

PORT=4433
SERVER=build/https_server
LOG_FILE="logs/https_access.log"

$SERVER &
PID=$!
echo "Started HTTPS server [PID=$PID]"
sleep 1

function cleanup {
    echo "Stopping HTTPS server..."
    kill $PID
}
trap cleanup EXIT

# Ensure log is empty to start clean
: > $LOG_FILE

function request_ssl() {
    local PATH_REQUEST=$1
    echo -e "GET $PATH_REQUEST HTTP/1.1\r\nHost: localhost\r\nUser-Agent: TestClient/1.0\r\nConnection: close\r\n\r\n" | \
    openssl s_client -connect localhost:$PORT -quiet 2>/dev/null
}

echo "Testing: GET /"
request_ssl / | grep -q "Net-Forge" && echo "/ index test passed"

echo "Testing: GET /index.html"
request_ssl /index.html | grep -q "Net-Forge" && echo "/index.html served"

echo "Testing: GET /style.css"
request_ssl /style.css | grep -q "background" && echo "/style.css served"

echo "Testing: GET /script.js"
request_ssl /script.js | grep -q "Net-Forge JS" && echo "/script.js served"

echo "Testing: GET /info.txt"
request_ssl /info.txt | grep -q "info file" && echo "/info.txt served"

echo "Testing: 404 response"
if ! request_ssl /missing.html | grep -q "200 OK"; then
    echo "404 error triggered correctly"
else
    echo "404 error test failed" && exit 1
fi

echo "Testing: Log entries"
if grep -q "/info.txt" "$LOG_FILE" && grep -q "/missing.html" "$LOG_FILE"; then
    echo "Logging working"
else
    echo "Logging test failed" && exit 1
fi

exit 0
