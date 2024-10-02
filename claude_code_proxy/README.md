# Claude Code Proxy Server

## Goal

We want to run Claude Code under a proxy, so we can route OpenAI models (or Claude models from third party) for Claude Code to use. This enables intelligent model routing and format conversion between different AI providers.

## ✅ Completed Implementation

All steps have been successfully implemented and tested! The proxy server now supports:

- **Universal Format Conversion**: Always converts Claude format to OpenAI format for upstream compatibility
- **Any Model Support**: Works with any model (Claude, GPT, or future models) without configuration
- **OpenAI-Compatible Upstream**: Can work with any OpenAI-compatible API provider
- **Request Logging**: Comprehensive logging of all API traffic
- **Seamless Integration**: Claude Code works transparently with any upstream server

## 🚀 Quick Start

Get up and running in 2 minutes:

### Prerequisites

1. **Install Python dependencies:**
   ```bash
   pip install -r requirements.txt
   ```

2. **Install Bun (for Claude Code):**
   ```bash
   curl -fsSL https://bun.sh/install | bash
   ```

3. **Set required environment variables:**
   ```bash
   # Required: Target server
   export TARGET_BASE_URL="https://your-openai-compatible-server.com"
   
   # Optional: Default API key (can also be provided per-request)
   export OPENAI_API_KEY="your-api-key-here"
   
   # Examples:
   # For OpenAI: export TARGET_BASE_URL="https://api.openai.com"
   # For aihubmix: export TARGET_BASE_URL="https://aihubmix.com"
   ```

### Start the Proxy Server

```bash
# Option 1: Use the convenience script (recommended)
./scripts/run_proxy.sh

# Option 2: Run directly
python3 fastapi_proxy.py
```

The script will:
- ✅ Check all required environment variables
- ✅ Verify Python dependencies
- ✅ Check if port is available
- ✅ Start the proxy server with proper configuration

### Run Claude Code

```bash
# Option 1: Use the convenience script (recommended)
./scripts/run_claude_code.sh -p "What is 2+2?"

# Option 2: Set environment variables manually
export NODE_TLS_REJECT_UNAUTHORIZED=0
export ANTHROPIC_BASE_URL=http://127.0.0.1:8080
export ANTHROPIC_AUTH_TOKEN=$OPENAI_API_KEY
export ANTHROPIC_CUSTOM_HEADERS="x-api-key: $ANTHROPIC_AUTH_TOKEN"
bunx @anthropic-ai/claude-code -p "What is 2+2?"
```

### Use Different Models

```bash
# Claude models
export ANTHROPIC_MODEL='claude-3-7-sonnet-20250219'
./scripts/run_claude_code.sh -p "Your prompt"

# OpenAI models  
export ANTHROPIC_MODEL='gpt-4.1'
./scripts/run_claude_code.sh -p "Your prompt"

# Any model works automatically!
export ANTHROPIC_MODEL='gpt-4.1-mini'
export ANTHROPIC_MODEL='claude-3-5-haiku-20241022'
```

### Verify Everything Works

```bash
# Check proxy health
curl http://localhost:8080/health

# View proxy configuration
curl http://localhost:8080/routes

# Monitor logs
tail -f fastapi_proxy_log.json
```

That's it! 🎉 Claude Code now routes through your proxy to any OpenAI-compatible server.

## Steps & Progress

### ✅ Step 1: Verify Third-Party Endpoint
**Status: COMPLETED**

Test the existing workable third-party endpoint for Claude Code:

```bash
# Run the existing script to verify aihubmix.com works
bash use_aihubmix.sh
```

**Result**: Successfully verified that Claude Code works with aihubmix.com endpoint.

### ✅ Step 2: Create API Format Test Scripts
**Status: COMPLETED**

Created curl scripts to test both OpenAI and Claude API request formats:

```bash
# Test OpenAI format with gpt-4.1
./scripts/test_openai_format.sh

# Test Claude format with claude-3-7-sonnet-20250219
./scripts/test_claude_format.sh
```

**Files Created**:
- `scripts/test_openai_format.sh` - Tests OpenAI chat/completions endpoint
- `scripts/test_claude_format.sh` - Tests Claude messages endpoint

**Result**: Both API formats work correctly with the third-party endpoint.

### ✅ Step 3: Create Logging Proxy
**Status: COMPLETED**

Developed a simple HTTP proxy to intercept and log Claude Code traffic:

