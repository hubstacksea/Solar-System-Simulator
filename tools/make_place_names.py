#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""地表の近くで今いる所の地名を出すための表 (assets/place_names.txt) を作る。

データ:
  - 地球の都市: Natural Earth 1:10m Populated Places（パブリックドメイン）。名前（日本語は NAME_JA）、緯度・経度と、
    市街地の広さ MAX_AREAKM（LandScan から求めた面積 km²）。広さの無い都市は人口から 1 km² に 2,000 人として広さを推定
  - 地球の地域: Natural Earth（パブリックドメイン）の 1:50m の国（Admin 0 Countries）、1:10m の海（Marine Areas: 大洋・海・湾・
    海峡など）、1:10m の地域（Physical Labels: 山脈・砂漠・高原・島・半島・平原など）の多角形と、1:10m の山頂
    （Elevation Points の mountain。山頂から半径 5 km を山頂の地名とする）
  - 地球以外: IAU の惑星の地名（USGS Gazetteer of Planetary Nomenclature。米国政府の著作物でパブリックドメイン）の
    天体ごとの中心点のシェープファイル。IAU が承認した名前だけで、衛星クレーター（「Copernicus A」のような
    文字の付いたもの）は除く。中心（惑星中心緯度・東経 0〜360°）、直径、範囲の緯度・経度

出力（UTF-8 のタブ区切り）:
  K  種類の日本語  英語名に付ける語        … 地形の種類の表（出てきた順に 0, 1, 2, …）
  B  天体の英語名                          … 以下はこの天体の地名
  緯度  経度（東が正、-180〜180）  半径 km  範囲（南端,北端,西端,東端 の度。無ければ空）  種類（-1: 無し）  名前  [日本語名]
  日本語名が無ければ「名前（種類の日本語）」、英語名は「名前 英語名に付ける語」。
  地名の場所: 中心から半径以内で、範囲があればその中（実際の形ではなく円と四角で近似）
  A  層（0: 国, 1: 海, 2: 地域）  面積 km²  日本語名  英語名  … 地球の地域（多角形）。続く P の行がその輪
  P  輪の点（経度・緯度を 0.01° 単位の整数にし、最初の点はそのまま、以降は前の点との差を、符号を交互にした
     可変長整数（7 bit ずつ、続きがあれば最上位を 1）で並べて Base64 にしたもの）。点は Douglas–Peucker で間引く

