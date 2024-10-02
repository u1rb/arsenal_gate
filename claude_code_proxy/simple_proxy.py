#!/usr/bin/env python3
"""
Simple HTTP proxy to intercept and log Claude Code traffic
"""
import http.server
import socketserver
import urllib.request
import urllib.parse
import json
import sys
from datetime import datetime

class ProxyHandler(http.server.BaseHTTPRequestHandler):
    def log_request_response(self, method, url, headers, body, response_status, response_headers, response_body):
        timestamp = datetime.now().isoformat()
        log_entry = {
            "timestamp": timestamp,
            "method": method,
            "url": url,
            "request_headers": dict(headers),
            "request_body": body.decode('utf-8', errors='ignore') if body else None,
            "response_status": response_status,
            "response_headers": dict(response_headers),
            "response_body": response_body.decode('utf-8', errors='ignore') if response_body else None
        }
        
        print(f"\n{'='*80}")
        print(f"[{timestamp}] {method} {url}")
        print(f"{'='*80}")
        print("REQUEST HEADERS:")
        for key, value in headers:
            print(f"  {key}: {value}")
        
        if body:
            print("\nREQUEST BODY:")
            try:
                parsed_body = json.loads(body.decode('utf-8'))
                print(json.dumps(parsed_body, indent=2))
            except:
                print(body.decode('utf-8', errors='ignore'))
        
        print(f"\nRESPONSE STATUS: {response_status}")
        print("RESPONSE HEADERS:")
        for key, value in response_headers:
            print(f"  {key}: {value}")
        
        if response_body:
            print("\nRESPONSE BODY:")
            try:
                parsed_response = json.loads(response_body.decode('utf-8'))
                print(json.dumps(parsed_response, indent=2))
            except:
                print(response_body.decode('utf-8', errors='ignore'))
        
        # Also save to file
        with open('proxy_log.json', 'a') as f:
            f.write(json.dumps(log_entry) + '\n')

    def do_GET(self):
        self.proxy_request()

    def do_POST(self):
        self.proxy_request()

    def do_PUT(self):
        self.proxy_request()

    def do_DELETE(self):
        self.proxy_request()

    def proxy_request(self):
        # Read request body
        content_length = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(content_length) if content_length > 0 else b''
        
        # Build target URL - forward to aihubmix.com
        target_url = f"https://aihubmix.com{self.path}"
        
        # Create request
        req = urllib.request.Request(target_url, data=body, method=self.command)
        
        # Copy headers (except Host)
        for key, value in self.headers.items():
            if key.lower() not in ['host', 'connection']:
                req.add_header(key, value)
        
        try:
            # Make the request
            with urllib.request.urlopen(req) as response:
                response_body = response.read()
                response_headers = list(response.headers.items())
                
                # Log the request and response
                self.log_request_response(
                    self.command, target_url, list(self.headers.items()), 
                    body, response.status, response_headers, response_body
                )
                
                # Send response back to client
                self.send_response(response.status)
                for key, value in response_headers:
                    if key.lower() not in ['connection', 'transfer-encoding']:
                        self.send_header(key, value)
                self.end_headers()
                self.wfile.write(response_body)
                
        except Exception as e:
            print(f"Error proxying request: {e}")
            self.send_error(500, f"Proxy error: {e}")

if __name__ == "__main__":
    PORT = 8080
    print(f"Starting proxy server on port {PORT}")
    print(f"Forwarding requests to https://aihubmix.com")
    print(f"Logs will be saved to proxy_log.json")
    
    with socketserver.TCPServer(("", PORT), ProxyHandler) as httpd:
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nShutting down proxy server...") 