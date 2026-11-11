#!/usr/bin/env python3
"""Mock Pixoo 64 for the host test (and for trying PixooCtl in an emulator).

Answers POST /post like the device: a JSON command in, {"error_code": 0}
(plus data for Channel/GetAllConf) out. Every command is logged as one
deterministic line; the last picture of Draw/SendHttpGif is written as PPM.

Like the real device (checked with a Pixoo 64), the answer is sent with
"Transfer-Encoding: chunked", without Content-Length, and the connection
is kept open although the client asked to close it: the client must stop
reading at the last, empty chunk.

Special commands for tests: "Test/Fail" answers error_code 1,
"Test/Big" answers a body of 4000 bytes in chunks of 1000 bytes,
"Test/Plain" answers with Content-Length instead of chunks.

Usage: mockpixoo.py [--port 18080] [--log FILE] [--ppm FILE] [--bind 0.0.0.0]
"""
import argparse
import base64
import json
import sys
import zlib
from http.server import BaseHTTPRequestHandler, HTTPServer

STATE = {
    "Brightness": 100, "RotationFlag": 0, "ClockTime": 60, "GalleryTime": 60,
    "SingleGalleyTime": -1, "PowerOnChannelId": 1, "GalleryShowTimeFlag": 0,
    "CurClockId": 182, "Time24Flag": 1, "TemperatureMode": 0, "GyrateAngle": 0,
    "MirrorFlag": 0, "LightSwitch": 1, "SelectIndex": 0,
}


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    server_version = "MockPixoo/1.0"

    def log_message(self, fmt, *args):  # quiet
        pass

    def log_cmd(self, line):
        self.server.logfile.write(line + "\n")
        self.server.logfile.flush()

    def answer(self, obj, status=200, plain=False, chunk=0):
        body = json.dumps(obj).encode()
        self.send_response(status)
        self.send_header("Content-Type", "text/html")
        if plain:
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        else:
            self.send_header("Transfer-Encoding", "chunked")
            self.end_headers()
            chunk = chunk or len(body)
            for i in range(0, len(body), chunk):
                part = body[i:i + chunk]
                self.wfile.write(b"%x\r\n" % len(part) + part + b"\r\n")
            self.wfile.write(b"0\r\n\r\n")
        self.wfile.flush()
        self.close_connection = False   # see module comment

    def do_POST(self):
        if self.path != "/post":
            self.log_cmd("BAD PATH " + self.path)
            self.answer({"error": "not found"}, 404)
            return
        n = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(n)
        if self.headers.get("Host") is None:
            self.log_cmd("NO HOST HEADER")
        try:
            cmd = json.loads(raw)
        except ValueError:
            self.log_cmd("BAD JSON %r" % raw[:60])
            self.answer({"error_code": "request data illegal json"})
            return
        name = cmd.get("Command", "?")
        params = []
        for k in sorted(cmd):
            if k == "Command":
                continue
            v = cmd[k]
            if k == "PicData":
                data = base64.b64decode(v, validate=True)
                params.append("PicData=%d bytes crc %08x" % (len(data), zlib.crc32(data)))
                width = cmd.get("PicWidth", 64)
                if self.server.ppm and len(data) == width * width * 3:
                    with open(self.server.ppm, "wb") as f:
                        f.write(b"P6\n%d %d\n255\n" % (width, width) + data)
            else:
                params.append("%s=%s" % (k, json.dumps(v)))
        self.log_cmd(" ".join([name] + params))

        reply = {"error_code": 0}
        if name == "Channel/SetBrightness":
            STATE["Brightness"] = cmd.get("Brightness", 0)
        elif name == "Channel/OnOffScreen":
            STATE["LightSwitch"] = cmd.get("OnOff", 1)
        elif name == "Channel/SetIndex":
            STATE["SelectIndex"] = cmd.get("SelectIndex", 0)
        elif name == "Channel/SetClockSelectId":
            STATE["CurClockId"] = cmd.get("ClockId", 0)
        elif name == "Channel/GetAllConf":
            reply.update(STATE)
        elif name == "Draw/GetHttpGifId":
            reply["PicId"] = 1
        elif name == "Test/Fail":
            reply["error_code"] = 1
        elif name == "Test/Big":
            reply["data"] = "x" * 4000
            self.answer(reply, chunk=1000)
            return
        elif name == "Test/Plain":
            self.answer(reply, plain=True)
            return
        self.answer(reply)


def main():
    ap = argparse.ArgumentParser(description="Mock Pixoo 64 HTTP server")
    ap.add_argument("--port", type=int, default=18080)
    ap.add_argument("--bind", default="127.0.0.1")
    ap.add_argument("--log", help="file for the command log (default: stdout)")
    ap.add_argument("--ppm", help="write the last picture to this PPM file")
    a = ap.parse_args()
    srv = HTTPServer((a.bind, a.port), Handler)
    srv.logfile = open(a.log, "w") if a.log else sys.stdout
    srv.ppm = a.ppm
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
