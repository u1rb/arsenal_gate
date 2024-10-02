#!/usr/bin/env python3
"""
FastAPI-based proxy server for Claude Code
Routes requests to different AI providers based on configuration
"""
import json
import logging
import os
from datetime import datetime
from typing import Any, Dict, Optional

import httpx
import uvicorn
from fastapi import FastAPI, Request, Response, HTTPException
from fastapi.middleware.cors import CORSMiddleware

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

app = FastAPI(title="Claude Code Proxy", version="1.0.0")

# Add CORS middleware
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Configuration
TARGET_BASE_URL = os.getenv("TARGET_BASE_URL")
if not TARGET_BASE_URL:
    raise ValueError("TARGET_BASE_URL environment variable is required")
LOG_REQUESTS = os.getenv("LOG_REQUESTS", "true").lower() == "true"
DEFAULT_API_KEY = os.getenv("OPENAI_API_KEY")  # Optional default API key

# Universal proxy: Always converts Claude format to OpenAI format for upstream
# This allows the proxy to work with any OpenAI-compatible upstream server

def log_request_response(method: str, url: str, headers: dict, body: Any, 
                        response_status: int, response_headers: dict, response_body: Any):
    """Log request and response details"""
    if not LOG_REQUESTS:
        return
        
    timestamp = datetime.now().isoformat()
    log_entry = {
        "timestamp": timestamp,
        "method": method,
        "url": url,
        "request_headers": dict(headers),
        "request_body": body,
        "response_status": response_status,
        "response_headers": dict(response_headers),
        "response_body": response_body
    }
    
    logger.info(f"[{timestamp}] {method} {url} -> {response_status}")
    
    # Save to file
    with open('fastapi_proxy_log.json', 'a') as f:
        f.write(json.dumps(log_entry) + '\n')

def convert_claude_tools_to_openai(claude_tools: list) -> list:
    """Convert Claude tools format to OpenAI tools format"""
    openai_tools = []
    for tool in claude_tools:
        if isinstance(tool, dict):
            # Claude format: {"name": "...", "description": "...", "input_schema": {...}}
            # OpenAI format: {"type": "function", "function": {"name": "...", "description": "...", "parameters": {...}}}
            openai_tool = {
                "type": "function",
                "function": {
                    "name": tool.get("name"),
                    "description": tool.get("description"),
                    "parameters": tool.get("input_schema", {})
                }
            }
            openai_tools.append(openai_tool)
    return openai_tools

def convert_claude_tool_choice_to_openai(claude_tool_choice) -> dict:
    """Convert Claude tool_choice format to OpenAI tool_choice format"""
    if isinstance(claude_tool_choice, dict):
        choice_type = claude_tool_choice.get("type")
        if choice_type == "auto":
            return "auto"
        elif choice_type == "any":
            return "required"  # OpenAI equivalent of "any"
        elif choice_type == "tool":
            tool_name = claude_tool_choice.get("name")
            if tool_name:
                return {"type": "function", "function": {"name": tool_name}}
        elif choice_type == "none":
            return "none"
    return claude_tool_choice

def convert_claude_messages_to_openai(claude_messages: list) -> list:
    """Convert Claude message format to OpenAI message format"""
    openai_messages = []
    
    for message in claude_messages:
        content = message.get("content")
        if isinstance(content, list):
            # Claude format: content is array of blocks
            text_parts = []
            tool_calls = []
            tool_results = []
            
            for block in content:
                if block.get("type") == "text":
                    text_parts.append(block.get("text", ""))
                elif block.get("type") == "tool_use":
                    # Convert Claude tool_use to OpenAI tool_calls
                    tool_call = {
                        "id": block.get("id"),
                        "type": "function",
                        "function": {
                            "name": block.get("name"),
                            "arguments": json.dumps(block.get("input", {}))
                        }
                    }
                    tool_calls.append(tool_call)
                elif block.get("type") == "tool_result":
                    # Collect tool results to handle separately
                    result_content = block.get("content", "")
                    tool_use_id = block.get("tool_use_id", "")
                    
                    if isinstance(result_content, list):
                        # Extract text from content blocks
                        result_texts = []
                        for result_block in result_content:
                            if isinstance(result_block, dict) and result_block.get("type") == "text":
                                result_texts.append(result_block.get("text", ""))
                        result_text = " ".join(result_texts)
                    else:
                        result_text = str(result_content)
                    
                    tool_results.append({
                        "tool_call_id": tool_use_id,
                        "content": result_text
                    })
            
            # Handle different message types
            if tool_calls:
                # Assistant message with tool calls
                openai_message = {
                    "role": "assistant",
                    "content": " ".join(text_parts) if text_parts else "",
                    "tool_calls": tool_calls
                }
                openai_messages.append(openai_message)
            elif tool_results:
                # User message with tool results - convert to separate tool messages
                # First add any text content as a user message
                if text_parts:
                    openai_messages.append({
                        "role": "user",
                        "content": " ".join(text_parts)
                    })
                
                # Then add each tool result as a separate tool message
                for tool_result in tool_results:
                    openai_messages.append({
                        "role": "tool",
                        "tool_call_id": tool_result["tool_call_id"],
                        "content": tool_result["content"]
                    })
            else:
                # Regular text message
                openai_message = {
                    "role": message.get("role"),
                    "content": " ".join(text_parts) if text_parts else ""
                }
                openai_messages.append(openai_message)
                
        else:
            # Simple string content
            openai_message = {
                "role": message.get("role"),
                "content": content or ""
            }
            openai_messages.append(openai_message)
    
    return openai_messages

