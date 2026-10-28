#!/usr/bin/env python3
"""衛星の公転位相を JPL Horizons に合わせる（src/solar_data.h の JPL 表の phase / P を出力する）。

軌道面は JPL の平均軌道要素（https://ssd.jpl.nasa.gov/sats/elem/ の Laplace 面・傾斜角・昇交点）を使い、
面内の角度 u(t) = u0 + 360 t / P + A sin(2π t / T) + B cos(2π t / T)（t: J2000 からの日数）を
1980〜2070 年の Horizons の位置に最小二乗で合わせる。
  - u0: J2000 での角度, P: 平均の公転周期（日）
  - A, B: ミマスとテティスの共鳴による秤動（周期 T = 70.56 年。ミマスで振幅 約 44°）
円軌道の近似なので、離心率や軌道面の歳差による数度以内のずれは残る。

使い方: python3 tools/fit_moon_orbits.py
"""
import json
import math
import re
import urllib.parse
import urllib.request

D = math.radians
UA = "SolarSystemVoyager-orbit-fit/1.0 (https://github.com/hubstacksea/Solar-System-Simulator)"

# 名前, Horizons ID, 母天体 ID, 傾斜角, 昇交点, 基準面の極 RA/Dec（None: 母天体の赤道面）, 周期の目安（日）
MOONS = [
    ("Phobos", 401, 499, 1.1, 169.2, (317.7, 52.9), 0.31891),
    ("Deimos", 402, 499, 1.8, 54.3, (316.6, 53.5), 1.26244),
    ("Io", 501, 599, 0.0, 0.0, (268.1, 64.5), 1.769138),
    ("Europa", 502, 599, 0.5, 184.0, (268.1, 64.5), 3.551181),
    ("Ganymede", 503, 599, 0.2, 58.5, (268.2, 64.6), 7.154553),
    ("Callisto", 504, 599, 0.3, 309.1, (268.7, 64.8), 16.689018),
    ("Mimas", 601, 699, 1.6, 66.2, (40.6, 83.5), 0.942422),
    ("Enceladus", 602, 699, 0.0, 0.0, (40.6, 83.5), 1.370218),
    ("Tethys", 603, 699, 1.1, 273.0, (40.6, 83.5), 1.887802),
    ("Dione", 604, 699, 0.0, 0.0, (40.6, 83.5), 2.736915),
    ("Rhea", 605, 699, 0.3, 133.7, (40.6, 83.5), 4.517503),
    ("Titan", 606, 699, 0.3, 78.6, (36.4, 84.0), 15.945),
    ("Iapetus", 608, 699, 7.6, 86.5, (288.7, 78.9), 79.3215),
    # 天王星は逆行自転なので、衛星の公転の向きに合わせて IAU の北極と反対側の極を基準にする
    ("Miranda", 705, 799, 4.4, 100.9, (77.31, 15.18), 1.413479),
    ("Ariel", 701, 799, 0.0, 0.0, (77.31, 15.18), 2.520379),
    ("Umbriel", 702, 799, 0.1, 174.8, (77.31, 15.18), 4.144177),
    ("Titania", 703, 799, 0.1, 29.5, (77.31, 15.18), 8.705872),
    ("Oberon", 704, 799, 0.1, 76.8, (77.31, 15.18), 13.463239),
    ("Triton", 801, 899, 157.3, 177.7, None, 5.876854),
    ("Charon", 901, 999, 0.0, 0.0, None, 6.387230),
]
# 母天体の自転軸（solar_data.h と同じ IAU の値）
PLANET_POLE = {899: (299.36, 43.46), 999: (132.99, -6.16)}
LIBRATION = {"Mimas", "Tethys"}
T_LIB = 70.56 * 365.25

J2000 = 2451545.0
SAMPLES = [2444239.5 + k * 1150.0 for k in range(29)]   # 1980〜2070 年


