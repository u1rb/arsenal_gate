#!/bin/bash

# Test Claude format with claude-3-7-sonnet-20250219
curl -X POST https://aihubmix.com/v1/messages \
  -H "Content-Type: application/json" \
  -H "x-api-key: $OPENAI_API_KEY" \
  -H "anthropic-version: 2023-06-01" \
  -d '{
    "model": "claude-3-7-sonnet-20250219",
    "max_tokens": 150,
    "messages": [
      {
        "role": "user",
        "content": "What is CRTP in C++?"
      }
    ]
  }' 