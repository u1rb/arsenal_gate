#!/bin/bash

# Claude Code with Proxy Setup Script
# This script configures Claude Code to use the local proxy server

set -e

echo "🤖 Claude Code Proxy Setup"
echo "=========================="

# Check if OPENAI_API_KEY is set
if [ -z "$OPENAI_API_KEY" ]; then
    echo "❌ Error: OPENAI_API_KEY environment variable is required."
    echo "Please set your API key:"
    echo "   export OPENAI_API_KEY=\"your-api-key-here\""
    echo ""
    exit 1
fi

# Default proxy settings
PROXY_HOST=${PROXY_HOST:-127.0.0.1}
PROXY_PORT=${PROXY_PORT:-8080}
PROXY_URL="http://$PROXY_HOST:$PROXY_PORT"

# Check if proxy is running
if ! curl -s "$PROXY_URL/health" >/dev/null 2>&1; then
    echo "❌ Error: Proxy server is not running at $PROXY_URL"
    echo "Please start the proxy server first:"
    echo "   ./scripts/run_proxy.sh"
    echo ""
    echo "Or if using a different host/port:"
    echo "   export PROXY_HOST=\"your-proxy-host\""
    echo "   export PROXY_PORT=\"your-proxy-port\""
    echo ""
    exit 1
fi

# Set up Claude Code environment variables for proxy
export NODE_TLS_REJECT_UNAUTHORIZED=0
export ANTHROPIC_BASE_URL="$PROXY_URL"
export ANTHROPIC_AUTH_TOKEN="$OPENAI_API_KEY"
export ANTHROPIC_CUSTOM_HEADERS="x-api-key: $ANTHROPIC_AUTH_TOKEN"

# Display configuration
echo "✅ Proxy Configuration:"
echo "   Proxy URL: $PROXY_URL"
echo "   API Key: ${OPENAI_API_KEY:0:8}..."
echo "   Model: ${ANTHROPIC_MODEL:-claude-3-7-sonnet-20250219}"
echo ""

# Check if bunx is available
if ! command -v bunx >/dev/null 2>&1; then
    echo "❌ Error: bunx is not installed."
    echo "Please install bun first:"
    echo "   curl -fsSL https://bun.sh/install | bash"
    echo ""
    exit 1
fi

# Check if Claude Code is available
if ! bunx @anthropic-ai/claude-code --version >/dev/null 2>&1; then
    echo "⚠️  Warning: Claude Code may not be installed."
    echo "It will be downloaded automatically on first use."
    echo ""
fi

echo "🚀 Starting Claude Code with proxy..."
echo "   All requests will be routed through: $PROXY_URL"
echo "   Using model: ${ANTHROPIC_MODEL:-claude-3-7-sonnet-20250219}"
echo ""

# If arguments are provided, pass them to Claude Code
if [ $# -gt 0 ]; then
    echo "Running: bunx @anthropic-ai/claude-code $@"
    echo ""
    bunx @anthropic-ai/claude-code "$@"
else
    echo "No arguments provided. Starting interactive Claude Code..."
    echo "You can also run with arguments:"
    echo "   ./scripts/run_claude_code.sh -p \"Your prompt here\""
    echo "   ./scripts/run_claude_code.sh --help"
    echo ""
    bunx @anthropic-ai/claude-code
fi 