def convert_claude_to_openai(claude_request: dict) -> dict:
    """Convert Claude API format to OpenAI format"""
    openai_request = {
        "model": claude_request.get("model"),
        "messages": convert_claude_messages_to_openai(claude_request.get("messages", [])),
        "max_tokens": claude_request.get("max_tokens", 1000),
        "temperature": claude_request.get("temperature", 0.7),
        "stream": claude_request.get("stream", False)
    }
    
    # Handle tool-related fields
    if "tools" in claude_request:
        openai_request["tools"] = convert_claude_tools_to_openai(claude_request["tools"])
    
    if "tool_choice" in claude_request:
        openai_request["tool_choice"] = convert_claude_tool_choice_to_openai(claude_request["tool_choice"])
    
    # Handle parallel tool use (OpenAI uses parallel_tool_calls, Claude uses disable_parallel_tool_use)
    if "disable_parallel_tool_use" in claude_request:
        openai_request["parallel_tool_calls"] = not claude_request["disable_parallel_tool_use"]
    
    return openai_request

def convert_openai_to_claude(openai_response: dict) -> dict:
    """Convert OpenAI response format to Claude format"""
    if "choices" in openai_response and openai_response["choices"]:
        choice = openai_response["choices"][0]
        message = choice.get("message", {})
        content = message.get("content", "")
        tool_calls = message.get("tool_calls", [])
        
        # Build Claude content blocks
        claude_content = []
        
        # Add text content if present
        if content:
            claude_content.append({"type": "text", "text": content})
        
        # Convert tool calls to Claude format
        if tool_calls:
            for tool_call in tool_calls:
                if tool_call.get("type") == "function":
                    function = tool_call.get("function", {})
                    claude_tool_use = {
                        "type": "tool_use",
                        "id": tool_call.get("id"),
                        "name": function.get("name"),
                        "input": json.loads(function.get("arguments", "{}")) if function.get("arguments") else {}
                    }
                    claude_content.append(claude_tool_use)
        
        # If no content blocks, add empty text
        if not claude_content:
            claude_content = [{"type": "text", "text": ""}]
        
        # Determine stop reason
        finish_reason = choice.get("finish_reason")
        if finish_reason == "tool_calls":
            stop_reason = "tool_use"
        elif finish_reason == "stop":
            stop_reason = "end_turn"
        elif finish_reason == "length":
            stop_reason = "max_tokens"
        else:
            stop_reason = "end_turn"
        
        claude_response = {
            "id": openai_response.get("id", "msg_proxy_" + str(int(datetime.now().timestamp()))),
            "type": "message",
            "role": "assistant",
            "content": claude_content,
            "model": openai_response.get("model"),
            "stop_reason": stop_reason,
            "usage": {
                "input_tokens": openai_response.get("usage", {}).get("prompt_tokens", 0),
                "output_tokens": openai_response.get("usage", {}).get("completion_tokens", 0)
            }
        }
        return claude_response
    
    return openai_response

