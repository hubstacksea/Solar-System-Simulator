#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""背景の星空 (assets/stars.bin, assets/sky.txt) を作る。

データ: d3-celestial (Olaf Frohn, BSD 3-Clause, https://github.com/ofrohn/d3-celestial)
  - stars.8.json: ヒッパルコス星表 (ESA, 1997) の 8 等級までの恒星（位置 J2000, 等級 V, 色指数 B-V）
  - constellations.json / constellations.lines.json: 88 星座の名前（日本語・英語）・名前の位置・星座線
  - starnames.json: 恒星の固有名（IAU 承認名と日本語表記）

出力:
  stars.bin  リトルエンディアン。uint32 個数, 以降 1 個あたり 12 バイト
             float32 赤経 [deg], float32 赤緯 [deg], int16 等級×100, int16 (B-V)×1000
             明るい順に並べる
  sky.txt    UTF-8 テキスト。1 行 1 項目（タブ区切り）
             C  赤経  赤緯  日本語名  学名          星座（名前の位置）
             L  赤経 赤緯 赤経 赤緯 ...             直前の星座の星座線（折れ線 1 本）
             S  赤経  赤緯  等級  日本語名  英語名  恒星の固有名

使い方: python3 tools/make_sky_assets.py <作業ディレクトリ> [出力先=assets]
"""
import json
import os
import struct
import sys
import urllib.request

UA = "SolarSystemVoyager-asset-build/1.0 (https://github.com/hubstacksea/Solar-System-Simulator)"
BASE = "https://raw.githubusercontent.com/ofrohn/d3-celestial/master/data/"
FILES = ["stars.8.json", "constellations.json", "constellations.lines.json", "starnames.json"]
NAME_MAG = 2.6  # この等級より明るい星に名前を付ける
# starnames.json に日本語表記がない星
JA_FIX = {"Regor": "レゴール", "Avior": "アヴィオール", "Alsephina": "アルセフィナ", "Menkent": "メンケント",
          "Tiaki": "ティアキ", "Ankaa": "アンカー", "Navi": "ナビ", "Muhlifain": "ムリフェイン", "Men": "メン",
          "Dschubba": "ジュバ", "Larawag": "ララワグ", "Mula": "ムーラ"}


def fetch(work, name):
    path = os.path.join(work, name)
    if not os.path.exists(path):
        print("download", BASE + name)
        req = urllib.request.Request(BASE + name, headers={"User-Agent": UA})
        with urllib.request.urlopen(req, timeout=120) as r, open(path + ".part", "wb") as f:
            f.write(r.read())
        os.rename(path + ".part", path)
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def ra360(lon):
    return lon % 360.0


def main():
    work = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else "assets"
    os.makedirs(work, exist_ok=True)
    stars, cons, lines, names = (fetch(work, n) for n in FILES)

    rows = []
    mag_of = {}
    for f in stars["features"]:
        p = f["properties"]
        lon, lat = f["geometry"]["coordinates"]
        bv = float(p["bv"]) if p.get("bv") not in (None, "") else 0.6
        rows.append((p["mag"], ra360(lon), lat, bv))
        mag_of[f["id"]] = (p["mag"], ra360(lon), lat)
    rows.sort()
    with open(os.path.join(out, "stars.bin"), "wb") as f:
        f.write(struct.pack("<I", len(rows)))
        for mag, ra, dec, bv in rows:
            f.write(struct.pack("<ffhh", ra, dec, round(mag * 100), round(max(-2.0, min(4.0, bv)) * 1000)))

    text = []
    line_of = {f["id"]: f["geometry"]["coordinates"] for f in lines["features"]}
    for f in cons["features"]:
        p = f["properties"]
        lon, lat = f["geometry"]["coordinates"]
        text.append("C\t%.3f\t%.3f\t%s\t%s" % (ra360(lon), lat, p["ja"], p["name"]))
        for poly in line_of.get(f["id"], []):
            text.append("L\t" + "\t".join("%.4f\t%.4f" % (ra360(a), b) for a, b in poly))
    missing = []
    named = []
    for hip, n in names.items():
        hip = int(hip)
        if not n.get("name") or hip not in mag_of or mag_of[hip][0] >= NAME_MAG:
            continue
        ja = n.get("ja") or JA_FIX.get(n["name"])
        if not ja:
            missing.append(n["name"])
            ja = n["name"]
        named.append((mag_of[hip], ja, n["name"]))
    for (mag, ra, dec), ja, en in sorted(named):
        text.append("S\t%.4f\t%.4f\t%.2f\t%s\t%s" % (ra, dec, mag, ja, en))
    with open(os.path.join(out, "sky.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(text) + "\n")
    print("stars %d, constellations %d, names %d" % (len(rows), len(cons["features"]), len(named)))
    if missing:
        print("no Japanese name:", ", ".join(missing))


if __name__ == "__main__":
    main()