```bash
# Start the simple logging proxy
python3 simple_proxy.py

# Test with curl (in another terminal)
curl http://localhost:8080/v1/messages \
  -H "Content-Type: application/json" \
  -H "x-api-key: $OPENAI_API_KEY" \
  -d '{"model": "claude-3-7-sonnet-20250219", "messages": [{"role": "user", "content": "Hello"}], "max_tokens": 10}'
```

**Files Created**:
- `simple_proxy.py` - Basic HTTP proxy with request/response logging
- `proxy_log.json` - Log file with captured traffic

**Result**: Successfully captured and logged API requests and responses.

### ✅ Step 4: Develop FastAPI Proxy Server
**Status: COMPLETED**

Created an advanced FastAPI-based proxy server with intelligent model routing:

```bash
# Install dependencies
pip install -r requirements.txt

# Start the FastAPI proxy server
python3 fastapi_proxy.py &

# Test health endpoint
curl http://localhost:8080/health

# Check proxy configuration
curl http://localhost:8080/routes
```

**Files Created**:
- `fastapi_proxy.py` - Universal proxy with format conversion for any OpenAI-compatible upstream
- `requirements.txt` - Python dependencies
- `fastapi_proxy_log.json` - Detailed request/response logs

**Key Features**:
- **Universal Format Conversion**: Always converts Claude format to OpenAI format for upstream
- **Header Conversion**: Automatically converts authentication headers (`x-api-key` → `Authorization: Bearer`)
- **Response Conversion**: Converts OpenAI responses back to Claude format for client compatibility
- **Model Agnostic**: Works with any model without needing to configure supported model lists
- **Comprehensive Logging**: Logs all traffic with timestamps and detailed information

### ✅ Step 5: Claude Code Integration
**Status: COMPLETED ✨**

Successfully integrated Claude Code with our proxy server:

```bash
# Quick test - should work immediately
export NODE_TLS_REJECT_UNAUTHORIZED=0
export ANTHROPIC_BASE_URL=http://127.0.0.1:8080
export ANTHROPIC_AUTH_TOKEN=$OPENAI_API_KEY
export ANTHROPIC_CUSTOM_HEADERS="x-api-key: $ANTHROPIC_AUTH_TOKEN"

# Test with Claude model
export ANTHROPIC_MODEL='claude-3-7-sonnet-20250219'
bunx @anthropic-ai/claude-code -p "What is 2+2?"

# Test with OpenAI model (will be converted automatically)
export ANTHROPIC_MODEL='gpt-4.1'
bunx @anthropic-ai/claude-code -p "What is 3+3?"
```

**Test Scripts**:
```bash
# Comprehensive testing
./scripts/test_claude_code_http.sh     # Test different HTTP configurations
./scripts/test_model_routing.sh        # Test model routing functionality
```

**Files Created**:
- `scripts/test_claude_code_proxy.sh` - Basic Claude Code proxy test
- `scripts/test_claude_code_http.sh` - HTTP configuration tests
- `scripts/test_model_routing.sh` - Model routing verification

## 🚀 How to Use

### 1. Start the Proxy Server

```bash
# Option 1: Use the convenience script (recommended)
export OPENAI_API_KEY="your-api-key-here"
export TARGET_BASE_URL="https://your-openai-compatible-server.com"
./scripts/run_proxy.sh

# Option 2: Manual setup
export OPENAI_API_KEY="your-api-key-here"
export TARGET_BASE_URL="https://your-openai-compatible-server.com"
python3 fastapi_proxy.py &
```

### 2. Run Claude Code

```bash
# Option 1: Use the convenience script (recommended)
./scripts/run_claude_code.sh -p "Your prompt here"

# Option 2: Manual setup
export NODE_TLS_REJECT_UNAUTHORIZED=0
export ANTHROPIC_BASE_URL=http://127.0.0.1:8080
export ANTHROPIC_AUTH_TOKEN=$OPENAI_API_KEY
export ANTHROPIC_CUSTOM_HEADERS="x-api-key: $ANTHROPIC_AUTH_TOKEN"
bunx @anthropic-ai/claude-code -p "Your prompt here"
```

### 3. Use Any Supported Model

