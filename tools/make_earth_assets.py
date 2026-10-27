#!/usr/bin/env python3
"""NASA の地球データからゲーム用アセット (assets/earth_color.jpg, assets/earth_data.png) を作る。

データ（いずれも NASA Visible Earth / Earth Observatory、パブリックドメイン）:
  - Blue Marble Next Generation (2004年4月, 地形陰影なし)   … 地表の色
  - GEBCO_08 地形 (elev) / 水深 (bath)                       … 標高・水深・海陸
  - Black Marble 2016                                       … 夜の街明かり

出力:
  earth_color.jpg  地表の色（海は周囲の陸の色で埋めてある）
  earth_data.png   R: 高さコード, G: 夜景, B: 海陸マスク（少しぼかし）, A: 海氷

使い方: python3 tools/make_earth_assets.py <作業ディレクトリ> [出力先=assets]
必要なもの: Pillow
"""
import os
import sys
import urllib.request
from PIL import Image, ImageChops, ImageFilter, ImageMath

BASE = "https://assets.science.nasa.gov/content/dam/science/esd/eo/images"
SOURCES = {
    "color": BASE + "/bmng/bmng-base/april/world.200404.3x5400x2700.jpg",
    "elev": BASE + "/bmng/topography/gebco_08_rev_elev_5400x2700.jpg",
    "bath": BASE + "/bmng/bathymetry/gebco_08_rev_bath_5400x2700.jpg",
    "lights": BASE + "/imagerecords/144000/144898/BlackMarble_2016_3km.jpg",
}
W, H = 4096, 2048


def fetch(work, url):
    path = os.path.join(work, os.path.basename(url))
    if not os.path.exists(path):
        print("download", url)
        urllib.request.urlretrieve(url, path)
    return path


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

    elev = Image.open(src["elev"]).convert("L").resize((W, H), Image.BILINEAR)
    bath = Image.open(src["bath"]).convert("L").resize((W, H), Image.BILINEAR)
    color = Image.open(src["color"]).convert("RGB").resize((W, H), Image.LANCZOS)
    # 海陸: 水深画像は陸が白。ただしごく浅い海も白くなるので、地表画像で濃い青の所は海とする
    r, g, b = [c.convert("F") for c in color.split()]
    water = ev("convert(max(b > max(r, g) + 8, (r + g + b < 100) * (b >= r)) * 255, 'L')", r=r, g=g, b=b)
    land = ImageChops.multiply(bath.point(lambda v: 255 if v >= 250 else 0), ImageChops.invert(water))
    land = land.filter(ImageFilter.MedianFilter(3))
    # 海氷: 海の上の白い所（元画像の明るさから）
    gray = color.convert("L")
    seaice = ImageChops.multiply(gray.point(lambda v: max(0, min(255, (v - 100) * 4))), ImageChops.invert(land))
    # 陸の内側だけを使って海を埋める（海岸のテクセルは海の色が混ざっているので除く）
    inner = land.filter(ImageFilter.MinFilter(7))
    fill_ocean(color, inner).save(os.path.join(out, "earth_color.jpg"), quality=88, optimize=True)
    # R: 高さコード 海 0..127（127 が浅い）, 陸 128..255（255 ≒ 6400 m）
    hl = elev.point(lambda v: int(round(128 + min(v, 253) * 127 / 253)))
    hs = bath.point(lambda v: int(round(min(v, 249) * 127 / 249)))
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
