#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""小惑星帯・トロヤ群・太陽系外縁天体などの粒 (assets/asteroids.bin) を JPL の小天体データベースから作る。

データ: JPL Small-Body Database Query API (https://ssd-api.jpl.nasa.gov/sbdb_query.api) の軌道要素
（太陽中心・黄道 J2000 の接触軌道要素）。選ぶ天体:
  - 小惑星帯 (MBA): 絶対等級 H < 15（明るい約 6 万個）
  - 小惑星帯の内側・外側 (IMB, MCA, OMB: ヒルダ群など): H < 16
  - 地球近傍小惑星 (NEO): H < 18
  - 木星のトロヤ群 (TJN)、ケンタウルス族 (CEN)、太陽系外縁天体 (TNO): すべて

出力形式（リトルエンディアン）:
  uint32 個数
  1 個あたり 15 バイト: float32 軌道長半径 [AU],
                       float16 離心率, 軌道傾斜角, 昇交点黄経, 近日点引数, J2000 での平均近点角 [deg],
                       uint8 種類（0: 小惑星帯, 1: 地球近傍, 2: トロヤ群, 3: ケンタウルス族・太陽系外縁天体）
（平均近点角は元期から 2 体問題の平均運動で J2000 へ戻した値。描画は目安なので精度は 0.1° 程度で足りる）

使い方: python3 tools/make_asteroid_assets.py <作業ディレクトリ> [出力先=assets]
必要なもの: NumPy（ダウンロード 約 20 MB）
"""
import json
import os
import sys
import urllib.parse
import urllib.request

import numpy as np

UA = "SolarSystemVoyager-asset-build/1.0 (https://github.com/hubstacksea/Solar-System-Simulator)"
API = "https://ssd-api.jpl.nasa.gov/sbdb_query.api"
J2000 = 2451545.0
GAUSS = 0.01720209895  # rad/日（太陽の重力定数の平方根）
# 名前, 条件, 種類
GROUPS = [
    ("mba", {"sb-class": "MBA", "sb-cdata": '{"AND":["H|LT|15"]}'}, 0),
    ("imb_mca_omb", {"sb-class": "IMB,MCA,OMB", "sb-cdata": '{"AND":["H|LT|16"]}'}, 0),
    ("neo", {"sb-group": "neo", "sb-cdata": '{"AND":["H|LT|18"]}'}, 1),
    ("tjn", {"sb-class": "TJN"}, 2),
    ("cen_tno", {"sb-class": "CEN,TNO"}, 3),
]


def query(work, name, cond):
    path = os.path.join(work, name + ".json")
    if not os.path.exists(path):
        q = {"fields": "a,e,i,om,w,ma,epoch", "sb-kind": "a"}
        q.update(cond)
        url = API + "?" + urllib.parse.urlencode(q)
        print("download", name)
        req = urllib.request.Request(url, headers={"User-Agent": UA})
        with urllib.request.urlopen(req, timeout=900) as r, open(path + ".part", "wb") as f:
            f.write(r.read())
        os.rename(path + ".part", path)
    with open(path, encoding="utf-8") as f:
        d = json.load(f)
    rows = [r for r in d["data"] if all(x is not None for x in r)]
    return np.array(rows, dtype=np.float64)


def main():
    work = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else "assets"
    os.makedirs(work, exist_ok=True)
    parts = []
    for name, cond, kind in GROUPS:
        a = query(work, name, cond)
        a = a[(a[:, 0] > 0) & (a[:, 1] < 1)]   # 楕円軌道だけ
        n = np.degrees(GAUSS / a[:, 0] ** 1.5)  # deg/日
        m0 = (a[:, 5] - n * (a[:, 6] - J2000)) % 360.0
        rec = np.zeros(len(a), dtype=[("a", "<f4"), ("e", "<f2"), ("i", "<f2"), ("om", "<f2"), ("w", "<f2"),
                                      ("m", "<f2"), ("k", "u1")])
        rec["a"], rec["e"], rec["i"], rec["om"], rec["w"], rec["m"], rec["k"] = a[:, 0], a[:, 1], a[:, 2], a[:, 3], a[:, 4], m0, kind
        parts.append(rec)
        print("%-12s %6d" % (name, len(a)))
    allrec = np.concatenate(parts)
    with open(os.path.join(out, "asteroids.bin"), "wb") as f:
        f.write(np.uint32(len(allrec)).tobytes())
        f.write(allrec.tobytes())
    print("total", len(allrec), "->", 4 + allrec.nbytes, "bytes")


if __name__ == "__main__":
    main()
