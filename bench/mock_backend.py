import sys
import json
import time
import random
import argparse
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from collections import OrderedDict

class LRUCache:
    def __init__(self, capacity: int):
        self.cache = OrderedDict()
        self.capacity = capacity

    def get(self, key):
        if key not in self.cache:
            return -1
        self.cache.move_to_end(key)
        return self.cache[key]

    def put(self, key, value):
        self.cache[key] = value
        self.cache.move_to_end(key)
        if len(self.cache) > self.capacity:
            self.cache.popitem(last=False)

# Global prefix cache
prefix_cache = LRUCache(1000)

class MockBackendHandler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def do_POST(self):
        if self.path == '/v1/completions':
            self.handle_completions()
        else:
            self.send_error(404, "Not Found")

    def handle_completions(self):
        content_length = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(content_length).decode('utf-8')
        
        try:
            req_data = json.loads(body)
        except json.JSONDecodeError:
            self.send_error(400, "Bad Request")
            return
            
        prompt = req_data.get("prompt", "")
        max_tokens = req_data.get("max_tokens", 50)
        stream = req_data.get("stream", False)
        
        # Determine prefix cache hit based on first 512 chars
        prefix = prompt[:512]
        is_hit = prefix_cache.get(prefix) != -1
        
        if is_hit:
            ttft = random.uniform(0.020, 0.050) # 20-50ms
        else:
            ttft = random.uniform(0.200, 0.500) # 200-500ms
            prefix_cache.put(prefix, True)
            
        time.sleep(ttft)
        
        if stream:
            self.send_response(200)
            self.send_header('Content-Type', 'text/event-stream')
            self.send_header('Cache-Control', 'no-cache')
            self.send_header('Connection', 'keep-alive')
            self.end_headers()
            
            for i in range(max_tokens):
                inter_token_delay = random.uniform(0.020, 0.040) # 20-40ms
                time.sleep(inter_token_delay)
                
                chunk = json.dumps({
                    "id": "cmpl-mock",
                    "object": "text_completion",
                    "choices": [{"text": f" token_{i}", "index": 0, "finish_reason": None}]
                })
                
                try:
                    self.wfile.write(f"data: {chunk}\n\n".encode('utf-8'))
                    self.wfile.flush()
                except BrokenPipeError:
                    # Client disconnected mid-stream
                    return
                    
            try:
                self.wfile.write(b"data: [DONE]\n\n")
                self.wfile.flush()
            except BrokenPipeError:
                pass
        else:
            # Simulate non-streaming
            total_delay = sum(random.uniform(0.020, 0.040) for _ in range(max_tokens))
            time.sleep(total_delay)
            
            resp = {
                "id": "cmpl-mock",
                "object": "text_completion",
                "choices": [{"text": " ".join([f"token_{i}" for i in range(max_tokens)]), "index": 0, "finish_reason": "length"}]
            }
            resp_body = json.dumps(resp).encode('utf-8')
            
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Content-Length', str(len(resp_body)))
            self.end_headers()
            self.wfile.write(resp_body)

def run(port=9001):
    server_address = ('', port)
    httpd = ThreadingHTTPServer(server_address, MockBackendHandler)
    print(f"Mock backend listening on port {port}...")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    httpd.server_close()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Mock LLM Backend")
    parser.add_argument("--port", type=int, default=9001, help="Port to listen on")
    args = parser.parse_args()
    run(port=args.port)
