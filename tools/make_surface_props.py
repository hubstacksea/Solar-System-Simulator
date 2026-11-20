#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""地表の素材の地図 (assets/moon_rock.png, assets/mars_ti.png) を作る。地図より細かい岩塊・砂のさざ波などの模様の多さに使う。

データ（NASA PDS Geosciences Node。米国政府の著作物でパブリックドメイン）:
  - 月: LRO Diviner の岩の多さ（Bandfield ほか 2011, J. Geophys. Res. 116, E00H02）dgdr_ra_avg_cyl_032_img.img
    （32 ピクセル/度、南北 60° まで。値は地表を 1 m ほどより大きな岩が覆う割合）
  - 火星: MGS TES の夜の熱慣性（Putzig & Mellon 2007, Icarus 191, 68–94）global_ti_night_2007.img
    （20 ピクセル/度。J/(m² K s^½)。低い所は細かい塵、中ほどは砂、高い所は岩・岩盤・固まった地面）

出力（正距円筒図法。左端が経度 -180°、上端が北緯 90°。8 bit グレーの PNG）:
  moon_rock.png  2048x1024  v = sqrt(岩の割合 / 0.5) × 255（観測の無い所は周りの値で埋め、南北 60° より極側は全体の中央値へ）
  mars_ti.png    2048x1024  v = log(熱慣性 / 20) / log(125) × 255（20〜2500。観測の無い所は周りの値で埋める）

使い方: python3 tools/make_surface_props.py <作業ディレクトリ> [出力先=assets]
必要なもの: Pillow と NumPy（ダウンロード 約 140 MB）
"""
import os
import sys
import urllib.request

import numpy as np
from PIL import Image

MOON_URL = ("https://pds-geosciences.wustl.edu/lro/urn-nasa-pds-lro_diviner_derived1/data_derived_gdr_l3/"
            "cylindrical/img/dgdr_ra_avg_cyl_032_img.img")
MARS_URL = "https://pds-geosciences.wustl.edu/mgs/mgs-m-tes-5-timap-v1/mgst_9001/data/global_ti_night_2007.img"
TI_MIN, TI_MAX = 20.0, 2500.0
RA_MAX = 0.5


def fetch(work, url):
    path = os.path.join(work, os.path.basename(url))
    if not os.path.exists(path):
        print("download", url)
        urllib.request.urlretrieve(url, path + ".part")
        os.rename(path + ".part", path)
    return path


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
    img = (full * 255 + 0.5).clip(0, 255).astype(np.uint8)
    Image.fromarray(img, "L").save(os.path.join(out, "mars_ti.png"), optimize=True)
    print("mars_ti.png  median TI %.0f" % (TI_MIN * (TI_MAX / TI_MIN) ** np.median(full)))


def main():
    work = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else "assets"
    os.makedirs(work, exist_ok=True)
    os.makedirs(out, exist_ok=True)
    moon(work, out)
    mars(work, out)


if __name__ == "__main__":
    main()
