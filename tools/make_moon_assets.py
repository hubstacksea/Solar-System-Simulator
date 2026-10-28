#!/usr/bin/env python3
"""NASA SVS CGI Moon Kit から月のアセット (assets/moon_color.jpg, assets/moon_data.png) を作る。

データ（NASA's Scientific Visualization Studio, https://svs.gsfc.nasa.gov/4720）:
  - lroc_color_poles_4k.tif  LRO カメラ (LROC WAC) の色  4096x2048
  - ldem_16_uint.tif         LRO レーザー高度計 (LOLA) の標高 5760x2880, 0.5 m 単位, 半径 1727.4 km 基準

出力:
  moon_color.jpg  地表の色 4096x2048
  moon_data.png   標高 2048x1024（グレー+アルファ = 16 bit の上位 / 下位バイト。
                  線形補間しても 上位 + 下位/256 の関係が保たれるのでそのままフィルタできる）

使い方: python3 tools/make_moon_assets.py <作業ディレクトリ> [出力先=assets]
必要なもの: Pillow（ダウンロード 約 45 MB）
"""
import os
import sys
import urllib.request

from PIL import Image

BASE = "https://svs.gsfc.nasa.gov/vis/a000000/a004700/a004720/"
FILES = ["lroc_color_poles_4k.tif", "ldem_16_uint.tif"]


def fetch(work, name):
    path = os.path.join(work, name)
    if not os.path.exists(path):
        print("download", BASE + name)
        urllib.request.urlretrieve(BASE + name, path)
    return path


def main():
    work = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else "assets"
    os.makedirs(work, exist_ok=True)
    os.makedirs(out, exist_ok=True)
    Image.MAX_IMAGE_PIXELS = None
    color_path, dem_path = (fetch(work, f) for f in FILES)

    Image.open(color_path).convert("RGB").save(os.path.join(out, "moon_color.jpg"), quality=88, optimize=True)

    # 16 bit 標高 → 2048x1024 に平均縮小し、上位・下位バイトに分ける
    dem = Image.open(dem_path).convert("F").resize((2048, 1024), Image.BOX)
    w, h = dem.size
    px = dem.load()
    hi = Image.new("L", (w, h))
    lo = Image.new("L", (w, h))
    hp, lp = hi.load(), lo.load()
    for y in range(h):
        for x in range(w):
            v = max(0, min(65535, int(round(px[x, y]))))
            hp[x, y] = v >> 8
            lp[x, y] = v & 255
    Image.merge("LA", (hi, lo)).save(os.path.join(out, "moon_data.png"), optimize=True)
    print("done:", out)


if __name__ == "__main__":
    main()