@app.api_route("/v1/{path:path}", methods=["GET", "POST", "PUT", "DELETE", "PATCH"])
async def proxy_request(request: Request, path: str):
    """Proxy API requests to the target server"""
    try:
        # Get request body
        body = await request.body()
        request_data = None
        
        if body:
            try:
                request_data = json.loads(body.decode('utf-8'))
            except json.JSONDecodeError:
                request_data = body.decode('utf-8')
        
        # Always convert Claude format to OpenAI format for upstream
        target_base_url = TARGET_BASE_URL
        
        # Always convert Claude format to OpenAI format
        if request_data and isinstance(request_data, dict) and path.startswith("messages"):
            logger.info(f"Converting Claude format to OpenAI format for model: {request_data.get('model', 'unknown')}")
            logger.info(f"Original request has tools: {'tools' in request_data}")
            if 'tools' in request_data:
                logger.info(f"Tools count: {len(request_data['tools'])}")
            
            # Log original messages structure
            original_messages = request_data.get('messages', [])
            logger.info(f"Original messages count: {len(original_messages)}")
            for i, msg in enumerate(original_messages):
                content = msg.get('content', [])
                if isinstance(content, list):
                    content_types = [block.get('type') for block in content if isinstance(block, dict)]
                    logger.info(f"Message {i} ({msg.get('role')}): content types = {content_types}")
                else:
                    logger.info(f"Message {i} ({msg.get('role')}): content = string")
            
            # Convert Claude request to OpenAI format
            request_data = convert_claude_to_openai(request_data)
            logger.info(f"Converted request has tools: {'tools' in request_data}")
            if 'tools' in request_data:
                logger.info(f"Converted tools count: {len(request_data['tools'])}")
            
            # Log converted messages structure
            converted_messages = request_data.get('messages', [])
            logger.info(f"Converted messages count: {len(converted_messages)}")
            for i, msg in enumerate(converted_messages):
                content = msg.get('content', '')
                tool_calls = msg.get('tool_calls', [])
                logger.info(f"Converted message {i} ({msg.get('role')}): content_len={len(content)}, tool_calls={len(tool_calls)}")
            
            path = "chat/completions"  # Change endpoint to OpenAI format
            body = json.dumps(request_data).encode('utf-8')
        
        target_url = f"{target_base_url}/v1/{path}"
        
        # Prepare headers
        headers = dict(request.headers)
        headers.pop("host", None)  # Remove host header
        headers.pop("content-length", None)  # Remove content-length to let httpx calculate it
        
        # Always convert headers to OpenAI format
        # Convert x-api-key to Authorization header for OpenAI
        if "x-api-key" in headers:
            headers["authorization"] = f"Bearer {headers.pop('x-api-key')}"
        elif DEFAULT_API_KEY and "authorization" not in headers:
            # Use default API key if no authorization header provided
            headers["authorization"] = f"Bearer {DEFAULT_API_KEY}"
        
        # Check if we have an API key
        if "authorization" not in headers:
            raise HTTPException(
                status_code=401, 
                detail="No API key provided. Set OPENAI_API_KEY environment variable or include x-api-key header."
            )
        
        # Make the request
        async with httpx.AsyncClient(timeout=60.0) as client:
            response = await client.request(
                method=request.method,
                url=target_url,
                headers=headers,
                content=body
            )
            
            response_data = response.content
            response_json = None
            
            try:
                response_json = response.json()
                
                # Always convert OpenAI response back to Claude format
                if path.startswith("chat/completions"):
                    # Convert OpenAI response back to Claude format
                    response_json = convert_openai_to_claude(response_json)
                    response_data = json.dumps(response_json).encode('utf-8')
                    
            except json.JSONDecodeError:
                pass
            
            # Log the request and response
            log_request_response(
                request.method, target_url, headers, request_data,
                response.status_code, dict(response.headers), response_json or response_data.decode('utf-8', errors='ignore')
            )
            
            # Log errors for debugging
            if response.status_code >= 400:
                logger.error(f"HTTP {response.status_code} error from upstream:")
                logger.error(f"Response: {response_json or response_data.decode('utf-8', errors='ignore')}")
                if request_data:
                    logger.error(f"Request that caused error: {json.dumps(request_data, indent=2)}")
            
            # Return response
            response_headers = dict(response.headers)
            # Remove problematic headers
            response_headers.pop("content-length", None)
            response_headers.pop("transfer-encoding", None)
            response_headers.pop("connection", None)
            
            return Response(
                content=response_data,
                status_code=response.status_code,
                headers=response_headers
            )
            
    except Exception as e:
        logger.error(f"Proxy error: {e}")
        raise HTTPException(status_code=500, detail=f"Proxy error: {e}")

@app.get("/health")
async def health_check():
    """Health check endpoint"""
    return {"status": "healthy", "target": TARGET_BASE_URL}

@app.get("/routes")
async def get_routes():
    """Get current proxy configuration"""
    return {
        "mode": "universal", 
        "description": "Always converts Claude format to OpenAI format for upstream",
        "target_base_url": TARGET_BASE_URL
    }

if __name__ == "__main__":
    port = int(os.getenv("PORT", 8080))
    host = os.getenv("HOST", "0.0.0.0")
    
    logger.info(f"Starting FastAPI proxy server on {host}:{port}")
    logger.info(f"Target base URL: {TARGET_BASE_URL}")
    logger.info(f"Default API key: {'Set' if DEFAULT_API_KEY else 'Not set (will require x-api-key header)'}")
    logger.info(f"Request logging: {LOG_REQUESTS}")
    
    uvicorn.run(app, host=host, port=port) 