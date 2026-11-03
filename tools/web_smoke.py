#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""ブラウザ版をヘッドレスの Chrome で開いて、スクリーンショットとコンソールの出力を保存する（動作確認用）。

Chrome の DevTools プロトコルを標準ライブラリだけで話す（WebSocket も自前）。WebGL2 はソフトウェア描画
（SwiftShader）で動かす。各 URL について、起動（raylib の初期化のログ）を待ってから指定の秒数ごとに画面を撮る。
起動しなかった URL や、ページで捕まえられなかった例外があれば終了コード 1。

使い方: python3 tools/web_smoke.py <出力ディレクトリ> <ページの URL> [<クエリ>[|<操作>] ...]
  例: python3 tools/web_smoke.py shots-web http://localhost:8000/ "" "?voyage=BepiColombo&scene=arrive"
操作（クエリの後ろに | で区切って ; でつなぐ）を書くと、決まった秒数ごとに撮る代わりに、起動してから順に行う:
  wait:<秒>  shot:<名前>  js:<JavaScript（; は使えない）>  key:<キー>（Enter / BracketRight / BracketLeft / KeyP / F3 など）
  click:<x>,<y>  drag:<x1>,<y1>,<x2>,<y2>（x は 1 以下なら画面の幅に対する割合、y は負なら下端からのピクセル）
  例: "?voyage=BepiColombo&scene=7|wait:15;shot:before;click:0.5,-41;wait:3;shot:after"
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
# キー・ボタンは、押してからページが 2 フレーム描くのを待って離す（アプリは毎フレームの押下の変化で判定する。
# ヘッドレスの Chrome はソフトウェア描画で 1 フレームが 1 秒近くかかり、時間で待つと押す・離すが同じフレームに入る）
HOLD_FRAMES = 2
BOOT_TIMEOUT = 120          # 起動を待つ最長の秒数


# キー: (key, code, 仮想キーコード, 入力される文字)
KEYS = {"Enter": ("Enter", "Enter", 13, "\r"), "BracketRight": ("]", "BracketRight", 221, "]"),
        "BracketLeft": ("[", "BracketLeft", 219, "["), "KeyP": ("p", "KeyP", 80, "p"), "F3": ("F3", "F3", 114, ""),
        "Escape": ("Escape", "Escape", 27, ""), "ArrowLeft": ("ArrowLeft", "ArrowLeft", 37, ""),
        "ArrowRight": ("ArrowRight", "ArrowRight", 39, "")}


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

        def evaluate(expr):
            return call("Runtime.evaluate", expression=expr, returnByValue=True)

        backlog = []   # フレームを待つ間に届いた、ほかのメッセージ

        def wait_reply(i):
            """呼び出し i の結果が届くまで待つ（その間に届いたほかのメッセージは後で処理する）"""
            while True:
                m = ws.recv(60)
                if m is None:
                    return None
                msg = json.loads(m)
                if msg.get("id") == i:
                    return msg
                backlog.append(m)

        def wait_frames(n):
            """ページが n フレーム描くまで待つ"""
            wait_reply(call("Runtime.evaluate", awaitPromise=True, returnByValue=True,
                            expression="new Promise(r => { let k = %d; const f = () => (--k > 0 ? requestAnimationFrame(f) : r(1)); "
                                       "requestAnimationFrame(f); })" % n))

        for k, spec in enumerate(queries):
            q, _, script = spec.partition("|")
            actions = [a for a in script.split(";") if a] if script else []
            name = "page%d" % k
            log = []
            call("Runtime.enable")
            call("Log.enable")
            call("Page.enable")
            call("Page.navigate", url=base + q)
            t0 = time.time()
            booted = None
            shots = [] if actions else list(SHOT_AT)
            pending = {}
            size_id = None
            size = (1280, 720)
            next_at = None   # 次の操作を行う時刻
            errors = 0
            limit = BOOT_TIMEOUT + max(SHOT_AT) + 10 + sum(float(a[5:]) for a in actions if a.startswith("wait:"))
            while time.time() - t0 < limit:
                if booted is not None and shots and time.time() - booted >= shots[0]:
                    pending[call("Page.captureScreenshot", format="png")] = "%s_%ds.png" % (name, shots.pop(0))
                if booted is not None and actions and size_id is None:
                    size_id = evaluate("[window.innerWidth, window.innerHeight]")
                    next_at = time.time()
                while booted is not None and actions and next_at is not None and time.time() >= next_at:
                    kind, _, arg = actions.pop(0).partition(":")
                    log.append("[action] %s:%s" % (kind, arg))
                    nums = [float(x) for x in arg.split(",")] if kind in ("click", "drag") else []
                    px = lambda x: x * size[0] if x <= 1 else x
                    py = lambda y: size[1] + y if y < 0 else y
                    if kind == "wait":
                        next_at = time.time() + float(arg)
                    elif kind == "js":   # ページで JavaScript を実行する（確認用）
                        evaluate(arg)
                    elif kind == "shot":   # 撮り終わるまで次の操作に進まない
                        msg = wait_reply(call("Page.captureScreenshot", format="png"))
                        if msg and "result" in msg:
                            with open(os.path.join(out, "%s_%s.png" % (name, arg)), "wb") as f:
                                f.write(base64.b64decode(msg["result"]["data"]))
                    elif kind == "key":
                        key, code, vk, text = KEYS[arg]
                        # 文字の出るキーは keyDown（text 付き）、出ないキーは rawKeyDown（Puppeteer と同じ）
                        down = dict(type="keyDown", text=text) if text else dict(type="rawKeyDown")
                        call("Input.dispatchKeyEvent", key=key, code=code, windowsVirtualKeyCode=vk, nativeVirtualKeyCode=vk, **down)
                        wait_frames(HOLD_FRAMES)
                        call("Input.dispatchKeyEvent", type="keyUp", key=key, code=code, windowsVirtualKeyCode=vk, nativeVirtualKeyCode=vk)
                    elif kind == "click":
                        x, y = px(nums[0]), py(nums[1])
                        call("Input.dispatchMouseEvent", type="mouseMoved", x=x, y=y)
                        for t in ("mousePressed", "mouseReleased"):
                            call("Input.dispatchMouseEvent", type=t, x=x, y=y, button="left", clickCount=1)
                            if t == "mousePressed":
                                wait_frames(HOLD_FRAMES)
                    elif kind == "drag":   # 5 回に分けて、1 フレームずつ動かす
                        x1, y1, x2, y2 = px(nums[0]), py(nums[1]), px(nums[2]), py(nums[3])
                        call("Input.dispatchMouseEvent", type="mousePressed", x=x1, y=y1, button="left", clickCount=1)
                        wait_frames(HOLD_FRAMES)
                        for i in range(1, 6):
                            call("Input.dispatchMouseEvent", type="mouseMoved", x=x1 + (x2 - x1) * i / 5,
                                 y=y1 + (y2 - y1) * i / 5, button="left", buttons=1)
                            wait_frames(1)
                        call("Input.dispatchMouseEvent", type="mouseReleased", x=x2, y=y2, button="left", clickCount=1)
                if booted is not None and not shots and not actions and not pending:
                    break
                if booted is None and time.time() - t0 > BOOT_TIMEOUT:
                    break
                m = backlog.pop(0) if backlog else ws.recv(0.5)
                if m is None:
                    continue
                msg = json.loads(m)
                if msg.get("id") == size_id and size_id is not None:
                    size = tuple(msg["result"]["result"]["value"])
                    continue
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
            ok = booted is not None and errors == 0 and not actions
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
