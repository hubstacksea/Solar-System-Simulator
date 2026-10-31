#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""背景の星空 (assets/stars.bin, assets/sky.txt) を作る。

データ: d3-celestial (Olaf Frohn, BSD 3-Clause, https://github.com/ofrohn/d3-celestial)
  - stars.8.json: ヒッパルコス星表 (ESA, 1997) の 8 等級までの恒星（位置 J2000, 等級 V, 色指数 B-V）。
    太陽から 10 光年ほどまでの恒星（NEARBY_HIP）は除く
  - constellations.json / constellations.lines.json: 88 星座の名前（日本語・英語）・名前の位置・星座線
  - starnames.json: 恒星の固有名（IAU 承認名と日本語表記）
データ: ヒッパルコス星表の新しい整約 (van Leeuwen 2007, A&A 474, 653) の年周視差と固有運動。
  ESA Gaia アーカイブの TAP (public.hipparcos_newreduction) から <作業ディレクトリ>/hip2pm.csv に取る

出力:
  stars.bin  リトルエンディアン。uint32 個数, 以降 1 個あたり 24 バイト
             float32 赤経 [deg], float32 赤緯 [deg], int16 等級×100, int16 (B-V)×1000, float32 距離 [pc],
             float32 固有運動 μα*・μδ [mas/年]
             明るい順に並べる
  sky.txt    UTF-8 テキスト。1 行 1 項目（タブ区切り）
             C  赤経  赤緯  距離  日本語名  学名            星座（名前の位置。距離は星座線の星の中央値）
             L  赤経 赤緯 距離 μα* μδ  赤経 赤緯 距離 μα* μδ ...   直前の星座の星座線（折れ線 1 本）
             S  赤経  赤緯  距離  μα*  μδ  等級  日本語名  英語名  恒星の固有名
  距離は pc、固有運動は mas/年。視差が測れていない（誤差が視差の 30% 以上の）星は FAR_PC に置く。
  位置は J2000 の元期（d3-celestial の値）で、シミュレータは固有運動で等速に動かす（視線速度は入れない）

使い方: python3 tools/make_sky_assets.py <作業ディレクトリ> [出力先=assets]
"""
import csv
import json
import os
import struct
import sys
import urllib.parse
import urllib.request

UA = "SolarSystemVoyager-asset-build/1.0 (https://github.com/hubstacksea/Solar-System-Simulator)"
BASE = "https://raw.githubusercontent.com/ofrohn/d3-celestial/master/data/"
FILES = ["stars.8.json", "constellations.json", "constellations.lines.json", "starnames.json"]
TAP = "https://gea.esac.esa.int/tap-server/tap/sync"
FAR_PC = 800.0    # 視差が測れていない星の距離（8 等級までの遠い星の目安）
MAX_PC = 3000.0
NAME_MAG = 2.6  # この等級より明るい星に名前を付ける
# 太陽から 10 光年ほどまでの恒星は、シミュレータが天体として実際の位置に描くので除く（tools/make_nearby_stars.py）
# シリウス、ケンタウルス座アルファ星 A・B、ラランド 21185、エリダヌス座イプシロン星（ほかの近い星は 8 等級より暗い）と、
# 17 光年ほどまでの主な恒星（プロキオン、アルタイル、くじら座タウ星、インディアン座イプシロン星、エリダヌス座 40 番星、
# はくちょう座 61 番星 A・B）
NEARBY_HIP = {32349, 71683, 71681, 54035, 16537, 37279, 97649, 8102, 108870, 19849, 104214, 104217}
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


def fetch_parallax(work):
    """ヒッパルコス番号 → (距離 pc, μα*, μδ)（新しい整約の年周視差と固有運動。同期 TAP の行数の上限があるので番号の範囲ごとに取る）"""
    path = os.path.join(work, "hip2pm.csv")
    if not os.path.exists(path):
        rows = ["hip,plx,e_plx,pm_ra,pm_de"]
        for lo in range(0, 160000, 40000):
            q = ("SELECT hip, plx, e_plx, pm_ra, pm_de FROM public.hipparcos_newreduction WHERE hip > %d AND hip <= %d"
                 % (lo, lo + 40000))
            data = urllib.parse.urlencode({"REQUEST": "doQuery", "LANG": "ADQL", "FORMAT": "csv", "QUERY": q}).encode()
            print("query", q)
            req = urllib.request.Request(TAP, data=data, headers={"User-Agent": UA})
            with urllib.request.urlopen(req, timeout=300) as r:
                rows += r.read().decode().strip().splitlines()[1:]
        with open(path, "w") as f:
            f.write("\n".join(rows) + "\n")
    dist = {}
    with open(path) as f:
        for r in csv.DictReader(f):
            plx, e = float(r["plx"]), float(r["e_plx"])
            pc = min(MAX_PC, 1000.0 / plx) if plx > 0 and e < plx * 0.3 else FAR_PC
            dist[int(r["hip"])] = (pc, float(r["pm_ra"] or 0), float(r["pm_de"] or 0))
    return dist


def ra360(lon):
    return lon % 360.0


def main():
    work = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else "assets"
    os.makedirs(work, exist_ok=True)
    stars, cons, lines, names = (fetch(work, n) for n in FILES)
    dist = fetch_parallax(work)

    rows = []
    mag_of = {}
    by_pos = {}   # 位置（0.01 度に丸めた）→ (距離, μα*, μδ)。星座線の頂点の星を探す
    for f in stars["features"]:
        p = f["properties"]
        lon, lat = f["geometry"]["coordinates"]
        pc, pmra, pmde = dist.get(f["id"], (FAR_PC, 0.0, 0.0))
        by_pos[(round(ra360(lon) * 100), round(lat * 100))] = (pc, pmra, pmde)
        if f["id"] in NEARBY_HIP:
            continue
        bv = float(p["bv"]) if p.get("bv") not in (None, "") else 0.6
        rows.append((p["mag"], ra360(lon), lat, bv, pc, pmra, pmde))
        mag_of[f["id"]] = (p["mag"], ra360(lon), lat, pc, pmra, pmde)
    rows.sort()
    with open(os.path.join(out, "stars.bin"), "wb") as f:
        f.write(struct.pack("<I", len(rows)))
        for mag, ra, dec, bv, pc, pmra, pmde in rows:
            f.write(struct.pack("<ffhhfff", ra, dec, round(mag * 100), round(max(-2.0, min(4.0, bv)) * 1000), pc, pmra, pmde))

    def vertex(ra, dec):
        ra, dec = ra360(ra), dec
        for dr in (0, -1, 1):
            for dd in (0, -1, 1):
                v = by_pos.get((round(ra * 100) + dr, round(dec * 100) + dd))
                if v is not None:
                    return v
        return (FAR_PC, 0.0, 0.0)

    text = []
    line_of = {f["id"]: f["geometry"]["coordinates"] for f in lines["features"]}
    unmatched = 0
    for f in cons["features"]:
        p = f["properties"]
        lon, lat = f["geometry"]["coordinates"]
        polys = line_of.get(f["id"], [])
        pcs = sorted(vertex(a, b)[0] for poly in polys for a, b in poly)
        unmatched += sum(1 for poly in polys for a, b in poly if vertex(a, b)[0] == FAR_PC)
        mid = pcs[len(pcs) // 2] if pcs else FAR_PC
        text.append("C\t%.3f\t%.3f\t%.1f\t%s\t%s" % (ra360(lon), lat, mid, p["ja"], p["name"]))
        for poly in polys:
            text.append("L\t" + "\t".join("%.4f\t%.4f\t%.1f\t%.2f\t%.2f" % ((ra360(a), b) + vertex(a, b)) for a, b in poly))
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
    for (mag, ra, dec, pc, pmra, pmde), ja, en in sorted(named):
        text.append("S\t%.4f\t%.4f\t%.1f\t%.2f\t%.2f\t%.2f\t%s\t%s" % (ra, dec, pc, pmra, pmde, mag, ja, en))
    with open(os.path.join(out, "sky.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(text) + "\n")
    print("stars %d, constellations %d, names %d, line vertices without parallax %d"
          % (len(rows), len(cons["features"]), len(named), unmatched))
    if missing:
        print("no Japanese name:", ", ".join(missing))


if __name__ == "__main__":
    main()
