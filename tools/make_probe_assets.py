#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""探査機の軌跡 (assets/probes.bin) を JPL Horizons から作る。

太陽中心の位置・速度（黄道座標 J2000, km, km/s）を、惑星の影響圏（シミュレータと同じ式の半径）の中では
惑星中心からの相対位置で持つ。シミュレータの惑星の位置には誤差（木星で最大 100 万 km 程度）があるので、
影響圏の中では「シミュレータの惑星 + 相対位置」として、フライバイが実際の距離・向きで起きるようにする。
点は 3 次エルミート補間（位置と速度）の誤差が許容値に収まるところまで間引く。

出力形式（リトルエンディアン）:
  uint32 探査機の数
  探査機ごと: char[32] 英語名, uint32 区間の数
    区間ごと: int32 基準天体（0: 太陽, それ以外: シミュレータの惑星番号）,
              float64[3] 区間の始まりでの基準天体の実際の位置, float64[3] 終わりでの位置（太陽中心）,
              uint32 点の数, 点ごとに float64 時刻 [J2000 からの日数], float64[3] 位置 [km], float32[3] 速度 [km/s]

使い方: python3 tools/make_probe_assets.py <作業ディレクトリ> [出力先=assets]
（Horizons の結果を作業ディレクトリにキャッシュする。初回は数百 MB を取得し数十分かかる）
"""
import hashlib
import json
import math
import os
import struct
import sys
import urllib.parse
import urllib.request

UA = "SolarSystemVoyager-asset-build/1.0 (https://github.com/hubstacksea/Solar-System-Simulator)"
AU = 149597870.7
MSUN = 1.989e30
J2000 = 2451545.0

# シミュレータの天体番号: Horizons の ID, 軌道長半径 [AU], 質量 [kg]（影響圏の半径 = a (m / M)^0.4）
# 彗星・小惑星は影響圏が小さすぎるので、4 番目の値を相対位置で持つ範囲の半径 [km] にする
PLANETS = {2: (299, 0.72333566, 4.867e24), 3: (399, 1.00000261, 5.972e24), 4: (499, 1.52371034, 6.417e23),
           5: (599, 5.20288700, 1.898e27), 6: (699, 9.53667594, 5.683e26), 7: (799, 19.18916464, 8.681e25),
           8: (899, 30.06992276, 1.024e26), 9: (999, 39.48211675, 1.303e22),
           37: (1000012, 3.47, 1e13, 1e5),        # チュリュモフ・ゲラシメンコ彗星 (67P)
           38: ("162173;", 1.19, 4.5e11, 1e5),    # リュウグウ
           39: (2101955, 1.13, 7.3e10, 1e5),      # ベンヌ
           41: (20065803, 1.64, 5.3e11, 1e6)}     # ディディモス（ディモルフォスとの重心）

# 名前, Horizons ID, データの範囲（UTC）, 天体への接近: (天体番号, 探す範囲の始まり, 終わり, 刻み[, ホームポジション km])
# ホームポジション: Horizons ではその天体を中心にできず、探査機と天体の軌道も別々の解で数 km〜十数 km 食い違うので、
# 範囲の間は天体から地球の方向へこの距離の点に置く（はやぶさ2 はリュウグウの地球側 20 km を基本の待機位置にした）
PROBES = [
    ("Voyager 1", -31, "1977-09-05 14:10", "2099-12-31", [
        (3, "1977-09-05 14:10", "1977-09-15", "10m"), (5, "1979-01-01", "1979-06-01", "10m"),
        (6, "1980-09-15", "1981-01-15", "10m")]),
    ("Voyager 2", -32, "1977-08-20 15:40", "2099-12-31", [
        (3, "1977-08-20 15:40", "1977-08-30", "10m"), (5, "1979-04-15", "1979-10-01", "10m"),
        (6, "1981-06-15", "1981-10-15", "10m"), (7, "1985-12-01", "1986-03-01", "10m"),
        (8, "1989-07-01", "1989-10-15", "10m")]),
    ("Pioneer 10", -23, "1972-03-03 02:15", "2049-12-31", [
        (3, "1972-03-03 02:15", "1972-03-10", "10m"), (5, "1973-10-15", "1974-01-31", "10m")]),
    ("Pioneer 11", -24, "1973-04-06 02:35", "2049-12-31", [
        (3, "1973-04-06 02:35", "1973-04-15", "10m"), (5, "1974-10-15", "1975-01-31", "10m"),
        (6, "1979-07-15", "1979-10-31", "10m")]),
    ("Galileo", -77, "1989-10-19 01:40", "2003-09-21 18:56", [
        (3, "1989-10-19 01:40", "1989-10-25", "10m"), (2, "1990-01-25", "1990-02-25", "10m"),
        (3, "1990-11-25", "1990-12-20", "10m"), (3, "1992-11-25", "1992-12-20", "10m"),
        (5, "1995-09-01", "2003-09-21 18:56", "20m")]),
    ("Cassini", -82, "1997-10-15 09:40", "2017-09-15 10:31", [
        (3, "1997-10-15 09:40", "1997-10-25", "10m"), (2, "1998-04-10", "1998-05-10", "10m"),
        (2, "1999-06-10", "1999-07-10", "10m"), (3, "1999-08-05", "1999-08-30", "10m"),
        (5, "2000-11-01", "2001-02-28", "10m"), (6, "2004-03-01", "2017-09-15 10:31", "20m")]),
    ("New Horizons", -98, "2006-01-19 20:00", "2049-12-31", [
        (3, "2006-01-19 20:00", "2006-01-25", "10m"), (5, "2007-01-15", "2007-04-15", "10m"),
        (9, "2015-06-15", "2015-08-15", "10m")]),
    ("Parker Solar Probe", -96, "2018-08-12 08:30", "2029-12-31", [
        (3, "2018-08-12 08:30", "2018-08-20", "10m")] + [
        (2, a, b, "10m") for a, b in [("2018-09-25", "2018-10-12"), ("2019-12-18", "2020-01-04"),
                                       ("2020-07-03", "2020-07-20"), ("2021-02-12", "2021-03-01"),
                                       ("2021-10-08", "2021-10-25"), ("2023-08-13", "2023-08-30"),
                                       ("2024-10-29", "2024-11-15")]]),
    ("Juno", -61, "2011-08-05 17:30", "2028-09-01", [
        (3, "2011-08-05 17:30", "2011-08-12", "10m"), (3, "2013-10-01", "2013-10-20", "10m"),
        (5, "2016-06-01", "2028-09-01", "10m")]),
    ("Rosetta", -226, "2004-03-02 09:40", "2016-09-30 10:39", [
        (3, "2004-03-02 09:40", "2004-03-10", "10m"), (3, "2005-02-25", "2005-03-15", "10m"),
        (4, "2007-02-15", "2007-03-10", "10m"), (3, "2007-11-05", "2007-11-25", "10m"),
        (3, "2009-11-05", "2009-11-25", "10m"), (37, "2014-08-06 09:00", "2016-09-30 10:39", "10m")]),
    ("Hayabusa2", -37, "2014-12-03 06:20", "2026-12-31", [
        (3, "2014-12-03 06:20", "2014-12-12", "10m"), (3, "2015-11-25", "2015-12-12", "10m"),
        (38, "2018-06-27 09:00", "2019-11-13 01:00", "1h", 20.0), (3, "2020-11-28", "2020-12-12", "10m")]),
    ("OSIRIS-REx", -64, "2016-09-09 00:00", "2030-03-22", [
        (3, "2016-09-09 00:00", "2016-09-14", "10m"), (3, "2017-09-15", "2017-09-29", "10m"),
        (39, "2018-12-03 19:00", "2021-05-10 20:00", "10m"), (3, "2023-09-17", "2023-10-01", "10m")]),
    ("DART", -135, "2021-11-24 07:20", "2022-09-26 23:14", [
        (3, "2021-11-24 07:20", "2021-11-30", "10m"), (41, "2022-09-25", "2022-09-26 23:14", "1m")]),
]
COARSE = "1d"
# 太陽に極端に近づく機体は太陽中心の区間も細かく取る（パーカーは近日点で 1 日に 140° 近く回り込む）
COARSE_BY_PROBE = {"Parker Solar Probe": "1h"}
MAX_ROWS = 40000


def jd_of(s):
    """'YYYY-MM-DD[ hh:mm]' (UTC) → ユリウス日"""
    d, _, hm = s.partition(" ")
    y, m, dd = (int(x) for x in d.split("-"))
    h, mi = (int(x) for x in hm.split(":")) if hm else (0, 0)
    if m <= 2:
        y, m = y - 1, m + 12
    a = y // 100
    b = 2 - a + a // 4
    return int(365.25 * (y + 4716)) + int(30.6001 * (m + 1)) + dd + b - 1524.5 + (h + mi / 60) / 24


def step_days(step):
    return {"m": 1 / 1440, "h": 1 / 24, "d": 1.0}[step[-1]] * int(step[:-1])


def horizons(work, cmd, center, jd0, jd1, step):
    """[(t, (x, y, z), (vx, vy, vz))] t: J2000 からの日数。結果は work にキャッシュする"""
    out = []
    n = step_days(step)
    a = jd0
    while a < jd1 - 1e-9:
        b = min(jd1, a + n * MAX_ROWS)
        q = {"format": "json", "COMMAND": "'%s'" % cmd, "CENTER": "'500@%s'" % center, "EPHEM_TYPE": "VECTORS",
             "START_TIME": "'JD%.6f'" % a, "STOP_TIME": "'JD%.6f'" % b, "STEP_SIZE": "'%s'" % step,
             "REF_PLANE": "ECLIPTIC", "REF_SYSTEM": "ICRF", "OUT_UNITS": "KM-S", "VEC_TABLE": "2",
             "CSV_FORMAT": "YES", "OBJ_DATA": "NO"}
        url = "https://ssd.jpl.nasa.gov/api/horizons.api?" + urllib.parse.urlencode(q)
        path = os.path.join(work, hashlib.sha1(url.encode()).hexdigest() + ".json")
        if not os.path.exists(path):
            print("  horizons %s @%s %s .. %s step %s" % (cmd, center, a, b, step), flush=True)
            for attempt in range(6):
                try:
                    req = urllib.request.Request(url, headers={"User-Agent": UA})
                    data = urllib.request.urlopen(req, timeout=300).read()
                    break
                except Exception as e:  # 一時的なエラーは待って再試行
                    print("  retry", attempt, e, flush=True)
                    import time
                    time.sleep(10 * (attempt + 1))
            else:
                raise SystemExit("Horizons に接続できません")
            if b"$$SOE" not in data:   # データの範囲外などのエラーはキャッシュしない
                raise SystemExit(json.loads(data)["result"][-600:])
            with open(path + ".part", "wb") as f:
                f.write(data)
            os.rename(path + ".part", path)
        with open(path, encoding="utf-8") as f:
            res = json.load(f)["result"]
        body = res.split("$$SOE")[1].split("$$EOE")[0]
        for line in body.strip().splitlines():
            v = [x.strip() for x in line.split(",")]
            t = float(v[0]) - J2000
            if out and t <= out[-1][0] + 1e-9:
                continue
            out.append((t, tuple(float(x) for x in v[2:5]), tuple(float(x) for x in v[5:8])))
        a = b
    return out


def hermite(p0, v0, p1, v1, t0, t1, t):
    h = t1 - t0
    s = (t - t0) / h
    h00, h10, h01, h11 = 2 * s ** 3 - 3 * s ** 2 + 1, s ** 3 - 2 * s ** 2 + s, -2 * s ** 3 + 3 * s ** 2, s ** 3 - s ** 2
    hs = h * 86400.0  # 速度は km/s
    return tuple(h00 * p0[k] + h10 * hs * v0[k] + h01 * p1[k] + h11 * hs * v1[k] for k in range(3))


def decimate(samples, tol):
    """エルミート補間で間の点が tol(位置) 以内に収まる範囲で点を減らす"""
    if len(samples) <= 2:
        return samples

    def ok(i, j):
        (t0, p0, v0), (t1, p1, v1) = samples[i], samples[j]
        for k in range(i + 1, j):
            t, p, _ = samples[k]
            q = hermite(p0, v0, p1, v1, t0, t1, t)
            if math.dist(p, q) > tol(p):
                return False
        return True

    keep = [0]
    i = 0
    n = len(samples)
    while i < n - 1:
        step = 1
        while i + step * 2 < n and ok(i, i + step * 2):
            step *= 2
        lo, hi = step, min(step * 2, n - 1 - i)
        while lo < hi:   # ok(i, i+lo) が成り立つ最大の lo
            mid = (lo + hi + 1) // 2
            if ok(i, i + mid):
                lo = mid
            else:
                hi = mid - 1
        i += lo
        keep.append(i)
    return [samples[k] for k in keep]


def soi(idx):
    _, a, m, *radius = PLANETS[idx]
    return radius[0] if radius else a * AU * (m / MSUN) ** 0.4


def home_position(work, idx, ja, jb, step, dist):
    """天体から地球の方向へ dist km の点（相対位置と、太陽中心の位置）"""
    body = horizons(work, PLANETS[idx][0], "10", ja, jb, step)
    earth = horizons(work, 399, "10", ja, jb, step)
    rel, hel = [], []
    for (t, pb, vb), (_, pe, _) in zip(body, earth):
        d = [pe[k] - pb[k] for k in range(3)]
        n = math.sqrt(sum(x * x for x in d))
        rel.append([t, tuple(x / n * dist for x in d)])
    for i, r in enumerate(rel):   # 相対速度は差分で（ほぼ 0）
        a, b = rel[max(0, i - 1)], rel[min(len(rel) - 1, i + 1)]
        dt = (b[0] - a[0]) * 86400.0
        r.append(tuple((b[1][k] - a[1][k]) / dt for k in range(3)))
    for (t, p, v), (_, pb, vb) in zip(rel, body):
        hel.append((t, tuple(pb[k] + p[k] for k in range(3)), tuple(vb[k] + v[k] for k in range(3))))
    return [tuple(r) for r in rel], hel


def main():
    work = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else "assets"
    os.makedirs(work, exist_ok=True)
    blob = struct.pack("<I", len(PROBES))
    for name, cmd, s0, s1, encounters in PROBES:
        print(name, flush=True)
        jd0, jd1 = jd_of(s0), jd_of(s1)
        helio = horizons(work, cmd, "10", jd0, jd1, COARSE_BY_PROBE.get(name, COARSE))
        windows = []   # (惑星番号, 相対位置の点, 始まりと終わりの惑星の位置)
        fine_helio = []
        for planet, a, b, step, *home in encounters:
            ja, jb = max(jd0, jd_of(a)), min(jd1, jd_of(b))
            if home:
                rel, hel = home_position(work, planet, ja, jb, step, home[0])
            else:
                rel = horizons(work, cmd, PLANETS[planet][0], ja, jb, step)
                hel = horizons(work, cmd, "10", ja, jb, step)
            assert len(rel) == len(hel), (name, planet)
            R = soi(planet)
            inside = [math.sqrt(sum(x * x for x in r[1])) < R for r in rel]
            k = 0
            while k < len(rel):
                if not inside[k]:
                    fine_helio.append(hel[k])
                    k += 1
                    continue
                e = k
                while e + 1 < len(rel) and inside[e + 1]:
                    e += 1
                pts = rel[k:e + 1]
                ends = [tuple(hel[i][1][j] - rel[i][1][j] for j in range(3)) for i in (k, e)]
                windows.append((planet, pts, ends))
                # 区間の境目の点は太陽中心でも持つ（前後の区間とつなぐため）
                fine_helio += [hel[k], hel[e]]
                k = e + 1
        windows.sort(key=lambda w: w[1][0][0])
        spans = [(w[1][0][0], w[1][-1][0]) for w in windows]

        def outside(t):
            return all(not (a < t < b) for a, b in spans)

        allh = sorted({s[0]: s for s in helio + fine_helio if outside(s[0])}.values())
        # 太陽中心の点列を窓の位置で切り分け、窓の区間を間に挟む（境目の点は前後の区間で共有）
        segs = []
        hi = 0
        for planet, pts, ends in windows:
            ta, tb = pts[0][0], pts[-1][0]
            before = [s for s in allh[hi:] if s[0] <= ta + 1e-9]
            if len(before) >= 2:
                segs.append((0, before, None))
            segs.append((planet, pts, ends))
            hi = next((i for i, s in enumerate(allh) if s[0] >= tb - 1e-9), len(allh))
        rest = allh[hi:]
        if len(rest) >= 2:
            segs.append((0, rest, None))

        blob += struct.pack("<32sI", name.encode()[:31], len(segs))
        total = 0
        for body, pts, ends in segs:
            if body == 0:
                d = decimate(pts, lambda p: max(50.0, 2e-7 * math.sqrt(sum(x * x for x in p))))
            else:
                d = decimate(pts, lambda p: max(0.5, 1e-5 * math.sqrt(sum(x * x for x in p))))
            e0, e1 = ends if ends else ((0, 0, 0), (0, 0, 0))
            blob += struct.pack("<i6dI", body, *e0, *e1, len(d))
            for t, p, v in d:
                blob += struct.pack("<4d3f", t, *p, *v)
            total += len(d)
            print("   %-8s %s .. %s  %6d -> %5d" % ("Sun" if body == 0 else "body %d" % body,
                                                   round(pts[0][0], 2), round(pts[-1][0], 2), len(pts), len(d)))
        print("  points", total, flush=True)
    with open(os.path.join(out, "probes.bin"), "wb") as f:
        f.write(blob)
    print("wrote", len(blob), "bytes")


if __name__ == "__main__":
    main()
