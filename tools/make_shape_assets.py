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
  - ベンヌ・イトカワ・ディディモス・ディモルフォス・フォボス・ドナルドジョハンソン: NASA NAIF の SPICE 形状データ (DSK)
    （ベンヌ: OSIRIS-REx、イトカワ: Gaskell et al. の Hayabusa AMICA モデル、ディディモス・ディモルフォス: DART、
    フォボス: Gaskell のモデル、ドナルドジョハンソン: Lucy の DLR チーム。いずれも NASA の惑星データ）
  - ダイモス: Thomas の形状モデル（Viking Orbiter の撮影から。PDS Small Bodies Node の ast-sat.thomas.shape-models）。
    配布元が自動の取得を受け付けないので、ブラウザで取得して作業ディレクトリに置く
  - ガスプラ・イダ・ハレー彗星: Stooke の形状モデル（Galileo、ハレー彗星は Giotto・Vega の撮影から。PDS Small Bodies Node の small_bodies.stooke.shape-models、
    パブリックドメイン）。配布元が自動の取得を受け付けないので、ブラウザで取得して作業ディレクトリに置く
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
    # ドナルドジョハンソン: ルーシーの撮影から DLR のチームが作った DSK（NASA PDS の Lucy SPICE アーカイブ）
    ("Donaldjohanson", NAIF + "pds/pds4/lucy/lucy_spice/spice_kernels/dsk/lcy_donj_k548_iso20m_v10.bds"),
    # ガスプラ・イダ: Stooke の形状モデル（Galileo の撮影から。PDS Small Bodies Node、パブリックドメイン）。
    # 配布元が自動の取得を受け付けないので、ブラウザで STOOKE_ZIP を作業ディレクトリに置いておく
    ("Gaspra", "stooke:951gaspra.tab"),
    ("Ida", "stooke:243ida.tab"),
    # ハレー彗星の核: Stooke の形状モデル（Giotto・Vega の撮影から。指向の計算は Abergel）。このモデルの北は長軸の向きで、
    # 長軸のまわりに回して見せると細長さが分からないので、長軸を赤道面に寝かせる（シミュレータの自転は 1 軸の単純な回転）
    ("Halley", "stooke:1682q1halley.tab"),
    # ダイモス: Thomas の形状モデル（Viking Orbiter の撮影から。PDS Small Bodies Node の ast-sat.thomas.shape-models、
    # NASA の惑星データ）。Stooke と同じく、ブラウザで THOMAS_ZIP を作業ディレクトリに置いておく
    ("Deimos", "thomas:m2deimos.tab"),
]
MAX_VERTS = 60000
NLON, NLAT = 96, 49
STOOKE_ZIP = "small_bodies.stooke.shape-models.zip"
STOOKE_URL = "https://sbnarchive.psi.edu/pds4/non_mission/small_bodies.stooke.shape-models.zip"
THOMAS_ZIP = "ast-sat.thomas.shape-models_V1_0.zip"
THOMAS_URL = "https://sbnarchive.psi.edu/pds4/non_mission/" + THOMAS_ZIP


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


def read_stooke(work, name, thomas=False):
    """Stooke・Thomas の形状モデル: 緯度・経度 5° ごとの格子の半径 [km]（0° と 360° は同じ点）。
    Stooke は 経度・緯度・半径 の順で経度は東が正。Thomas は 緯度・経度・半径 の順で、経度は西が正
    （同じ集まりのフォボスで、スティックニー・クレーターのくぼみが西経 49° に出ることで確認）"""
    import zipfile
    zname, url = (THOMAS_ZIP, THOMAS_URL) if thomas else (STOOKE_ZIP, STOOKE_URL)
    path = os.path.join(work, zname)
    if not os.path.exists(path):
        raise SystemExit("%s をブラウザで %s から取得して %s に置いてください" % (zname, url, work))
    with zipfile.ZipFile(path) as z:
        text = z.read(zname[:-4] + "/data/" + name).decode("ascii")
    grid = {}
    for line in text.splitlines():
        p = line.split()
        if len(p) == 3:
            if thomas:
                grid[(-int(round(float(p[1]))) % 360, int(round(float(p[0]))))] = float(p[2])
            else:
                grid[(int(p[0]) % 360, int(p[1]))] = float(p[2])
    lons = sorted({k[0] for k in grid})
    lats = sorted({k[1] for k in grid})
    nlon = len(lons)
    verts, index = [], {}
    for la in lats:
        ring = [0] if abs(la) == 90 else lons   # 極は 1 点に
        for lo in ring:
            r = grid[(lo, la)] if (lo, la) in grid else np.mean([grid[(x, la)] for x in lons if (x, la) in grid])
            a, b = np.radians(la), np.radians(lo)
            index[(lo, la)] = len(verts)
            verts.append([r * np.cos(a) * np.cos(b), r * np.cos(a) * np.sin(b), r * np.sin(a)])
    vid = lambda lo, la: index[(0, la)] if abs(la) == 90 else index[(lons[lo % nlon], la)]
    faces = []
    for i in range(len(lats) - 1):
        a, b = lats[i], lats[i + 1]
        for j in range(nlon):
            q = [vid(j, a), vid(j + 1, a), vid(j + 1, b), vid(j, b)]
            for t in ([q[0], q[1], q[2]], [q[0], q[2], q[3]]):
                if len(set(t)) == 3:
                    faces.append(t)
    return np.array(verts, dtype=float), np.array(faces, dtype=np.int64)


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
        if url.startswith(("stooke:", "thomas:")):
            v, f = read_stooke(work, url.split(":", 1)[1], url.startswith("thomas:"))
            if name == "Halley":
                v = v[:, [2, 0, 1]]   # 長軸（モデルの +Z）を +X に（並べ替えは巡回なので右手系のまま）
        else:
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
