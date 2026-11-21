#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""地表の近くで今いる所の地名を出すための表 (assets/place_names.txt) を作る。

データ:
  - 地球の都市: Natural Earth 1:10m Populated Places（パブリックドメイン）。名前（日本語は NAME_JA）、緯度・経度と、
    市街地の広さ MAX_AREAKM（LandScan から求めた面積 km²）。広さの無い都市は人口から 1 km² に 2,000 人として広さを推定
  - 地球以外: IAU の惑星の地名（USGS Gazetteer of Planetary Nomenclature。米国政府の著作物でパブリックドメイン）の
    天体ごとの中心点のシェープファイル。IAU が承認した名前だけで、衛星クレーター（「Copernicus A」のような
    文字の付いたもの）は除く。中心（惑星中心緯度・東経 0〜360°）、直径、範囲の緯度・経度

出力（UTF-8 のタブ区切り）:
  K  種類の日本語  英語名に付ける語        … 地形の種類の表（出てきた順に 0, 1, 2, …）
  B  天体の英語名                          … 以下はこの天体の地名
  緯度  経度（東が正、-180〜180）  半径 km  範囲（南端,北端,西端,東端 の度。無ければ空）  種類（-1: 無し）  名前  [日本語名]
  日本語名が無ければ「名前（種類の日本語）」、英語名は「名前 英語名に付ける語」。
  地名の場所: 中心から半径以内で、範囲があればその中（実際の形ではなく円と四角で近似）

使い方: python3 tools/make_place_names.py <作業ディレクトリ> [出力先=assets]
必要なもの: Python だけ（ダウンロード 約 45 MB）
"""
import io
import math
import os
import re
import struct
import sys
import unicodedata
import urllib.request
import zipfile

NE_URL = "https://naciscdn.org/naturalearth/10m/cultural/ne_10m_populated_places.zip"
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
            rec[name] = r[o:o + ln].decode("utf-8", "replace").strip()
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
    rows = earth(work) + iau(work)
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
    print("place_names.txt  %d places (Earth %d)" % (len(rows), sum(r[0] == "Earth" for r in rows)))


if __name__ == "__main__":
    main()
