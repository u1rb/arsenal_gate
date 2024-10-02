#!/bin/bash

echo "Testing Claude Code with HTTP proxy and different configurations..."

# Try with NODE_TLS_REJECT_UNAUTHORIZED=0 to disable SSL verification
export NODE_TLS_REJECT_UNAUTHORIZED=0
export ANTHROPIC_BASE_URL=http://localhost:8080
export ANTHROPIC_AUTH_TOKEN=$OPENAI_API_KEY
export ANTHROPIC_CUSTOM_HEADERS="x-api-key: $ANTHROPIC_AUTH_TOKEN"
export ANTHROPIC_MODEL='claude-3-7-sonnet-20250219'

echo "Attempt 1: With NODE_TLS_REJECT_UNAUTHORIZED=0"
timeout 10s bunx @anthropic-ai/claude-code --verbose -p "Hello" || echo "Failed"

echo ""
echo "Attempt 2: With different base URL format"
export ANTHROPIC_BASE_URL=http://127.0.0.1:8080
timeout 10s bunx @anthropic-ai/claude-code --verbose -p "Hello" || echo "Failed"

echo ""
echo "Attempt 3: Testing if the proxy receives any requests"
echo "Check proxy logs for any incoming requests..." 