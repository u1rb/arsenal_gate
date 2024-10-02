#!/bin/bash

echo "Testing model routing with Claude Code..."

export NODE_TLS_REJECT_UNAUTHORIZED=0
export ANTHROPIC_BASE_URL=http://127.0.0.1:8080
export ANTHROPIC_AUTH_TOKEN=$OPENAI_API_KEY
export ANTHROPIC_CUSTOM_HEADERS="x-api-key: $ANTHROPIC_AUTH_TOKEN"


export ANTHROPIC_SMALL_FAST_MODEL='claude-3-5-haiku-20241022'
export ANTHROPIC_MODEL='claude-3-7-sonnet-20250219'

# export ANTHROPIC_SMALL_FAST_MODEL='gpt-4.1-mini'
# export ANTHROPIC_MODEL='gpt-4.1'

bunx @anthropic-ai/claude-code
