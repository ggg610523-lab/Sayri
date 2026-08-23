import json
from http.server import BaseHTTPRequestHandler, HTTPServer

class H(BaseHTTPRequestHandler):
    def do_POST(self):
        ln = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(ln)
        print("MOCK GOT BODY:", body.decode(), flush=True)
        resp = json.dumps({
            "model": "mock",
            "message": {"role": "assistant",
                        "content": "Hello! Quotes \"ok\", back\\\\slash, unicode \\u00e9\\u2713 and\\nnewline."},
            "done": True
        }).encode()
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(resp)))
        self.end_headers()
        self.wfile.write(resp)
    def log_message(self, *a):
        pass

HTTPServer(('127.0.0.1', 11500), H).serve_forever()
