#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""彗星・小惑星・小さな衛星の実際の形 (assets/shapes.bin) を作る。

データ:
  - リュウグウ: Watanabe et al. (2019) の形状モデル SHAPE_SFM_49k_v20180804.obj
    （JAXA DARTS https://data.darts.isas.jaxa.jp/pub/hayabusa2/paper/Watanabe_2019/、出典 ISAS/JAXA）
  - チュリュモフ・ゲラシメンコ彗星 (67P): ESA の NAVCAM による形状モデル MTP019 の低解像度版
    cshp_dv_130_01_lores_obj.obj（NASA PDS Small Bodies Node, ro-c-multi-5-67p-shape-v2.0、
    ESA/Rosetta/NAVCAM – CC BY-SA IGO 3.0）
  - ベンヌ・イトカワ・ディディモス・ディモルフォス・フォボス: NASA NAIF の SPICE 形状データ (DSK)
    （ベンヌ: OSIRIS-REx、イトカワ: Gaskell et al. の Hayabusa AMICA モデル、ディディモス・ディモルフォス: DART、
    フォボス: Gaskell のモデル。いずれも NASA の惑星データ）
頂点が 65,536 個を超えるもの（フォボス）は格子でまとめて間引く。座標を 16 ビットに丸め、外側の表面までの半径の表を加える。

出力形式（リトルエンディアン）:
  uint32 形の数
  形ごとに: char[32] 英語名（シミュレータの天体名と同じ）, float32 座標の倍率 [km / 32767],
            uint32 頂点の数, uint32 三角形の数, uint16 半径の表の経度方向の数 NLON, 緯度方向の数 NLAT,
            int16[3] 頂点（天体固定座標。+Z が自転軸の北、km = 値 × 倍率）,
            uint16[3] 三角形の頂点番号（表から見て反時計回り）,
            float32[NLAT][NLON] 中心から各方向へ見た一番外側の表面までの距離 km
            （緯度 -90〜90°・経度 0〜360° を等間隔に、両端を含む NLAT 行 × 端を含まない NLON 列）

