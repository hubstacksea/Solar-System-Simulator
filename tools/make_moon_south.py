#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""月の南極の細かい標高 (assets/moon_south.bin) を作る。

データ: NASA PDS Geosciences Node の LRO レーザー高度計 LOLA の極の標高 LDEM_85S_40M.IMG（Smith ほか。南緯 85° より極側を
覆う極中心の平射図法（南極が中心）、7584x7584、1 画素 40 m、int16 の 0.5 m 単位、半径 1737.4 km 基準。115 MB）を、
2x2 の平均で 1 画素 80 m（3792x3792）に縮める。全球の細かい標高（moon_detail.bin、1 画素 約 1.9 km）は極の近くでは粗いので、
南緯 85° より極側ではこちらを重ねる。

出力形式（リトルエンディアン）:
  char[4] "EPS1", int32 一辺の画素数 (3792), 1 区画の画素数 (474), float32 1 画素の大きさ m (80)
  区画ごと（上の行から、各行は左から）に uint32 位置（ファイルの先頭から）, uint32 大きさ
  区画: 値 v（0.5 m 単位、-10000 m から。標高 = v × 0.5 − 10000 m、半径 1737.4 km から）を make_body_detail.py と同じく
  予測の差にして圧縮したもの
  並び: 平射図法。南極が画像の中心（画素の中心で数えて (3792 − 1) / 2）。緯度 φ・東経 λ の点は、極からの距離
  ρ = 2 × 1737.4 km × tan(45° + φ / 2) の所で、右へ ρ sin λ、上へ ρ cos λ

使い方: python3 tools/make_moon_south.py <作業ディレクトリ> [出力先=assets]
必要なもの: NumPy（ダウンロード 115 MB）
"""
import os
import struct
import sys

import numpy as np

from make_body_detail import encode_tile, fetch

URL = "https://pds-geosciences.wustl.edu/lro/lro-l-lola-3-rdr-v1/lrolol_1xxx/data/lola_gdr/polar/img/ldem_85s_40m.img"
SRC_N, N, TILE, SCALE_M = 7584, 3792, 474, 80.0


def main():
    work = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else "assets"
    os.makedirs(work, exist_ok=True)
    os.makedirs(out, exist_ok=True)
    dn = np.fromfile(fetch(work, URL), dtype="<i2").reshape(SRC_N, SRC_N).astype(np.int64)
    # 2x2 の平均（0.5 m 単位のまま）。−10000 m から数える値にする
    v = np.round(dn.reshape(N, 2, N, 2).mean(axis=(1, 3))).astype(np.int64) + 20000
    assert v.min() >= 0 and v.max() < 65536
    tiles = []
    for r in range(N // TILE):
        for c in range(N // TILE):
            tiles.append(encode_tile(v[r * TILE:(r + 1) * TILE, c * TILE:(c + 1) * TILE]))
    head = b"EPS1" + struct.pack("<iif", N, TILE, SCALE_M)
    pos = len(head) + 8 * len(tiles)
    index = b""
    for t in tiles:
        index += struct.pack("<II", pos, len(t))
        pos += len(t)
    path = os.path.join(out, "moon_south.bin")
    with open(path, "wb") as f:
        f.write(head + index + b"".join(tiles))
    print(path, os.path.getsize(path), "bytes")


if __name__ == "__main__":
    main()
