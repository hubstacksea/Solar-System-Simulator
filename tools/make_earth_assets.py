#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""NASA / NOAA の地球データからゲーム用アセット (assets/earth_color.jpg, assets/earth_data.png) を作る。

データ（いずれも商用を含め自由に再配布できるもの）:
  - NASA Blue Marble Next Generation (2004年4月, 地形陰影なし)  … 地表の色
  - NOAA NCEI ETOPO 2022 (60 秒角, 氷床表面) を 5 分角に間引いたもの … 標高・水深・海陸
  - NASA Black Marble 2016                                    … 夜の街明かり

出力:
  earth_color.jpg  地表の色（海は周囲の陸の色で埋めてある）
  earth_data.png   R: 高さコード, G: 夜景, B: 海陸マスク（少しぼかし）, A: 海氷

使い方: python3 tools/make_earth_assets.py <作業ディレクトリ> [出力先=assets]
必要なもの: Pillow（ダウンロード 約 50 MB）
"""
import os
import sys
import struct
import urllib.request
from PIL import Image, ImageChops, ImageFilter, ImageMath

BASE = "https://assets.science.nasa.gov/content/dam/science/esd/eo/images"
SOURCES = {
    "color": BASE + "/bmng/bmng-base/april/world.200404.3x5400x2700.jpg",
    "lights": BASE + "/imagerecords/144000/144898/BlackMarble_2016_3km.jpg",
}
# ETOPO 2022 60 秒角 (21600x10800) を 5 個おきに取得 → 4320x2160 (float32, OPeNDAP バイナリ)
ETOPO_URL = ("https://oceanwatch.pifsc.noaa.gov/erddap/griddap/ETOPO_2022_v1_60s.dods"
             "?z%5B0:5:10799%5D%5B0:5:21599%5D")
ETOPO_FILE = "etopo2022_60s_stride5.dods"
W, H = 4096, 2048
LAND_MAX, SEA_MAX = 8900.0, 11000.0  # 高さコードの範囲 (m)。シェーダの earthHeightM と合わせる


def fetch(work, url, name=None):
    path = os.path.join(work, name or os.path.basename(url))
    if not os.path.exists(path):
        print("download", url)
        urllib.request.urlretrieve(url, path)
    return path


def load_etopo(path):
    """OPeNDAP (.dods) の z 配列を読み、経度 -180〜180・北が上の標高画像 (m, mode F) にする"""
    with open(path, "rb") as f:
        raw = f.read()
    head, sep, body = raw.partition(b"\nData:\n")
    if not sep:
        raise RuntimeError("unexpected ETOPO response: " + raw[:200].decode("latin-1"))
    n = struct.unpack(">I", body[:4])[0]
    w, h = 4320, 2160
    assert n == w * h, n
    img = Image.frombytes("F", (w, h), body[8:8 + n * 4], "raw", "F;32BF")
    # 元データは南が先頭・経度 0〜360。北を上に、経度 -180 を左端にする
    img = img.transpose(Image.FLIP_TOP_BOTTOM)
    left, right = img.crop((0, 0, w // 2, h)), img.crop((w // 2, 0, w, h))
    out = Image.new("F", (w, h))
    out.paste(right, (0, 0))
    out.paste(left, (w // 2, 0))
    return out


def ev(expr, **kw):
    # Pillow 10.3 以降は eval が unsafe_eval に改名された（式は自前のものだけ）
    return getattr(ImageMath, "unsafe_eval")(expr, **kw) if hasattr(ImageMath, "unsafe_eval") else ImageMath.eval(expr, **kw)


def fill_ocean(color, land):
    """海のピクセルを周囲の陸の色で埋める（プッシュプル法）。
    シェーダは海岸線をテクセルより細かく引くので、陸の色に海の青が混ざらないようにする。"""
    m = land.convert("F")
    m = ev("a / 255.0", a=m)
    chans = [ev("c * m", c=c.convert("F"), m=m) for c in color.split()]
    levels = [(chans, m)]
    while min(levels[-1][1].size) > 1:
        cs, mm = levels[-1]
        size = (max(1, mm.size[0] // 2), max(1, mm.size[1] // 2))
        levels.append(([c.resize(size, Image.BOX) for c in cs], mm.resize(size, Image.BOX)))
    filled = None
    for cs, mm in reversed(levels):
        cur = [ev("c / max(m, 1e-6)", c=c, m=mm) for c in cs]
        if filled is not None:
            up = [f.resize(mm.size, Image.BILINEAR) for f in filled]
            cur = [ev("m * c + (1 - m) * u", m=ev("min(m * 4, 1)", m=mm), c=c, u=u)
                   for c, u in zip(cur, up)]
        filled = cur
    out = [ev("convert(min(max(c, 0), 255), 'L')", c=c) for c in filled]
    return Image.merge("RGB", out)


def main():
    work = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else "assets"
    os.makedirs(work, exist_ok=True)
    os.makedirs(out, exist_ok=True)
    Image.MAX_IMAGE_PIXELS = None
    src = {k: fetch(work, u) for k, u in SOURCES.items()}

    elev = load_etopo(fetch(work, ETOPO_URL, ETOPO_FILE)).resize((W, H), Image.BILINEAR)
    color = Image.open(src["color"]).convert("RGB").resize((W, H), Image.LANCZOS)
    # 海陸: 標高 0 m 以上を陸とする。ただし地表画像で水の色の所（カスピ海などの湖）は水、
    # 海面下でも水の色でない浅い所（オランダの干拓地・死海周辺など）は陸
    r, g, b = [c.convert("F") for c in color.split()]
    water = ev("convert(max(b > max(r, g) + 8, (r + g + b < 100) * (b >= r)) * 255, 'L')", r=r, g=g, b=b)
    landish = ev("convert(max(e >= 0, (e > -50) * (r >= b)) * 255, 'L')", e=elev, r=r, b=b)
    land = ImageChops.multiply(landish, ImageChops.invert(water))
    land = land.filter(ImageFilter.MedianFilter(3))
    # 海氷: 海の上の白い所（元画像の明るさから）
    gray = color.convert("L")
    seaice = ImageChops.multiply(gray.point(lambda v: max(0, min(255, (v - 100) * 4))), ImageChops.invert(land))
    # 陸の内側だけを使って海を埋める（海岸のテクセルは海の色が混ざっているので除く）
    inner = land.filter(ImageFilter.MinFilter(7))
    fill_ocean(color, inner).save(os.path.join(out, "earth_color.jpg"), quality=88, optimize=True)
    # R: 高さコード 海 0..127（127 が 0 m）, 陸 128..255。低い所ほど細かくなるよう平方根で詰める
    hl = ev("convert(128 + (min(max(e, 0), L) / L) ** 0.5 * 127 + 0.5, 'L')", e=elev, L=LAND_MAX)
    hs = ev("convert(127.5 - (min(max(-e, 0), S) / S) ** 0.5 * 127, 'L')", e=elev, S=SEA_MAX)
    height = Image.composite(hl, hs, land)
    # G: 夜景。月明かりに照らされた地表（〜55）を除く
    lights = Image.open(src["lights"]).convert("L").resize((W, H), Image.LANCZOS)
    lights = lights.point(lambda v: int(round((max(0, v - 55) / 200) ** 1.3 * 255)))
    # B: 少しぼかした海陸マスク（シェーダで 0.5 を境に海岸線を引き、ノイズで細かく揺らす）
    mask = land.filter(ImageFilter.GaussianBlur(1.2))
    # A: 海氷
    Image.merge("RGBA", (height, lights, mask, seaice)).save(os.path.join(out, "earth_data.png"), optimize=True)
    print("done:", out)


if __name__ == "__main__":
    main()