使い方: python3 tools/make_shape_assets.py <作業ディレクトリ> [出力先=assets]
必要なもの: NumPy, spiceypy（ダウンロード 約 38 MB）
"""
import os
import struct
import sys
import urllib.request

import numpy as np

UA = "SolarSystemVoyager-asset-build/1.0 (https://github.com/hubstacksea/Solar-System-Simulator)"
NAIF = "https://naif.jpl.nasa.gov/pub/naif/"
SHAPES = [
    ("Ryugu", "https://data.darts.isas.jaxa.jp/pub/hayabusa2/paper/Watanabe_2019/SHAPE_SFM_49k_v20180804.obj"),
    ("Churyumov-Gerasimenko", "https://pdssbn.astro.umd.edu/holdings/ro-c-multi-5-67p-shape-v2.0/data/triplate/"
                              "spc_esa/mtp019/cshp_dv_130_01_lores_obj.obj"),
    ("Bennu", NAIF + "ORX/kernels/dsk/bennu_g_06370mm_alt_obj_0000n00000_v021.bds"),
    ("Itokawa", NAIF + "generic_kernels/dsk/asteroids/itokawa/hay_a_amica_5_itokawashape_v1_0_64q.bds"),
    ("Didymos", NAIF + "pds/pds4/dart/dart_spice/spice_kernels/dsk/didymos_g_09309mm_spc_0000n00000_v003.bds"),
    ("Dimorphos", NAIF + "pds/pds4/dart/dart_spice/spice_kernels/dsk/dimorphos_g_01940mm_spc_0000n00000_v004.bds"),
    ("Phobos", NAIF + "generic_kernels/dsk/satellites/phobos_3_3.bds"),
]
MAX_VERTS = 60000
NLON, NLAT = 96, 49


def fetch(work, url):
    path = os.path.join(work, os.path.basename(url))
    if not os.path.exists(path):
        print("download", url)
        req = urllib.request.Request(url, headers={"User-Agent": UA})
        with urllib.request.urlopen(req, timeout=600) as r, open(path + ".part", "wb") as f:
            f.write(r.read())
        os.rename(path + ".part", path)
    return path


def read_obj(path):
    verts, faces = [], []
    with open(path, encoding="ascii", errors="replace") as f:
        for line in f:
            p = line.split()
            if not p:
                continue
            if p[0] == "v":
                verts.append([float(x) for x in p[1:4]])
            elif p[0] == "f":
                faces.append([int(x.split("/")[0]) - 1 for x in p[1:4]])
    return np.array(verts), np.array(faces, dtype=np.int64)


def read_dsk(path):
    import spiceypy as sp
    h = sp.dasopr(path)
    dla = sp.dlabfs(h)
    nv, nplates = sp.dskz02(h, dla)
    v = np.array(sp.dskv02(h, dla, 1, nv))
    f = np.array(sp.dskp02(h, dla, 1, nplates), dtype=np.int64) - 1
    sp.dascls(h)
    return v, f


def decimate(v, f, max_verts):
    """格子に入る頂点を 1 つにまとめる（格子の大きさは頂点数が max_verts 以下になるまで広げる）"""
    if len(v) <= max_verts:
        return v, f
    cell = np.abs(v).max() / 40
    while True:
        key = np.floor(v / cell).astype(np.int64)
        _, inv, cnt = np.unique(key, axis=0, return_inverse=True, return_counts=True)
        if len(cnt) <= max_verts:
            break
        cell *= 1.15
    inv = inv.reshape(-1)
    nv = np.zeros((len(cnt), 3))
    np.add.at(nv, inv, v)
    nv /= cnt[:, None]
    nf = inv[f]
    nf = nf[(nf[:, 0] != nf[:, 1]) & (nf[:, 1] != nf[:, 2]) & (nf[:, 0] != nf[:, 2])]
    return nv, nf


def outer_radius(v, f, dirs):
    """原点から各方向へ伸ばした半直線と三角形の交点のうち、一番遠いものまでの距離（Möller–Trumbore）"""
    a, b, c = v[f[:, 0]], v[f[:, 1]], v[f[:, 2]]
    e1, e2 = b - a, c - a
    out = np.zeros(len(dirs))
    for i, d in enumerate(dirs):
        pv = np.cross(d, e2)
        det = np.einsum("ij,ij->i", e1, pv)
        ok = np.abs(det) > 1e-12
        inv = np.where(ok, 1.0 / np.where(ok, det, 1.0), 0.0)
        tv = -a
        u = np.einsum("ij,ij->i", tv, pv) * inv
        qv = np.cross(tv, e1)
        w = (qv @ d) * inv
        t = np.einsum("ij,ij->i", e2, qv) * inv
        hit = ok & (u >= 0) & (w >= 0) & (u + w <= 1) & (t > 0)
        out[i] = t[hit].max() if hit.any() else np.linalg.norm(v, axis=1).mean()
    return out


def main():
    work = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else "assets"
    os.makedirs(work, exist_ok=True)
    blob = struct.pack("<I", len(SHAPES))
    lat = np.radians(np.linspace(-90, 90, NLAT))
    lon = np.radians(np.arange(NLON) * 360.0 / NLON)
    la, lo = np.meshgrid(lat, lon, indexing="ij")
    dirs = np.stack([np.cos(la) * np.cos(lo), np.cos(la) * np.sin(lo), np.sin(la)], axis=-1).reshape(-1, 3)
    for name, url in SHAPES:
        path = fetch(work, url)
        v, f = read_dsk(path) if path.endswith(".bds") else read_obj(path)
        v, f = decimate(v, f, MAX_VERTS)
        # 三角形の向きをそろえる（法線が中心から外を向くものが多数派になるように）
        a, b, c = v[f[:, 0]], v[f[:, 1]], v[f[:, 2]]
        n = np.cross(b - a, c - a)
        if np.einsum("ij,ij->i", n, (a + b + c) / 3).sum() < 0:
            f = f[:, [0, 2, 1]]
        assert len(v) < 65536
        scale = np.abs(v).max() / 32767.0
        r = outer_radius(v, f, dirs)
        blob += struct.pack("<32sfIIHH", name.encode(), scale, len(v), len(f), NLON, NLAT)
        blob += np.round(v / scale).astype("<i2").tobytes()
        blob += f.astype("<u2").tobytes()
        blob += r.astype("<f4").tobytes()
        vol = np.einsum("ij,ij->i", a, np.cross(b, c)).sum() / 6
        print("%-24s %6d vertices %7d faces  extent %.3f km  mean radius %.3f km  radius %.3f..%.3f km"
              % (name, len(v), len(f), np.abs(v).max(), (abs(vol) * 3 / (4 * np.pi)) ** (1 / 3), r.min(), r.max()))
    with open(os.path.join(out, "shapes.bin"), "wb") as fo:
        fo.write(blob)
    print("wrote", len(blob), "bytes")


if __name__ == "__main__":
    main()
