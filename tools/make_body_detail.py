#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""月と火星の細かい標高 (assets/moon_detail.bin, assets/mars_detail.bin + mars_detail2.bin) を作る。

データ:
  - 月: NASA's Scientific Visualization Studio, CGI Moon Kit (https://svs.gsfc.nasa.gov/4720) の
    ldem_16_uint.tif（LRO レーザー高度計 LOLA。16 ピクセル/度 = 5760x2880、1 画素 約 1.9 km。0.5 m 単位、半径 1727.4 km 基準）
  - 火星: USGS Astrogeology の Mars MGS MOLA - MEX HRSC Blended DEM Global 200m v2（Fergason ほか 2018。パブリックドメイン）
    Mars_HRSC_MOLA_BlendDEM_Global_200mp_v2.tif（MOLA に HRSC の立体視の標高を合わせたもの。296.4 ピクセル/度 = 106694x53347、
    1 画素 200 m。m 単位、MOLA の基準面から。11.4 GB）を 32 ピクセル/度 = 11520x5760（1 画素 約 1.85 km）に面積で平均して縮める
全球の標高（moon_data.png 2048x1024・maps.bin の火星 1440x720）は粗いので、地表の近くではカメラの周りだけこちらを使う。

出力形式（リトルエンディアン）:
  char[4] "EDT2", int32 列の数 (16), 行の数 (8), 1 区画の画素数 (360)
  区画ごと（北の行から、各行は経度 -180° から東へ）に uint32 位置（ファイルの先頭から）, uint32 大きさ
  火星は GitHub の 1 ファイルの上限（100 MB。50 MB から警告）に余裕を持たせるため 2 つに分ける: mars_detail.bin は見出し・索引と
  前半の区画、mars_detail2.bin は残りの区画だけ（区画の行の境目で、大きさがほぼ半分ずつになる所）。位置は 2 つをつないだものの先頭から
  区画: 値 v（0.5 m 単位、-10000 m から。標高 = v × 0.5 − 10000 m。月は半径 1737.4 km、火星は MOLA の基準面から）を、
  左 a・上 b・左上 c の値からの予測（a, b, a + b − c の中央値。先頭の行は左、先頭の列は上、区画の左上は先頭の uint16 の値）との
  差にし、zigzag（0, −1, 1, −2 … を 0, 1, 2, 3 …）にした 16 bit の値の上位バイトの面・下位バイトの面を続けて、
  deflate（zlib の見出しなし）で圧縮したもの。区画のデータは uint16 左上の値 + 圧縮したもの
  並びは正距円筒図法（左端が経度 -180°、上端が北緯 90°）

