#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""月と火星の細かい標高 (assets/moon_detail.bin, assets/mars_detail.bin) を作る。

データ:
  - 月: NASA's Scientific Visualization Studio, CGI Moon Kit (https://svs.gsfc.nasa.gov/4720) の
    ldem_16_uint.tif（LRO レーザー高度計 LOLA。16 ピクセル/度 = 5760x2880、1 画素 約 1.9 km。0.5 m 単位、半径 1727.4 km 基準）
  - 火星: NASA PDS, MGS MOLA MEGDR の megt90n000eb.img（16 ピクセル/度 = 5760x2880、1 画素 約 3.7 km。m 単位）
全球の標高（moon_data.png 2048x1024・maps.bin の火星 1440x720）は粗いので、地表の近くではカメラの周りだけこちらを使う。

出力形式（リトルエンディアン）:
  char[4] "EDT2", int32 列の数 (16), 行の数 (8), 1 区画の画素数 (360)
  区画ごと（北の行から、各行は経度 -180° から東へ）に uint32 位置（ファイルの先頭から）, uint32 大きさ
  区画: 値 v（0.5 m 単位、-10000 m から。標高 = v × 0.5 − 10000 m。月は半径 1737.4 km、火星は MOLA の基準面から）を、
  左 a・上 b・左上 c の値からの予測（a, b, a + b − c の中央値。先頭の行は左、先頭の列は上、区画の左上は先頭の uint16 の値）との
  差にし、zigzag（0, −1, 1, −2 … を 0, 1, 2, 3 …）にした 16 bit の値の上位バイトの面・下位バイトの面を続けて、
  deflate（zlib の見出しなし）で圧縮したもの。区画のデータは uint16 左上の値 + 圧縮したもの
  並びは正距円筒図法（左端が経度 -180°、上端が北緯 90°）

使い方: python3 tools/make_body_detail.py <作業ディレクトリ> [出力先=assets]
必要なもの: Pillow と NumPy（ダウンロード 約 66 MB）
"""
import os
import struct
import sys
import urllib.request
import zlib

import numpy as np
from PIL import Image

MOON_URL = "https://svs.gsfc.nasa.gov/vis/a000000/a004700/a004720/ldem_16_uint.tif"
MARS_URL = "https://pds-geosciences.wustl.edu/mgs/mgs-m-mola-5-megdr-l3-v1/mgsl_300x/meg016/megt90n000eb.img"
W, H, TILE = 5760, 2880, 360


def fetch(work, url):
    path = os.path.join(work, os.path.basename(url))
    if not os.path.exists(path):
        print("download", url)
        urllib.request.urlretrieve(url, path + ".part")
        os.rename(path + ".part", path)
    return path


def encode_tile(t):
    t = t.astype(np.int64)
    p = np.zeros_like(t)
    a, b, c = t[1:, :-1], t[:-1, 1:], t[:-1, :-1]
    p[1:, 1:] = np.maximum(np.minimum(a, b), np.minimum(np.maximum(a, b), a + b - c))   # 3 つの中央値
    p[0, 1:] = t[0, :-1]
    p[1:, 0] = t[:-1, 0]
    p[0, 0] = t[0, 0]
    r = t - p
    z = np.where(r >= 0, 2 * r, -2 * r - 1)
    assert z.max() < 65536
    z = z.astype(np.uint16)
    c = zlib.compressobj(9, zlib.DEFLATED, -15)
    return struct.pack("<H", int(t[0, 0])) + c.compress((z >> 8).astype(np.uint8).tobytes() + (z & 255).astype(np.uint8).tobytes()) + c.flush()


def write(path, v):
    assert v.shape == (H, W) and v.min() >= 0 and v.max() < 65536
    cols, rows = W // TILE, H // TILE
    tiles = [encode_tile(v[r * TILE:(r + 1) * TILE, c * TILE:(c + 1) * TILE]) for r in range(rows) for c in range(cols)]
    pos = 16 + 8 * len(tiles)
    index = b""
    for t in tiles:
        index += struct.pack("<II", pos, len(t))
        pos += len(t)
    with open(path, "wb") as f:
        f.write(b"EDT2" + struct.pack("<iii", cols, rows, TILE) + index + b"".join(tiles))
    print("done:", path, pos, "bytes")


def main():
    work = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else "assets"
    os.makedirs(work, exist_ok=True)
    os.makedirs(out, exist_ok=True)
    Image.MAX_IMAGE_PIXELS = None
    # 月: 0.5 m 単位・半径 1727.4 km 基準 = 1737.4 km から −10000 m（moon_data.png と同じ）。左端が経度 -180°
    moon = np.asarray(Image.open(fetch(work, MOON_URL))).astype(np.int64)
    write(os.path.join(out, "moon_detail.bin"), moon)
    # 火星: m・経度 0〜360°E・上端が北緯 90° → 左端を経度 -180° に、0.5 m 単位で −10000 m から（maps.bin と同じ）
    mars = np.fromfile(fetch(work, MARS_URL), dtype=">i2").reshape(H, W).astype(np.int64)
    mars = np.roll(mars, W // 2, axis=1)
    write(os.path.join(out, "mars_detail.bin"), np.clip((mars + 10000) * 2, 0, 65535))


if __name__ == "__main__":
    main()
