#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""惑星の位置の級数 VSOP87A (src/vsop87_data.h) を作る。

データ: VSOP87 (P. Bretagnon & G. Francou 1988, A&A 202, 309, Bureau des Longitudes) の VSOP87A（太陽中心、J2000 の
  黄道と春分点の直交座標 [AU]、時刻は J2000 からの力学時の千年）。係数は Greg Miller の VSOP87-Multilang
  (https://github.com/gmiller123456/vsop87-multilang, パブリックドメイン) の Languages/JSON/vsop87a.json から取る。
  地球は地球と月の重心 (emb) を使う（シミュレータが自分の月の位置で地球と月に分ける）

項は振幅 A × 3^べき（3 千年先での大きさ）が惑星ごとの閾値 EPS 未満のものを落とす。全部の項と比べた誤差は
紀元前 3000 年〜西暦 3000 年で 4″ 以内、1000〜3000 年で 1.5″ 以内（VSOP87 そのものの誤差は別。木星・土星は
西暦 0〜4000 年、ほかは紀元前 2000 年〜6000 年の外で大きくなる）

使い方: python3 tools/make_vsop87.py <作業ディレクトリ> [出力=src/vsop87_data.h]
"""
import json
import os
import sys
import urllib.request

UA = "SolarSystemVoyager-asset-build/1.0 (https://github.com/hubstacksea/Solar-System-Simulator)"
URL = "https://raw.githubusercontent.com/gmiller123456/vsop87-multilang/master/Languages/JSON/vsop87a.json"
# LONG_ELEMENTS（src/app/bodies.h）と同じ順
BODIES = [("mercury", 3e-7), ("venus", 3e-7), ("emb", 3e-7), ("mars", 3e-7),
          ("jupiter", 1e-6), ("saturn", 1e-6), ("uranus", 2e-6), ("neptune", 4e-6)]


def main():
    work = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else "src/vsop87_data.h"
    os.makedirs(work, exist_ok=True)
    path = os.path.join(work, "vsop87a.json")
    if not os.path.exists(path):
        print("download", URL)
        req = urllib.request.Request(URL, headers={"User-Agent": UA})
        with urllib.request.urlopen(req, timeout=120) as r, open(path + ".part", "wb") as f:
            f.write(r.read())
        os.rename(path + ".part", path)
    data = json.load(open(path))

    terms, index = [], []
    for name, eps in BODIES:
        coords = []
        for c in "xyz":
            starts = []
            for k in range(6):
                starts.append(len(terms))
                terms += [t for t in data[name][c].get(str(k), []) if t[0] * 3.0 ** k >= eps]
            starts.append(len(terms))
            coords.append(starts)
        index.append(coords)

    lines = [
        "// Solar System Voyager — Copyright (c) 2026 hubstacksea",
        "// MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator",
        "// 惑星の位置の級数 VSOP87A（Bretagnon & Francou 1988。tools/make_vsop87.py で項を間引いて作った。出典はそちらを参照）。",
        "// 太陽中心、J2000 の黄道と春分点の直交座標 [AU]。t は J2000 からの力学時の千年で、",
        "// 座標 = Σ_k t^k Σ A cos(B + C t)。水星・金星・地球と月の重心・火星・木星・土星・天王星・海王星の順",
        "#pragma once",
        "",
        "struct VsopTerm { double a, b, c; };",
        "",
        "// [惑星][x, y, z][べき 0〜5 の始まりと終わり] → VSOP_TERMS の番号",
        "static const int VSOP_INDEX[8][3][7] = {",
    ]
    for (name, _), coords in zip(BODIES, index):
        lines.append("    {" + ", ".join("{" + ", ".join(str(s) for s in st) + "}" for st in coords) + "},   // " + name)
    lines += ["};", "", "static const VsopTerm VSOP_TERMS[] = {"]
    for a, b, c in terms:
        lines.append("    {%.11g, %.11g, %.11g}," % (a, b, c))
    lines += ["};", ""]
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print("%d terms -> %s" % (len(terms), out))


if __name__ == "__main__":
    main()