```bash
# Use any Claude model (converted to OpenAI format upstream)
export ANTHROPIC_MODEL='claude-3-7-sonnet-20250219'
./scripts/run_claude_code.sh -p "Your prompt here"

# Use any OpenAI model (converted to OpenAI format upstream)
export ANTHROPIC_MODEL='gpt-4.1'
./scripts/run_claude_code.sh -p "Your prompt here"

# Use any other model - all work automatically
export ANTHROPIC_MODEL='claude-3-5-haiku-20241022'
export ANTHROPIC_MODEL='gpt-4.1-mini'
export ANTHROPIC_MODEL='gpt-4o'
export ANTHROPIC_MODEL='gpt-3.5-turbo'
# Any new model will work without configuration!
```

## 📁 Project Structure & Scripts

The project includes convenient scripts to simplify setup and usage:

### Core Files
- `fastapi_proxy.py` - Main proxy server with universal format conversion
- `simple_proxy.py` - Basic logging proxy for debugging
- `requirements.txt` - Python dependencies

### Convenience Scripts
- `scripts/run_proxy.sh` - Start proxy server with environment validation
- `scripts/run_claude_code.sh` - Run Claude Code with proxy configuration

### Test Scripts
- `scripts/test_openai_format.sh` - Test OpenAI API format
- `scripts/test_claude_format.sh` - Test Claude API format
- `scripts/test_claude_code_proxy.sh` - Basic Claude Code proxy test
- `scripts/test_claude_code_http.sh` - HTTP configuration tests
- `scripts/test_model_routing.sh` - Model routing verification
- `scripts/test_tool_calls.sh` - Tool call functionality tests
- `scripts/test_multi_turn_tool.sh` - Multi-turn tool conversation tests

### Script Features

**`scripts/run_proxy.sh`**:
- ✅ Validates required environment variables
- ✅ Checks Python dependencies
- ✅ Verifies port availability
- ✅ Displays configuration summary
- ✅ Provides helpful error messages

**`scripts/run_claude_code.sh`**:
- ✅ Validates API key
- ✅ Checks proxy server availability
- ✅ Configures Claude Code environment
- ✅ Supports command-line arguments
- ✅ Works with any model

### Usage Examples

```bash
# Start proxy with validation
./scripts/run_proxy.sh

# Run Claude Code with different models
export ANTHROPIC_MODEL='claude-3-7-sonnet-20250219'
./scripts/run_claude_code.sh -p "Hello world"

export ANTHROPIC_MODEL='gpt-4.1'
./scripts/run_claude_code.sh -p "What is AI?"

# Interactive mode
./scripts/run_claude_code.sh

# Run tests
./scripts/test_tool_calls.sh
./scripts/test_multi_turn_tool.sh
```

## ⚙️ Configuration

### Required Environment Variables

- `TARGET_BASE_URL`: The base URL of your OpenAI-compatible upstream server (e.g., `https://api.openai.com`, `https://your-provider.com`)

### Optional Environment Variables

- `OPENAI_API_KEY`: Default API key for the upstream server (can also be provided per-request via `x-api-key` header)
- `LOG_REQUESTS`: Enable/disable request logging (default: `true`)
- `PORT`: Port for the proxy server (default: `8080`)
- `HOST`: Host for the proxy server (default: `0.0.0.0`)

### Example Configuration

```bash
# For OpenAI
export TARGET_BASE_URL="https://api.openai.com"
export OPENAI_API_KEY="sk-your-openai-key"

# For a third-party provider
export TARGET_BASE_URL="https://aihubmix.com"
export OPENAI_API_KEY="your-provider-key"

# Start the proxy (recommended)
./scripts/run_proxy.sh

# Or start manually
python3 fastapi_proxy.py
```

### Flexible API Key Usage

The proxy supports multiple ways to provide API keys:

1. **Environment Variable (Default)**: Set `OPENAI_API_KEY` - used for all requests
2. **Per-Request Header**: Include `x-api-key: your-key` in request headers
3. **No Default Key**: Start proxy without `OPENAI_API_KEY`, provide key in each request

```bash
# Option 1: Start with default key
export OPENAI_API_KEY="your-key"
export TARGET_BASE_URL="https://api.openai.com"
./scripts/run_proxy.sh

# Option 2: Start without default key
export TARGET_BASE_URL="https://api.openai.com"
./scripts/run_proxy.sh

# Then provide key per request:
curl -H "x-api-key: your-key" -H "Content-Type: application/json" \
  http://localhost:8080/v1/messages -d '{"model": "gpt-4", "messages": [...]}'
```

