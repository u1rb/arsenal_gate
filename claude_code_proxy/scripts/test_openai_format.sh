#!/bin/bash

# Test OpenAI format with gpt-4.1
curl -X POST https://aihubmix.com/v1/chat/completions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $OPENAI_API_KEY" \
  -d '{
    "model": "gpt-4.1",
    "messages": [
      {
        "role": "user",
        "content": "What is CRTP in C++?"
      }
    ],
    "max_tokens": 150,
    "temperature": 0.7
  }' 