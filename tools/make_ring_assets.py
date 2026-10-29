#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""土星の環の不透明度 (assets/saturn_rings.png) を作る。

データ: NASA PDS Ring-Moon Systems Node, VG_2801（Voyager 2 光偏光計 (PPS) によるさそり座デルタ星の掩蔽、1981 年）の
半径 10 km ごとの環の光学的厚さ（垂直方向）KM010/PS1P01.TAB。

出力: 2048x1 のグレー PNG。半径 72,000〜142,500 km を等間隔に並べ、値は垂直に見た時の不透明度 1 - exp(-τ)（0〜255）。
（光学的厚さが測れないほど濃い所（表の値 99）は τ = 5 とする）

使い方: python3 tools/make_ring_assets.py <作業ディレクトリ> [出力先=assets]
必要なもの: Pillow, NumPy（ダウンロード 約 0.4 MB）
"""
import os
import sys
import urllib.request

import numpy as np
from PIL import Image

UA = "SolarSystemVoyager-asset-build/1.0 (https://github.com/hubstacksea/Solar-System-Simulator)"
URL = "https://pds-rings.seti.org/holdings/volumes/VG_28xx/VG_2801/EASYDATA/KM010/PS1P01.TAB"
R0, R1, W = 72000.0, 142500.0, 2048


def main():
    work = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else "assets"
    os.makedirs(work, exist_ok=True)
    path = os.path.join(work, "PS1P01.TAB")
    if not os.path.exists(path):
        print("download", URL)
        req = urllib.request.Request(URL, headers={"User-Agent": UA})
        with urllib.request.urlopen(req, timeout=300) as r, open(path, "wb") as f:
            f.write(r.read())
    rows = np.loadtxt(path, delimiter=",")
    r, tau = rows[:, 0], rows[:, 3]
    tau = np.where(tau >= 90, 5.0, np.clip(tau, 0, 5.0))
    edges = np.linspace(R0, R1, W + 1)
    idx = np.clip(np.digitize(r, edges) - 1, 0, W - 1)
    opa = 1 - np.exp(-tau)   # 画素の中で不透明度を平均する
    s = np.bincount(idx, opa, W) / np.maximum(np.bincount(idx, None, W), 1)
    Image.fromarray((s * 255 + 0.5).astype(np.uint8).reshape(1, W), "L").save(os.path.join(out, "saturn_rings.png"))
    print("rows", len(r), "mean opacity %.3f" % s.mean())


if __name__ == "__main__":
    main()
