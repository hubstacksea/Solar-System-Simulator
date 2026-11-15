#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""惑星・衛星・準惑星の実際の地図 (assets/maps.bin) を作る。

データ: USGS Astrogeology の天体地図サービス（WMS, https://planetarymaps.usgs.gov/）で配信している全球モザイク
（NASA の探査機の画像から USGS などが作成。米国政府の著作物でパブリックドメイン）と、火星の標高は
NASA PDS の MGS MOLA MEGDR（4 ピクセル/度）、水星・ケレス・ベスタ・冥王星・カロンの標高は USGS Astrogeology の
全球の標高データ（下の DEMS。利用条件は作成者の明記）。土星は NASA PDS 大気ノードの Cassini ISS の全球図
（Li, West, Jiang, Knowles 2023, doi:10.17189/rkkb-6y30）。天王星の衛星は JPL の Solar System Simulator
（https://space.jpl.nasa.gov/tmaps/uranus.html。Caltech/JPL/USGS、JPL の画像利用規定により許可なく利用可、
クレジット Courtesy NASA/JPL-Caltech）が配っている USGS の全球図。
いずれも正距円筒図法・東経が正・左端が経度 -180°・上端が北緯 90° で取得する（シミュレータの月の地図と同じ並び）。

出力形式（リトルエンディアン）:
  uint32 地図の数
  地図ごとに: char[32] 英語名（シミュレータの天体名と同じ）,
             uint8 種類（0: カラー, 1: 白黒（天体の色で着色）, 2: 白黒の模様だけ（明暗はシミュレータの塗り分けを使う）),
             uint8 標高（0: なし, 1: 0.5 m 単位で -10,000 m が 0, 2: 1 m 単位で -32,768 m が 0）, uint16 予備, float32 平均の明るさ（0〜1。天体の色に合わせる時に使う）,
             uint32 長さ + JPEG（地図。撮影されていない所は平均の明るさで埋める）,
             uint32 長さ + PNG（512x256 のグレー。撮影された範囲 = 255、境目はぼかす）,
             [標高ありなら uint32 長さ + PNG（グレー + アルファ = 16 bit の上位 / 下位バイト。単位と 0 の高さは
             標高の欄の値による。1 は月と同じ）]

