#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""地表の素材の地図 (assets/moon_rock.png, assets/mars_ti.png) を作る。地図より細かい岩塊・砂のさざ波などの模様の多さに使う。

データ（NASA PDS Geosciences Node。米国政府の著作物でパブリックドメイン）:
  - 月: LRO Diviner の岩の多さ（Bandfield ほか 2011, J. Geophys. Res. 116, E00H02）dgdr_ra_avg_cyl_032_img.img
    （32 ピクセル/度、南北 60° まで。値は地表を 1 m ほどより大きな岩が覆う割合）
  - 火星: MGS TES の夜の熱慣性（Putzig & Mellon 2007, Icarus 191, 68–94）global_ti_night_2007.img
    （20 ピクセル/度。J/(m² K s^½)。低い所は細かい塵、中ほどは砂、高い所は岩・岩盤・固まった地面）
  - 火星の砂丘地帯（USGS Mars Global Digital Dune Database。パブリックドメイン）: 南北 65° まで（Hayward ほか 2007,
    Open-File Report 2007-1158）、北極域（Hayward ほか 2010, OFR 2010-1170）、南極域（Hayward ほか 2012, OFR 2012-1259）の
    中〜大きな砂丘地帯の多角形（惑星中心緯度）。極域の分は数 GB の zip の中にあるので、多角形のファイルだけを範囲指定で読む

出力（正距円筒図法。左端が経度 -180°、上端が北緯 90°）:
  moon_rock.png  2048x1024 グレー  v = sqrt(岩の割合 / 0.5) × 255（観測の無い所は周りの値で埋め、南北 60° より極側は全体の中央値へ）
  mars_ti.png    2048x1024 グレー + アルファ
                 グレー:   log(熱慣性 / 20) / log(125) × 255（20〜2500。観測の無い所は周りの値で埋める）
                 アルファ: 画素のうち砂丘地帯が占める割合 × 255（8192x4096 で塗って縮める）

