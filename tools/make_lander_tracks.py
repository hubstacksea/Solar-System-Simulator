#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""着陸機が大気に入ってから着地するまでの道 (src/app/lander_tracks.h) を作る。

ホイヘンス（タイタン）:
  降下: NASA PDS 大気ノードの Huygens Descent Trajectory Working Group（DTWG）による再構成の軌道
  （HP-SSA-DTWG-6-TRAJECTORY-V1.0, https://atmos.nmsu.edu/PDS/data/hpdtwg_0001/。B. Kazeminejad, D. H. Atkinson ほか）。
  大気に入る所（高度 1,270 km、2005-01-14 09:05:53 UTC）からは HUY_DTWG_ENTRY、パラシュートの段階の始まり（09:10:21）から
  64 秒後より後は HUY_DTWG_DESCENT_POS（着地 11:38:10 まで 1 秒ごと）を使う（つなぎ目から 60 秒かけて移る）。高度は 2,575 km の球から、経度は西が正。
  惰性飛行: カッシーニから切り離された 2004-12-25 02:00 UTC から大気に入るまでは JPL Horizons の軌道（-150。2005-01-06 時点の
  予測の軌道を含む）。タイタンからの位置・速度と、土星から見たタイタンの位置・速度（黄道座標 J2000）。
キュリオシティ（MSL）とパーシビアランス（Mars 2020）（火星）:
  NAIF の再構成の EDL 軌道の SPK（msl_edl_v01.bsp: MSL EDL Team Solution 04/12/13、m2020_edl_v01.bsp: M2020 EDL Team
  Solution 07/10/21。https://naif.jpl.nasa.gov/pub/naif/）の慣性計測装置 DIMU-A の位置を、IAU の火星の自転（pck00010.tpc）で
  火星に固定した座標にする。ローバーの位置は、SPK の説明にある DIMU-A からローバーまでの距離（スカイクレーンで吊り下げる前は
  約 1.8 m、吊り下げた後は約 7.1 m 下）を真下へずらして求める。

出力: 時刻の基準からの秒, 高度 km（基準の球から）, 東経 deg, 緯度 deg（惑星中心）の表（高度で 30 m（火星は 3 m）、緯度・経度で
0.001°（火星は 0.00005°）の誤差に収まるところまで点を間引く）と、ホイヘンスの惰性飛行の表（秒, タイタンからの位置 km・速度 km/s,
土星から見たタイタンの位置 km・速度 km/s）

