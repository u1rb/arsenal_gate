#!/bin/bash

# Test tool calls through the proxy with forced tool use
echo "Testing forced tool calls through proxy..."

# Start proxy in background
python3 fastapi_proxy.py &
PROXY_PID=$!
sleep 2

# Test Claude format with tools and forced tool choice
curl -s http://localhost:8080/v1/messages \
  -H "Content-Type: application/json" \
  -H "x-api-key: $OPENAI_API_KEY" \
  -d '{
    "model": "claude-3-7-sonnet-20250219",
    "max_tokens": 1000,
    "tools": [
      {
        "name": "get_weather",
        "description": "Get the current weather in a given location",
        "input_schema": {
          "type": "object",
          "properties": {
            "location": {
              "type": "string",
              "description": "The city and state, e.g. San Francisco, CA"
            },
            "unit": {
              "type": "string",
              "enum": ["celsius", "fahrenheit"],
              "description": "The unit of temperature, either celsius or fahrenheit"
            }
          },
          "required": ["location"]
        }
      }
    ],
    "tool_choice": {
      "type": "tool",
      "name": "get_weather"
    },
    "messages": [
      {
        "role": "user",
        "content": "What is the weather like in San Francisco?"
      }
    ]
  }' | jq '.'

echo -e "\n\nStopping proxy..."
kill $PROXY_PID

echo -e "\nChecking proxy logs for tool conversion..."
tail -n 3 fastapi_proxy_log.json | jq '.' 