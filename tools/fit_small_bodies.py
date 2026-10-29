#!/usr/bin/env python3
"""準惑星・小惑星・彗星の軌道要素を JPL Horizons に合わせる（src/solar_data.h の SMALL_BODIES を出力する）。

惑星と同じ形（J2000 の軌道要素と 1 世紀あたりの変化率、src/main.cpp の PlanetPos）で、
1980〜2070 年の Horizons の太陽中心の位置（黄道座標 J2000）に最小二乗で合わせる。
誤差は太陽から見た方向の角度で測る（遠日点付近の大きな距離に引きずられないように）。
惑星の摂動でずれる分を変化率が吸収するので、Horizons の J2000 の接触軌道要素をそのまま使うより正確になる。

使い方: python3 tools/fit_small_bodies.py [名前 ...]（省略するとすべて）
"""
import json
import math
import re
import sys
import urllib.parse
import urllib.request

UA = "SolarSystemVoyager-orbit-fit/1.0 (https://github.com/hubstacksea/Solar-System-Simulator)"
BODIES = [("Ceres", "1;"), ("Vesta", "4;"), ("Eris", "136199;"), ("Halley", "DES=1P;CAP;NOFRAG"),
          ("Hale-Bopp", "DES=C/1995 O1;CAP;NOFRAG"), ("Hyakutake", "DES=C/1996 B2;CAP;NOFRAG")]
J2000 = 2451545.0
START, STOP, STEP = "1980-01-01", "2070-01-01", "20 d"
D = math.radians


def horizons(cmd, **kw):
    q = {"format": "json", "COMMAND": "'%s'" % cmd, "CENTER": "'500@10'", "REF_PLANE": "ECLIPTIC",
         "REF_SYSTEM": "ICRF", "OUT_UNITS": "AU-D", "CSV_FORMAT": "YES", "OBJ_DATA": "NO"}
    q.update(kw)
    req = urllib.request.Request("https://ssd.jpl.nasa.gov/api/horizons.api?" + urllib.parse.urlencode(q),
                                 headers={"User-Agent": UA})
    res = json.load(urllib.request.urlopen(req, timeout=120))["result"]
    body = re.search(r"\$\$SOE\s*(.*?)\$\$EOE", res, re.S).group(1)
    return [[x.strip() for x in line.split(",")] for line in body.strip().splitlines()]


def position(p, t):
    """PlanetPos と同じ計算。p = [a, e, i, L, varpi, node] + 各変化率 /世紀, t: J2000 からの日数"""
    T = t / 36525.0
    a, e, inc, L, varpi, node = (p[k] + p[k + 6] * T for k in range(6))
    M = math.radians((L - varpi + 180) % 360 - 180)
    E = M + 0.85 * e * (1 if M >= 0 else -1)
    for _ in range(60):
        dE = (E - e * math.sin(E) - M) / (1 - e * math.cos(E))
        E -= dE
        if abs(dE) < 1e-13:
            break
    w, O, I = D(varpi - node), D(node), D(inc)
    xp, yp = a * (math.cos(E) - e), a * math.sqrt(1 - e * e) * math.sin(E)
    cw, sw, cO, sO, cI, sI = math.cos(w), math.sin(w), math.cos(O), math.sin(O), math.cos(I), math.sin(I)
    return ((cw * cO - sw * sO * cI) * xp + (-sw * cO - cw * sO * cI) * yp,
            (cw * sO + sw * cO * cI) * xp + (-sw * sO + cw * cO * cI) * yp,
            (sw * sI) * xp + (cw * sI) * yp)


def residuals(p, samples):
    out = []
    for t, r in samples:
        q = position(p, t)
        n = math.sqrt(sum(x * x for x in r))
        out += [(q[k] - r[k]) / n for k in range(3)]   # 太陽から見た方向の誤差 [rad] にほぼ等しい
    return out


def solve(A, b):
    n = len(b)
    a = [A[i][:] + [b[i]] for i in range(n)]
    for i in range(n):
        piv = max(range(i, n), key=lambda k: abs(a[k][i]))
        a[i], a[piv] = a[piv], a[i]
        for k in range(n):
            if k != i:
                f = a[k][i] / a[i][i]
                a[k] = [x - f * y for x, y in zip(a[k], a[i])]
    return [a[i][n] / a[i][i] for i in range(n)]


def fit(p, samples):
    """レーベンバーグ・マーカート法（数値微分）"""
    lam = 1e-3
    r = residuals(p, samples)
    cost = sum(x * x for x in r)
    steps = [1e-7, 1e-8, 1e-6, 1e-5, 1e-5, 1e-5] + [1e-5, 1e-6, 1e-4, 1e-3, 1e-3, 1e-3]
    for _ in range(100):
        J = []
        for k in range(12):
            q = p[:]
            q[k] += steps[k]
            J.append([(x - y) / steps[k] for x, y in zip(residuals(q, samples), r)])
        JTJ = [[sum(a * b for a, b in zip(J[i], J[j])) for j in range(12)] for i in range(12)]
        JTr = [-sum(a * b for a, b in zip(J[i], r)) for i in range(12)]
        improved = False
        for _ in range(20):
            A = [[JTJ[i][j] * (1 + lam if i == j else 1) for j in range(12)] for i in range(12)]
            dp = solve(A, JTr)
            q = [x + y for x, y in zip(p, dp)]
            rq = residuals(q, samples)
            cq = sum(x * x for x in rq)
            if cq < cost:
                rel = (cost - cq) / cost
                p, r, cost, lam, improved = q, rq, cq, lam * 0.3, True
                break
            lam *= 10
        if not improved or rel < 1e-10:
            break
    return p, r


def main():
    print("// tools/fit_small_bodies.py の出力: a [AU], e, i, L, ϖ, Ω [deg] と 1 世紀あたりの変化率")
    for name, cmd in BODIES:
        if len(sys.argv) > 1 and name not in sys.argv[1:]:
            continue
        el = horizons(cmd, EPHEM_TYPE="ELEMENTS", TLIST="'%s'" % J2000)[0]
        e, i, node, w, M, a = (float(el[k]) for k in (2, 4, 5, 6, 9, 11))
        n = float(el[8])  # deg/日
        p = [a, e, i, (M + w + node) % 360, (w + node) % 360, node, 0, 0, 0, n * 36525, 0, 0]
        vec = horizons(cmd, EPHEM_TYPE="VECTORS", START_TIME="'%s'" % START, STOP_TIME="'%s'" % STOP,
                       STEP_SIZE="'%s'" % STEP, VEC_TABLE="1")
        samples = [(float(v[0]) - J2000, (float(v[2]), float(v[3]), float(v[4]))) for v in vec]
        r0 = residuals(p, samples)
        p, r = fit(p, samples)
        err = lambda rr: max(math.degrees(math.sqrt(sum(x * x for x in rr[k:k + 3]))) for k in range(0, len(rr), 3))
        print('    {"%s", %.8f, %.8f, %.6f, %.6f, %.6f, %.6f,' % (name, *p[:6]))
        print('     %.8f, %.8f, %.6f, %.6f, %.6f, %.6f},  // 最大誤差 %.3f°（J2000 の要素のままなら %.2f°）'
              % (*p[6:], err(r), err(r0)))


if __name__ == "__main__":
    main()
