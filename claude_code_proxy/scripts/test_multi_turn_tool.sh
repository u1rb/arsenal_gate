#!/bin/bash

# Test multi-turn tool conversation through the proxy
echo "Testing multi-turn tool conversation..."

# Simulate a conversation where:
# 1. User asks a question
# 2. Assistant responds with tool_use
# 3. User provides tool_result

curl -s http://localhost:8080/v1/messages \
  -H "Content-Type: application/json" \
  -H "x-api-key: $OPENAI_API_KEY" \
  -d '{
    "model": "claude-3-7-sonnet-20250219",
    "max_tokens": 100,
    "tools": [
      {
        "name": "get_weather",
        "description": "Get weather",
        "input_schema": {
          "type": "object",
          "properties": {
            "location": {"type": "string"}
          },
          "required": ["location"]
        }
      }
    ],
    "messages": [
      {
        "role": "user",
        "content": [
          {
            "type": "text",
            "text": "What is the weather in NYC?"
          }
        ]
      },
      {
        "role": "assistant",
        "content": [
          {
            "type": "tool_use",
            "id": "toolu_test_123",
            "name": "get_weather",
            "input": {
              "location": "NYC"
            }
          }
        ]
      },
      {
        "role": "user",
        "content": [
          {
            "type": "tool_result",
            "tool_use_id": "toolu_test_123",
            "content": [
              {
                "type": "text",
                "text": "The weather in NYC is sunny, 72°F"
              }
            ]
          }
        ]
      }
    ]
  }' | jq '.'

echo -e "\n\nChecking proxy logs for conversion details..."
tail -n 10 fastapi_proxy_log.json | jq '.' | tail -n 20 