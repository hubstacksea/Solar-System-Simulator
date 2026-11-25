#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""NASA / NOAA の地球データからゲーム用アセット (assets/earth_color.jpg, assets/earth_data.png) を作る。

データ（いずれも商用を含め自由に再配布できるもの）:
  - NASA Blue Marble Next Generation (2004年4月, 地形陰影なし)  … 地表の色
  - NOAA NCEI ETOPO 2022 (60 秒角, 氷床表面) を 2 分角に間引いたもの … 標高・水深
    同じデータの 60 秒角そのもの（make_earth_detail.py と同じファイル） … 海陸
  - NASA Black Marble 2016                                    … 夜の街明かり

出力:
  earth_color.jpg  地表の色（海は周囲の陸の色で埋めてある）
  earth_data.png   R: 高さコード, G: 夜景, B: 海陸マスク（テクセルの中の陸の割合）, A: 海氷
  どちらも 8192x4096（配布する版も。--hq を付けても同じ）

使い方: python3 tools/make_earth_assets.py <作業ディレクトリ> [出力先=assets]
必要なもの: Pillow と NumPy と SciPy（ダウンロード 約 300 MB。ほかに make_earth_detail.py と同じ 約 930 MB）
"""
import os
import sys
import struct
import urllib.request
import numpy as np
from PIL import Image, ImageChops, ImageFilter, ImageMath
from scipy import ndimage

import make_earth_detail

BASE = "https://assets.science.nasa.gov/content/dam/science/esd/eo/images"
SOURCES = {
    "color": BASE + "/bmng/bmng-base/april/world.200404.3x21600x10800.jpg",
    "lights": BASE + "/imagerecords/144000/144898/BlackMarble_2016_3km.jpg",
}
# ETOPO 2022 60 秒角 (21600x10800) を 2 個おきに取得 → 10800x5400（float32, OPeNDAP バイナリ)
ETOPO_STRIDE = 2
ETOPO_URL = ("https://oceanwatch.pifsc.noaa.gov/erddap/griddap/ETOPO_2022_v1_60s.dods"
             "?z%%5B0:%d:10799%%5D%%5B0:%d:21599%%5D" % (ETOPO_STRIDE, ETOPO_STRIDE))
ETOPO_FILE = "etopo2022_60s_stride%d.dods" % ETOPO_STRIDE
W, H = 8192, 4096
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
    w, h = 21600 // ETOPO_STRIDE, 10800 // ETOPO_STRIDE
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


def land_fraction(work, color):
    """60 秒角（1 画素 約 1.85 km）で海陸を決め、W x H の 1 テクセルの中の陸の割合 (0..255) にする。
    5 分角に間引いた標高で決めると、瀬戸内海のような幅 10〜50 km の海が陸になる（四国・九州が本州と陸続きに見えた）。
    海陸: 標高 0 m 以上を陸とする。ただし地表画像で水の色の所（カスピ海などの湖）は水、
    海面下でも暗くなく水の色でない浅い所（オランダの干拓地・カスピ海の北の低地など）は陸。
    地表画像の浅い内海はほとんど黒い（瀬戸内海・東京湾）ので、暗い所はこの例外から除く。
    海面より高い所の明るい青は水にしない（南極・グリーンランドの氷床の、雪が飛ばされて氷が出た所が水になっていた）。
    海とつながっていない海面下の土地は、地表画像で砂や岩の色（赤みがあって明るい）の所を陸にする（カッターラ低地 -133 m・
    トルファン盆地 -154 m・ダナキル低地 -125 m・デスバレー -86 m・ヨルダン渓谷など。どれも 50 m より深い）。
    海とつながった所は広げない（南極などの海氷・凍った入り江が陸になる）。水の色の所（死海・ソルトン湖）と、
    凍った湖（白い）・濁った湖の岸（赤みが無いか暗い）は水のまま"""
    big = make_earth_detail.W, make_earth_detail.H
    tile = make_earth_detail.TILE
    land = np.zeros((big[1], big[0]), bool)
    dry = np.zeros((big[1], big[0]), bool)   # 海面下でも、海から離れていれば陸にする色の所
    for r in range(big[1] // tile):
        e = make_earth_detail.load_band(work, r)
        e = np.concatenate([e[:, big[0] // 2:], e[:, :big[0] // 2]], axis=1)   # 経度 -180° を左端に
        ch = color.size[1] * tile // big[1]
        c = np.asarray(color.crop((0, r * ch, color.size[0], (r + 1) * ch)).resize((big[0], tile), Image.BILINEAR),
                       np.float32)
        R, G, B = c[..., 0], c[..., 1], c[..., 2]
        water = (B > np.maximum(R, G) + 8) | ((R + G + B < 100) & (B >= R))
        water &= (e <= 0) | (R + G + B < 450)   # 海面より高い所の明るい青は氷（南極・グリーンランドの裸氷。凍った湖は暗い）
        landish = (e >= 0) | ((e > -50) & (R >= B) & (R + G + B >= 60))
        land[r * tile:(r + 1) * tile] = landish & ~water
        dry[r * tile:(r + 1) * tile] = ~water & (R >= B + 15) & (R + G + B >= 150)
    # 水のつながり（斜めも含む）。経度 ±180° の端どうしもつなぎ、いちばん大きいものを海とする
    lab, n = ndimage.label(~land, np.ones((3, 3), bool))
    parent = np.arange(n + 1)

    def root(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a
    for dy in (-1, 0, 1):
        a = lab[max(0, -dy):big[1] - max(0, dy), -1]
        b = lab[max(0, dy):big[1] - max(0, -dy), 0]
        for p, q in set(zip(a[(a > 0) & (b > 0)].tolist(), b[(a > 0) & (b > 0)].tolist())):
            parent[root(p)] = root(q)
    roots = np.array([root(i) for i in range(n + 1)])
    size = np.bincount(roots[lab].ravel(), minlength=n + 1)
    size[0] = 0
    sea = roots[lab] == size.argmax()
    del lab
    land |= dry & ~sea
    return Image.fromarray(land.astype(np.uint8) * 255).resize((W, H), Image.BOX)


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
    args = [a for a in sys.argv[1:] if a != "--hq"]
    work = args[0]
    out = args[1] if len(args) > 1 else "assets"
    os.makedirs(work, exist_ok=True)
    os.makedirs(out, exist_ok=True)
    Image.MAX_IMAGE_PIXELS = None
    src = {k: fetch(work, u) for k, u in SOURCES.items()}

    elev = load_etopo(fetch(work, ETOPO_URL, ETOPO_FILE)).resize((W, H), Image.BILINEAR)
    color_src = Image.open(src["color"]).convert("RGB")
    color = color_src.resize((W, H), Image.LANCZOS)
    frac = land_fraction(work, color_src)
    land = frac.point(lambda v: 255 if v >= 128 else 0)
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
    # B: 陸の割合（シェーダで 0.5 を境に海岸線を引き、ノイズで細かく揺らす）。テクセルの半分より狭い海峡は閉じる
    mask = frac
    # A: 海氷
    Image.merge("RGBA", (height, lights, mask, seaice)).save(os.path.join(out, "earth_data.png"), optimize=True)
    print("done:", out)


if __name__ == "__main__":
    main()