使い方: python3 tools/make_place_names.py <作業ディレクトリ> [出力先=assets]
必要なもの: Python だけ（ダウンロード 約 50 MB）
"""
import io
import math
import os
import re
import base64
import struct
import sys
import unicodedata
import urllib.request
import zipfile

NE_URL = "https://naciscdn.org/naturalearth/10m/cultural/ne_10m_populated_places.zip"
NE_AREAS = [   # (層, URL, 間引きの許容誤差 度)
    (0, "https://naciscdn.org/naturalearth/50m/cultural/ne_50m_admin_0_countries.zip", 0.02),
    (1, "https://naciscdn.org/naturalearth/10m/physical/ne_10m_geography_marine_polys.zip", 0.05),
    (2, "https://naciscdn.org/naturalearth/10m/physical/ne_10m_geography_regions_polys.zip", 0.05),
]
NE_PEAKS = "https://naciscdn.org/naturalearth/10m/physical/ne_10m_geography_regions_elevation_points.zip"
PEAK_RADIUS = 5.0   # Natural Earth の山頂の位置は数 km ずれるものがある（エベレストは西に約 4 km）
IAU_URL = "https://asc-planetarynames-data.s3.us-west-2.amazonaws.com/%s_nomenclature_center_pts.zip"
IAU_TARGETS = ("AMALTHEA ARIEL BENNU CALLISTO CERES CHARON DACTYL DEIMOS DIDYMOS DIMORPHOS DINKINESH DIONE DONALDJOHANSON "
               "ENCELADUS EPIMETHEUS EROS EUROPA GANYMEDE GASPRA HYPERION IAPETUS IDA IO ITOKAWA JANUS LUTETIA MARS MATHILDE "
               "MERCURY MIMAS MIRANDA MOON NIX OBERON PHOBOS PHOEBE PLUTO PROTEUS PUCK RHEA RYUGU STEINS TETHYS THEBE TITAN "
               "TITANIA TRITON UMBRIEL VENUS VESTA").split()
UA = {"User-Agent": "curl/8.5.0"}

# 地形の種類（IAU の記述語）の日本語。名前に記述語の入らないクレーターなどは、日本語名に括弧で種類を付ける
TYPE_JA = {
    "Crater": "クレーター", "Corona": "コロナ（環状の地形）", "Mons": "山", "Montes": "山脈", "Patera": "火口状のくぼ地",
    "Paterae": "火口状のくぼ地", "Vallis": "谷", "Valles": "峡谷", "Dorsum": "尾根", "Dorsa": "尾根",
    "Albedo Feature": "明暗の模様", "Fossa": "溝", "Fossae": "溝", "Chasma": "峡谷", "Chasmata": "峡谷", "Rupes": "断崖",
    "Rupēs": "断崖", "Lacus": "湖", "Lacūs": "湖", "Rima": "溝", "Rimae": "溝", "Tholus": "丘", "Tholi": "丘", "Regio": "地域",
    "Regiones": "地域", "Planitia": "平原", "Planitiae": "平原", "Sulcus": "筋状の地形", "Sulci": "筋状の地形",
    "Fluctus": "流れの跡", "Fluctūs": "流れの跡", "Astronaut-named features": "宇宙飛行士の命名",
    "Linea": "線状の模様", "Lineae": "線状の模様", "Catena": "クレーターの列", "Catenae": "クレーターの列",
    "Tessera": "入り組んだ高地", "Tesserae": "入り組んだ高地", "Facula": "明るい斑点", "Faculae": "明るい斑点",
    "Planum": "高原", "Plana": "高原", "Mensa": "卓状の台地", "Mensae": "卓状の台地", "Collis": "丘", "Colles": "丘陵",
    "Cavus": "くぼ地", "Cavi": "くぼ地", "Sinus": "入江", "Chaos": "崩れた地形", "Saxum": "岩", "Saxa": "岩",
    "Labyrinthus": "迷路状の谷", "Labyrinthi": "迷路状の谷", "Terra": "大陸", "Terrae": "大陸", "Mare": "海", "Maria": "海",
    "Macula": "暗い斑点", "Maculae": "暗い斑点", "Eruptive center": "噴火の中心", "Unda": "砂丘", "Undae": "砂丘",
    "Labes": "地すべり", "Labēs": "地すべり", "Scopulus": "断崖", "Scopuli": "断崖", "Insula": "島", "Insulae": "島",
    "Lacuna": "くぼ地", "Lacunae": "くぼ地", "Flumen": "川", "Flumina": "川", "Large ringed feature": "多重リング構造",
    "Palus": "沼", "Paludes": "沼", "Promontorium": "岬", "Farrum": "平たいドーム", "Farra": "平たいドーム",
    "Virga": "色の筋", "Virgae": "色の筋", "Statio": "着陸地点", "Flexus": "曲がった尾根", "Flexūs": "曲がった尾根",
    "Lingula": "舌状の台地", "Lingulae": "舌状の台地", "Serpens": "蛇行した尾根", "Serpentes": "蛇行した尾根",
    "Fretum": "海峡", "Freta": "海峡", "Lobus": "張り出した地形", "Lobi": "張り出した地形", "Plume": "噴煙",
    "Collum": "狭い水路", "Vastitas": "大低地", "Oceanus": "大洋", "Arcus": "弧状の地形", "Satellite Feature": "",
}


def fetch(work, url, name=None):
    path = os.path.join(work, name or os.path.basename(url))
    if not os.path.exists(path):
        print("download", url)
        with urllib.request.urlopen(urllib.request.Request(url, headers=UA)) as r, open(path + ".part", "wb") as f:
            f.write(r.read())
        os.rename(path + ".part", path)
    return path


def dbf_rows(data):
    n, hlen, rlen = struct.unpack("<IHH", data[4:12])
    fields, p = [], 32
    while data[p] != 0x0D:
        fields.append((data[p:p + 11].split(b"\0")[0].decode(), data[p + 16]))
        p += 32
    for i in range(n):
        r = data[hlen + i * rlen:hlen + (i + 1) * rlen]
        if r[:1] == b"*":   # 消された行
            continue
        o, rec = 1, {}
        for name, ln in fields:
            rec[name] = r[o:o + ln].decode("utf-8", "replace").strip(" \x00")   # 空きは空白か NUL
            o += ln
        yield rec


def num(s, default=0.0):
    try:
        return float(s)
    except ValueError:
        return default


def wrap180(lon):
    return (lon + 180.0) % 360.0 - 180.0


def clean(s):
    return s.replace("\t", " ").strip()


FOLD = {"Ł": "L", "ł": "l", "ı": "i", "ə": "e", "Ə": "E", "Đ": "D", "đ": "d", "Ħ": "H", "ħ": "h", "Ø": "O", "ø": "o"}


def latin(s):
    """ラテン文字の名前: Latin-1 より先の文字（ブラウザ版のフォント Noto Sans JP に無い ą・ş など）は、記号を外した文字に"""
    out = ""
    for c in s:
        if ord(c) > 0xFF:
            c = FOLD.get(c, "".join(x for x in unicodedata.normalize("NFKD", c) if not unicodedata.combining(x)))
        out += c
    return out


def japanese(s):
    """日本語名: Shift_JIS（cp932）に無い字（中国の簡体字など）を含むものは使わない"""
    try:
        s.encode("cp932")
        return s
    except UnicodeEncodeError:
        return ""


def earth(work):
    z = zipfile.ZipFile(fetch(work, NE_URL))
    rows = []
    for r in dbf_rows(z.read("ne_10m_populated_places.dbf")):
        area, pop = num(r["MAX_AREAKM"]), num(r["POP_MAX"])
        if area <= 0:
            area = max(pop, 0.0) / 2000.0
        rad = max(2.0, math.sqrt(area / math.pi))
        en = latin(clean(r["NAME_EN"] or r["NAME"]))
        ja = japanese(clean(r["NAME_JA"]))
        rows.append(("Earth", num(r["LATITUDE"]), num(r["LONGITUDE"]), rad, "", None, en, "" if ja == en else ja))
    return rows


def lower_keys(r):
    return {k.lower(): v for k, v in r.items()}


def shp_polygons(data):
    """シェープファイルの多角形を、記録ごとに輪（(経度, 緯度) のリスト）のリストで返す（多角形でない記録は空）"""
    pos = 100
    while pos < len(data):
        clen = struct.unpack(">i", data[pos + 4:pos + 8])[0] * 2
        rec = data[pos + 8:pos + 8 + clen]
        pos += 8 + clen
        if struct.unpack("<i", rec[:4])[0] != 5:
            yield []
            continue
        nparts, npts = struct.unpack("<2i", rec[36:44])
        idx = list(struct.unpack("<%di" % nparts, rec[44:44 + 4 * nparts])) + [npts]
        xy = struct.unpack("<%dd" % (2 * npts), rec[44 + 4 * nparts:44 + 4 * nparts + 16 * npts])
        yield [[(xy[2 * i], xy[2 * i + 1]) for i in range(idx[k], idx[k + 1])] for k in range(nparts)]


def simplify(pts, tol):
    """Douglas–Peucker で点を間引く（端の点は残す）"""
    if len(pts) < 4:
        return pts
    keep = [False] * len(pts)
    keep[0] = keep[-1] = True
    stack = [(0, len(pts) - 1)]
    while stack:
        a, b = stack.pop()
        (ax, ay), (bx, by) = pts[a], pts[b]
        dx, dy = bx - ax, by - ay
        L = math.hypot(dx, dy)
        best, bi = -1.0, -1
        for i in range(a + 1, b):
            px, py = pts[i][0] - ax, pts[i][1] - ay
            d = abs(dx * py - dy * px) / L if L > 0 else math.hypot(px, py)
            if d > best:
                best, bi = d, i
        if best > tol:
            keep[bi] = True
            stack += [(a, bi), (bi, b)]
    return [p for p, k in zip(pts, keep) if k]


def encode_ring(pts):
    q = [(round(x * 100), round(y * 100)) for x, y in pts]
    q = [p for i, p in enumerate(q) if i == 0 or p != q[i - 1]]
    out, prev = bytearray(), (0, 0)
    for x, y in q:
        for v in (x - prev[0], y - prev[1]):
            z = (v << 1) ^ (v >> 63)   # 符号を交互に（0, -1, 1, -2, …）
            while True:
                if z < 0x80:
                    out.append(z)
                    break
                out.append(z & 0x7F | 0x80)
                z >>= 7
        prev = (x, y)
    return base64.b64encode(bytes(out)).decode(), len(q)


def ring_area(pts):
    """輪の面積 km²（緯度の cos を掛けた平面の近似）"""
    a = 0.0
    for (x0, y0), (x1, y1) in zip(pts, pts[1:] + pts[:1]):
        a += (x0 * y1 - x1 * y0) * math.cos(math.radians((y0 + y1) / 2))
    return abs(a) / 2 * (math.pi / 180 * 6371.0) ** 2


def earth_areas(work):
    areas = []
    for layer, url, tol in NE_AREAS:
        z = zipfile.ZipFile(fetch(work, url))
        stem = os.path.basename(url)[:-4]
        recs = [lower_keys(r) for r in dbf_rows(z.read(stem + ".dbf"))]
        for r, rings in zip(recs, shp_polygons(z.read(stem + ".shp"))):
            if not rings or r.get("featurecla") == "Dragons-be-here":
                continue
            en = latin(clean(r.get("name_en") or r.get("name", "")))
            ja = japanese(clean(r.get("name_ja", ""))) or en
            if not en:
                continue
            rs = [simplify(ring, tol) for ring in rings]
            rs = [ring for ring in rs if len(ring) >= 3]
            if rs:
                areas.append((layer, sum(ring_area(ring) for ring in rs), ja, en, rs))
    return areas


def earth_peaks(work):
    z = zipfile.ZipFile(fetch(work, NE_PEAKS))
    rows = []
    for r in dbf_rows(z.read("ne_10m_geography_regions_elevation_points.dbf")):
        r = lower_keys(r)
        en = latin(clean(r.get("name_en") or r["name"]))
        if r["featurecla"] != "mountain" or not en:
            continue
        ja = japanese(clean(r.get("name_ja", "")))
        rows.append(("Earth", num(r["lat_y"]), num(r["long_x"]), PEAK_RADIUS, "", None, en, "" if ja == en else ja))
    return rows


def iau(work):
    rows = []
    for t in IAU_TARGETS:
        z = zipfile.ZipFile(fetch(work, IAU_URL % t))
        body = t.capitalize()
        for r in dbf_rows(z.read([n for n in z.namelist() if n.endswith(".dbf")][0])):
            name, typ = latin(clean(r["name"])), r["type"]
            if r["approval"] != "Adopted by IAU" or typ == "Satellite Feature" or re.search(r" [A-Z]{1,2}$", r["clean_name"]):
                continue
            d = num(r["diameter"])
            if d <= 0:
                continue
            # 名前に記述語が入っていれば、その語の日本語。入っていなければ（クレーターなど）種類の単数形の日本語
            words = typ.split(", ")
            word = next((w for w in name.split() if w in TYPE_JA and w.lower() in [x.lower() for x in words]), None)
            kind = (TYPE_JA.get(word or typ, TYPE_JA.get(words[0].capitalize(), "")), "crater" if words[0] == "Crater" else "")
            box = ""
            la0, la1, lo0, lo1 = (num(r[k], float("nan")) for k in ("min_lat", "max_lat", "min_lon", "max_lon"))
            if all(math.isfinite(v) for v in (la0, la1, lo0, lo1)) and lo1 - lo0 < 359.9:
                box = "%.3f,%.3f,%.3f,%.3f" % (la0, la1, wrap180(lo0), wrap180(lo1))
            rows.append((body, num(r["center_lat"]), wrap180(num(r["center_lon"])), d / 2, box, kind, name, ""))
    return rows


def main():
    work = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else "assets"
    os.makedirs(work, exist_ok=True)
    os.makedirs(out, exist_ok=True)
    rows = earth(work) + earth_peaks(work) + iau(work)
    areas = earth_areas(work)
    kinds, body = [], None
    with open(os.path.join(out, "place_names.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write("# tools/make_place_names.py: Natural Earth populated places, IAU/USGS Gazetteer of Planetary Nomenclature\n")
        for b, lat, lon, rad, box, kind, name, ja in rows:
            if kind is not None and kind[0] and kind not in kinds:
                kinds.append(kind)
                f.write("K\t%s\t%s\n" % kind)
            if b != body:
                body = b
                f.write("B\t%s\n" % b)
            k = kinds.index(kind) if kind is not None and kind[0] else -1
            f.write("%.3f\t%.3f\t%.3f\t%s\t%d\t%s%s\n" % (lat, lon, rad, box, k, name, "\t" + ja if ja else ""))
        f.write("B\tEarth\n")
        npts = 0
        for layer, area, ja, en, rings in areas:
            f.write("A\t%d\t%.0f\t%s\t%s\n" % (layer, area, ja, en))
            for ring in rings:
                code, n = encode_ring(ring)
                npts += n
                f.write("P\t%s\n" % code)
    print("place_names.txt  %d places (Earth %d), %d Earth areas (%d points)" % (
        len(rows), sum(r[0] == "Earth" for r in rows), len(areas), npts))


if __name__ == "__main__":
    main()
