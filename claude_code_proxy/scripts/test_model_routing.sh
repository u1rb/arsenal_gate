#!/bin/bash

echo "Testing model routing with Claude Code..."

export NODE_TLS_REJECT_UNAUTHORIZED=0
export ANTHROPIC_BASE_URL=http://127.0.0.1:8080
export ANTHROPIC_AUTH_TOKEN=$OPENAI_API_KEY
export ANTHROPIC_CUSTOM_HEADERS="x-api-key: $ANTHROPIC_AUTH_TOKEN"

echo "=== Test 1: Claude model (should use Claude format) ==="
export ANTHROPIC_MODEL='claude-3-7-sonnet-20250219'
bunx @anthropic-ai/claude-code -p "What is 2+2? Answer briefly."

echo ""
echo "=== Test 2: GPT model (should convert to OpenAI format) ==="
export ANTHROPIC_MODEL='gpt-4.1'
bunx @anthropic-ai/claude-code -p "What is 3+3? Answer briefly."

echo ""
echo "=== Test 3: Check proxy logs ==="
echo "Recent proxy activity:"
tail -2 fastapi_proxy_log.json | jq -r '.timestamp + " " + .method + " " + .url + " -> " + (.response_status|tostring)' 