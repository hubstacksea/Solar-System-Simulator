#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""惑星・衛星・準惑星の実際の地図 (assets/maps.bin) を作る。

データ: USGS Astrogeology の天体地図サービス（WMS, https://planetarymaps.usgs.gov/）で配信している全球モザイク
（NASA の探査機の画像から USGS などが作成。米国政府の著作物でパブリックドメイン）と、火星の標高は
NASA PDS の MGS MOLA MEGDR（4 ピクセル/度）。土星は NASA PDS 大気ノードの Cassini ISS の全球図
（Li, West, Jiang, Knowles 2023, doi:10.17189/rkkb-6y30）。天王星の衛星は JPL の Solar System Simulator
（https://space.jpl.nasa.gov/tmaps/uranus.html。Caltech/JPL/USGS、JPL の画像利用規定により許可なく利用可、
クレジット Courtesy NASA/JPL-Caltech）が配っている USGS の全球図。
いずれも正距円筒図法・東経が正・左端が経度 -180°・上端が北緯 90° で取得する（シミュレータの月の地図と同じ並び）。

出力形式（リトルエンディアン）:
  uint32 地図の数
  地図ごとに: char[32] 英語名（シミュレータの天体名と同じ）,
             uint8 種類（0: カラー, 1: 白黒（天体の色で着色）, 2: 白黒の模様だけ（明暗はシミュレータの塗り分けを使う）),
             uint8 標高あり, uint16 予備, float32 平均の明るさ（0〜1。天体の色に合わせる時に使う）,
             uint32 長さ + JPEG（地図。撮影されていない所は平均の明るさで埋める）,
             uint32 長さ + PNG（512x256 のグレー。撮影された範囲 = 255、境目はぼかす）,
             [標高ありなら uint32 長さ + PNG（グレー + アルファ = 16 bit の上位 / 下位バイト、0.5 m 単位、
             -10,000 m を 0 とする。月と同じ）]

使い方: python3 tools/make_planet_maps.py <作業ディレクトリ> [出力先=assets]
必要なもの: Pillow, NumPy（ダウンロード 約 105 MB）
"""
import io
import os
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
            f.write(r.read())
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


def main():
    work = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else "assets"
    os.makedirs(work, exist_ok=True)
    blob = struct.pack("<I", len(MAPS))
    for name, mapfile, layer, w, kind, src in MAPS:
        img = wms(work, mapfile, layer, w)
        a = np.asarray(img).astype(np.float32) / 255
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
        height = mars_height(work) if name == "Mars" else b""
        blob += struct.pack("<32sBBHfI", name.encode(), kind, 1 if height else 0, 0, mean, len(jpg)) + jpg
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