使い方: python3 tools/make_body_detail.py <作業ディレクトリ> [出力先=assets]
必要なもの: Pillow と NumPy（ダウンロード 約 11.5 GB）
"""
import os
import struct
import sys
import urllib.request
import zlib

import numpy as np
from PIL import Image

MOON_URL = "https://svs.gsfc.nasa.gov/vis/a000000/a004700/a004720/ldem_16_uint.tif"
MARS_URL = "https://planetarymaps.usgs.gov/mosaic/Mars/HRSC_MOLA_Blend/Mars_HRSC_MOLA_BlendDEM_Global_200mp_v2.tif"
MOON_W, MARS_W, TILE = 5760, 11520, 360
UA = {"User-Agent": "curl/8.5.0"}


def fetch(work, url):
    path = os.path.join(work, os.path.basename(url))
    if not os.path.exists(path):
        print("download", url)
        with urllib.request.urlopen(urllib.request.Request(url, headers=UA)) as r, open(path + ".part", "wb") as f:
            while True:
                b = r.read(1 << 22)
                if not b:
                    break
                f.write(b)
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


def tiff_strips(path):
    """非圧縮・1 バンドの int16 の BigTIFF の幅・高さ・最初の帯の位置（帯は続けて並んでいること）"""
    with open(path, "rb") as f:
        d = f.read(1 << 16)
        assert d[:4] == b"II+\0"
        off = struct.unpack("<Q", d[8:16])[0]
        f.seek(off)
        n = struct.unpack("<Q", f.read(8))[0]
        tags = {}
        for _ in range(n):
            tag, typ, cnt = struct.unpack("<HHQ", f.read(12))
            tags[tag] = (typ, cnt, f.read(8))
        val = lambda t: struct.unpack("<Q" if tags[t][0] == 16 else "<I" if tags[t][0] == 4 else "<H", tags[t][2][:8 if tags[t][0] == 16 else 4 if tags[t][0] == 4 else 2])[0]
        w, h = val(256), val(257)
        assert val(258) == 16 and val(259) == 1 and val(339) == 2 and tags[273][1] == h
        f.seek(val(273))
        first = struct.unpack("<Q", f.read(8))[0]
        f.seek(val(273) + 8 * (h - 1))
        assert struct.unpack("<Q", f.read(8))[0] == first + 2 * w * (h - 1)
    return w, h, first


def box_cols(row, w2):
    """1 行を w2 画素に、面積の重みで平均して縮める（端の画素は重なる割合で分ける）"""
    c = np.concatenate([[0.0], np.cumsum(row)])
    x = np.arange(w2 + 1) * (len(row) / w2)
    i = np.minimum(x.astype(np.int64), len(row) - 1)
    cx = c[i] + (x - i) * row[i]
    return np.diff(cx)


def mars_blend(path, w2, h2):
    """HRSC・MOLA 合成の標高を w2 x h2 に面積で平均して縮める（m）"""
    w, h, first = tiff_strips(path)
    src = np.memmap(path, dtype="<i2", mode="r", offset=first, shape=(h, w))
    out = np.zeros((h2, w2))
    r = h / h2
    for j in range(h2):
        y0, y1 = j * r, (j + 1) * r
        s = np.zeros(w2)
        wsum = np.zeros(w2)
        for y in range(int(y0), min(int(np.ceil(y1)), h)):
            k = min(y + 1, y1) - max(y, y0)
            row = np.asarray(src[y], dtype=np.float64)
            ok = row != -32768
            s += k * box_cols(np.where(ok, row, 0.0), w2)
            wsum += k * box_cols(ok.astype(np.float64), w2)
        assert wsum.min() > 0, "no data in row %d" % j
        out[j] = s / wsum
        if j % 500 == 0:
            print("  row %d / %d" % (j, h2))
    return out


def write(path, v, parts=1):
    """parts = 2: 後半の区画を path の名前に 2 を付けたファイルに分ける"""
    H, W = v.shape
    assert W % TILE == 0 and H % TILE == 0 and v.min() >= 0 and v.max() < 65536
    cols, rows = W // TILE, H // TILE
    tiles = [encode_tile(v[r * TILE:(r + 1) * TILE, c * TILE:(c + 1) * TILE]) for r in range(rows) for c in range(cols)]
    pack(path, cols, rows, tiles, parts)


def pack(path, cols, rows, tiles, parts=1):
    head = 16 + 8 * len(tiles)
    pos, index = head, b""
    for t in tiles:
        index += struct.pack("<II", pos, len(t))
        pos += len(t)
    cut = len(tiles)   # 後半のファイルの最初の区画
    if parts == 2:
        ends = [head + sum(len(t) for t in tiles[:r * cols]) for r in range(1, rows)]
        cut = cols * (1 + min(range(rows - 1), key=lambda r: abs(ends[r] - pos / 2)))
    with open(path, "wb") as f:
        f.write(b"EDT2" + struct.pack("<iii", cols, rows, TILE) + index + b"".join(tiles[:cut]))
    if parts == 2:
        with open(path[:-4] + "2.bin", "wb") as f:
            f.write(b"".join(tiles[cut:]))
    print("done:", path, pos, "bytes")


def main():
    work = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else "assets"
    os.makedirs(work, exist_ok=True)
    os.makedirs(out, exist_ok=True)
    Image.MAX_IMAGE_PIXELS = None
    # 月: 0.5 m 単位・半径 1727.4 km 基準 = 1737.4 km から −10000 m（moon_data.png と同じ）。左端が経度 -180°
    moon = np.asarray(Image.open(fetch(work, MOON_URL))).astype(np.int64)
    assert moon.shape == (MOON_W // 2, MOON_W)
    write(os.path.join(out, "moon_detail.bin"), moon)
    # 火星: m・左端が経度 -180°・上端が北緯 90° → 0.5 m 単位で −10000 m から（maps.bin と同じ）
    mars = mars_blend(fetch(work, MARS_URL), MARS_W, MARS_W // 2)
    write(os.path.join(out, "mars_detail.bin"), np.clip(np.round((mars + 10000) * 2), 0, 65535).astype(np.int64), 2)


if __name__ == "__main__":
    main()
