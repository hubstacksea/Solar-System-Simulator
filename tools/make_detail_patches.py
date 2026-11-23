#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""有名な山などの地域だけの、もっと細かい標高 (assets/earth_patches.bin, moon_patches.bin, mars_patches.bin) を作る。

全球の細かい標高（mars_detail.bin。1 画素 約 1.85 km）では、急な崖が平均でならされる（オリンポス山の裾の断崖は、
元のデータで傾きが最大 71° あるのに 35° になり、25° より急な所の面積が 4 割に減る）。地表の近くでは、ここに挙げた地域
（長方形）だけ、この細かい標高を重ねて使う（地域の縁から内側へなじませる）。

データ:
  - 地球: Copernicus DEM GLO-90（ESA。3 秒角 = 1200 ピクセル/度、約 90 m。地表面の高さで、森や建物の高さを含む。海は
    区画が無く 0 m。北緯 50° より極側は東西の間隔が広いので、東西に線形に補間する）。AWS の公開バケット
    copernicus-dem-90m（アカウント不要）から 1 度四方の区画を落とす。利用条件は THIRD_PARTY_NOTICES.md
  - 月: SLDEM2015（Barker ほか 2016。LRO の LOLA と かぐや の地形カメラ。512 ピクセル/度、約 60 m。半径 1737.4 km 基準。
    南緯 60°〜北緯 60°）。https://imbrium.mit.edu/DATA/SLDEM2015/TILES/FLOAT_IMG/ の 30° x 45° のファイル（1 つ 1.4 GB）から、
    要る行だけを範囲を指定して落とす
  - 火星: USGS Astrogeology の Mars MGS MOLA - MEX HRSC Blended DEM Global 200m v2（Fergason ほか 2018。パブリックドメイン。
    make_body_detail.py と同じファイル。296.4 ピクセル/度）を 288 ピクセル/度（1 画素 約 206 m。mars_detail.bin の 9 倍）に
    面積で平均して縮める

出力形式（リトルエンディアン）:
  char[4] "EDP1", int32 全球の幅の画素数, 高さの画素数, 1 区画の画素数, 地域の数
  地域ごとに int32 左上の区画の列, 行, 列の数, 行の数
  続けて、地域ごと・区画ごと（北の行から、各行は西から東へ）に uint32 位置（ファイルの先頭から）, uint32 大きさ
  区画のデータは make_body_detail.py と同じ（値 v は 0.5 m 単位で -10000 m から。予測の残差を deflate したもの）
  並びは正距円筒図法（左端が経度 -180°、上端が北緯 90°）

