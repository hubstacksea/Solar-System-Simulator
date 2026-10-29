#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""地球の季節の地表の色と雲 (assets/earth_months.jpg, assets/earth_clouds.jpg) を作る。

データ（NASA Earth Observatory、パブリックドメイン）:
  - Blue Marble Next Generation の 12 か月（2004 年 1〜12 月、地形陰影なし）… 季節による雪・緑・砂漠の色の変化
  - Blue Marble の雲（cloud_combined_8192, MODIS 2001 年）… 雲の分布

出力:
  earth_months.jpg  1024x512 の 12 か月を 4 列 × 3 行に並べたもの（1 月が左上、横へ 2・3・4 月）。
                    ゲームでは今の月の色と 4 月の色の差を、4 月の高解像度の地表（earth_color.jpg）に足す
  earth_clouds.jpg  4096x2048 のグレー（白いほど厚い雲）
どちらも正距円筒図法・左端が経度 -180°・上端が北緯 90°（earth_color.jpg と同じ並び）。

使い方: python3 tools/make_earth_season_assets.py <作業ディレクトリ> [出力先=assets]
必要なもの: Pillow（ダウンロード 約 150 MB）
"""
import os
import sys
import urllib.request

from PIL import Image

UA = "SolarSystemVoyager-asset-build/1.0 (https://github.com/hubstacksea/Solar-System-Simulator)"
BASE = "https://assets.science.nasa.gov/content/dam/science/esd/eo/images/bmng/bmng-base"
MONTHS = ["january", "february", "march", "april", "may", "june", "july", "august", "september", "october",
          "november", "december"]
CLOUDS = "https://eoimages.gsfc.nasa.gov/images/imagerecords/57000/57747/cloud_combined_8192.tif"
TW, TH = 1024, 512
Image.MAX_IMAGE_PIXELS = None


def fetch(work, url):
    path = os.path.join(work, os.path.basename(url))
    if not os.path.exists(path):
        print("download", url)
        req = urllib.request.Request(url, headers={"User-Agent": UA})
        with urllib.request.urlopen(req, timeout=900) as r, open(path + ".part", "wb") as f:
            while True:
                b = r.read(1 << 20)
                if not b:
                    break
                f.write(b)
        os.rename(path + ".part", path)
    return path


def main():
    work = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else "assets"
    os.makedirs(work, exist_ok=True)
    atlas = Image.new("RGB", (TW * 4, TH * 3))
    for m, name in enumerate(MONTHS):
        url = "%s/%s/world.2004%02d.3x5400x2700.jpg" % (BASE, name, m + 1)
        img = Image.open(fetch(work, url)).convert("RGB").resize((TW, TH), Image.LANCZOS)
        atlas.paste(img, ((m % 4) * TW, (m // 4) * TH))
    atlas.save(os.path.join(out, "earth_months.jpg"), quality=88, optimize=True)
    clouds = Image.open(fetch(work, CLOUDS)).convert("L").resize((4096, 2048), Image.LANCZOS)
    clouds.save(os.path.join(out, "earth_clouds.jpg"), quality=85, optimize=True)
    print("done")


if __name__ == "__main__":
    main()