## 📊 Monitoring

View real-time logs:
```bash
# Watch proxy activity
tail -f fastapi_proxy_log.json

# Check proxy status
curl http://localhost:8080/health

# View proxy configuration
curl http://localhost:8080/routes
```

### Log Analysis

The proxy logs show detailed conversion information:
- Original Claude format requests
- Converted OpenAI format requests sent upstream
- Tool call conversions and multi-turn flows
- Response format conversions back to Claude format
- Error details for debugging

## 🎯 What Works

- ✅ **Any Model**: Universal support for Claude, OpenAI, and future models without configuration
- ✅ **Universal Format Conversion**: Always converts Claude format to OpenAI format for upstream
- ✅ **Tool Calls**: Complete support for Claude Code tool calls with proper format conversion
- ✅ **Multi-turn Tool Conversations**: Handles complex tool use → tool result → response flows
- ✅ **Streaming**: Full support for streaming responses from any model
- ✅ **Authentication**: Automatic header conversion (`x-api-key` → `Authorization: Bearer`)
- ✅ **Error Handling**: Proper error propagation and logging
- ✅ **Request Logging**: Complete request/response capture for debugging
- ✅ **Future Proof**: New models work automatically without code changes

## 🔧 Technical Details

The proxy server provides universal format conversion:

1. **Universal Conversion**: Always converts Claude format to OpenAI format for upstream compatibility
2. **Format Translation**: 
   - Incoming: Claude format (`/v1/messages`) → OpenAI format (`/v1/chat/completions`)
   - Headers: `x-api-key` → `Authorization: Bearer`
   - Messages: Claude content blocks → OpenAI message format
   - Tools: Claude tools/tool_choice → OpenAI tools/tool_choice
   - Tool Results: Claude tool_result blocks → OpenAI tool messages
   - Outgoing: OpenAI responses → Claude format for client compatibility
3. **Model Agnostic**: Works with any model without needing configuration or model lists
4. **Tool Call Support**: Complete conversion of tool conversations including multi-turn flows
5. **Streaming Support**: Maintains streaming capabilities for all models
6. **Upstream Flexibility**: Can work with any OpenAI-compatible API provider

This universal approach allows Claude Code to work with **any** OpenAI-compatible upstream server, making it future-proof and highly flexible.

## 🚀 Latest Update: Universal Format Conversion

**Major Improvement**: The proxy now uses a **universal format conversion** approach instead of model-specific routing:

### Before (Model-Specific Routing)
- ❌ Required maintaining a list of supported models
- ❌ New models needed manual configuration
- ❌ Different logic paths for Claude vs OpenAI models
- ❌ Limited to pre-configured model types

### After (Universal Conversion)
- ✅ **Any model works automatically** - no configuration needed
- ✅ **Future-proof** - new models work immediately
- ✅ **Simplified architecture** - single conversion path
- ✅ **Universal upstream compatibility** - works with any OpenAI-compatible API

### How It Works
```
Claude Code (Claude format) 
    ↓
Proxy (always converts to OpenAI format)
    ↓  
Any OpenAI-Compatible Upstream
    ↓
Proxy (converts response back to Claude format)
    ↓
Claude Code (receives expected Claude format)
```

This means you can now use **any model** (including `gpt-4.1-mini`, `claude-3-7-sonnet-20250219`, or future models) without any proxy configuration changes!

## 🔧 Recent Updates

### Tool Call Support (Latest)

**Issue Resolved**: Claude Code was losing the ability to use tool calls when going through the proxy.

**Root Cause**: The format conversion was incomplete - while tools and tool_calls were being converted correctly, tool_result blocks were being converted to regular user messages instead of proper OpenAI tool messages.

**Solution**: Enhanced the message conversion to properly handle tool results:
- Claude `tool_result` blocks → OpenAI `tool` messages with `tool_call_id`
- Maintains proper tool conversation flow for multi-turn interactions
- Preserves all tool functionality while enabling universal upstream compatibility

**Result**: Claude Code now has complete tool call support through any OpenAI-compatible upstream server.

### Security Enhancement

**Issue**: `TARGET_BASE_URL` had a default value, which could accidentally route traffic to unintended servers.

**Solution**: Made `TARGET_BASE_URL` a required environment variable with no default value.

**Result**: Explicit configuration required, preventing accidental traffic routing.