使い方: python3 tools/make_surface_props.py <作業ディレクトリ> [出力先=assets]
必要なもの: Pillow と NumPy（ダウンロード 約 140 MB）
"""
import io
import os
import struct
import sys
import urllib.request
import zipfile

import numpy as np
from PIL import Image, ImageDraw

MOON_URL = ("https://pds-geosciences.wustl.edu/lro/urn-nasa-pds-lro_diviner_derived1/data_derived_gdr_l3/"
            "cylindrical/img/dgdr_ra_avg_cyl_032_img.img")
MARS_URL = "https://pds-geosciences.wustl.edu/mgs/mgs-m-tes-5-timap-v1/mgst_9001/data/global_ti_night_2007.img"
DUNE_SRC = [   # (zip, zip の中の多角形のファイル（拡張子 .shp を除く）)
    ("https://pubs.usgs.gov/of/2007/1158/Shapefiles.zip", "Shapefiles/Geocentric/Dune_Field"),
    ("https://pubs.usgs.gov/of/2010/1170/NP_Dunes_DVD.zip", "NP_Shapefiles/Geocentric/NP_Dune_Field"),
    ("https://pubs.usgs.gov/of/2012/1259/SP_GIS_Download_Package.zip",
     "SP_GIS_Download_Package/SP_Shapefiles/Geocentric/SP_Dune_Field_Geog"),
]
TI_MIN, TI_MAX = 20.0, 2500.0
RA_MAX = 0.5
UA = {"User-Agent": "curl/8.5.0"}   # pubs.usgs.gov は Python の既定の名前を断る


def fetch(work, url):
    path = os.path.join(work, os.path.basename(url))
    if not os.path.exists(path):
        print("download", url)
        urllib.request.urlretrieve(url, path + ".part")
        os.rename(path + ".part", path)
    return path


class RemoteFile(io.RawIOBase):
    """HTTP の範囲指定で読むファイル（大きな zip から一部だけを取り出すため）"""
    def __init__(self, url):
        super().__init__()
        self.url, self.pos = url, 0
        head = urllib.request.urlopen(urllib.request.Request(url, method="HEAD", headers=UA))
        self.size = int(head.headers["Content-Length"])

    def readable(self):
        return True

    def seekable(self):
        return True

    def seek(self, off, whence=0):
        self.pos = (off, self.pos + off, self.size + off)[whence]
        return self.pos

    def tell(self):
        return self.pos

    def readinto(self, b):
        n = min(len(b), self.size - self.pos)
        if n <= 0:
            return 0
        req = urllib.request.Request(self.url, headers={**UA, "Range": "bytes=%d-%d" % (self.pos, self.pos + n - 1)})
        d = urllib.request.urlopen(req).read()
        b[:len(d)] = d
        self.pos += len(d)
        return len(d)


def fetch_shp(work, url, member):
    path = os.path.join(work, os.path.basename(member) + ".shp")
    if not os.path.exists(path):
        print("download", url, member + ".shp")
        with zipfile.ZipFile(io.BufferedReader(RemoteFile(url), 1 << 20)) as z, open(path + ".part", "wb") as f:
            f.write(z.read(member + ".shp"))
        os.rename(path + ".part", path)
    return path


def polygons(path):
    """シェープファイルの多角形の輪を (経度, 緯度) の配列で返す"""
    d = open(path, "rb").read()
    pos = 100
    while pos < len(d):
        clen = struct.unpack(">i", d[pos + 4:pos + 8])[0] * 2
        rec = d[pos + 8:pos + 8 + clen]
        pos += 8 + clen
        if struct.unpack("<i", rec[:4])[0] != 5:
            continue
        nparts, npts = struct.unpack("<2i", rec[36:44])
        idx = list(struct.unpack("<%di" % nparts, rec[44:44 + 4 * nparts])) + [npts]
        pts = np.frombuffer(rec[44 + 4 * nparts:44 + 4 * nparts + 16 * npts], "<f8").reshape(-1, 2)
        yield [pts[idx[k]:idx[k + 1]] for k in range(nparts)]


def dune_cover(work, w, h):
    """砂丘地帯が画素に占める割合（w x h の 4 倍の細かさで塗って縮める）"""
    W, H = w * 4, h * 4
    img = Image.new("L", (W, H))
    dr = ImageDraw.Draw(img)
    for url, member in DUNE_SRC:
        for rings in polygons(fetch_shp(work, url, member)):
            rs = []
            for r in rings:
                lon = np.degrees(np.unwrap(np.radians(r[:, 0])))   # 経度 ±180° をまたぐ輪をつなげる
                lat = r[:, 1]
                hole = np.sum(lon * np.roll(lat, -1) - np.roll(lon, -1) * lat) > 0   # 反時計回りは穴
                rs.append((hole, lon, lat))
            for hole, lon, lat in sorted(rs, key=lambda x: x[0]):   # 外周を塗ってから穴を抜く
                for sh in (-360, 0, 360):
                    if lon.max() + sh >= -180 and lon.min() + sh <= 180:
                        dr.polygon([((x + sh + 180) / 360 * W, (90 - y) / 180 * H) for x, y in zip(lon, lat)],
                                   fill=0 if hole else 255)
    return np.asarray(img.resize((w, h), Image.BOX), dtype=np.float64) / 255


def box_blur(a, r):
    """幅 2r+1 の箱型のぼかし（経度方向は左右がつながる）"""
    p = np.concatenate([a[:, -r:], a, a[:, :r]], axis=1)
    c = np.cumsum(np.pad(p, ((0, 0), (1, 0))), axis=1)
    a = (c[:, 2 * r + 1:] - c[:, :-2 * r - 1]) / (2 * r + 1)
    p = np.pad(a, ((r, r), (0, 0)), mode="edge")
    c = np.cumsum(np.pad(p, ((1, 0), (0, 0))), axis=0)
    return (c[2 * r + 1:] - c[:-2 * r - 1]) / (2 * r + 1)


def shrink(v, ok, w, h):
    """観測のある画素だけを平均して w x h に縮め、観測の無い所は周りの値で埋める"""
    vs = np.asarray(Image.fromarray((v * ok).astype(np.float32), "F").resize((w, h), Image.BOX), dtype=np.float64)
    ws = np.asarray(Image.fromarray(ok.astype(np.float32), "F").resize((w, h), Image.BOX), dtype=np.float64)
    r = 2
    while (ws < 0.05).any():
        vb, wb = box_blur(vs, r), box_blur(ws, r)
        hole = ws < 0.05
        vs[hole], ws[hole] = vb[hole], wb[hole]
        r *= 2
    return vs / ws


def moon(work, out):
    ra = np.fromfile(fetch(work, MOON_URL), dtype="<i2").reshape(3840, 11520)
    ok = ra != -32768
    v = np.where(ok, ra * 0.001, 0.0).clip(0, RA_MAX)
    v, ok = np.roll(v, 5760, axis=1), np.roll(ok, 5760, axis=1)   # 左端を経度 0° から -180° へ
    W, H = 2048, 1024
    top, bot = H // 6, H - H // 6                                    # 南北 60° の帯（全体の高さの 2/3）
    band = shrink(v, ok, W, bot - top)
    med = float(np.median(v[ok]))
    full = np.full((H, W), med)
    full[top:bot] = band
    lat = 90 - (np.arange(H) + 0.5) * 180 / H
    k = np.clip((np.abs(lat) - 55) / 5, 0, 1)[:, None]              # 55〜60° で中央値へなじませる
    full = full * (1 - k) + med * k
    img = (np.sqrt(full / RA_MAX) * 255 + 0.5).clip(0, 255).astype(np.uint8)
    Image.fromarray(img, "L").save(os.path.join(out, "moon_rock.png"), optimize=True)
    print("moon_rock.png  median %.4f  max %.3f" % (med, full.max()))


def mars(work, out):
    ti = np.fromfile(fetch(work, MARS_URL), dtype=">i2").reshape(3600, 7200).astype(np.float64)
    ok = ti > 0
    v = np.log(np.clip(ti, TI_MIN, TI_MAX) / TI_MIN) / np.log(TI_MAX / TI_MIN)
    full = shrink(np.where(ok, v, 0.0), ok, 2048, 1024)
    dune = dune_cover(work, 2048, 1024)
    img = np.stack([full, dune], axis=-1)
    img = (img * 255 + 0.5).clip(0, 255).astype(np.uint8)
    Image.fromarray(img, "LA").save(os.path.join(out, "mars_ti.png"), optimize=True)
    lat = np.radians(90 - (np.arange(1024) + 0.5) * 180 / 1024)
    print("mars_ti.png  median TI %.0f  dune fields %.2f%% of the surface" % (
        TI_MIN * (TI_MAX / TI_MIN) ** np.median(full), 100 * (dune * np.cos(lat)[:, None]).sum() / (np.cos(lat).sum() * 2048)))


def main():
    work = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else "assets"
    os.makedirs(work, exist_ok=True)
    os.makedirs(out, exist_ok=True)
    moon(work, out)
    mars(work, out)


if __name__ == "__main__":
    main()
