#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""地球の陸の細かい標高 (assets/earth_detail.bin) を NOAA の ETOPO 2022 から作る。

データ: NOAA NCEI ETOPO 2022 (60 秒角, 氷床表面。21600x10800、1 画素 約 1.85 km)。米国政府の著作物で再配布自由。
全球の標高（earth_data.png の R、4096x2048）は粗いので、地表の近くではカメラの周りだけこちらを使う
（富士山の山頂は earth_data.png で約 1,700 m、こちらで約 3,400 m）。

出力形式（リトルエンディアン）:
  char[4] "EDT1", int32 列の数 (36), 行の数 (18), 1 区画の画素数 (600)
  区画ごと（北の行から、各行は経度 -180° から東へ）に uint32 位置（ファイルの先頭から）, uint32 大きさ（0 は陸の無い区画）
  区画の 600x600 の 8 bit グレーの PNG。値は earth_data.png の R と同じ高さコード（陸 128..255 = 0..8900 m を平方根で詰める）。
  海は 127（海面）。並びは正距円筒図法（左端が経度 -180°、上端が北緯 90°、画素の中心が 1 分ごとの格子点）

使い方: python3 tools/make_earth_detail.py <作業ディレクトリ> [出力先=assets]
必要なもの: Pillow と NumPy（ダウンロード 約 930 MB）
"""
import io
import os
import struct
import sys
import urllib.request

import numpy as np
from PIL import Image

# 区画の 1 行（600 行 = 10°）ずつ取得する（全体を 1 回で頼むよりずっと速い）。南が先頭の行番号 a..b
ETOPO_URL = ("https://oceanwatch.pifsc.noaa.gov/erddap/griddap/ETOPO_2022_v1_60s.dods"
             "?z%%5B%d:1:%d%%5D%%5B0:1:21599%%5D")
ETOPO_FILE = "etopo2022_60s_rows%05d.dods"
W, H = 21600, 10800
TILE = 600
LAND_MAX = 8900.0  # 高さコードの範囲 (m)。make_earth_assets.py・シェーダの earthHeightM と合わせる


def fetch(work, url, name):
    path = os.path.join(work, name)
    if not os.path.exists(path):
        print("download", url)
        urllib.request.urlretrieve(url, path + ".part")
        os.rename(path + ".part", path)
    return path


def load_band(work, r):
    """北から r 番目の区画の行の標高 (TILE x W、北が上・各行は経度 0° から。m)"""
    a = H - (r + 1) * TILE   # 元のデータは南が先頭
    path = fetch(work, ETOPO_URL % (a, a + TILE - 1), ETOPO_FILE % a)
    with open(path, "rb") as f:
        raw = f.read()
    head, sep, body = raw.partition(b"\nData:\n")
    if not sep:
        raise RuntimeError("unexpected ETOPO response: " + raw[:200].decode("latin-1"))
    n = struct.unpack(">I", body[:4])[0]
    assert n == TILE * W, n
    return np.frombuffer(body[8:8 + n * 4], ">f4").reshape(TILE, W)[::-1]


def main():
    work = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else "assets"
    os.makedirs(work, exist_ok=True)
    os.makedirs(out, exist_ok=True)
    cols, rows = W // TILE, H // TILE
    tiles = []
    for r in range(rows):
        src = load_band(work, r)
        for c in range(cols):
            # 元の経度の並び（make_earth_assets.py と同じく半周ずらすと -180° が左端）
            x0 = (c * TILE + W // 2) % W
            e = np.asarray(src[:, x0:x0 + TILE], dtype=np.float64)
            if not (e >= 0).any():
                tiles.append(b"")
                continue
            code = np.where(e >= 0, 128 + np.sqrt(np.minimum(e, LAND_MAX) / LAND_MAX) * 127 + 0.5, 127).astype(np.uint8)
            buf = io.BytesIO()
            Image.fromarray(code, "L").save(buf, "PNG", optimize=True)
            tiles.append(buf.getvalue())
    pos = 16 + 8 * len(tiles)
    index = b""
    for t in tiles:
        index += struct.pack("<II", pos if t else 0, len(t))
        pos += len(t)
    with open(os.path.join(out, "earth_detail.bin"), "wb") as f:
        f.write(b"EDT1" + struct.pack("<iii", cols, rows, TILE) + index + b"".join(tiles))
    print("done:", sum(1 for t in tiles if t), "tiles,", pos, "bytes")


if __name__ == "__main__":
    main()
