#!/usr/bin/env python3
"""
HTTPS FastAPI proxy server for Claude Code
"""
import ssl
import uvicorn
from fastapi_proxy import app

if __name__ == "__main__":
    # Create SSL context with self-signed certificate
    ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    
    # Generate self-signed certificate (for testing only)
    import subprocess
    import os
    
    if not os.path.exists("server.crt") or not os.path.exists("server.key"):
        print("Generating self-signed certificate...")
        subprocess.run([
            "openssl", "req", "-x509", "-newkey", "rsa:4096", "-keyout", "server.key",
            "-out", "server.crt", "-days", "365", "-nodes", "-subj",
            "/C=US/ST=State/L=City/O=Organization/CN=localhost"
        ], check=True)
    
    ssl_context.load_cert_chain("server.crt", "server.key")
    
    port = 8443
    host = "0.0.0.0"
    
    print(f"Starting HTTPS FastAPI proxy server on {host}:{port}")
    print(f"Target base URL: https://aihubmix.com")
    
    uvicorn.run(app, host=host, port=port, ssl_keyfile="server.key", ssl_certfile="server.crt") 