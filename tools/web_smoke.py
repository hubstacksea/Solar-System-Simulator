#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""ブラウザ版をヘッドレスの Chrome で開いて、スクリーンショットとコンソールの出力を保存する（動作確認用）。

Chrome の DevTools プロトコルを標準ライブラリだけで話す（WebSocket も自前）。WebGL2 はソフトウェア描画
（SwiftShader）で動かす。各 URL について、起動（raylib の初期化のログ）を待ってから指定の秒数ごとに画面を撮る。
起動しなかった URL や、ページで捕まえられなかった例外があれば終了コード 1。

使い方: python3 tools/web_smoke.py <出力ディレクトリ> <ページの URL> [<クエリ> ...]
  例: python3 tools/web_smoke.py shots-web http://localhost:8000/ "" "?voyage=BepiColombo&scene=arrive"
環境変数 CHROME で Chrome の実行ファイルを指定できる（既定は google-chrome）
"""
import base64
import json
import os
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import time
import urllib.request

SHOT_AT = (20, 40)          # 起動から何秒後に撮るか
BOOT_TIMEOUT = 120          # 起動を待つ最長の秒数


class WebSocket:
    def __init__(self, url):
        assert url.startswith("ws://")
        hostport, path = url[5:].split("/", 1)
        host, port = hostport.split(":")
        self.sock = socket.create_connection((host, int(port)), timeout=30)
        key = base64.b64encode(os.urandom(16)).decode()
        self.sock.sendall(("GET /%s HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                           "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n" % (path, hostport, key)).encode())
        head = b""
        while b"\r\n\r\n" not in head:
            head += self.sock.recv(1)
        if b" 101 " not in head.split(b"\r\n")[0]:
            raise RuntimeError(head.decode(errors="replace"))
        self.buf = b""

    def send(self, text):
        data = text.encode()
        mask = os.urandom(4)
        n = len(data)
        hdr = bytes([0x81]) + (bytes([0x80 | n]) if n < 126 else bytes([0x80 | 126]) + struct.pack(">H", n) if n < 65536
                                else bytes([0x80 | 127]) + struct.pack(">Q", n))
        self.sock.sendall(hdr + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(data)))

    def _read(self, n):
        while len(self.buf) < n:
            chunk = self.sock.recv(1 << 16)
            if not chunk:
                raise ConnectionError("closed")
            self.buf += chunk
        out, self.buf = self.buf[:n], self.buf[n:]
        return out

    def recv(self, timeout):
        """1 つのメッセージ（テキスト）。timeout 秒で None"""
        self.sock.settimeout(timeout)
        try:
            msg = b""
            while True:
                b0, b1 = self._read(2)
                n = b1 & 0x7F
                if n == 126:
                    n = struct.unpack(">H", self._read(2))[0]
                elif n == 127:
                    n = struct.unpack(">Q", self._read(8))[0]
                payload = self._read(n)
                if b0 & 0x0F == 9:   # ping
                    continue
                msg += payload
                if b0 & 0x80:
                    return msg.decode(errors="replace")
        except socket.timeout:
            return None


def main():
    out, base, queries = sys.argv[1], sys.argv[2], sys.argv[3:] or [""]
    os.makedirs(out, exist_ok=True)
    chrome = os.environ.get("CHROME") or shutil.which("google-chrome") or shutil.which("chromium")
    prof = tempfile.mkdtemp()
    port = 9300 + os.getpid() % 500
    proc = subprocess.Popen([chrome, "--headless=new", "--no-sandbox", "--use-angle=swiftshader", "--enable-unsafe-swiftshader",
                             "--ignore-gpu-blocklist", "--window-size=1280,720", "--remote-debugging-port=%d" % port,
                             "--user-data-dir=" + prof, "about:blank"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    failed = 0
    try:
        for _ in range(100):
            try:
                pages = json.load(urllib.request.urlopen("http://127.0.0.1:%d/json" % port, timeout=2))
                if any(p["type"] == "page" for p in pages):
                    break
            except OSError:
                pass
            time.sleep(0.2)
        ws = WebSocket(next(p for p in pages if p["type"] == "page")["webSocketDebuggerUrl"])
        mid = [0]

        def call(method, **params):
            mid[0] += 1
            ws.send(json.dumps({"id": mid[0], "method": method, "params": params}))
            return mid[0]

        for k, q in enumerate(queries):
            name = "page%d" % k
            log = []
            call("Runtime.enable")
            call("Log.enable")
            call("Page.enable")
            call("Page.navigate", url=base + q)
            t0 = time.time()
            booted = None
            shots = list(SHOT_AT)
            pending = {}
            errors = 0
            while time.time() - t0 < BOOT_TIMEOUT + max(SHOT_AT) + 10:
                if booted is not None and shots and time.time() - booted >= shots[0]:
                    pending[call("Page.captureScreenshot", format="png")] = "%s_%ds.png" % (name, shots.pop(0))
                if booted is not None and not shots and not pending:
                    break
                if booted is None and time.time() - t0 > BOOT_TIMEOUT:
                    break
                m = ws.recv(0.5)
                if m is None:
                    continue
                msg = json.loads(m)
                if msg.get("id") in pending:
                    with open(os.path.join(out, pending.pop(msg["id"])), "wb") as f:
                        f.write(base64.b64decode(msg["result"]["data"]))
                    continue
                meth = msg.get("method", "")
                p = msg.get("params", {})
                if meth == "Runtime.consoleAPICalled":
                    text = " ".join(str(a.get("value", a.get("description", ""))) for a in p.get("args", []))
                    log.append("[%s] %s" % (p.get("type"), text))
                    if booted is None and "Initializing raylib" in text:
                        booted = time.time()
                elif meth == "Runtime.exceptionThrown":
                    d = p.get("exceptionDetails", {})
                    log.append("[exception] %s %s" % (d.get("text", ""), d.get("exception", {}).get("description", "")))
                    errors += 1
                elif meth == "Log.entryAdded":
                    e = p.get("entry", {})
                    log.append("[%s] %s %s" % (e.get("level"), e.get("text"), e.get("url", "")))
            with open(os.path.join(out, name + ".log"), "w", encoding="utf-8") as f:
                f.write("URL: %s\n" % (base + q) + "\n".join(log) + "\n")
            ok = booted is not None and errors == 0
            failed += not ok
            print("%-8s %s  booted=%s  exceptions=%d  %s" % (name, "OK" if ok else "FAIL", booted is not None, errors, base + q),
                  flush=True)
    finally:
        proc.terminate()
        try:
            proc.wait(10)
        except subprocess.TimeoutExpired:
            proc.kill()
        shutil.rmtree(prof, ignore_errors=True)
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