def equ(ra, dec):
    return (math.cos(D(dec)) * math.cos(D(ra)), math.cos(D(dec)) * math.sin(D(ra)), math.sin(D(dec)))


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def norm(a):
    l = math.sqrt(sum(x * x for x in a))
    return tuple(x / l for x in a)


def lin(*terms):
    return tuple(sum(k * v[i] for k, v in terms) for i in range(3))


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def basis(m):
    """solar_data.h / main.cpp と同じ軌道面の基底 (oN, oQ)。ICRF 赤道座標"""
    _, _, parent, inc, node, pole, _ = m
    ref = equ(*(pole if pole else PLANET_POLE[parent]))
    e1 = norm(cross((0, 0, 1), ref))
    e2 = cross(ref, e1)
    n = lin((math.cos(D(node)), e1), (math.sin(D(node)), e2))
    q = lin((math.cos(D(inc)), cross(ref, n)), (math.sin(D(inc)), ref))
    return n, q


def horizons(code, center, jds):
    q = urllib.parse.urlencode({
        "format": "json", "COMMAND": "'%d'" % code, "CENTER": "'500@%d'" % center, "EPHEM_TYPE": "VECTORS",
        "TLIST": " ".join("'%s'" % j for j in jds), "REF_PLANE": "FRAME", "REF_SYSTEM": "ICRF",
        "OUT_UNITS": "KM-S", "VEC_TABLE": "1", "CSV_FORMAT": "YES", "OBJ_DATA": "NO"})
    req = urllib.request.Request("https://ssd.jpl.nasa.gov/api/horizons.api?" + q, headers={"User-Agent": UA})
    res = json.load(urllib.request.urlopen(req, timeout=60))["result"]
    body = re.search(r"\$\$SOE\s*(.*?)\$\$EOE", res, re.S).group(1)
    out = []
    for line in body.strip().splitlines():
        f = [x.strip() for x in line.split(",")]
        out.append((float(f[2]), float(f[3]), float(f[4])))
    return out


def solve(rows, ys):
    """最小二乗（正規方程式をガウスの消去法で解く）"""
    n = len(rows[0])
    a = [[sum(r[i] * r[j] for r in rows) for j in range(n)] + [sum(r[i] * y for r, y in zip(rows, ys))] for i in range(n)]
    for i in range(n):
        piv = max(range(i, n), key=lambda k: abs(a[k][i]))
        a[i], a[piv] = a[piv], a[i]
        for k in range(n):
            if k != i:
                f = a[k][i] / a[i][i]
                a[k] = [x - f * y for x, y in zip(a[k], a[i])]
    return [a[i][n] / a[i][i] for i in range(n)]


def main():
    print("// tools/fit_moon_orbits.py の出力: 名前, u0 [deg], P [日], 秤動 A, B [deg]")
    for m in MOONS:
        name, code, parent, _, _, _, p0 = m
        n, q = basis(m)
        pos = horizons(code, parent, SAMPLES)
        lib = name in LIBRATION
        rows, ys = [], []
        for jd, r in zip(SAMPLES, pos):
            t = jd - J2000
            u = math.degrees(math.atan2(dot(r, q), dot(r, n)))
            pred = 360.0 / p0 * t
            ys.append(pred + (u - pred + 180) % 360 - 180)   # 目安の周期で展開した連続な角度
            w = 2 * math.pi * t / T_LIB
            rows.append([1.0, t] + ([math.sin(w), math.cos(w)] if lib else []))
        c = solve(rows, ys)
        u0, rate = c[0], c[1]
        la, lb = (c[2], c[3]) if lib else (0.0, 0.0)
        res = max(abs(y - sum(k * x for k, x in zip(c, r))) for r, y in zip(rows, ys))
        print('    {"%s", %.3f, %.7f, %.2f, %.2f},  // 最大残差 %.1f°' % (name, u0 % 360, 360.0 / rate, la, lb, res))


if __name__ == "__main__":
    main()