使い方: python3 tools/make_lander_tracks.py <作業ディレクトリ> [出力先=src/app/lander_tracks.h]
（火星の 2 機には SpiceyPy（pip install spiceypy）が要る）
"""
import datetime
import math
import os
import sys
import urllib.parse
import urllib.request

UA = "SolarSystemVoyager-asset-build/1.0 (https://github.com/hubstacksea/Solar-System-Simulator)"
BASE = "https://atmos.nmsu.edu/PDS/data/hpdtwg_0001/DATA/"
NAIF = "https://naif.jpl.nasa.gov/pub/naif/"
HORIZONS = "https://ssd.jpl.nasa.gov/api/horizons.api"
EPOCH = datetime.datetime(2005, 1, 14, 9, 0, 0)
MARS_REF = 3390.0   # 火星の高度の基準の球 km

# 火星の着陸機: 名前, SPK, DIMU-A の ID, 公式の着地の時刻（TDB）, DIMU-A からローバーまでの距離（ET, 真下へ m）
MARS = [
    ("CURIOSITY", "MSL/kernels/spk/msl_edl_v01.bsp", -76031, "2012-08-06 05:19:04.475 TDB",
     [(397502325.8968500, 1.730), (397502330.5968900, 8.829)]),
    ("PERSEVERANCE", "MARS2020/kernels/spk/m2020_edl_v01.bsp", -168031, "2021-02-18 20:44:58.003 TDB",
     [(666953081.6396100, 1.826), (666953087.1396100, 8.998)]),
]


def fetch(work, url, name):
    path = os.path.join(work, name)
    if not os.path.exists(path):
        print("download", url)
        req = urllib.request.Request(url, headers={"User-Agent": UA})
        with urllib.request.urlopen(req, timeout=600) as r, open(path + ".part", "wb") as f:
            f.write(r.read())
        os.rename(path + ".part", path)
    return path


def rows(path, alt_col):
    """(秒, 経過時間 s, 高度 km, 東経, 緯度)。列は空白区切り: ET, 経過時間, UTC, [気圧,] 高度, 西経, 緯度, ..."""
    out = []
    for ln in open(path):
        f = ln.split()
        if len(f) < alt_col + 3:
            continue
        t = datetime.datetime.strptime(f[2][:19], "%Y-%m-%dT%H:%M:%S") + datetime.timedelta(seconds=float("0" + f[2][19:]))
        lon = -float(f[alt_col + 1])
        lon = (lon + 180) % 360 - 180
        out.append(((t - EPOCH).total_seconds(), float(f[1]), float(f[alt_col]), lon, float(f[alt_col + 2])))
    return out


def simplify(pts, tol):
    """直線で補間した時の誤差が tol（高度 km, 経度・緯度 deg）に収まるまで点を間引く（Douglas-Peucker）"""
    keep = [False] * len(pts)
    keep[0] = keep[-1] = True
    stack = [(0, len(pts) - 1)]
    while stack:
        a, b = stack.pop()
        worst, wi = 0.0, -1
        for i in range(a + 1, b):
            u = (pts[i][0] - pts[a][0]) / (pts[b][0] - pts[a][0])
            e = max(abs(pts[a][k] + (pts[b][k] - pts[a][k]) * u - pts[i][k]) / tol[k - 1] for k in (1, 2, 3))
            if e > worst:
                worst, wi = e, i
        if worst > 1:
            keep[wi] = True
            stack += [(a, wi), (wi, b)]
    return [p for p, k in zip(pts, keep) if k]


def huygens_descent(work):
    entry = rows(fetch(work, BASE + "HUY_DTWG_ENTRY.TAB", "HUY_DTWG_ENTRY.TAB"), 3)
    descent = rows(fetch(work, BASE + "HUY_DTWG_DESCENT_POS.TAB", "HUY_DTWG_DESCENT_POS.TAB"), 4)
    start = descent[0][1]
    # 2 つの再構成はつなぎ目で高度 0.7 km・経度 0.09° ずれているので、つなぎ目から 60 秒かけて突入の値から降下の値へ移す
    ent = {round(el): (a, lo, la) for t, el, a, lo, la in entry}
    pts = [(t, a, lo, la) for t, el, a, lo, la in entry if el < start]
    for t, el, a, lo, la in descent:
        w = min(1.0, (el - start) / 60.0)
        if w < 1 and round(el) in ent:
            e = ent[round(el)]
            a, lo, la = e[0] + (a - e[0]) * w, e[1] + (lo - e[1]) * w, e[2] + (la - e[2]) * w
        pts.append((t, a, lo, la))
    return pts


def horizons(work, name, command, center, start, stop, step):
    q = {"format": "text", "COMMAND": "'%s'" % command, "EPHEM_TYPE": "VECTORS", "CENTER": "'%s'" % center,
         "START_TIME": "'%s'" % start, "STOP_TIME": "'%s'" % stop, "STEP_SIZE": "'%s'" % step, "VEC_TABLE": "2",
         "CSV_FORMAT": "YES", "TIME_TYPE": "UT", "OUT_UNITS": "KM-S", "REF_PLANE": "ECLIPTIC"}
    path = fetch(work, HORIZONS + "?" + urllib.parse.urlencode(q), name)
    out, on = [], False
    for ln in open(path):
        if ln.startswith("$$SOE"):
            on = True
        elif ln.startswith("$$EOE"):
            break
        elif on:
            f = [x.strip() for x in ln.split(",")]
            d = f[1].replace("A.D.", "").strip()   # 例: 2004-Dec-25 02:01:00.0000
            t = datetime.datetime.strptime(d[:20], "%Y-%b-%d %H:%M:%S") + datetime.timedelta(seconds=float("0" + d[20:]))
            out.append(((t - EPOCH).total_seconds(), [float(x) for x in f[2:8]]))
    return out


def huygens_coast(work, entry_s):
    """切り離しから大気に入るまで: 遠くは 1 時間ごと、最後の 6 時間は 5 分ごと、最後に大気に入る時刻の点"""
    entry = EPOCH + datetime.timedelta(seconds=entry_s)
    e = entry.strftime("%Y-%m-%d %H:%M:%S")
    parts = [("2004-12-25 02:01", "2005-01-14 03:00", "1 h"), ("2005-01-14 03:05", "2005-01-14 09:05", "5 m"), (e, e + ".5", "1")]
    out = []
    for i, (a, b, st) in enumerate(parts):
        h = horizons(work, "huygens_%d.txt" % i, "-150", "500@606", a, b, st)
        ti = horizons(work, "titan_%d.txt" % i, "606", "500@699", a, b, st)
        for (t, v), (t2, w) in zip(h, ti):
            assert abs(t - t2) < 1e-3
            if not out or t > out[-1][0] + 1:
                out.append((t, v + w))
    out = [p for p in out if p[0] <= entry_s + 1e-3]
    return out


def mars_track(work, name, spk, dimu, td_tdb, offset):
    import spiceypy as sp
    for k in ("generic_kernels/lsk/naif0012.tls", "generic_kernels/pck/pck00010.tpc", spk):
        sp.furnsh(fetch(work, NAIF + k, os.path.basename(k)))
    td = sp.str2et(td_tdb)
    cov = sp.spkcov(os.path.join(work, os.path.basename(spk)), dimu)
    t0 = sp.wnfetd(cov, 0)[0] + 1.0
    (e0, o0), (e1, o1) = offset
    pts = []
    n = int((td - t0) / 0.5)
    for k in range(n + 1):
        t = td - (n - k) * 0.5
        p, _ = sp.spkpos(str(dimu), t, "IAU_MARS", "NONE", "499")
        u = min(1.0, max(0.0, (t - e0) / (e1 - e0)))
        off = (o0 + (o1 - o0) * u * u * (3 - 2 * u)) / 1000   # 吊り下げの間は 3 次でつなぐ（SPK と同じ扱い）
        r = math.sqrt(sum(c * c for c in p))
        r2 = r - off
        lon = math.degrees(math.atan2(p[1], p[0]))
        lat = math.degrees(math.asin(p[2] / r))
        pts.append((t - td, r2 - MARS_REF, lon, lat))
    utc = sp.et2utc(td, "ISOC", 3)
    print("%s: touchdown %s UTC, %.4f %.4f, start %.1f s" % (name, utc, pts[-1][3], pts[-1][2], pts[0][0]))
    return pts, utc


def main():
    work = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else "src/app/lander_tracks.h"
    os.makedirs(work, exist_ok=True)
    pts = huygens_descent(work)
    s = simplify(pts, (0.03, 0.001, 0.001))
    coast = huygens_coast(work, s[0][0])
    tables = []
    for name, spk, dimu, td, off in MARS:
        mp, utc = mars_track(work, name, spk, dimu, td, off)
        ms = simplify(mp, (0.003, 0.00005, 0.00005))
        tables.append((name, utc, ms, len(mp)))
    with open(out, "w") as f:
        f.write("// Solar System Voyager — Copyright (c) 2026 hubstacksea\n")
        f.write("// MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator\n")
        f.write("// tools/make_lander_tracks.py で作った表（手で書き換えない）。着陸機が大気に入ってから着地するまでの道。\n")
        f.write("#pragma once\n\n")
        f.write("// ホイヘンス: NASA PDS の Huygens DTWG の再構成の軌道（HP-SSA-DTWG-6-TRAJECTORY-V1.0）。\n")
        f.write("// 2005-01-14 09:00:00 UTC からの秒, 高度 km（2,575 km の球から）, 東経 deg, 緯度 deg\n")
        f.write("static const float HUYGENS_TRACK[][4] = {\n")
        for t, a, lo, la in s:
            f.write("    {%.1ff, %.3ff, %.4ff, %.4ff},\n" % (t, a, lo, la))
        f.write("};\n\n")
        f.write("// ホイヘンスがカッシーニから切り離されてから大気に入るまで: JPL Horizons の軌道（-150）。2005-01-14 09:00:00 UTC からの秒,\n")
        f.write("// タイタンからの位置 km・速度 km/s, 土星から見たタイタンの位置 km・速度 km/s（黄道座標 J2000）\n")
        f.write("static const double HUYGENS_COAST[][13] = {\n")
        for t, v in coast:
            f.write("    {%.1f, %s},\n" % (t, ", ".join("%.3f" % x if i % 6 < 3 else "%.6f" % x for i, x in enumerate(v))))
        f.write("};\n")
        for name, utc, ms, n in tables:
            f.write("\n// %s: NAIF の再構成の EDL 軌道（SPK）。着地 %s UTC からの秒, 高度 km（%.0f km の球から）, 東経 deg, 緯度 deg\n" % (name.capitalize(), utc, MARS_REF))
            f.write("static const float %s_TRACK[][4] = {\n" % name)
            for t, a, lo, la in ms:
                f.write("    {%.1ff, %.4ff, %.5ff, %.5ff},\n" % (t, a, lo, la))
            f.write("};\n")
            print("%s: wrote %d of %d points" % (name, len(ms), n))
    print("Huygens: wrote %d of %d points, %.1f s .. %.1f s, %.1f km .. %.3f km; coast %d points from %.0f s"
          % (len(s), len(pts), s[0][0], s[-1][0], s[0][1], s[-1][1], len(coast), coast[0][0]))


if __name__ == "__main__":
    main()
