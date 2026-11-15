#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""ホイヘンスがタイタンの大気に入ってから着地するまでの道 (src/app/huygens_track.h) を作る。

データ: NASA PDS 大気ノードの Huygens Descent Trajectory Working Group（DTWG）による再構成の軌道
（HP-SSA-DTWG-6-TRAJECTORY-V1.0, https://atmos.nmsu.edu/PDS/data/hpdtwg_0001/。B. Kazeminejad, D. H. Atkinson ほか）。
大気に入る所（高度 1,270 km、2005-01-14 09:05:53 UTC）からは HUY_DTWG_ENTRY、パラシュートの段階の始まり（09:10:21）から
64 秒後より後は HUY_DTWG_DESCENT_POS（着地 11:38:10 まで 1 秒ごと）を使う（つなぎ目から 60 秒かけて移る）。高度は 2,575 km の球から、経度は西が正。

出力: 2005-01-14 09:00:00 UTC からの秒, 高度 km, 東経 deg, 緯度 deg の表（高度で 30 m、緯度・経度で 0.001° の誤差に
収まるところまで点を間引く）

使い方: python3 tools/make_huygens_track.py <作業ディレクトリ> [出力先=src/app/huygens_track.h]
"""
import datetime
import os
import sys
import urllib.request

UA = "SolarSystemVoyager-asset-build/1.0 (https://github.com/hubstacksea/Solar-System-Simulator)"
BASE = "https://atmos.nmsu.edu/PDS/data/hpdtwg_0001/DATA/"
EPOCH = datetime.datetime(2005, 1, 14, 9, 0, 0)


def fetch(work, name):
    path = os.path.join(work, name)
    if not os.path.exists(path):
        print("download", BASE + name)
        req = urllib.request.Request(BASE + name, headers={"User-Agent": UA})
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


def main():
    work = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else "src/app/huygens_track.h"
    os.makedirs(work, exist_ok=True)
    entry = rows(fetch(work, "HUY_DTWG_ENTRY.TAB"), 3)
    descent = rows(fetch(work, "HUY_DTWG_DESCENT_POS.TAB"), 4)
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
    s = simplify(pts, (0.03, 0.001, 0.001))
    with open(out, "w") as f:
        f.write("// Solar System Voyager — Copyright (c) 2026 hubstacksea\n")
        f.write("// MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator\n")
        f.write("// tools/make_huygens_track.py で作った表（手で書き換えない）。ホイヘンスがタイタンの大気に入ってから着地するまでの道:\n")
        f.write("// NASA PDS の Huygens DTWG の再構成の軌道（HP-SSA-DTWG-6-TRAJECTORY-V1.0）。\n")
        f.write("// 2005-01-14 09:00:00 UTC からの秒, 高度 km（2,575 km の球から）, 東経 deg, 緯度 deg\n")
        f.write("#pragma once\n\nstatic const float HUYGENS_TRACK[][4] = {\n")
        for t, a, lo, la in s:
            f.write("    {%.1ff, %.3ff, %.4ff, %.4ff},\n" % (t, a, lo, la))
        f.write("};\n")
    print("wrote %d of %d points, %.1f s .. %.1f s, %.1f km .. %.3f km" % (len(s), len(pts), s[0][0], s[-1][0], s[0][1], s[-1][1]))


if __name__ == "__main__":
    main()