使い方: python3 tools/make_detail_patches.py <作業ディレクトリ> [出力先=assets] [earth] [moon] [mars]（天体を省くとすべて）
必要なもの: NumPy（ダウンロード: 地球 約 80 MB、月 約 0.6 GB、火星 約 11.4 GB。火星は make_body_detail.py と同じ作業ディレクトリ
なら再び落とさない）
"""
import os
import struct
import sys
import urllib.error
import urllib.request
import zlib

import numpy as np

from make_body_detail import MARS_URL, UA, encode_tile, fetch, tiff_strips

TILE = 360
EARTH_PPD = 1200
# 地球の地域: 名前, 南端・北端の緯度, 西端・東端の経度（度。区画の境目 0.3° まで広げる）
EARTH_PATCHES = [
    ("Everest", 27.0, 29.0, 85.8, 88.2),          # ヒマラヤ（エベレスト・ローツェ・マカルー・チョ・オユー）
    ("Fuji", 35.1, 35.7, 138.4, 139.0),
    ("Mont Blanc - Matterhorn", 45.6, 46.2, 6.6, 8.1),
    ("Kilimanjaro", -3.4, -2.8, 37.0, 37.8),
    ("Denali", 62.8, 63.4, -151.5, -150.3),
]
MOON_PPD = 512
# 月の地域（SLDEM2015 の 30° x 45° のファイルの境目をまたがない）
MOON_PATCHES = [
    ("Apollo 15 Hadley - Apennines", 24.5, 28.0, 1.0, 6.0),   # ハドレー山・アペニン山脈・ハドレー谷
    ("Mons Huygens - Mons Ampere", 18.0, 21.5, -5.0, -1.0),
    ("Apollo 17 Taurus-Littrow", 19.0, 21.5, 29.5, 32.0),
    ("Tycho", -45.0, -41.5, -13.5, -9.0),
]
MARS_PPD = 288
# 火星の地域: 名前, 南端・北端の緯度, 西端・東端の経度（度。区画の境目まで広げる）
MARS_PATCHES = [
    ("Olympus Mons", 12.0, 26.0, -142.0, -125.0),   # 山頂のカルデラと裾の断崖（オリンポス断崖）
    ("Arsia Mons", -12.5, -4.0, -124.5, -115.5),
    ("Pavonis Mons", -2.5, 5.5, -117.0, -109.0),
    ("Ascraeus Mons", 7.5, 16.5, -108.5, -99.5),
]


def box_rows(src, w, ppd2, row0, row1, col0, col1):
    """元の標高 src（w 画素/周、m）を ppd2 ピクセル/度に面積で平均して縮めたものの、行 row0..row1・列 col0..col1 の部分（m）"""
    h = src.shape[0]
    r = (w / 360.0) / ppd2           # 縮めた 1 画素あたりの元の画素
    xs = (np.arange(col0, col1 + 1) * r)
    out = np.zeros((row1 - row0, col1 - col0))
    for j in range(row0, row1):
        y0, y1 = j * r, (j + 1) * r
        s = np.zeros(col1 - col0)
        ws = np.zeros(col1 - col0)
        for y in range(int(y0), min(int(np.ceil(y1)), h)):
            k = min(y + 1, y1) - max(y, y0)
            row = np.asarray(src[y], dtype=np.float64)
            ok = row != -32768
            for vals, acc in ((np.where(ok, row, 0.0), s), (ok.astype(np.float64), ws)):
                c = np.concatenate([[0.0], np.cumsum(vals)])
                i = np.minimum(xs.astype(np.int64), len(vals) - 1)
                cx = c[i] + (xs - i) * vals[i]
                acc += k * np.diff(cx)
        assert ws.min() > 0, "no data in row %d" % j
        out[j - row0] = s / ws
    return out


def tiff_float(path):
    """Copernicus DEM の COG（float32、deflate、浮動小数点の予測 3、1 枚目の画像）の値の配列と、左上の経度・緯度、画素の大きさ（度）"""
    d = open(path, "rb").read()
    assert d[:4] == b"II*\0"
    off = struct.unpack("<I", d[4:8])[0]
    typ = {1: "B", 3: "H", 4: "I", 12: "d"}
    size = {1: 1, 3: 2, 4: 4, 12: 8}
    tags = {}
    for i in range(struct.unpack("<H", d[off:off + 2])[0]):
        tag, t, cnt, val = struct.unpack("<HHII", d[off + 2 + 12 * i:off + 14 + 12 * i])
        if t not in typ:
            continue
        n = size[t] * cnt
        raw = d[off + 10 + 12 * i:off + 14 + 12 * i] if n <= 4 else d[val:val + n]
        tags[tag] = struct.unpack("<%d%s" % (cnt, typ[t]), raw[:n])
    w, h, tw, th = tags[256][0], tags[257][0], tags[322][0], tags[323][0]
    assert tags[258][0] == 32 and tags[259][0] == 8 and tags[339][0] == 3 and tags[317][0] == 3
    out = np.zeros((((h + th - 1) // th) * th, ((w + tw - 1) // tw) * tw), np.float32)
    across = (w + tw - 1) // tw
    for k, (o, n) in enumerate(zip(tags[324], tags[325])):
        b = np.frombuffer(zlib.decompress(d[o:o + n]), np.uint8).reshape(th, 4 * tw)
        b = np.cumsum(b, axis=1, dtype=np.uint8)   # 浮動小数点の予測: バイトごとの差分を戻し、上位のバイトから並べ直す
        v = b.reshape(th, 4, tw).transpose(0, 2, 1).copy().view(">f4").reshape(th, tw)
        r, c = divmod(k, across)
        out[r * th:(r + 1) * th, c * tw:(c + 1) * tw] = v
    sx, sy = tags[33550][0], tags[33550][1]
    return out[:h, :w], tags[33922][3], tags[33922][4], sx, sy


def earth_box(work, row0, row1, col0, col1):
    """Copernicus DEM GLO-90 を 1200 ピクセル/度の行 row0..row1・列 col0..col1 に並べたもの（m。海は 0）"""
    lat = 90 - (np.arange(row0, row1) + 0.5) / EARTH_PPD
    lon = -180 + (np.arange(col0, col1) + 0.5) / EARTH_PPD
    out = np.zeros((len(lat), len(lon)))
    for la in range(int(np.floor(lat.min())), int(np.floor(lat.max())) + 1):
        for lo in range(int(np.floor(lon.min())), int(np.floor(lon.max())) + 1):
            name = "Copernicus_DSM_COG_30_%s%02d_00_%s%03d_00_DEM" % ("N" if la >= 0 else "S", abs(la), "E" if lo >= 0 else "W", abs(lo))
            path = os.path.join(work, name + ".tif")
            if not os.path.exists(path) and not os.path.exists(path + ".none"):
                url = "https://copernicus-dem-90m.s3.amazonaws.com/%s/%s.tif" % (name, name)
                try:
                    with urllib.request.urlopen(urllib.request.Request(url, headers=UA)) as r:
                        data = r.read()
                    open(path, "wb").write(data)
                except urllib.error.HTTPError as e:   # 海だけの所は区画が無い
                    if e.code not in (403, 404):
                        raise
                    open(path + ".none", "w").close()
            if not os.path.exists(path):
                continue
            v, lon0, lat0, sx, sy = tiff_float(path)
            jj = np.nonzero((lat >= la) & (lat < la + 1))[0]
            ii = np.nonzero((lon >= lo) & (lon < lo + 1))[0]
            y = np.clip(np.round((lat0 - lat[jj]) / sy - 0.5).astype(int), 0, v.shape[0] - 1)
            x = (lon[ii] - lon0) / sx - 0.5   # 東西に線形に補間（間隔が広い高緯度）
            x0 = np.clip(np.floor(x).astype(int), 0, v.shape[1] - 1)
            x1 = np.clip(x0 + 1, 0, v.shape[1] - 1)
            f = np.clip(x - x0, 0, 1)
            rows = v[y]
            out[np.ix_(jj, ii)] = rows[:, x0] * (1 - f) + rows[:, x1] * f
    return out


def moon_box(work, row0, row1, col0, col1):
    """SLDEM2015 の 512 ピクセル/度の行 row0..row1・列 col0..col1（m。半径 1737.4 km 基準）"""
    lat_n, lat_s = 90 - row0 / MOON_PPD, 90 - row1 / MOON_PPD
    lon_w = -180 + col0 / MOON_PPD
    band = int(np.floor(lat_s / 30)) * 30            # ファイルの南端の緯度
    lw = (lon_w % 360) // 45 * 45                     # ファイルの西端の経度（東経 0〜360）
    assert lat_n <= band + 30 and (lon_w % 360) + (col1 - col0) / MOON_PPD <= lw + 45, "box crosses an SLDEM2015 file"
    tag = lambda a: "%02d%s" % (abs(a), "N" if a > 0 else "S" if a < 0 else "N")
    name = "SLDEM2015_512_%s_%s_%03d_%03d_FLOAT.IMG" % ("00N" if band == 0 else tag(band) if band > 0 else tag(band),
                                                        tag(band + 30) if band + 30 != 0 else "00S", lw, lw + 45)
    width = 45 * MOON_PPD
    r0 = int(round((band + 30 - lat_n) * MOON_PPD))   # ファイルの中の行（画素の境目が揃う）
    c0 = int(round(((lon_w % 360) - lw) * MOON_PPD))
    path = os.path.join(work, "%s.rows%d-%d" % (name, r0, r0 + row1 - row0))
    if not os.path.exists(path):
        url = "https://imbrium.mit.edu/DATA/SLDEM2015/TILES/FLOAT_IMG/" + name
        rng = "bytes=%d-%d" % (r0 * width * 4, (r0 + row1 - row0) * width * 4 - 1)
        print("download", url, rng)
        with urllib.request.urlopen(urllib.request.Request(url, headers=dict(UA, Range=rng))) as r, open(path + ".part", "wb") as f:
            assert r.status == 206
            while True:
                b = r.read(1 << 22)
                if not b:
                    break
                f.write(b)
        os.rename(path + ".part", path)
    v = np.fromfile(path, "<f4").reshape(row1 - row0, width)
    return v[:, c0:c0 + col1 - col0].astype(np.float64) * 1000


def write(path, ppd, boxes, heights):
    """boxes: (区画の列, 行, 列の数, 行の数), heights: 地域ごとの標高 m（区画の数 × TILE の配列）"""
    tiles = []
    for (c0, r0, nc, nr), hm in zip(boxes, heights):
        v = np.clip(np.round((hm + 10000) * 2), 1, 65535).astype(np.int64)
        assert v.shape == (nr * TILE, nc * TILE)
        tiles += [encode_tile(v[r * TILE:(r + 1) * TILE, c * TILE:(c + 1) * TILE]) for r in range(nr) for c in range(nc)]
    head = 20 + 16 * len(boxes) + 8 * len(tiles)
    pos, index = head, b""
    for t in tiles:
        index += struct.pack("<II", pos, len(t))
        pos += len(t)
    with open(path, "wb") as f:
        f.write(b"EDP1" + struct.pack("<iiii", 360 * ppd, 180 * ppd, TILE, len(boxes)))
        for b in boxes:
            f.write(struct.pack("<iiii", *b))
        f.write(index + b"".join(tiles))
    print("done:", path, pos, "bytes")


def make(out, file, ppd, patches, sample):
    """地域ごとに区画の境目まで広げ、sample(行 0, 行 1, 列 0, 列 1) の標高（m）を書き出す"""
    deg = TILE / ppd   # 1 区画の度
    boxes, heights = [], []
    for name, s, n, west, east in patches:
        c0, c1 = int(np.floor((west + 180) / deg + 1e-9)), int(np.ceil((east + 180) / deg - 1e-9))
        r0, r1 = int(np.floor((90 - n) / deg + 1e-9)), int(np.ceil((90 - s) / deg - 1e-9))
        print("%s: tiles %d x %d" % (name, c1 - c0, r1 - r0))
        boxes.append((c0, r0, c1 - c0, r1 - r0))
        heights.append(sample(r0 * TILE, r1 * TILE, c0 * TILE, c1 * TILE))
    write(os.path.join(out, file), ppd, boxes, heights)


def main():
    args = [a for a in sys.argv[1:] if a not in ("earth", "moon", "mars")]
    bodies = [a for a in sys.argv[1:] if a in ("earth", "moon", "mars")] or ["earth", "moon", "mars"]
    work = args[0]
    out = args[1] if len(args) > 1 else "assets"
    os.makedirs(work, exist_ok=True)
    os.makedirs(out, exist_ok=True)
    if "earth" in bodies:
        make(out, "earth_patches.bin", EARTH_PPD, EARTH_PATCHES, lambda *b: earth_box(work, *b))
    if "moon" in bodies:
        make(out, "moon_patches.bin", MOON_PPD, MOON_PATCHES, lambda *b: moon_box(work, *b))
    if "mars" in bodies:
        path = fetch(work, MARS_URL)
        w, h, first = tiff_strips(path)
        src = np.memmap(path, dtype="<i2", mode="r", offset=first, shape=(h, w))
        make(out, "mars_patches.bin", MARS_PPD, MARS_PATCHES, lambda *b: box_rows(src, w, MARS_PPD, *b))


if __name__ == "__main__":
    main()
