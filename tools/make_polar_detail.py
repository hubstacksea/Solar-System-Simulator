#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""月と火星の極の細かい標高 (assets/moon_south.bin, moon_north.bin, mars_south.bin, mars_north.bin) を作る。

データ（どれも極中心の平射図法。極で縮尺が正しく、画素は (一辺 − 1) / 2 が極）:
  - 月: NASA PDS Geosciences Node の LRO レーザー高度計 LOLA の極の標高 LDEM_85S_40M.IMG・LDEM_85N_40M.IMG（Smith ほか。
    緯度 85° より極側、7584x7584、1 画素 40 m、int16 の 0.5 m 単位、半径 1737.4 km 基準。各 115 MB）を、2x2 の平均で
    1 画素 80 m（3792x3792）に縮める
  - 火星: NASA PDS Geosciences Node の MGS レーザー高度計 MOLA の極の標高 MEGT_S_512_1.IMG・MEGT_N_512_1.IMG（Smith ほか。
    512 ピクセル/度 = 1 画素 115 m、12288x12288、uint16 の 0.25 m 単位で −8000 m から、MOLA の基準面（等重力面）から。
    投影の球は極半径 3376.2 km。各 302 MB）の、緯度 80° より極側（中央の 10240x10240）を 4x4 の平均で 1 画素 460 m
    （2560x2560）に縮める。MOLA の測った点は 115 m の格子では 1〜4 割の升にしか無く、460 m で 9 割を超える（緯度 80°〜87°）ので
    この細かさにする。探査機の軌道が極の 3° 以内に届かないため、緯度 87° より極側はほとんどが補間
全球の細かい標高（moon_detail.bin・mars_detail.bin、1 画素 約 1.9 km）は極の近くでは粗いので、極の近くではこちらを重ねる。

出力形式（リトルエンディアン）:
  char[4] "EPS2", int32 一辺の画素数, 1 区画の画素数, float32 1 画素の大きさ m, 投影の球の半径 km, int32 極（1 北, −1 南）
  区画ごと（上の行から、各行は左から）に uint32 位置（ファイルの先頭から）, uint32 大きさ
  区画: 値 v（0.5 m 単位、-10000 m から。標高 = v × 0.5 − 10000 m。月は半径 1737.4 km、火星は MOLA の基準面から）を
  make_body_detail.py と同じく予測の差にして圧縮したもの
  並び: 平射図法。極が画像の中心（画素の中心で数えて (一辺 − 1) / 2）。緯度 φ・東経 λ の点は、極からの距離
  ρ = 2 × 投影の球の半径 × tan(45° − |φ| / 2) の所で、右へ ρ sin λ、南極は上へ ρ cos λ、北極は下へ ρ cos λ

使い方: python3 tools/make_polar_detail.py <作業ディレクトリ> [出力先=assets]
必要なもの: NumPy（ダウンロード 約 830 MB）
"""
import os
import struct
import sys

import numpy as np

from make_body_detail import encode_tile, fetch

LOLA = "https://pds-geosciences.wustl.edu/lro/lro-l-lola-3-rdr-v1/lrolol_1xxx/data/lola_gdr/polar/img/"
MOLA = "https://pds-geosciences.wustl.edu/mgs/mgs-m-mola-5-megdr-l3-v1/mgsl_300x/polar/"
# 出力の名前, URL, 元の一辺, 使う一辺, 縮める倍率, 1 区画, 元の 1 画素 m, 投影の球の半径 km, 極
SETS = [
    ("moon_south.bin", LOLA + "ldem_85s_40m.img", 7584, 7584, 2, 474, 40.0, 1737.4, -1),
    ("moon_north.bin", LOLA + "ldem_85n_40m.img", 7584, 7584, 2, 474, 40.0, 1737.4, 1),
    ("mars_south.bin", MOLA + "megt_s_512_1.img", 12288, 10240, 4, 320, 3376.2e3 * np.pi / 180 / 512, 3376.2, -1),
    ("mars_north.bin", MOLA + "megt_n_512_1.img", 12288, 10240, 4, 320, 3376.2e3 * np.pi / 180 / 512, 3376.2, 1),
]


def main():
    work = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else "assets"
    os.makedirs(work, exist_ok=True)
    os.makedirs(out, exist_ok=True)
    for name, url, src_n, use_n, f, tile, src_m, radius_km, pole in SETS:
        path = fetch(work, url)
        if "lola" in url:   # int16 の 0.5 m 単位
            half = np.fromfile(path, dtype="<i2").reshape(src_n, src_n).astype(np.int64)
        else:   # uint16 の 0.25 m 単位で −8000 m から（ビッグエンディアン）を 0.5 m 単位に
            half = np.fromfile(path, dtype=">u2").reshape(src_n, src_n).astype(np.int64) * 0.5 - 16000
        a = (src_n - use_n) // 2
        half = half[a:a + use_n, a:a + use_n]
        n = use_n // f
        # f x f の平均（0.5 m 単位のまま）。−10000 m から数える値にする
        v = np.round(half.reshape(n, f, n, f).mean(axis=(1, 3))).astype(np.int64) + 20000
        assert v.min() >= 0 and v.max() < 65536 and n % tile == 0
        tiles = []
        for r in range(n // tile):
            for c in range(n // tile):
                tiles.append(encode_tile(v[r * tile:(r + 1) * tile, c * tile:(c + 1) * tile]))
        head = b"EPS2" + struct.pack("<iiffi", n, tile, src_m * f, radius_km, pole)
        pos = len(head) + 8 * len(tiles)
        index = b""
        for t in tiles:
            index += struct.pack("<II", pos, len(t))
            pos += len(t)
        dst = os.path.join(out, name)
        with open(dst, "wb") as fo:
            fo.write(head + index + b"".join(tiles))
        print(dst, n, "x", n, "%.1f m" % (src_m * f), os.path.getsize(dst), "bytes")


if __name__ == "__main__":
    main()
