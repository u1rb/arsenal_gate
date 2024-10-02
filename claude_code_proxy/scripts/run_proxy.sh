#!/bin/bash

# Claude Code Proxy Server Startup Script
# This script checks for required environment variables and starts the FastAPI proxy

set -e

echo "🚀 Claude Code Proxy Server Startup"
echo "===================================="

# Check for required environment variables
MISSING_VARS=()

if [ -z "$TARGET_BASE_URL" ]; then
    MISSING_VARS+=("TARGET_BASE_URL")
fi

# Report missing variables
if [ ${#MISSING_VARS[@]} -ne 0 ]; then
    echo "❌ Error: Missing required environment variables:"
    for var in "${MISSING_VARS[@]}"; do
        echo "   - $var"
    done
    echo ""
    echo "Please set the required environment variables:"
    echo ""
    echo "export TARGET_BASE_URL=\"https://your-openai-compatible-server.com\""
    echo ""
    echo "Examples:"
    echo "  # For OpenAI"
    echo "  export TARGET_BASE_URL=\"https://api.openai.com\""
    echo ""
    echo "  # For aihubmix.com"
    echo "  export TARGET_BASE_URL=\"https://aihubmix.com\""
    echo ""
    exit 1
fi

# Check if API key is provided (optional at startup)
if [ -z "$OPENAI_API_KEY" ]; then
    echo "⚠️  Warning: OPENAI_API_KEY not set."
    echo "   The proxy will start, but requests will fail without an API key."
    echo "   You can:"
    echo "   1. Set it now: export OPENAI_API_KEY=\"your-key\""
    echo "   2. Set it later before making requests"
    echo "   3. Pass it in request headers (x-api-key)"
    echo ""
fi

# Display configuration
echo "✅ Configuration:"
echo "   TARGET_BASE_URL: $TARGET_BASE_URL"
if [ -n "$OPENAI_API_KEY" ]; then
    echo "   OPENAI_API_KEY: ${OPENAI_API_KEY:0:8}..."
else
    echo "   OPENAI_API_KEY: (not set - will need to be provided in requests)"
fi
echo "   PORT: ${PORT:-8080}"
echo "   HOST: ${HOST:-0.0.0.0}"
echo "   LOG_REQUESTS: ${LOG_REQUESTS:-true}"
echo ""

# Check if Python dependencies are installed
if ! python3 -c "import fastapi, uvicorn, httpx" 2>/dev/null; then
    echo "❌ Error: Required Python dependencies not found."
    echo "Please install dependencies:"
    echo "   pip install -r requirements.txt"
    echo ""
    exit 1
fi

# Check if port is already in use
PORT=${PORT:-8080}
if lsof -Pi :$PORT -sTCP:LISTEN -t >/dev/null 2>&1; then
    echo "❌ Error: Port $PORT is already in use."
    echo "Please stop the existing service or use a different port:"
    echo "   export PORT=8081"
    echo ""
    exit 1
fi

echo "🚀 Starting FastAPI proxy server..."
echo "   Access health check: http://localhost:$PORT/health"
echo "   View routes: http://localhost:$PORT/routes"
echo "   Logs: fastapi_proxy_log.json"
echo ""
echo "Press Ctrl+C to stop the server"
echo ""

# Start the proxy server
python3 fastapi_proxy.py 