使い方: python3 tools/make_planet_maps.py <作業ディレクトリ> [出力先=assets]
必要なもの: Pillow, NumPy（ダウンロード 約 2.5 GB。うち標高データが約 2.4 GB）
"""
import io
import os
import re
import shutil
import struct
import sys
import urllib.parse
import urllib.request

import numpy as np
from PIL import Image, ImageFilter

UA = "SolarSystemVoyager-asset-build/1.0 (https://github.com/hubstacksea/Solar-System-Simulator)"
WMS = "https://planetarymaps.usgs.gov/cgi-bin/mapserv"
# 英語名, 地図ファイル, レイヤー, 幅, 種類（0: カラー, 1: 白黒, 2: 模様だけ）, 元データ
# イアペトゥスのモザイクは明暗がならされていて、暗い半球と明るい半球の差が残っていないので模様だけ使う。
# 木星の雲は場所ごとに違う速さで流れるので、地図は大赤斑が経度 0°（画像の中央）に来るよう回して保存し、
# シミュレータが大赤斑の今の経度（体系 II）に合わせて回す。
# 土星は NASA PDS 大気ノードのカッシーニの全球図（2011-08-11。赤・緑・青のフィルター別の FITS、地図ファイルの欄に pds:）。
# 両極と、環の影で写っていない赤道〜南緯 17° の帯は手続き的な雲にする。
# 天王星・海王星・金星は、再配布できる全球図がない（天王星と金星は目に見える光ではほとんど模様もない）ので入れない。
# 天王星の 5 衛星は USGS の WMS に画像の層がないので、JPL の Solar System Simulator が配っている USGS の全球図
# （ボイジャー 2 号。1440x720 のグレー、東経が正・左端が経度 -180°。ミランダのアーデン・エルシノアの両コロナの位置で確認）
# を使う（地図ファイルの欄に jpl: とファイル名）。撮影されたのは南半球だけ（北半球は黒）
MAPS = [
    ("Mercury", "mercury/mercury_simp_cyl.map", "MESSENGER", 2048, 1, "MESSENGER MDIS"),
    ("Jupiter", "jupiter/jupiter_simp_cyl.map", "CASSINI", 3600, 0, "Cassini ISS, PIA07782 (December 2000)"),
    ("Saturn", "pds:Cassini_ISS_%s_Saturn_global_map.fits", "", 3600, 0, "Cassini ISS, 2011-08-11 (PDS Atmospheres Node)"),
    ("Mars", "mars/mars_simp_cyl.map", "MDIM21_color", 4096, 0, "Viking MDIM 2.1 (colorized by NASA Ames)"),
    ("Ceres", "asteroid_belt/ceres_simp_cyl.map", "Ceres_FC_global", 2048, 1, "Dawn FC"),
    ("Vesta", "asteroid_belt/vesta_simp_cyl.map", "Dawn_HAMO_global", 2048, 1, "Dawn FC HAMO"),
    ("Io", "jupiter/io_simp_cyl.map", "SSI_VGR_color", 2048, 0, "Galileo SSI / Voyager"),
    ("Europa", "jupiter/europa_simp_cyl.map", "GALILEO_VOYAGER", 2048, 1, "Galileo SSI / Voyager"),
    ("Ganymede", "jupiter/ganymede_simp_cyl.map", "GALILEO_VOYAGER", 2048, 1, "Galileo SSI / Voyager"),
    ("Callisto", "jupiter/callisto_simp_cyl.map", "GALILEO_VOYAGER", 2048, 1, "Galileo SSI / Voyager"),
    ("Mimas", "saturn/mimas_simp_cyl.map", "CASSINI_MIMAS_MOSAIC", 1024, 1, "Cassini ISS"),
    ("Enceladus", "saturn/enceladus_simp_cyl.map", "CASSINI", 2048, 1, "Cassini ISS"),
    ("Tethys", "saturn/tethys_simp_cyl.map", "CASSINI", 2048, 1, "Cassini ISS"),
    ("Dione", "saturn/dione_simp_cyl.map", "CASSINI_VOYAGER", 2048, 1, "Cassini ISS / Voyager"),
    ("Rhea", "saturn/rhea_simp_cyl.map", "CASSINI_VOYAGER", 2048, 1, "Cassini ISS / Voyager"),
    ("Iapetus", "saturn/iapetus_simp_cyl.map", "CASSINI_VOYAGER", 2048, 2, "Cassini ISS / Voyager"),
    ("Miranda", "jpl:ura5vuu2.jpg", "", 1440, 1, "Voyager 2 (USGS, via JPL Solar System Simulator)"),
    ("Ariel", "jpl:ura1vuu2.jpg", "", 1440, 1, "Voyager 2 (USGS, via JPL Solar System Simulator)"),
    ("Umbriel", "jpl:ura2vuu2.jpg", "", 1440, 1, "Voyager 2 (USGS, via JPL Solar System Simulator)"),
    ("Titania", "jpl:ura3vuu2.jpg", "", 1440, 1, "Voyager 2 (USGS, via JPL Solar System Simulator)"),
    ("Oberon", "jpl:ura4vuu2.jpg", "", 1440, 1, "Voyager 2 (USGS, via JPL Solar System Simulator)"),
    ("Triton", "neptune/triton_simp_cyl.map", "TRITON_VOYAGER2", 1024, 1, "Voyager 2"),
    ("Pluto", "pluto/pluto_simp_cyl.map", "NEWHORIZONS_PLUTO_MOSAIC", 2048, 1, "New Horizons LORRI / MVIC"),
    ("Charon", "pluto/charon_simp_cyl.map", "NEWHORIZONS_CHARON_MOSAIC", 1024, 0, "New Horizons LORRI / MVIC"),
]
MOLA = "https://pds-geosciences.wustl.edu/mgs/mgs-m-mola-5-megdr-l3-v1/mgsl_300x/meg004/megt90n000cb.img"


def fetch(work, name, url):
    path = os.path.join(work, name)
    if not os.path.exists(path):
        print("download", url[:150])
        req = urllib.request.Request(url, headers={"User-Agent": UA})
        with urllib.request.urlopen(req, timeout=600) as r, open(path + ".part", "wb") as f:
            shutil.copyfileobj(r, f, 1 << 20)
        os.rename(path + ".part", path)
    return path


JPL_TMAPS = "https://space.jpl.nasa.gov/tmaps/pix/"
PDS_ATMOS = "https://atmos.nmsu.edu/PDS/data/PDS4/co_iss_global-maps/data_derived/"
SATURN_COLOR = (0.93, 0.86, 0.66)   # src/solar_data.h の土星の色（地図の色合いをここにそろえ、手続き的な雲とつなぐ）
LUM_CONTRAST, CHROMA = 0.7, 0.5


def pds_saturn(work, mapfile, w):
    """カッシーニの土星の全球図: 1801x3601 の float32（ビッグエンディアン、ヘッダー 2880 バイト）。
    0.1°ごと、上端が北緯 90°、左端が西経 360°（= 東経 0°）で右へ東経が増える。写っていない所は 0 か -999.9"""
    ch = []
    for f in ("RED", "GRN", "BL1"):
        name = mapfile[4:] % f
        a = np.fromfile(fetch(work, name, PDS_ATMOS + name), dtype=">f4", offset=2880, count=1801 * 3601)
        ch.append(a.reshape(1801, 3601)[:-1, :-1].astype(np.float32))
    a = np.stack(ch, -1)
    a[:, 0] = (a[:, 1] + a[:, -1]) / 2   # 経度 0° の列は継ぎ目で値がずれている
    valid = (a > 0).all(axis=2)
    # 赤道から南緯 6° までは環やその影がかかって明るさが段になっている（その南は写っていない）ので使わない
    valid[900:960] = False
    # 写っていない所の縁は画像の端で乱れているので 0.5° 削る
    v = Image.fromarray(valid.astype(np.uint8) * 255, "L").filter(ImageFilter.MinFilter(11))
    valid = np.asarray(v) > 0
    # 白の釣り合い: フィルターごとの平均を土星の色に合わせる
    a = a / a[valid].mean(axis=0)
    # 3 つのフィルターは撮影時刻が少しずつ違い、雲が流れて色がずれるので、模様は 3 色の平均の明暗だけ使い、
    # 色は緯度ごとの平均（帯ごとの色の違い）を半分に弱めて付ける
    lum = a.mean(axis=2)
    lum = 1 + (lum / lum[valid].mean() - 1) * LUM_CONTRAST
    chroma = np.ones_like(a)
    for y in range(a.shape[0]):
        if valid[y].sum() > 100:
            c = a[y][valid[y]].mean(axis=0)
            chroma[y] = 1 + (c / c.mean() - 1) * CHROMA
    a = lum[:, :, None] * chroma * np.array(SATURN_COLOR, np.float32)
    a = a / np.percentile(a[valid], 99.9) * 0.97
    a[~valid] = 0
    a = np.roll(np.clip(a, 0, 1), 1800, axis=1)   # 左端を経度 -180° に
    img = Image.fromarray((a * 255 + 0.5).astype(np.uint8), "RGB")
    return img.resize((w, w // 2), Image.LANCZOS) if img.width != w else img


def wms(work, mapfile, layer, w):
    if mapfile.startswith("pds:"):
        return pds_saturn(work, mapfile, w)
    if mapfile.startswith("jpl:"):
        name = mapfile[4:]
        return Image.open(fetch(work, "jpl_" + name, JPL_TMAPS + name)).convert("RGB")
    q = {"map": "/maps/" + mapfile, "SERVICE": "WMS", "VERSION": "1.1.1", "REQUEST": "GetMap", "LAYERS": layer,
         "STYLES": "", "SRS": "EPSG:4326", "BBOX": "-180,-90,180,90", "WIDTH": str(w), "HEIGHT": str(w // 2),
         "FORMAT": "image/png"}
    path = fetch(work, "%s_%s_%d.png" % (mapfile.split("/")[1].split("_")[0], layer, w), WMS + "?" + urllib.parse.urlencode(q))
    return Image.open(path).convert("RGB")


def mars_height(work):
    """MOLA MEGDR 4 ピクセル/度: 1440x720, ビッグエンディアン int16 [m]、経度 0〜360°E、上端が北緯 90°"""
    raw = np.fromfile(fetch(work, os.path.basename(MOLA), MOLA), dtype=">i2").reshape(720, 1440).astype(np.int32)
    raw = np.roll(raw, 720, axis=1)   # 左端を経度 -180° に
    v = np.clip((raw + 10000) * 2, 0, 65535).astype(np.uint16)
    hi, lo = Image.fromarray((v >> 8).astype(np.uint8), "L"), Image.fromarray((v & 255).astype(np.uint8), "L")
    buf = io.BytesIO()
    Image.merge("LA", (hi, lo)).save(buf, "PNG", optimize=True)
    return buf.getvalue()


# 標高: USGS Astrogeology の全球の標高データ（NASA の探査機の画像から作成。USGS の S3 にある mosaic/ から取得）。
# 英語名: ファイル, 出力の幅, 値 → 中心からの半径 m, 左端の経度（0 か -180）
# 値の定義は各ファイルのラベル（.lbl）による。水星は 2439.4 km の球からの高さ（0.5 m 単位）、ケレスは 470 km の球からの
# 高さ m、ベスタは半径 m、冥王星・カロンは 1188.3 km・606 km の球からの高さ m。冥王星・カロンは New Horizons が近くで
# 見た半球だけで、ほかは -32768（データなし）
S3_MOSAIC = "https://asc-pds-services.s3.us-west-2.amazonaws.com/mosaic/"
DEMS = {
    "Mercury": ("Mercury_Messenger_USGS_DEM_Global_665m_v2.tif", 2048, lambda v: 2439400 + v * 0.5, 0),
    "Ceres": ("Ceres_Dawn_FC_HAMO_DTM_DLR_Global_60ppd_Oct2016.tif", 1024, lambda v: 470000 + v, 0),
    "Vesta": ("Vesta_Dawn_HAMO_DTM_DLR_Global_48ppd.cub", 1024, lambda v: v, -180),
    "Pluto": ("Pluto_NewHorizons_Global_DEM_300m_Jul2017_16bit.tif", 2048, lambda v: 1188300 + v, 0),
    "Charon": ("Charon_NewHorizons_Global_DEM_300m_Jul2017_16bit.tif", 1024, lambda v: 606000 + v, -180),
}
# シミュレータの赤道半径 km と扁平率（src/solar_data.h）。地図は、球面の点 (x, y, z) の緯度 asin(y) で引き、
# 楕円体の点 (x, y (1 − 扁平率), z) に貼るので、扁平な天体では地図の行の緯度と実際の中心からの緯度がずれる。
# 標高と画像はこのずれの分だけ行を引き直す（ベスタは最大 7° ほど）
SHAPE = {"Mercury": (2439.7, 0.0), "Ceres": (482.1, 0.075), "Vesta": (286.3, 0.22), "Pluto": (1188.3, 0.0), "Charon": (606.0, 0.0)}


def read_tiff(path):
    """無圧縮・1 行 1 ストリップの int16 の GeoTIFF（行が隙間なく並んでいることを確かめる）"""
    with open(path, "rb") as f:
        h = f.read(8)
        assert h[:2] == b"II" and struct.unpack("<H", h[2:4])[0] == 42, path
        f.seek(struct.unpack("<I", h[4:8])[0])
        tags = {}
        for _ in range(struct.unpack("<H", f.read(2))[0]):
            tag, typ, cnt, val = struct.unpack("<HHII", f.read(12))
            tags[tag] = (typ, cnt, val)
        w, h = tags[256][2] & 0xFFFF, tags[257][2] & 0xFFFF
        assert tags[259][2] & 0xFFFF == 1 and tags[258][2] & 0xFFFF == 16 and tags[339][2] & 0xFFFF == 2, path
        typ, cnt, off = tags[273]
        f.seek(off)
        offs = np.frombuffer(f.read(4 * cnt), "<u4").astype(np.int64)
    assert cnt == h and np.all(np.diff(offs) == w * 2), path
    return np.memmap(path, "<i2", "r", int(offs[0]), (h, w)), -32768


def read_cub(path):
    """ISIS のキューブ（float32、タイル並び）。データなしの値は ISIS の NULL（-3.4028226550889045e+38）"""
    head = open(path, "rb").read(65536).decode("latin1")
    g = lambda k: int(re.search(r"\n\s*" + k + r"\s*=\s*(\d+)", head).group(1))
    sb, w, h, tw, th = g("StartByte"), g("Samples"), g("Lines"), g("TileSamples"), g("TileLines")
    assert "Format      = Tile" in head and "Type       = Real" in head and w % tw == 0 and h % th == 0, path
    m = np.memmap(path, "<f4", "r", sb - 1, (h // th, w // tw, th, tw))
    return np.asarray(m).transpose(0, 2, 1, 3).reshape(h, w), -3e38


def fill_holes(v, ok):
    """データのない所を、まわりの値からなめらかにつなぐ（縮小しながら平均で埋め、拡大して戻す）"""
    if ok.all():
        return v
    h, w = v.shape
    if h < 2 or w < 2:
        return np.where(ok, v, v[ok].mean() if ok.any() else 0.0)
    pv = np.pad(np.where(ok, v, 0.0), ((0, h % 2), (0, w % 2)), mode="edge")   # 奇数なら端を複製して偶数に
    pk = np.pad(ok, ((0, h % 2), (0, w % 2)), mode="edge")
    H2, W2 = pv.shape[0] // 2, pv.shape[1] // 2
    s = pv.reshape(H2, 2, W2, 2).sum((1, 3))
    n = pk.reshape(H2, 2, W2, 2).sum((1, 3))
    sv = fill_holes(s / np.maximum(n, 1), n > 0)
    up = np.asarray(Image.fromarray(sv.astype(np.float32), "F").resize((W2 * 2, H2 * 2), Image.BILINEAR))[:h, :w]
    return np.where(ok, v, up)


def remap_rows(a, flat):
    """地図の行の緯度 → 中心からの緯度の行を引き直す（扁平な天体。行の方向に線形補間）"""
    if flat == 0:
        return a
    h = a.shape[0]
    lat = np.radians(90 - (np.arange(h) + 0.5) * 180 / h)
    latc = np.arctan((1 - flat) * np.tan(lat))
    y = np.clip((np.pi / 2 - latc) / np.pi * h - 0.5, 0, h - 1)
    y0 = np.floor(y).astype(int)
    y1 = np.minimum(y0 + 1, h - 1)
    f = (y - y0).reshape((h,) + (1,) * (a.ndim - 1))
    return a[y0] * (1 - f) + a[y1] * f


def dem_height(work, name):
    """標高の PNG（シミュレータの楕円体からの高さ。グレー + アルファ = 16 bit、0.5 m 単位で -10,000 m を 0。
    それに収まらない天体（ベスタ・カロン）は 1 m 単位で -32,768 m を 0）と、その形式（1 か 2）"""
    fn, w, radius, lon0 = DEMS[name]
    path = fetch(work, fn, S3_MOSAIC + fn)
    raw, nodata = read_cub(path) if fn.endswith(".cub") else read_tiff(path)
    H, W = raw.shape
    # 出力の 2 倍の細かさまで、データのある画素の平均で縮める（行のまとまりごとに）
    k = max(1, H // w)
    h2, w2 = H // k, W // k
    acc = np.zeros((h2, w2))
    cnt = np.zeros((h2, w2))
    for y in range(h2):
        blk = np.asarray(raw[y * k:(y + 1) * k, :w2 * k]).astype(np.float64)
        ok = blk > nodata + 1 if nodata < 0 else blk != nodata
        r = np.where(ok, radius(blk), 0.0)
        acc[y] = r.reshape(k, w2, k).sum((0, 2))
        cnt[y] = ok.reshape(k, w2, k).sum((0, 2))
    ok = cnt > 0
    r = acc / np.maximum(cnt, 1)
    if lon0 == 0:
        r, ok = np.roll(r, w2 // 2, axis=1), np.roll(ok, w2 // 2, axis=1)   # 左端を経度 -180° に
    r = fill_holes(np.tile(r, 3), np.tile(ok, 3))[:, w2:2 * w2]   # 東西は一周つながるように 3 つ並べて埋める
    # 行の数が k で割り切れない分（最大 k − 1 行）は下の端を捨てている。その分の緯度のずれは 1 画素未満
    a, flat = SHAPE[name]
    r = remap_rows(r, flat)
    img = Image.fromarray(r.astype(np.float32), "F").resize((w, w // 2), Image.BOX)
    r = np.asarray(img).astype(np.float64)
    lat = np.radians(90 - (np.arange(w // 2) + 0.5) * 180 / (w // 2))
    rell = a * 1000 * np.sqrt(np.cos(lat) ** 2 + ((1 - flat) * np.sin(lat)) ** 2)   # 楕円体の点の中心からの距離 m
    hgt = r - rell[:, None]
    covered = np.asarray(Image.fromarray((ok * 255).astype(np.uint8), "L").resize((w, w // 2), Image.BOX)).mean() / 255
    fmt = 1 if hgt.min() >= -10000 and hgt.max() <= 22767 else 2
    v = np.round((hgt + 10000) * 2) if fmt == 1 else np.round(hgt + 32768)
    clipped = ((v < 0) | (v > 65535)).mean()
    v = np.clip(v, 0, 65535).astype(np.uint16)
    hi, lo = Image.fromarray((v >> 8).astype(np.uint8), "L"), Image.fromarray((v & 255).astype(np.uint8), "L")
    buf = io.BytesIO()
    Image.merge("LA", (hi, lo)).save(buf, "PNG", optimize=True)
    print("  %s height %dx%d from %dx%d: %.1f .. %.1f km above the ellipsoid, %s m steps, data covers %.0f%%%s"
          % (name, w, w // 2, W, H, hgt.min() / 1000, hgt.max() / 1000, "0.5" if fmt == 1 else "1", covered * 100,
             ", CLIPPED %.3f%%" % (clipped * 100) if clipped else ""))
    return buf.getvalue(), fmt


def main():
    work = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else "assets"
    os.makedirs(work, exist_ok=True)
    blob = struct.pack("<I", len(MAPS))
    for name, mapfile, layer, w, kind, src in MAPS:
        img = wms(work, mapfile, layer, w)
        a = np.asarray(img).astype(np.float32) / 255
        if name in SHAPE:
            a = remap_rows(a, SHAPE[name][1])
        if name == "Jupiter":
            # 大赤斑: 南緯 17〜27° の帯で一番赤い所（赤 − 青 を経度方向にならして最大の所）
            h, wd = a.shape[:2]
            band = a[int(h * (90 + 17) / 180):int(h * (90 + 27) / 180)]
            red = (band[:, :, 0] - band[:, :, 2]).mean(axis=0)
            k = np.ones(wd // 60) / (wd // 60)
            red = np.convolve(np.concatenate([red[-len(k):], red, red[:len(k)]]), k, "same")[len(k):-len(k)]
            x = int(np.argmax(red))
            print("  Great Red Spot at longitude %.1f E in the source map" % (x / wd * 360 - 180))
            a = np.roll(a, wd // 2 - x, axis=1)
        lum = a.mean(axis=2)
        # 撮影されていない所: 真っ黒か真っ白（サービスが埋めた色）
        valid = (lum > 0.01) & (a.min(axis=2) < 0.985)
        mean = float(lum[valid].mean()) if valid.any() else 0.5
        a[~valid] = a[valid].mean(axis=0) if valid.any() else 0.5
        filled = Image.fromarray((a * 255 + 0.5).astype(np.uint8), "RGB")
        buf = io.BytesIO()
        (filled.convert("L") if kind else filled).save(buf, "JPEG", quality=85, optimize=True)
        jpg = buf.getvalue()
        m = Image.fromarray((valid * 255).astype(np.uint8), "L").resize((512, 256), Image.BILINEAR)
        m = m.filter(ImageFilter.GaussianBlur(1.5))
        mbuf = io.BytesIO()
        m.save(mbuf, "PNG", optimize=True)
        mask = mbuf.getvalue()
        height, fmt = (mars_height(work), 1) if name == "Mars" else dem_height(work, name) if name in DEMS else (b"", 0)
        blob += struct.pack("<32sBBHfI", name.encode(), kind, fmt, 0, mean, len(jpg)) + jpg
        blob += struct.pack("<I", len(mask)) + mask
        if height:
            blob += struct.pack("<I", len(height)) + height
        print("%-10s %-28s %4dx%-4d %s mean %.3f  coverage %.0f%%  %d KB%s"
              % (name, layer, img.width, img.height, ("color", "gray ", "relief")[kind], mean, valid.mean() * 100,
                 len(jpg) // 1024, "  + height %d KB" % (len(height) // 1024) if height else ""))
    with open(os.path.join(out, "maps.bin"), "wb") as f:
        f.write(blob)
    print("wrote", len(blob), "bytes")


if __name__ == "__main__":
    main()
