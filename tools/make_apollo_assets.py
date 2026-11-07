#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""アポロ計画の宇宙船の軌跡 (assets/apollo.bin) を、NASA の飛行記録の数値から作る。

宇宙船そのものの軌道の暦は公開されていないので、記録にある時刻ごとの位置と速度（緯度・経度・高度・慣性速度・経路角・方位角）
を元に、次のように組み立てる。
  - 慣性飛行の間: 地球・月・太陽の重力（地球と月は扁平 J2 も）で積分する。前後の記録の位置を両方通るよう、始まりの速度を
    少しだけ直す（狙い撃ち法）。直した量と、終わりの記録の速度との差を表示する（記録の確かめ）
  - 噴射の間（点火と停止の記録の間）: 前後の位置と速度をつなぐ 3 次エルミート曲線
  - 打ち上げ: 上昇の記録の点（地球固定座標）をつなぐ
  - 月面への降下・月面からの上昇: 月固定座標で、記録の時刻・高度・速さを通る水平と垂直の動き（中間は等加速度・エルミート補間）
  - 大気圏への突入: 揚抗比・質量・空気抵抗から運動方程式で解き、記録の着水点へ届く傾け角（バンク角）を探す
月の位置・速度と太陽の位置は JPL Horizons（DE441）から取る。

出典（いずれも米国政府の著作物）:
  - Richard W. Orloff, "Apollo by the Numbers", NASA SP-2000-4029（上昇・月遷移軌道投入・着陸・突入と着水の表）
  - Apollo 11 Mission Report, MSC-00171（1969 年 11 月。表 7-II 軌道の値、表 7-VII 突入、表 5-I 降下のできごと）

出力形式（リトルエンディアン）:
  uint32 飛行の数
  飛行ごと: char[16] 名前, uint32 機体の数
    機体ごと: char[16] 名前, uint32 区間の数
      区間ごと: int32 基準天体（3: 地球, 10: 月）, int32 座標（0: 慣性・黄道座標 J2000, 1: 天体固定 x 経度 0・z 北極）,
                float64[3] 区間の始まりでの月の実際の位置（地球中心、黄道座標）, float64[3] 終わりでの位置,
                uint32 点の数, 点ごとに float64 時刻 [J2000 からの日数, UTC], float64[3] 位置 [km], float32[3] 速度 [km/s]
  uint32 噴射の数（ここから下が無ければ噴射なし）
  噴射ごと: char[16] 飛行の名前, char[16] 機体の名前, int32 エンジン（0: その時の主エンジン, 1: 姿勢制御の小さな噴射口,
            2: つながった月着陸船の下降エンジン, 3: つながった月着陸船の小さな噴射口）, float64 点火, float64 停止 [J2000 からの日数, UTC],
            uint32 向きの数, 向きごとに float64 時刻, float32[3] 推力の向き（黄道座標の単位ベクトル。0 個なら天体固定の区間の
            動力飛行（打ち上げ・月面への降下と上昇）で、向きは機体の進む向きから）

使い方: python3 tools/make_apollo_assets.py <作業ディレクトリ> [出力先=assets] [号 ...]
（号を指定するとその号だけ計算し直す。none なら計算し直さずに書き出すだけ）
（Horizons の結果を作業ディレクトリにキャッシュする）
"""
import json
import math
import os
import struct
import sys
import urllib.parse
import urllib.request

import numpy as np

UA = "SolarSystemVoyager-asset-build/1.0 (https://github.com/hubstacksea/Solar-System-Simulator)"
FT, NMI = 0.0003048, 1.852      # km
MU_E, MU_M, MU_S = 398600.4418, 4902.800, 1.32712440018e11
RE, J2E = 6378.137, 1.08263e-3
RM, J2M = 1737.4, 2.034e-4      # 月の平均半径（シミュレータの月の表面）と扁平 J2
SOI_M = 66100.0                 # 月の影響圏の半径 km（この内側は月中心で持つ）
OMEGA_E = 7.292115e-5           # 地球の自転 rad/s
DEG = math.pi / 180
J2000 = 2451545.0
B_EARTH, B_MOON = 3, 10         # シミュレータの天体番号
OBL = 23.4392911 * DEG          # J2000 の黄道傾斜角


# ---------------------------------------------------------------- 時刻と座標
def jd_utc(y, mo, d, h=0, mi=0, s=0.0):
    if mo <= 2:
        y -= 1
        mo += 12
    a = y // 100
    b = 2 - a + a // 4
    return int(365.25 * (y + 4716)) + int(30.6001 * (mo + 1)) + d + b - 1524.5 + (h + mi / 60 + s / 3600) / 24


def tt_minus_utc(jd):
    """TT − UTC 秒（1972 年より前の UTC は TAI との差が少しずつ変わる式）"""
    mjd = jd - 2400000.5
    if mjd < 41317:
        tai = 4.2131700 + (mjd - 39126) * 0.002592
    else:
        tai = 10.0 + sum(1 for m in (41499, 41683) if mjd >= m)
    return tai + 32.184


def R1(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[1, 0, 0], [0, c, s], [0, -s, c]])


def R2(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, 0, -s], [0, 1, 0], [s, 0, c]])


def R3(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, s, 0], [-s, c, 0], [0, 0, 1]])


def earth_fixed_to_icrf(jd_u):
    """地球固定 → ICRF の回転行列（IAU 1976 歳差、章動の主な項、グリニッジ視恒星時。UT1 は UTC で代用）"""
    jt = jd_u + tt_minus_utc(jd_u) / 86400
    T = (jt - J2000) / 36525
    z = (2306.2181 * T + 1.09468 * T * T + 0.018203 * T ** 3) / 3600 * DEG
    zeta = (2306.2181 * T + 0.30188 * T * T + 0.017998 * T ** 3) / 3600 * DEG
    th = (2004.3109 * T - 0.42665 * T * T - 0.041833 * T ** 3) / 3600 * DEG
    P = R3(-z) @ R2(th) @ R3(-zeta)
    om = (125.04452 - 1934.136261 * T) * DEG
    L, Lp = (280.4665 + 36000.7698 * T) * DEG, (218.3165 + 481267.8813 * T) * DEG
    dpsi = (-17.20 * math.sin(om) - 1.32 * math.sin(2 * L) - 0.23 * math.sin(2 * Lp) + 0.21 * math.sin(2 * om)) / 3600 * DEG
    deps = (9.20 * math.cos(om) + 0.57 * math.cos(2 * L) + 0.10 * math.cos(2 * Lp) - 0.09 * math.cos(2 * om)) / 3600 * DEG
    eps0 = (23.439291 - 0.0130042 * T) * DEG
    N = R1(-(eps0 + deps)) @ R3(-dpsi) @ R1(eps0)
    Tu = (jd_u - J2000) / 36525
    g = (280.46061837 + 360.98564736629 * (jd_u - J2000) + 0.000387933 * Tu * Tu) * DEG + dpsi * math.cos(eps0 + deps)
    return (N @ P).T @ R3(-g)


def moon_fixed_to_icrf(jd_t):
    """月固定 → ICRF（IAU WGCCRE 2009 の月の向き。シミュレータと同じ式）"""
    d = jd_t - J2000
    T = d / 36525
    E = [(a + b * d) * DEG for a, b in [(125.045, -0.0529921), (250.089, -0.1059842), (260.008, 13.0120009), (176.625, 13.3407154),
                                         (357.529, 0.9856003), (311.589, 26.4057084), (134.963, 13.0649930), (276.617, 0.3287146),
                                         (34.226, 1.7484877), (15.134, -0.1589763), (119.743, 0.0036096), (239.961, 0.1643573),
                                         (25.053, 12.9590088)]]
    s, c = math.sin, math.cos
    ra = (269.9949 + 0.0031 * T - 3.8787 * s(E[0]) - 0.1204 * s(E[1]) + 0.0700 * s(E[2]) - 0.0172 * s(E[3]) + 0.0072 * s(E[5])
          - 0.0052 * s(E[9]) + 0.0043 * s(E[12]))
    dec = (66.5392 + 0.0130 * T + 1.5419 * c(E[0]) + 0.0239 * c(E[1]) - 0.0278 * c(E[2]) + 0.0068 * c(E[3]) - 0.0029 * c(E[5])
           + 0.0009 * c(E[6]) + 0.0008 * c(E[9]) - 0.0009 * c(E[12]))
    W = (38.3213 + 13.17635815 * d - 1.4e-12 * d * d + 3.5610 * s(E[0]) + 0.1208 * s(E[1]) - 0.0642 * s(E[2]) + 0.0158 * s(E[3])
         + 0.0252 * s(E[4]) - 0.0066 * s(E[5]) - 0.0047 * s(E[6]) - 0.0046 * s(E[7]) + 0.0028 * s(E[8]) + 0.0052 * s(E[9])
         + 0.0040 * s(E[10]) + 0.0019 * s(E[11]) - 0.0044 * s(E[12]))
    return (R3(W * DEG) @ R1((90 - dec) * DEG) @ R3((ra + 90) * DEG)).T


def icrf_to_ecl(v):
    return R1(OBL) @ v


def unit_ll(lat, lon):
    la, lo = lat * DEG, lon * DEG
    return np.array([math.cos(la) * math.cos(lo), math.cos(la) * math.sin(lo), math.sin(la)])


def earth_radius(latc):
    """地心緯度での地球の楕円体（フィッシャー 1960、記録の高度の基準）の半径 km"""
    a, f = 6378.166, 1 / 298.3
    b = a * (1 - f)
    c, s = math.cos(latc * DEG), math.sin(latc * DEG)
    return a * b / math.sqrt((b * c) ** 2 + (a * s) ** 2)


def geodetic_to_geocentric(lat):
    f = 1 / 298.3
    return math.atan((1 - f) ** 2 * math.tan(lat * DEG)) / DEG


# ---------------------------------------------------------------- 天体暦（Horizons）
class Ephem:
    """地球中心の天体の位置・速度（ICRF、km, km/s）。時刻は TDB の JD。3 次エルミート補間"""

    def __init__(self, work, cmd, jd0, jd1, step):
        os.makedirs(work, exist_ok=True)
        path = os.path.join(work, "hz_%s_%.3f_%.3f_%s.txt" % (cmd, jd0, jd1, step))
        if not os.path.exists(path):
            q = {"format": "json", "COMMAND": "'%s'" % cmd, "CENTER": "'500@399'", "EPHEM_TYPE": "VECTORS", "MAKE_EPHEM": "YES",
                 "OBJ_DATA": "NO", "START_TIME": "'JD%.6f'" % jd0, "STOP_TIME": "'JD%.6f'" % jd1, "STEP_SIZE": "'%s'" % step,
                 "REF_PLANE": "FRAME", "REF_SYSTEM": "ICRF", "VEC_TABLE": "2", "OUT_UNITS": "KM-S", "CSV_FORMAT": "YES",
                 "VEC_LABELS": "NO", "TIME_TYPE": "TDB"}
            url = "https://ssd.jpl.nasa.gov/api/horizons.api?" + urllib.parse.urlencode(q)
            res = json.load(urllib.request.urlopen(urllib.request.Request(url, headers={"User-Agent": UA}), timeout=120))["result"]
            a, b = res.find("$$SOE"), res.find("$$EOE")
            if a < 0:
                raise SystemExit("Horizons: " + res[:500])
            with open(path, "w") as f:
                for line in res[a + 5:b].strip().splitlines():
                    c = [x.strip() for x in line.split(",")]
                    f.write(" ".join([c[0]] + c[2:8]) + "\n")
        a = np.loadtxt(path)
        self.t, self.p, self.v = a[:, 0], a[:, 1:4], a[:, 4:7]

    def state(self, jd):
        i = int(np.clip(np.searchsorted(self.t, jd), 1, len(self.t) - 1))
        t0, h = self.t[i - 1], self.t[i] - self.t[i - 1]
        u, hs = (jd - t0) / h, h * 86400
        p = (self.p[i - 1] * (2 * u ** 3 - 3 * u ** 2 + 1) + self.v[i - 1] * hs * (u ** 3 - 2 * u ** 2 + u)
             + self.p[i] * (-2 * u ** 3 + 3 * u ** 2) + self.v[i] * hs * (u ** 3 - u ** 2))
        v = (self.p[i - 1] * (6 * u ** 2 - 6 * u) / hs + self.v[i - 1] * (3 * u ** 2 - 4 * u + 1)
             + self.p[i] * (-6 * u ** 2 + 6 * u) / hs + self.v[i] * (3 * u ** 2 - 2 * u))
        return p, v

    def pos(self, jd):
        return self.state(jd)[0]


# ---------------------------------------------------------------- 飛行の計算
class Flight:
    def __init__(self, work, launch, days):
        self.get0 = jd_utc(*launch)
        jd0 = self.get0 - 0.5
        jd1 = self.get0 + days + 0.5
        self.moon = Ephem(work, "301", jd0, jd1, "10m")
        self.sun = Ephem(work, "10", jd0, jd1, "1h")

    # GET 秒 → UTC と TDB の JD（TDB − TT は 2 ms 未満なので TT で代用）
    def jd(self, get):
        u = self.get0 + get / 86400
        return u, u + tt_minus_utc(u) / 86400

    def accel(self, r, jt, center):
        """center（3: 地球, 10: 月）中心の加速度 km/s²"""
        m = self.moon.pos(jt)
        s = self.sun.pos(jt)

        def j2(rr, mu, R, J2, pole):
            n = np.linalg.norm(rr)
            z = np.dot(rr, pole)
            k = 1.5 * J2 * mu * R * R / n ** 5
            return k * ((5 * z * z / (n * n) - 1) * rr - 2 * z * pole)

        if center == B_EARTH:
            a = -MU_E * r / np.linalg.norm(r) ** 3 + j2(r, MU_E, RE, J2E, np.array([0, 0, 1.0]))
            d = m - r
            a += MU_M * (d / np.linalg.norm(d) ** 3 - m / np.linalg.norm(m) ** 3)
            d = s - r
            a += MU_S * (d / np.linalg.norm(d) ** 3 - s / np.linalg.norm(s) ** 3)
            return a
        pole = moon_fixed_to_icrf(jt) @ np.array([0, 0, 1.0])
        a = -MU_M * r / np.linalg.norm(r) ** 3 + j2(r, MU_M, RM, J2M, pole)
        e = -m   # 月から見た地球
        d = e - r
        a += MU_E * (d / np.linalg.norm(d) ** 3 - e / np.linalg.norm(e) ** 3)
        sm = s - m
        d = sm - r
        a += MU_S * (d / np.linalg.norm(d) ** 3 - sm / np.linalg.norm(sm) ** 3)
        return a

    def propagate(self, r, v, center, get0, get1, record=False):
        """RK4。刻みは中心からの距離に応じて（近いほど細かく）。record なら途中の点を返す"""
        t, T = get0, get1
        sgn = 1 if T >= t else -1
        mu = MU_E if center == B_EARTH else MU_M
        pts = [(t, r.copy(), v.copy())] if record else None
        while sgn * (T - t) > 1e-9:
            n = np.linalg.norm(r)
            h = min(120.0, max(2.0, 0.01 * math.sqrt(n ** 3 / mu)))
            if center == B_EARTH:   # 月に近づいたら月の重力の変化に合わせて細かく
                dm = np.linalg.norm(r - self.moon.pos(self.jd(t)[1]))
                h = min(h, max(2.0, 0.01 * math.sqrt(dm ** 3 / MU_M)))
            h = sgn * min(h, sgn * (T - t))
            f = lambda tt, rr: self.accel(rr, self.jd(tt)[1], center)
            k1v, k1r = f(t, r), v
            k2v, k2r = f(t + h / 2, r + k1r * h / 2), v + k1v * h / 2
            k3v, k3r = f(t + h / 2, r + k2r * h / 2), v + k2v * h / 2
            k4v, k4r = f(t + h, r + k3r * h), v + k3v * h
            r = r + (k1r + 2 * k2r + 2 * k3r + k4r) * h / 6
            v = v + (k1v + 2 * k2v + 2 * k3v + k4v) * h / 6
            t += h
            if record:
                pts.append((t, r.copy(), v.copy()))
        return (r, v, pts) if record else (r, v)

    def to_center(self, r, v, get, frm, to):
        if frm == to:
            return r, v
        mp, mv = self.moon.state(self.jd(get)[1])
        return (r - mp, v - mv) if to == B_MOON else (r + mp, v + mv)

    def shoot(self, r0, v0, center, get0, get1, r1):
        """get0 の位置 r0 から、get1 に位置 r1 を通る始まりの速度（v0 から直す）。ニュートン法、差分のヤコビ行列"""
        v = v0.copy()
        for it in range(40):
            rf, _ = self.propagate(r0, v, center, get0, get1)
            err = rf - r1
            if np.linalg.norm(err) < 1e-4:
                break
            J = np.zeros((3, 3))
            eps = 1e-6
            for k in range(3):
                dv = np.zeros(3)
                dv[k] = eps
                J[:, k] = (self.propagate(r0, v + dv, center, get0, get1)[0] - rf) / eps
            # 速度ではほとんど動かせない向き（半周先での軌道面の外など）は直さず、残りは coast で区間に分けて埋める
            U, sv, Vt = np.linalg.svd(J)
            inv = np.array([1 / x if x > 0.05 * sv[0] else 0.0 for x in sv])
            step = Vt.T @ (inv * (U.T @ err))
            if np.linalg.norm(step) < 1e-9:
                break
            n = np.linalg.norm(step)
            if n > 0.005:   # 1 回に秒速 5 m まで（遠くの別の解へ跳ばないように）
                step *= 0.005 / n
            v = v - step
        return v

    # 記録の状態（基準天体の固定座標の緯度・経度、高度、慣性速度・経路角・方位角）→ 中心天体の慣性座標
    # 方位の北: 地球は地球の自転軸。月のまわりの記録の方位も、地球の自転軸を北として測っている
    # （1969 年の記録で確かめた: 月の自転軸を北とすると、同じ周回軌道の傾きが記録ごとに 20° 以上ばらつく）
    def state(self, rec):
        ref, get, lat, lon, alt, vel, fpa, hdg = rec[:8]
        earth_north = len(rec) > 8 and rec[8] == "earth-north"   # この記録だけ地球の自転軸を北として測っている
        u, jt = self.jd(get)
        ME = earth_fixed_to_icrf(u)
        north = ME @ np.array([0, 0, 1.0])
        if ref == B_EARTH:
            M, rad = ME, earth_radius(lat) + alt * NMI
        else:
            M, rad = moon_fixed_to_icrf(jt), self.site_radius + alt * NMI
            if getattr(self, "moon_north", False) and not earth_north:   # 14・16・17 号の記録は月の自転軸を北として測っている（前後の記録とよく合う）
                north = M @ np.array([0, 0, 1.0])
        up = M @ unit_ll(lat, lon)
        east = np.cross(north, up)
        east /= np.linalg.norm(east)
        nn = np.cross(up, east)
        f, h = fpa * DEG, hdg * DEG
        vv = vel * FT * (math.sin(f) * up + math.cos(f) * (math.cos(h) * nn + math.sin(h) * east))
        return rad * up, vv


def hms(s):
    """'102:45:39.9' → 秒"""
    h, m, x = s.split(":")
    return int(h) * 3600 + int(m) * 60 + float(x)


# ---------------------------------------------------------------- 区間の組み立て
class Track:
    """1 機の軌跡。pieces: (基準天体, 座標 0 慣性 / 1 天体固定, [(GET 秒, 位置, 速度)])。慣性は中心天体の ICRF"""

    def __init__(self, name):
        self.name = name
        self.pieces = []
        self.burns = []               # 噴射: (点火 GET, 停止 GET, エンジン, [(GET, 推力の向き ICRF)])
        self.lifted = False           # 月着陸船の上昇段が月面を離れた後（主エンジンと小さな噴射口を加速度で分ける境が変わる）
        self.lm_drives_after = None   # この GET より後の噴射は、つながった月着陸船のエンジン（13 号）

    def add(self, ref, frame, pts):
        if self.pieces and self.pieces[-1][0] == ref and self.pieces[-1][1] == frame:
            last = self.pieces[-1][2]
            self.pieces[-1] = (ref, frame, last + [p for p in pts if p[0] > last[-1][0] + 1e-6])
        else:
            self.pieces.append((ref, frame, list(pts)))


def hermite_pts(get0, r0, v0, get1, r1, v1, step=2.0):
    """2 つの状態をつなぐ 3 次エルミート曲線（噴射の間）の点"""
    T = get1 - get0
    n = max(2, int(math.ceil(T / step)))
    out = []
    for k in range(n + 1):
        u = k / n
        h00, h10, h01, h11 = 2 * u ** 3 - 3 * u ** 2 + 1, u ** 3 - 2 * u ** 2 + u, -2 * u ** 3 + 3 * u ** 2, u ** 3 - u ** 2
        d00, d10, d01, d11 = 6 * u ** 2 - 6 * u, 3 * u ** 2 - 4 * u + 1, -6 * u ** 2 + 6 * u, 3 * u ** 2 - 2 * u
        out.append((get0 + u * T, r0 * h00 + v0 * T * h10 + r1 * h01 + v1 * T * h11, (r0 * d00 + r1 * d01) / T + v0 * d10 + v1 * d11))
    return out


def fixed_rot(F, ref, get):
    """天体固定 → ICRF の行列と、その時間微分（差分）"""
    def M(g):
        u, jt = F.jd(g)
        return earth_fixed_to_icrf(u) if ref == B_EARTH else moon_fixed_to_icrf(jt)
    h = 1.0
    return M(get), (M(get + h) - M(get - h)) / (2 * h)


def to_fixed(F, ref, get, r, v):
    M, dM = fixed_rot(F, ref, get)
    return M.T @ r, M.T @ v + dM.T @ r


def from_fixed(F, ref, get, rf, vf):
    M, dM = fixed_rot(F, ref, get)
    return M @ rf, M @ vf + dM @ rf


def pchip(ts, ys, ds):
    """単調な 3 次補間（Fritsch–Carlson）。ds の None は自動（端は片側の傾き）"""
    n = len(ts)
    dk = [(ys[i + 1] - ys[i]) / (ts[i + 1] - ts[i]) for i in range(n - 1)]
    m = []
    for i in range(n):
        if ds[i] is not None:
            m.append(ds[i])
        elif i == 0:
            m.append(dk[0])
        elif i == n - 1:
            m.append(dk[-1])
        elif dk[i - 1] * dk[i] <= 0:
            m.append(0.0)
        else:
            w1, w2 = 2 * (ts[i + 1] - ts[i]) + (ts[i] - ts[i - 1]), (ts[i + 1] - ts[i]) + 2 * (ts[i] - ts[i - 1])
            m.append((w1 + w2) / (w1 / dk[i - 1] + w2 / dk[i]))

    def f(t):
        i = max(0, min(n - 2, int(np.searchsorted(ts, t, side="right")) - 1))
        h = ts[i + 1] - ts[i]
        u = (t - ts[i]) / h
        return (ys[i] * (2 * u ** 3 - 3 * u ** 2 + 1) + m[i] * h * (u ** 3 - 2 * u ** 2 + u) + ys[i + 1] * (-2 * u ** 3 + 3 * u ** 2)
                + m[i + 1] * h * (u ** 3 - u ** 2))
    return f


class GroundPath:
    """天体の表面に沿った道。site を中心にした正距方位図法の平面で、start から site へ 2 次ベジェ曲線で結ぶ
    （start で向き dir0 に接する）。長さ L、弧長 s から表面の単位ベクトル（天体固定）"""

    def __init__(self, start_u, site_u, dir0, R, reverse=False):
        self.site = site_u
        z = site_u
        x = np.cross([0, 0, 1.0], z)
        x = x / np.linalg.norm(x) if np.linalg.norm(x) > 1e-9 else np.array([1.0, 0, 0])
        y = np.cross(z, x)
        self.ax, self.R = (x, y, z), R

        def proj(u):
            th = math.acos(max(-1.0, min(1.0, float(np.dot(u, z)))))
            d = u - z * np.dot(u, z)
            d = d / np.linalg.norm(d) if np.linalg.norm(d) > 1e-12 else x
            return np.array([np.dot(d, x), np.dot(d, y)]) * th * R
        self.proj = proj
        p0 = proj(start_u)
        e = 1e-4   # 向きを平面に写す
        q = proj((start_u + dir0 * e) / np.linalg.norm(start_u + dir0 * e)) - p0
        q /= np.linalg.norm(q)
        D = np.linalg.norm(p0)
        self.P = [p0, p0 + q * D * 0.5, np.zeros(2)]
        us = np.linspace(0, 1, 2001)
        pts = np.array([self.bez(u) for u in us])
        seg = np.linalg.norm(np.diff(pts, axis=0), axis=1)
        self.s = np.concatenate([[0], np.cumsum(seg)])
        self.us = us
        self.L = self.s[-1]

    def bez(self, u):
        a, b, c = self.P
        return a * (1 - u) ** 2 + b * 2 * u * (1 - u) + c * u * u

    def unit(self, s):
        u = float(np.interp(s, self.s, self.us))
        p = self.bez(u)
        th = np.linalg.norm(p) / self.R
        x, y, z = self.ax
        if th < 1e-12:
            return z
        d = (x * p[0] + y * p[1]) / np.linalg.norm(p)
        return z * math.cos(th) + d * math.sin(th)


def surface_motion(F, ref, get0, get1, path_unit, s_of_t, r_of_t, step=1.0):
    """天体固定の道の点（位置と、差分で求めた速度）"""
    n = max(2, int(math.ceil((get1 - get0) / step)))
    ts = [get0 + (get1 - get0) * k / n for k in range(n + 1)]
    pos = [path_unit(s_of_t(t)) * r_of_t(t) for t in ts]
    out = []
    for k, t in enumerate(ts):
        a, b = max(0, k - 1), min(n, k + 1)
        out.append((t, pos[k], (pos[b] - pos[a]) / (ts[b] - ts[a])))
    return out


# ---------------------------------------------------------------- 飛行の段階ごとの組み立て
def report(msg):
    print("   " + msg, flush=True)


def add_inertial(F, trk, center, pts):
    """中心天体で積分した点を、月の影響圏の内外で地球中心・月中心に分けて加える"""
    run, run_ref = [], None
    for t, r, v in pts:
        rr, vv = F.to_center(r, v, t, center, B_EARTH)
        ref = B_MOON if np.linalg.norm(rr - F.moon.pos(F.jd(t)[1])) < SOI_M else B_EARTH
        if ref != run_ref and run:   # 境目の点は前後の区間の両方に入れる
            trk.add(run_ref, 0, run)
            t0, r0, v0 = run[-1]
            run = [(t0,) + F.to_center(*F.to_center(r0, v0, t0, run_ref, B_EARTH), t0, B_EARTH, ref)]
        run_ref = ref
        run.append((t,) + F.to_center(rr, vv, t, B_EARTH, ref))
    if run:
        trk.add(run_ref, 0, run)


def coast(F, trk, center, get0, r0, v0, get1, r1=None, checks=()):
    """慣性飛行。r1 があれば、その位置を通るよう始まりの速度を直す。checks: 途中の記録（位置の差を表示）"""
    v = F.shoot(r0, v0, center, get0, get1, r1) if r1 is not None else v0
    r, ve, pts = F.propagate(r0, v, center, get0, get1, record=True)
    gap = 0.0
    if r1 is not None:   # 速度で直せなかった残り: 両端で速度が変わらないよう、区間全体に滑らかに足す
        e = r1 - r
        gap = float(np.linalg.norm(e))
        T = get1 - get0
        pts = [(t, rr + e * (3 * ((t - get0) / T) ** 2 - 2 * ((t - get0) / T) ** 3),
                vv + e * (6 * (t - get0) / T - 6 * ((t - get0) / T) ** 2) / T) for t, rr, vv in pts]
        r = r1
    add_inertial(F, trk, center, pts)
    report("coast %9.1f..%9.1f s  start dv %7.2f m/s  gap %.2f km" % (get0, get1, 1000 * np.linalg.norm(v - v0), gap))
    for rec in checks:
        g = rec[1]
        rc, _ = F.propagate(r0, v, center, get0, g)
        rr = F.to_center(*F.state(rec), g, rec[0], center)[0]
        report("   check %9.1f s: %.1f km" % (g, np.linalg.norm(rc - rr)))
    return r, ve, v


def coast_blend(F, trk, center, get0, r0, v0, get1, r1):
    """噴射のない区間（速度は前から続く）で、終わりの記録の位置 r1 を通るようにする。そのまま進めた軌道との差を、
    区間全体にわたって滑らかに（両端で速度が変わらないように）足し込む。月の重力のむら（計算に入れていない）の分"""
    r, ve, pts = F.propagate(r0, v0, center, get0, get1, record=True)
    e = r1 - r
    T = get1 - get0
    out = []
    for t, rr, vv in pts:
        u = (t - get0) / T
        out.append((t, rr + e * (3 * u * u - 2 * u ** 3), vv + e * (6 * u - 6 * u * u) / T))
    add_inertial(F, trk, center, out)
    report("coast(blend) %9.1f..%9.1f s  gap %.1f km" % (get0, get1, np.linalg.norm(e)))
    return r1, ve


ENG_MAIN, ENG_RCS, ENG_LM_DPS, ENG_LM_RCS = 0, 1, 2, 3


def add_burn(trk, get0, get1, thrust=None):
    """噴射を記録する。thrust: [(GET, 推力の加速度 km/s² ICRF)]（None は天体固定の区間の動力飛行）。
    平均の加速度が小さい噴射（司令・機械船や下降段の付いた月着陸船で 0.25 m/s²、上昇段で 1 m/s² 未満）は姿勢制御の
    小さな噴射口（推力 445 N を 4 つで 0.04〜0.4 m/s²）、それ以上は主エンジン（0.7〜5 m/s²）とみる"""
    if not thrust:
        trk.burns.append((get0, get1, ENG_MAIN, [], None))
        return
    T = max(get1 - get0, 1e-6)
    ts = [t for t, _ in thrust]
    mean = sum((thrust[k][1] + thrust[k + 1][1]) * (ts[k + 1] - ts[k]) / 2 for k in range(len(ts) - 1)) / max(ts[-1] - ts[0], 1e-9) \
        if len(ts) > 1 else thrust[0][1]
    rcs = 1000 * np.linalg.norm(mean) < (1.0 if trk.lifted else 0.25)
    lm = trk.lm_drives_after is not None and get0 > trk.lm_drives_after
    eng = (ENG_LM_RCS if lm else ENG_RCS) if rcs else (ENG_LM_DPS if lm else ENG_MAIN)
    # 向き: 30 秒より短い噴射は平均の向きだけ（エルミート曲線の加速度は区間の中で直線的に変わり、短い噴射では向きが揺れる）。
    # 長い噴射は 30 秒ほどの区切りごとの平均
    if T < 30:
        dirs = [(get0, mean / np.linalg.norm(mean)), (get1, mean / np.linalg.norm(mean))]
    else:
        n = min(16, int(math.ceil(T / 30)))
        dirs = []
        for k in range(n):
            a, b = get0 + T * k / n, get0 + T * (k + 1) / n
            m = sum((v for t, v in thrust if a <= t <= b), np.zeros(3))
            if np.linalg.norm(m) == 0:
                m = mean
            dirs.append(((a + b) / 2, m / np.linalg.norm(m)))
        dirs = [(get0, dirs[0][1])] + dirs + [(get1, dirs[-1][1])]
    turn = max(math.degrees(math.acos(max(-1.0, min(1.0, float(np.dot(a[1], b[1])))))) for a, b in zip(dirs, dirs[1:]))
    report("burn %9.1f..%9.1f s  %s  %.2f m/s2, direction changes up to %.1f deg" % (
        get0, get1, ["main", "RCS", "LM DPS", "LM RCS"][eng], 1000 * np.linalg.norm(mean), turn))
    trk.burns.append((get0, get1, eng, dirs, mean))


def burn(F, trk, center, get0, r0, v0, get1, r1, v1):
    add_inertial(F, trk, center, hermite_pts(get0, r0, v0, get1, r1, v1))
    # 推力 = エルミート曲線の加速度 − 重力
    T = get1 - get0
    n = max(4, int(math.ceil(T / 2)))
    thrust = []
    for k in range(n + 1):
        u = k / n
        r = r0 * (2 * u ** 3 - 3 * u ** 2 + 1) + v0 * T * (u ** 3 - 2 * u ** 2 + u) + r1 * (-2 * u ** 3 + 3 * u ** 2) + v1 * T * (u ** 3 - u ** 2)
        a = (r0 * (12 * u - 6) + r1 * (6 - 12 * u)) / T ** 2 + (v0 * (6 * u - 4) + v1 * (6 * u - 2)) / T
        thrust.append((get0 + u * T, a - F.accel(r, F.jd(get0 + u * T)[1], center)))
    add_burn(trk, get0, get1, thrust)


def rec_in(F, rec, center):
    r, v = F.state(rec)
    return F.to_center(r, v, rec[1], rec[0], center)


def turn_like(v, a, b):
    """速度 v を、a から b への回転と同じだけ回し、b の速さにする"""
    ua, ub = a / np.linalg.norm(a), b / np.linalg.norm(b)
    axis = np.cross(ua, ub)
    sn, cs = np.linalg.norm(axis), float(np.dot(ua, ub))
    u = v / np.linalg.norm(v)
    if sn > 1e-12:
        k = axis / sn
        u = u * cs + np.cross(k, u) * sn + k * np.dot(k, u) * (1 - cs)
    return u * np.linalg.norm(b)


def cut_pos(F, center, get0, r0, v0, get1, v1):
    """短い噴射の停止の位置を、点火の状態から求める（噴射の間の速度の変化は一様として）。地球から遠いと記録の緯度・経度の
    丸め（0.01°）だけで数十 km になり、記録の位置を使うと数秒の噴射の間に機体が跳んでしまう"""
    rp, vp = F.propagate(r0, v0, center, get0, get1)
    return rp + (v1 - vp) * (get1 - get0) / 2


def chain(F, trk, center_of, start, legs):
    """噴射と慣性飛行を順につなぐ。start: (GET, 位置, 速度, 中心)。legs: [(点火の記録, 停止の記録)]（最後は (目標の記録, None)）
    center_of(i): i 番目の慣性飛行で積分する中心"""
    get, r, v, c = start
    for i, (ign, cut) in enumerate(legs):
        cc = center_of(i)
        r, v = F.to_center(r, v, get, c, cc)
        rt = rec_in(F, ign, cc)[0]
        rend, vend, vstart = coast(F, trk, cc, get, r, v, ign[1], rt)
        vrec = rec_in(F, ign, cc)[1]
        report("   at %s: speed rec %.4f calc %.4f km/s, dir %.2f deg" % (
            "%.1f" % ign[1], np.linalg.norm(vrec), np.linalg.norm(vend),
            math.degrees(math.acos(max(-1, min(1, np.dot(vrec, vend) / np.linalg.norm(vrec) / np.linalg.norm(vend)))))))
        if cut is None:
            return ign[1], rend, vend, cc
        # 噴射: 次の慣性飛行の始まりの速度が決まってから曲線を引くので、停止の状態を先に求める
        cc2 = center_of(i + 1) if i + 1 < len(legs) else cc
        rc, vc = rec_in(F, cut, cc2)
        rs, vs = F.to_center(rend, vend, ign[1], cc, cc2)
        # 停止の速度の推定: 噴射の前の速度を、点火と停止の記録の速度の向きの違いの分だけ回し、記録の速さにする
        # （方位の北の取り方が記録によって違っても、同じ噴射の点火と停止では同じなので、差には効かない）
        # 長い噴射（1 分以上）は場所が大きく動き、記録ごとの北の向きの違いが効くので、停止の記録の速度をそのまま使う
        if cut[1] - ign[1] < 60:
            vi = rec_in(F, ign, cc2)[1]
            vc = turn_like(vs, vi, vc)
            rc = cut_pos(F, cc2, ign[1], rs, vs, cut[1], vc)
        if i + 1 == len(legs):
            burn(F, trk, cc2, ign[1], rs, vs, cut[1], rc, vc)
            return cut[1], rc, vc, cc2
        nxt = legs[i + 1][0]
        rn = rec_in(F, nxt, cc2)[0]
        vc2 = F.shoot(rc, vc, cc2, cut[1], nxt[1], rn)
        report("   cut-off %.1f: velocity adjusted %.2f m/s" % (cut[1], 1000 * np.linalg.norm(vc2 - vc)))
        burn(F, trk, cc2, ign[1], rs, vs, cut[1], rc, vc2)
        get, r, v, c = cut[1], rc, vc2, cc2
    return get, r, v, c


def still(t0, t1, pos):
    """止まっている間の点（1 分ごと。間引きで両端だけが残ると、前後の区間とつながった曲線で補間されてしまうので）"""
    n = max(1, int(math.ceil((t1 - t0) / 60.0)))
    return [(t0 + (t1 - t0) * k / n, pos, np.zeros(3)) for k in range(n + 1)]


def earth_ascent(F, trk, rows, hold_from):
    """打ち上げ: 上昇の記録の点（GET, 地心緯度, 経度, 高度 nmi, 慣性速度 ft/s, 経路角, 方位角）を地球固定座標で
    エルミート曲線でつなぐ。発射台の高さ（最初の点の高度）を引いて、点が機体の底になるようにする"""
    base = rows[0][3]
    st = []
    for get, lat, lon, alt, vel, fpa, hdg in rows:
        r, v = F.state((B_EARTH, get, lat, lon, alt - base, vel, fpa, hdg))
        rf, vf = to_fixed(F, B_EARTH, get, r, v)
        st.append((get, rf, vf))
    st[0] = (st[0][0], st[0][1], np.zeros(3))
    trk.add(B_EARTH, 1, still(hold_from, st[0][0], st[0][1]))
    trk.add(B_EARTH, 1, liftoff_pts(*st[0][:2], *st[1]))
    for (g0, r0, v0), (g1, r1, v1) in zip(st[1:], st[2:]):
        trk.add(B_EARTH, 1, hermite_pts(g0, r0, v0, g1, r1, v1, step=1.0))
    add_burn(trk, -8.9, st[-1][0])   # S-IC のエンジンの始動の指令は GET -8.9 秒（SP-4029）
    return st[-1]


def liftoff_pts(g0, r0, g1, r1, v1, step=1.0):
    """発射台から最初の記録の点まで（地球固定座標）。高さは止まった状態からのエルミート曲線、水平の動きは u^n の形
    （u は時刻の割合。n は 3 以上）で、真上に上がり始めてから向きを倒していく。エルミート曲線のままでは、最初の記録の
    速度に合わせるために、始めの数十秒に西へ傾いて上がってしまう"""
    up = r0 / np.linalg.norm(r0)
    T = g1 - g0
    D = r1 - r0
    h1, vh1 = float(np.dot(D, up)), float(np.dot(v1, up))
    Dh, Vh = D - up * h1, v1 - up * vh1
    X = float(np.linalg.norm(Dh))
    n = max(3.0, float(np.dot(Vh, Dh)) / X * T / X) if X > 1e-6 else 3.0
    C = Vh * T - Dh * n   # 終わりの水平の速度を合わせる項（u^n (u − 1) の係数）
    N = max(2, int(math.ceil(T / step)))
    out = []
    for k in range(N + 1):
        u = k / N
        h = h1 * (3 * u * u - 2 * u ** 3) + vh1 * T * (u ** 3 - u * u)
        dh = h1 * (6 * u - 6 * u * u) / T + vh1 * (3 * u * u - 2 * u)
        g1n, g1d = u ** n, n * u ** (n - 1)
        g2n, g2d = u ** n * (u - 1), (n + 1) * u ** n - n * u ** (n - 1)
        out.append((g0 + u * T, r0 + up * h + Dh * g1n + C * g2n, up * dh + (Dh * g1d + C * g2d) / T))
    return out


def lunar_descent(F, trk, get0, r0, v0, site, get1, alt_knots, speed_knots):
    """月面への降下（月固定座標）。get0 の状態（月中心の慣性）から、着陸地点 site（緯度, 経度）に get1 に降りる。
    alt_knots: [(GET, 着陸地点からの高度 km, 高度の変化率 km/s か None)]（降下の始まりと着地は自動で加える）
    speed_knots: [(GET, 水平の速さ km/s か None)]（None の 1 つは、道のりが合うように決める。間は速さが直線的に変わる）"""
    rf, vf = to_fixed(F, B_MOON, get0, r0, v0)
    up = rf / np.linalg.norm(rf)
    vh = vf - up * np.dot(vf, up)
    site_u = unit_ll(*site)
    path = GroundPath(up, site_u, vh / np.linalg.norm(vh), RM)
    h0 = np.linalg.norm(rf) - F.site_radius
    ts = [get0] + [k[0] for k in alt_knots] + [get1]
    hs = [h0] + [k[1] for k in alt_knots] + [0.0]
    ds = [float(np.dot(vf, up))] + [k[2] for k in alt_knots] + [-0.0005]
    hf = pchip(ts, hs, ds)
    # 水平の速さ: 区間ごとに直線。未知の 1 つは道のりの合計から（速さについて 1 次）
    sk = [(get0, float(np.linalg.norm(vh)))] + list(speed_knots) + [(get1, 0.0)]
    def dist(x):
        v = [x if s is None else s for _, s in sk]
        return sum((sk[i + 1][0] - sk[i][0]) * (v[i] + v[i + 1]) / 2 for i in range(len(sk) - 1))
    a, b = dist(0.0), dist(1.0)
    x = (path.L - a) / (b - a)
    vs = [x if s is None else s for _, s in sk]
    report("descent: path %.1f km, solved speed %.3f km/s" % (path.L, x))
    tk = [k[0] for k in sk]

    def s_of(t):
        s = 0.0
        for i in range(len(tk) - 1):
            if t <= tk[i]:
                break
            tb = min(t, tk[i + 1])
            va, vb = vs[i], vs[i] + (vs[i + 1] - vs[i]) * (tb - tk[i]) / (tk[i + 1] - tk[i])
            s += (tb - tk[i]) * (va + vb) / 2
        return s
    # 半径: 記録の高度は着陸地点の実際の半径から。シミュレータの月は平均半径の球なので、着地までに滑らかに移す
    w = lambda t: (lambda u: u * u * (3 - 2 * u))(min(1.0, max(0.0, (t - get0) / (get1 - get0))))
    r_of = lambda t: F.site_radius + hf(t) + (RM - F.site_radius) * w(t)
    pts = surface_motion(F, B_MOON, get0, get1, path.unit, s_of, r_of)
    # 始まりの点の位置と速度は記録の状態そのもの
    pts[0] = (get0, rf, vf)
    trk.add(B_MOON, 1, pts)
    add_burn(trk, get0, get1)
    return site_u * RM


def lunar_ascent(F, trk, site_pos, get0, get1, r1, v1, rise=10.0):
    """月面からの上昇（月固定座標）。rise 秒まっすぐ上がってから、get1 の状態（月中心の慣性）へ。
    水平の加速度は時間に対して直線的に変わり（機体が軽くなる）、終わりの速さと道のりが合うように決める"""
    rf, vf = to_fixed(F, B_MOON, get1, r1, v1)
    up1 = rf / np.linalg.norm(rf)
    vh = vf - up1 * np.dot(vf, up1)
    site_u = site_pos / np.linalg.norm(site_pos)
    path = GroundPath(up1, site_u, -vh / np.linalg.norm(vh), RM)   # 終わりから逆にたどる道
    L, V, T = path.L, float(np.linalg.norm(vh)), get1 - get0 - rise
    # v(τ) = a0 τ + (a1 - a0) τ² / (2T), s(T) = a0 T²/2 + (a1 - a0) T²/6
    A = np.array([[T / 2, T / 2], [T * T / 3, T * T / 6]])
    a0, a1 = np.linalg.solve(A, [V, L])
    report("ascent: path %.1f km, horizontal accel %.2f -> %.2f m/s2" % (L, 1000 * a0, 1000 * a1))

    def s_of(t):
        tau = max(0.0, t - get0 - rise)
        return L - (a0 * tau * tau / 2 + (a1 - a0) * tau ** 3 / (6 * T))   # 逆にたどる道の上の位置
    h1 = np.linalg.norm(rf) - F.site_radius
    g = 1.62e-3
    hr = 0.5 * 1.6e-3 * rise * rise   # まっすぐ上がる間（推力 − 月の重力 ≒ 1.6 m/s²）
    hf = pchip([get0, get0 + rise, get1], [0.0, hr, h1], [0.0, 1.6e-3 * rise, float(np.dot(vf, up1))])
    w = lambda t: (lambda u: u * u * (3 - 2 * u))(min(1.0, max(0.0, (t - get0) / (get1 - get0))))
    r_of = lambda t: RM + hf(t) + (F.site_radius - RM) * w(t)
    pts = surface_motion(F, B_MOON, get0, get1, path.unit, s_of, r_of)
    pts[-1] = (get1, rf, vf)
    trk.add(B_MOON, 1, pts)
    trk.lifted = True
    add_burn(trk, get0, get1)


# 大気（米国標準大気 1976 の密度 kg/m³、高度 km ごと）
ATM = [(0, 1.225), (10, 0.4135), (20, 0.08891), (30, 0.01841), (40, 0.003996), (50, 0.001027), (60, 3.097e-4), (70, 8.283e-5),
       (80, 1.846e-5), (90, 3.416e-6), (100, 5.604e-7), (110, 9.708e-8), (120, 2.222e-8), (150, 2.076e-9)]


def rho(h):
    if h >= ATM[-1][0]:
        return 0.0
    for (h0, r0), (h1, r1) in zip(ATM, ATM[1:]):
        if h < h1:
            return r0 * (r1 / r0) ** ((max(h, 0) - h0) / (h1 - h0))
    return 0.0


def entry(F, trk, rec_ei, target, get_drogue, get_main, get_splash, mass, ld, gmax_rec, h_drogue=7.315, h_main=3.048,
          t_switch=60.0):
    """大気圏への突入。突入の記録の状態から、揚抗比 ld・質量 mass の司令船を運動方程式で進め、ドローグ・パラシュートを
    開く高度で記録の場所（target: 緯度, 経度）に届くよう傾け角（バンク角）を決める。傾け角は、最初の t_switch 秒
    （最大の減速を含む）は記録の最大加速度 gmax_rec になる角度、その後は距離が合う角度で、途中で左右を 1 回入れ替えて
    横のずれを合わせる。その後はパラシュートで着水点へ（地球固定座標）"""
    cd, area = 1.29, math.pi * 1.956 ** 2
    beta = mass / (cd * area)   # kg/m²
    r0, v0 = F.state(rec_ei)
    # 突入の十数分の間、地球の自転軸の向きは変わらないとみなす
    wi = earth_fixed_to_icrf(F.jd(rec_ei[1])[0]) @ np.array([0, 0, OMEGA_E])
    flat = 1 / 298.3

    def fly(banks, record=False):
        t, r, v = rec_ei[1], r0.copy(), v0.copy()
        out, gmax = [], 0.0
        while True:
            def acc(rr, vv):
                n = np.linalg.norm(rr)
                g = -MU_E * rr / n ** 3
                h = n - 6378.166 * (1 - flat * (rr[2] / n) ** 2)
                va = vv - np.cross(wi, rr)   # 大気に対する速度
                sp = np.linalg.norm(va) * 1000
                q = 0.5 * rho(h) * sp * sp
                dmag = q / beta / 1000   # km/s²
                dv = -va / np.linalg.norm(va)
                upv = rr / n
                lv = upv - dv * np.dot(upv, dv)
                lv /= np.linalg.norm(lv)
                side = np.cross(dv, lv)
                tt = t - rec_ei[1]
                bank = banks[0] if tt < banks[3] else banks[1] if tt < banks[2] else -banks[1]
                lift = (lv * math.cos(bank) + side * math.sin(bank)) * dmag * ld
                return g + dv * dmag + lift, dmag * math.sqrt(1 + ld * ld), h
            h_step = 1.0
            a1, gl, h = acc(r, v)
            gmax = max(gmax, gl / 9.80665e-3)
            if record:
                out.append((t, r.copy(), v.copy()))
            if h < h_drogue:
                return t, r, v, out, gmax
            k1v, k1r = a1, v
            k2v, k2r = acc(r + k1r * h_step / 2, v + k1v * h_step / 2)[0], v + k1v * h_step / 2
            k3v, k3r = acc(r + k2r * h_step / 2, v + k2v * h_step / 2)[0], v + k2v * h_step / 2
            k4v, k4r = acc(r + k3r * h_step, v + k3v * h_step)[0], v + k3v * h_step
            r = r + (k1r + 2 * k2r + 2 * k3r + k4r) * h_step / 6
            v = v + (k1v + 2 * k2v + 2 * k3v + k4v) * h_step / 6
            t += h_step

    tgt = unit_ll(geodetic_to_geocentric(target[0]), target[1])
    ei = earth_fixed_to_icrf(F.jd(rec_ei[1])[0]).T @ r0
    ei /= np.linalg.norm(ei)
    # 突入点から目標への大円の面で、進んだ角度（距離）と横のずれ（km）を測る
    nrm = np.cross(ei, tgt)
    nrm /= np.linalg.norm(nrm)
    full = math.acos(np.dot(ei, tgt))

    def miss(banks):
        t, r, v, _, _ = fly(banks)
        rf = earth_fixed_to_icrf(F.jd(t)[0]).T @ r
        u = rf / np.linalg.norm(rf)
        side = math.asin(np.dot(u, nrm))
        along = math.atan2(np.dot(u, np.cross(nrm, ei)), np.dot(u, ei))
        return np.array([(along - full) * RE, side * RE])
    # 傾け角: 最初の ts 秒は揚力を真上に（大気の上の方に留まる）、その後は b2 で、途中 trev 秒で左右を入れ替える。
    # b2 は距離が合うように（大きいほど手前に落ちる）、ts は最大加速度が記録に合うように（長いほど浅く減速する）、
    # trev は横のずれが合うように決める
    def solve_b2(ts, trev):
        lo, hi = 0.0, math.pi
        for _ in range(14):
            mid = (lo + hi) / 2
            if miss((0.0, mid, trev, ts))[0] > 0:
                lo = mid
            else:
                hi = mid
        return (lo + hi) / 2
    # ts は 10 秒ごとに試し、距離が合う中で最大加速度が記録に最も近いもの（届かないものは除く）
    trev = 300.0
    cands = []
    for ts in np.arange(20.0, 150.0, 10.0):
        b2 = solve_b2(ts, max(trev, ts))
        t_, r_, v_, _, gm = fly((0.0, b2, max(trev, ts), ts))
        along = float(miss((0.0, b2, max(trev, ts), ts))[0])   # 横のずれは後で左右の入れ替えで合わせるので、ここでは距離だけ
        bad = abs(along) > 20 or t_ - rec_ei[1] > 1400       # 届かない・大気から飛び出すものは除く
        cands.append((bad, abs(gm - gmax_rec), ts, b2))
    cands.sort()
    ts = cands[0][2]
    fine = []
    for t2 in np.arange(max(20.0, ts - 8), min(150.0, ts + 8.1), 2.0):   # 前後 8 秒を 2 秒ごとに（加速度は ts について単調ではない）
        b2 = solve_b2(t2, max(trev, t2))
        t_, r_, v_, _, gm = fly((0.0, b2, max(trev, t2), t2))
        bad = abs(float(miss((0.0, b2, max(trev, t2), t2))[0])) > 20 or t_ - rec_ei[1] > 1400
        fine.append((bad, abs(gm - gmax_rec), t2))
    fine.sort()
    ts = fine[0][2]
    trev = max(trev, ts)
    for it in range(5):   # 左右を入れ替える時刻で横のずれを合わせる
        b2 = solve_b2(ts, trev)
        e1 = miss((0.0, b2, trev, ts))[1]
        e2 = miss((0.0, b2, trev + 20.0, ts))[1]
        if abs(e1) < 0.5 or abs(e2 - e1) < 1e-6:
            break
        trev = max(ts, min(900.0, trev - e1 * 20.0 / (e2 - e1)))
    b2 = solve_b2(ts, trev)
    for sgn in (1, -1):   # 最初に倒す向き（左右）は横のずれが小さい方
        m = float(np.linalg.norm(miss((0.0, sgn * b2, trev, ts))))
        if sgn == 1 or m < best[0]:
            best = (m, (0.0, sgn * b2, trev, ts))
    report("   entry: lift-up %.0f s, then bank %.1f, reverse +%.0f s" % (ts, math.degrees(best[1][1]), trev))
    bank = best[1]
    t, r, v, pts, gmax = fly(bank, record=True)
    report("entry: drogue at GET %.1f (record %.1f), miss %.1f km, max %.2f g (record %.2f)" % (t, get_drogue, best[0], gmax, gmax_rec))
    # 地球固定に直して加える（ドローグを開く時刻のずれは、突入の点の時刻を詰めて合わせる）
    scale = (get_drogue - rec_ei[1]) / (t - rec_ei[1])
    fpts = []
    for (tt, rr, vv) in pts:
        rf, vf = to_fixed(F, B_EARTH, tt, rr, vv)
        fpts.append((rec_ei[1] + (tt - rec_ei[1]) * scale, rf, vf / scale))
    trk.add(B_EARTH, 1, fpts)
    # パラシュート: ドローグを開いた所から着水点へ。主パラシュートを開く時刻と高度、着水の時刻を通る
    g0, rf0, vf0 = fpts[-1]
    up0 = rf0 / np.linalg.norm(rf0)
    vh = vf0 - up0 * np.dot(vf0, up0)
    splash_u = unit_ll(geodetic_to_geocentric(target[0]), target[1])
    path = GroundPath(up0, splash_u, vh / np.linalg.norm(vh), RE)
    Rs = earth_radius(geodetic_to_geocentric(target[0]))
    h0 = np.linalg.norm(rf0) - Rs
    hf = pchip([g0, get_main, get_splash], [h0, h_main, 0.0], [float(np.dot(vf0, up0)), None, -0.0095])
    v0h = float(np.linalg.norm(vh))
    tau = 20.0   # 横の速さはドローグで数十秒のうちに落ちる
    total = v0h * tau * (1 - math.exp(-(get_splash - g0) / tau))
    s_of = lambda t: path.L * (v0h * tau * (1 - math.exp(-(t - g0) / tau))) / total if total > 0 else 0.0
    report("chute: drift %.2f km" % path.L)
    trk.add(B_EARTH, 1, surface_motion(F, B_EARTH, g0, get_splash, path.unit, s_of, lambda t: Rs + hf(t)))
    trk.add(B_EARTH, 1, still(get_splash, get_splash + 3600.0, splash_u * Rs))


# ---------------------------------------------------------------- 飛行ごとの記録
E, M = B_EARTH, B_MOON


def R(ref, t, lat, lon, alt, vel, fpa, hdg):
    """記録の 1 行: 基準天体, GET, 緯度（北が正）, 経度（東が正）, 高度 nmi, 慣性速度 ft/s, 経路角, 方位角（北から東回り）"""
    return (ref, hms(t), lat, lon, alt, vel, fpa, hdg)


def follow(dst, src, t0, t1):
    """t0〜t1 の間、src と同じ道をたどる（ドッキングしている間など。慣性の区間だけ）。両端はその時刻ちょうどの状態から
    （間の点だけを写すと、切り離しの時刻から次の点までの数秒、機体の位置が決まらない）"""
    for ref, frame, pts in src.pieces:
        if frame == 0:
            part = [p for p in pts if t0 + 1e-6 < p[0] < t1 - 1e-6]
            if pts[0][0] <= t0 <= pts[-1][0]:
                part = [state_in(pts, t0)] + part
            if pts[0][0] <= t1 <= pts[-1][0] and t1 > t0:
                part = part + [state_in(pts, t1)]
            if part:
                dst.add(ref, 0, part)


def state_in(pts, t):
    """点列 pts（時刻の順）の時刻 t の状態（点の間はエルミート補間）"""
    for (t0, r0, v0), (t1, r1, v1) in zip(pts, pts[1:]):
        if t0 <= t <= t1:
            h = t1 - t0
            if h <= 0:
                return (t, r0, v0)
            u = (t - t0) / h
            rr = (r0 * (2 * u ** 3 - 3 * u ** 2 + 1) + v0 * h * (u ** 3 - 2 * u ** 2 + u) + r1 * (-2 * u ** 3 + 3 * u ** 2)
                  + v1 * h * (u ** 3 - u ** 2))
            vv = ((r0 * (6 * u ** 2 - 6 * u) + r1 * (-6 * u ** 2 + 6 * u)) / h + v0 * (3 * u ** 2 - 4 * u + 1)
                  + v1 * (3 * u ** 2 - 2 * u))
            return (t, rr, vv)
    return pts[0] if t <= pts[0][0] else pts[-1]


def state_at(trk, t):
    """軌跡の点のうち時刻 t のもの（慣性の区間。無ければ None）"""
    for ref, frame, pts in trk.pieces:
        if frame == 0:
            for p in pts:
                if abs(p[0] - t) < 1e-6:
                    return ref, p
    return None


def relative_burn(F, trk, center, ign, cut, get, r, v):
    """点火の位置の記録が計算した軌道と合わない噴射: 記録の点火から停止への変化（位置と速度）を、計算した点火の状態に
    当てはめる（点火の記録の位置と軌道面を、計算した点火の位置と軌道面に重ねる回転で）"""
    ri, vi = rec_in(F, ign, center)
    rc, vc = rec_in(F, cut, center)

    def frame(rr, vv):
        x = rr / np.linalg.norm(rr)
        z = np.cross(rr, vv)
        z /= np.linalg.norm(z)
        return np.array([x, np.cross(z, x), z]).T
    Q = frame(r, v) @ frame(ri, vi).T
    rc2, vc2 = Q @ rc, Q @ vc
    burn(F, trk, center, get, r, v, cut[1], rc2, vc2)
    return rc2, vc2


def state_at_time(F, trk, t):
    """軌跡の慣性の区間の、時刻 t の位置と速度（点の間はエルミート補間）"""
    for ref, frame, pts in trk.pieces:
        if frame == 0 and pts[0][0] <= t <= pts[-1][0]:
            for (t0, r0, v0), (t1, r1, v1) in zip(pts, pts[1:]):
                if t0 <= t <= t1:
                    h = t1 - t0
                    u = (t - t0) / h if h > 0 else 0.0
                    rr = (r0 * (2 * u ** 3 - 3 * u ** 2 + 1) + v0 * h * (u ** 3 - 2 * u ** 2 + u) + r1 * (-2 * u ** 3 + 3 * u ** 2)
                          + v1 * h * (u ** 3 - u ** 2))
                    vv = ((r0 * (6 * u ** 2 - 6 * u) + r1 * (-6 * u ** 2 + 6 * u)) / h + v0 * (3 * u ** 2 - 4 * u + 1)
                          + v1 * (3 * u ** 2 - 2 * u)) if h > 0 else v0
                    return ref, (t, rr, vv)
    return None


def apollo10(work):
    """アポロ 10 号（1969 年 5 月 18 日 16:49:00 UTC 打ち上げ。月着陸船スヌーピーが高度 約 15 km まで降りた着陸の予行演習）。
    軌道の値は Apollo 10 Mission Report（MSC-00126）の表 6-II と 6-VII、上昇とできごとの時刻は SP-4029"""
    F = Flight(work, (1969, 5, 18, 16, 49, 0.0), 8.5)
    F.site_radius = 1738.09
    csm, lm = Track("Charlie Brown"), Track("Snoopy")
    rows = [(0.58, 28.4658, -80.6209, 0.035, 1340.4, 0.06, 90.00), (66.8, 28.4714, -80.6023, 4.244, 2028.6, 27.82, 85.03),
            (82.6, 28.4813, -80.5690, 7.137, 2645.8, 28.83, 82.23), (135.16, 28.5967, -80.1577, 23.430, 6473.20, 22.807, 76.461),
            (161.63, 28.7182, -79.7090, 35.247, 9028.58, 18.946, 75.538), (162.31, 28.7222, -79.6943, 35.580, 9052.79, 18.848, 75.538),
            (460.61, 30.9579, -69.4941, 96.710, 18630.15, 1.029, 79.585), (552.64, 31.7505, -64.0222, 101.204, 22632.02, 0.741, 82.458),
            (553.50, 31.7574, -63.9647, 101.247, 22639.93, 0.730, 82.490), (703.76, 32.5150, -53.2920, 103.385, 25562.40, -0.0064, 88.497)]
    earth_ascent(F, csm, rows, -3 * 3600.0)
    g, lat, lon, alt, vel, fpa, hdg = rows[-1]
    r, v = F.state((E, g, lat, lon, alt, vel, fpa, hdg))
    sep_ign = R(M, "98:47:17.4", 0.62, 38.37, 59.2, 5352.2, 0.15, -90.84)
    legs = [
        (R(E, "2:33:27.6", -25.76, 135.54, 106.9, 25561, 0.05, 69.17), R(E, "2:39:10.5", -14.07, 159.13, 172.7, 35586, 6.92, 61.26)),
        (R(E, "4:39:09.8", 31.70, -114.86, 17938.5, 14220.2, 65.15, 91.21), R(E, "4:39:12.7", 31.70, -114.87, 17944.7, 14203.7, 65.10, 91.22)),
        (R(E, "26:32:56.8", 26.34, -49.82, 110150.2, 5094.4, 77.30, 108.36), R(E, "26:33:03.9", 26.34, -49.85, 110155.9, 5111.0, 77.80, 108.92)),
        (R(M, "75:55:54.0", -1.76, -162.68, 95.1, 8232.3, -11.70, -65.71), R(M, "76:01:50.1", 0.19, 174.60, 61.2, 5471.9, -0.90, -77.75)),
        (R(M, "80:25:08.1", 0.55, 153.46, 60.4, 5484.7, -0.01, -84.79), R(M, "80:25:22.0", 0.57, 152.70, 59.3, 5348.9, 0.01, -85.09)),
        (sep_ign, None)]
    centers = [E, E, E, E, M, M, M]
    g, r, v, c = chain(F, csm, lambda i: centers[i], (g, r, v, E), legs)
    sep = (g, r.copy(), v.copy(), c)
    # 司令船: 分離の噴射（司令船が離れた）→ 地球へ向かう噴射。この間の記録（ドッキング・最終分離）は、月を回る低い軌道の
    # 重力のむらのためか、前後の記録と軌道が数百 km 合わないので、位置には使わない（月着陸船は計算した司令船の位置にドッキングする。
    # 最終分離は秒速 0.1 m の小さな噴射なので入れない）
    sep_cut = R(M, "98:47:25.7", 0.61, 38.00, 59.2, 5352.1, 0.15, -90.84)
    tei = (R(M, "137:36:28.9", 0.34, 155.72, 56.0, 5362.7, -0.44, -73.60), R(M, "137:39:13.7", 0.42, 144.62, 56.5, 8987.2, 2.53, -76.68))
    mcc = (R(E, "188:49:58", 0.59, 88.84, 25570.4, 12540.0, -69.65, 119.34), R(E, "188:50:04.7", 0.59, 88.82, 25557.4, 12543.5, -69.64, 119.34))
    ei = R(E, "191:48:54.5", geodetic_to_geocentric(-23.60), 174.39, 65.8, 36314, -6.54, 71.89)
    rc, vc = rec_in(F, sep_cut, M)
    vc = turn_like(v, rec_in(F, sep_ign, M)[1], vc)
    vc = F.shoot(rc, vc, M, sep_cut[1], tei[0][1], rec_in(F, tei[0], M)[0])
    burn(F, csm, M, g, r, v, sep_cut[1], rc, vc)
    centers = [M, E, E]
    g, r, v, c = chain(F, csm, lambda i: centers[i], (sep_cut[1], rc, vc, M), [tei, mcc, (ei, None)])
    dock_t = hms("106:22:02")
    ref, (gd, rd, vd) = state_at_time(F, csm, dock_t)
    # 突入（表 6-VII）: ドローグ 191:57:18.0（南緯 15.06°・西経 164.65°）、主パラシュート 191:58:05、着水 192:03:23（SP-4029）
    entry(F, csm, ei, (-15.06, -164.65), hms("191:57:18.0"), hms("191:58:05"), hms("192:03:23"),
          mass=11558 * 0.4536, ld=0.300, gmax_rec=6.78)

    # 月着陸船: 切り離し（98:11:57）から司令船の分離の噴射までは司令船と同じ道
    follow(lm, csm, hms("98:11:57"), sep[0])
    legs = [
        (R(M, "99:46:01.6", -0.66, -139.67, 61.6, 5339.6, -0.15, -89.19), R(M, "99:46:28.0", -0.69, -141.12, 61.2, 5271.2, -0.03, -89.13)),
        (R(M, "100:58:25.9", -0.22, -11.19, 17.7, 5512.4, 1.19, -91.09), R(M, "100:59:05.9", -0.34, -13.67, 19.0, 5672.9, 1.88, -91.05)),
        # 下降段の切り離し（102:45:16.9、噴射なし）は位置に使わず、上昇段の軌道投入へ
        (R(M, "102:55:02.1", 0.30, 19.58, 11.6, 5705.2, -0.78, -91.06), R(M, "102:55:17.6", 0.29, 18.72, 11.7, 5520.6, 0.49, -91.06)),

        (R(M, "103:45:55.3", -0.64, -141.57, 44.7, 5335.5, -0.16, -89.10), R(M, "103:46:22.6", -0.58, -143.13, 44.6, 5381.7, -0.19, -89.08)),
        (R(M, "104:43:53.3", 0.59, 36.98, 44.3, 5394.7, 0.20, -90.91), R(M, "104:43:55.0", 0.59, 36.89, 43.8, 5394.9, 0.17, -90.92)),
        (R(M, "105:22:55.6", -1.08, -84.16, 48.4, 5369.2, -0.02, -90.04), None)]
    g, r, v, c = chain(F, lm, lambda i: M, sep, legs)
    rd = F.to_center(rd, vd, gd, ref, M)[0]
    tpi_cut = R(M, "105:23:12.1", -1.09, -85.63, 47.0, 5396.7, -0.10, -90.34)
    rt, vt = rec_in(F, tpi_cut, M)
    vt = turn_like(v, rec_in(F, R(M, "105:22:55.6", -1.08, -84.16, 48.4, 5369.2, -0.02, -90.04), M)[1], vt)
    rt = cut_pos(F, M, g, r, v, tpi_cut[1], vt)
    vt2 = F.shoot(rt, vt, M, tpi_cut[1], gd, rd)
    report("   TPI cut-off: velocity adjusted %.2f m/s" % (1000 * np.linalg.norm(vt2 - vt)))
    burn(F, lm, M, g, r, v, tpi_cut[1], rt, vt2)
    coast(F, lm, M, tpi_cut[1], rt, vt2, gd, rd)
    # ドッキング → 切り離し → 最終分離（司令船が離れる）まで司令船と同じ道。その後、上昇段のエンジンを燃料が尽きるまで噴射して
    # 太陽を回る軌道へ（ホライズンズに上昇段の軌跡がある）。噴射の後 1 日まで描く
    follow(lm, csm, gd, hms("108:43:23.3"))
    ref, (tj, rj, vj) = state_at_time(F, csm, hms("108:43:23.3"))
    aps = (R(M, "108:52:05.5", 0.18, -3.23, 59.1, 5343.0, 0.21, -91.15), R(M, "108:56:14.5", 0.44, -20.22, 89.7, 9056.4, 11.63, -90.81))
    rj, vj = F.to_center(rj, vj, tj, ref, M)
    # 切り離しから点火までは位置の記録に合わせず、そのまま進める（記録の点火の位置は計算した司令船の軌道と合わない）
    rend, vend, _ = coast(F, lm, M, tj, rj, vj, aps[0][1])
    rc, vc = relative_burn(F, lm, M, aps[0], aps[1], aps[0][1], rend, vend)
    r, v = F.to_center(rc, vc, aps[1][1], M, E)
    coast(F, lm, E, aps[1][1], r, v, aps[1][1] + 86400.0)
    return "Apollo 10", F, [csm, lm]


def apollo11(work):
    """アポロ 11 号（1969 年 7 月 16 日 13:32:00 UTC 打ち上げ）。軌道の値は Mission Report の表 7-II、上昇は SP-4029"""
    F = Flight(work, (1969, 7, 16, 13, 32, 0.0), 9.0)
    F.site_radius = 1735.47   # 着陸地点の月の半径（月の高度の基準）
    csm, lm = Track("Columbia"), Track("Eagle")
    # 上昇（SP-4029 Apollo 11 Ascent Phase。S-IC の中央エンジン停止の経度 -81.1517 は誤植で -80.1517）
    rows = [(0.63, 28.4470, -80.6041, 0.032, 1340.7, 0.06, 90.00), (66.30, 28.4523, -80.5853, 4.236, 2023.9, 27.88, 85.32),
            (83.00, 28.4624, -80.5499, 7.326, 2671.9, 29.23, 82.41), (135.20, 28.5739, -80.1517, 23.761, 6492.8, 22.957, 76.315),
            (161.63, 28.7007, -79.6908, 35.701, 9068.6, 19.114, 75.439), (460.62, 30.9513, -69.4309, 97.280, 18725.5, 0.897, 79.646),
            (548.22, 31.7089, -64.1983, 101.142, 22690.8, 0.619, 82.396), (699.33, 32.4865, -53.4588, 103.202, 25561.6, 0.011, 88.414)]
    earth_ascent(F, csm, rows, -3 * 3600.0)
    g, lat, lon, alt, vel, fpa, hdg = rows[-1]
    r, v = F.state((E, g, lat, lon, alt, vel, fpa, hdg))
    legs = [
        (R(E, "2:44:16.2", -5.03, 172.55, 105.8, 25562, 0.02, 57.78), R(E, "2:50:03.2", 9.52, -165.61, 173.3, 35567, 6.91, 59.93)),
        (R(E, "4:40:01.8", 21.16, -68.46, 16620.8, 14680.0, 64.30, 113.73), R(E, "4:40:04.7", 21.16, -68.46, 16627.3, 14663.0, 64.25, 113.74)),
        (R(E, "26:44:58.7", 5.99, -11.16, 109475.3, 5025.0, 77.05, 120.88), R(E, "26:45:01.8", 6.00, -11.17, 109477.2, 5010.0, 76.88, 120.87)),
        (R(M, "75:49:50.4", -1.57, -169.58, 86.7, 8250.0, -9.99, -62.80), R(M, "75:55:48.0", 0.16, 167.13, 60.1, 5479.0, -0.20, -66.89)),
        (R(M, "80:11:36.8", -0.02, 170.09, 61.8, 5477.3, -0.49, -66.55), R(M, "80:11:53.5", -0.02, 169.16, 61.6, 5338.3, 0.32, -66.77)),
        (R(M, "100:39:52.9", 0.99, 31.86, 62.7, 5332.7, -0.13, -106.89), None),
    ]
    centers = [E, E, E, E, M, M, M]
    g, r, v, c = chain(F, csm, lambda i: centers[i], (g, r, v, E), legs)
    sep = (g, r.copy(), v.copy(), c)   # 月着陸船はここから分かれる（切り離しの噴射は司令船が行った）
    # 司令船: 分離の噴射 → ドッキング → 最終分離 → 地球へ向かう噴射 → 中間修正 → 司令船と機械船の分離
    dock = R(M, "128:03:00.0", 1.18, 67.31, 60.6, 5341.5, 0.16, -87.63)
    legs = [
        (R(M, "100:39:52.9", 0.99, 31.86, 62.7, 5332.7, -0.13, -106.89), R(M, "100:40:01.9", 1.05, 31.41, 62.5, 5332.2, -0.16, -106.90)),
        (dock, None)]
    # 分離の噴射の点火は、上の慣性飛行の終わりそのもの（同じ記録）なので、噴射から始める
    rc, vc = rec_in(F, legs[0][1], M)
    vc = F.shoot(rc, vc, M, legs[0][1][1], dock[1], rec_in(F, dock, M)[0])
    burn(F, csm, M, g, r, v, legs[0][1][1], rc, vc)
    g, r, v, c = chain(F, csm, lambda i: M, (legs[0][1][1], rc, vc, M), [(dock, None)])
    dock_state = (g, r.copy(), v.copy())
    # ドッキング → 最終分離（130:30、秒速 0.7 m の小さな噴射。記録の方位は前後の軌道と 60° 食い違うので、噴射の前の向きを使う）
    # → 地球へ向かう噴射
    tei_ign = R(M, "135:23:42.3", -0.16, 164.02, 52.4, 5376.0, -0.03, -62.77)
    tei_cut = R(M, "135:26:13.7", 0.50, 154.02, 58.1, 8589.0, 5.13, -62.60)
    mcc2 = (R(E, "150:29:57.4", -13.16, -37.79, 169087.2, 4075.0, -80.34, 129.30), R(E, "150:30:07.4", -13.16, -37.83, 169080.6, 4074.0, -80.41, 129.30))
    ei = R(E, "195:03:05.7", geodetic_to_geocentric(-3.19), 171.96, 65.8, 36194.4, -6.48, 50.18)
    fsep = (R(M, "130:30:01.0", 0.08, -20.19, 62.7, 5330.1, -0.05, -52.86), R(M, "130:30:08.1", 0.19, -20.58, 62.7, 5326.9, -0.02, -52.73))
    g, r, v, c = chain(F, csm, lambda i: M, (g, r, v, M), [fsep, (tei_ign, None)])
    rc, vc = rec_in(F, tei_cut, E)
    vc = F.shoot(rc, vc, E, tei_cut[1], mcc2[0][1], rec_in(F, mcc2[0], E)[0])
    rs, vs = F.to_center(r, v, tei_ign[1], M, E)
    burn(F, csm, E, tei_ign[1], rs, vs, tei_cut[1], rc, vc)
    g, r, v, c = chain(F, csm, lambda i: E, (tei_cut[1], rc, vc, E), [mcc2, (ei, None)])
    # 突入（表 7-VII）: ドローグ・パラシュート 195:12:06.9、着水 195:18:35（北緯 13.30°・西経 169.15°）
    entry(F, csm, R(E, "195:03:05.7", geodetic_to_geocentric(-3.19), 171.96, 65.8, 36194.4, -6.48, 50.18), (13.30, -169.15),
          # 主パラシュートの時刻は記録が見当たらないので、ドローグの約 50 秒後（8・16・17 号の記録の間隔）。質量は主パラシュートを
          # 開いた時の司令船（SP-4029。突入の時もほぼ同じ）
          hms("195:12:06.9"), hms("195:12:54"), hms("195:18:35"), mass=11318.9 * 0.4536, ld=0.309, gmax_rec=6.51)

    # 月着陸船: 切り離し（100:12:00）から分離の噴射までは司令船と同じ道
    follow(lm, csm, hms("100:12:00.0"), sep[0])
    legs = [
        (R(M, "101:36:14.0", -1.12, -140.20, 56.4, 5364.9, 0.10, -75.70), R(M, "101:36:44", -1.16, -141.88, 57.8, 5284.9, -0.06, -75.19)),
        (R(M, "102:33:05", 1.02, 39.39, 6.4, 5564.9, 0.03, -104.23), None)]
    g, r, v, c = chain(F, lm, lambda i: M, sep, legs)
    # 降下（表 5-I と本文: 高度 30,000 ft を下回った時刻、ハイゲート 7,129 ft・毎秒 125 ft で降下、高度 2,500 ft、
    # 手動の操縦を始めた約 600 ft、毎秒の降下速度の制御を始めた約 400 ft）。着陸 102:45:39.9、北緯 0.67408°・東経 23.47297°
    site = (0.67408, 23.47297)
    ft = FT
    site_pos = lunar_descent(F, lm, g, r, v, site, hms("102:45:39.9"),
                             [(hms("102:38:50"), 30000 * ft, None), (hms("102:41:32"), 7129 * ft, -125 * ft),
                              (hms("102:42:19"), 2500 * ft, None), (hms("102:43:13"), 600 * ft, None), (hms("102:43:22"), 400 * ft, None)],
                             [(hms("102:38:50"), 2000 * ft), (hms("102:41:32"), None), (hms("102:43:22"), 0.010)])
    lm.add(M, 1, still(hms("102:45:39.9"), hms("124:22:00.79"), site_pos))
    ins = R(M, "124:29:15.7", 0.73, 12.99, 10.0, 5537.9, 0.28, -108.15)
    csi = R(M, "125:19:35.0", -0.98, -147.12, 47.4, 5328.1, 0.11, -77.98)
    ri, vi = rec_in(F, ins, M)
    vi = F.shoot(ri, vi, M, ins[1], csi[1], rec_in(F, csi, M)[0])
    lunar_ascent(F, lm, site_pos, hms("124:22:00.79"), ins[1], ri, vi)
    # ランデブー: 軌道投入 → 同軌道の高さ合わせ（CSI）→ 終端段階の開始（TPI）→ ドッキング（司令船の位置）
    tpi_cut = R(M, "127:04:14.5", -1.17, -111.46, 44.0, 5413.2, -0.03, -92.65)
    legs = [(csi, R(M, "125:20:22.0", -0.91, -149.57, 48.4, 5376.6, 0.09, -76.98)),
            (R(M, "127:03:51.8", -1.17, -110.28, 44.1, 5391.5, -0.16, -93.16), None)]
    g, r, v, c = chain(F, lm, lambda i: M, (ins[1], ri, vi, M), legs)
    # TPI の後: 司令船とドッキングした位置へ
    gd, rd, vd = dock_state
    rtpi, vtpi = rec_in(F, tpi_cut, M)
    vtpi2 = F.shoot(rtpi, vtpi, M, tpi_cut[1], gd, rd)
    report("   TPI cut-off: velocity adjusted %.2f m/s" % (1000 * np.linalg.norm(vtpi2 - vtpi)))
    burn(F, lm, M, g, r, v, tpi_cut[1], rtpi, vtpi2)
    coast(F, lm, M, tpi_cut[1], rtpi, vtpi2, gd, rd)
    # ドッキングから上昇段を切り離すまでは司令船と同じ。切り離した後は月を回り続ける（地球へ向かう噴射まで描く）
    jet = hms("130:09:31.2")
    follow(lm, csm, gd, jet)
    tj, rj, vj = lm.pieces[-1][2][-1]
    coast(F, lm, M, tj, rj, vj, hms("135:23:42.3"))
    return "Apollo 11", F, [csm, lm]


def apollo8(work):
    """アポロ 8 号（1968 年 12 月 21 日 12:51:00 UTC 打ち上げ。月を 10 周して帰還、月着陸船なし）。
    軌道の値は Apollo 8 Mission Report の表 5-II と 5-V、上昇は SP-4029"""
    F = Flight(work, (1968, 12, 21, 12, 51, 0.0), 7.0)
    F.site_radius = 1738.09   # 月の平均半径（当時の値 938.49 n mi。月の高度の基準）
    csm = Track("Apollo 8")
    rows = [(0.67, 28.4470, -80.6041, 0.032, 1340.7, 0.00, 90.00), (61.45, 28.4526, -80.5805, 3.971, 2078.4, 26.79, 85.21),
            (78.9, 28.4645, -80.5398, 7.252, 2754.7, 29.56, 82.43), (125.93, 28.5581, -80.1934, 22.398, 6213.78, 24.527, 76.572),
            (153.82, 28.6856, -79.7302, 35.503, 8899.77, 20.699, 75.387), (154.47, 28.6893, -79.7168, 35.838, 8930.15, 20.605, 75.384),
            (524.04, 31.5492, -65.3897, 103.424, 22379.1, 0.646, 81.777), (524.90, 31.5565, -65.3338, 103.460, 22391.60, 0.636, 81.807),
            (684.98, 32.4541, -54.0565, 103.324, 25562.43, -0.001, 88.098)]
    earth_ascent(F, csm, rows, -3 * 3600.0)
    g, lat, lon, alt, vel, fpa, hdg = rows[-1]
    r, v = F.state((E, g, lat, lon, alt, vel, fpa, hdg))
    legs = [
        (R(E, "2:50:37.1", 9.25, -166.55, 106.4, 25558, 0.02, 58.64), R(E, "2:55:55.5", 21.12, -144.79, 179.3, 35532, 7.44, 67.16)),
        (R(E, "10:59:59.5", -1.70, -123.74, 52768.4, 8187, 73.82, 120.65), R(E, "11:00:01.9", -1.71, -123.75, 52771.7, 8172, 73.75, 120.54)),
        (R(M, "60:59:55", 14.09, -44.96, 21064.5, 4101, -84.41, -86.90), R(M, "61:00:07.8", 14.09, -44.97, 21059.2, 4103, -84.41, -87.01)),
        (R(M, "69:08:20.4", -7.46, -163.98, 76.6, 8391, -6.43, -123.79), R(M, "69:12:27.3", -9.89, 179.56, 62.0, 5458, -0.79, -117.13)),
        (R(M, "73:35:07", -11.61, 160.29, 59.3, 5479, -0.03, -110.18), R(M, "73:35:16", -11.66, 159.79, 60.7, 5345, -0.02, -110.18)),
        (R(M, "89:19:16.6", -9.27, 174.78, 60.2, 5342, -0.16, -118.59), R(M, "89:22:40.3", -11.17, 160.19, 66.1, 8842, 5.10, -115.00)),
        # 第 3 回の中間修正の点火の高度は記録では 165 561.5 n mi だが、14 秒後の停止の 167 552.0 と 2,000 n mi 違い、
        # 前の記録から進めた軌道（167 562）とも合わないので誤植とみて、停止の高度を使う
        (R(E, "103:59:54", -5.67, -57.27, 167552.0, 4299, -80.59, 52.65), R(E, "104:00:08", -5.67, -57.33, 167552.0, 4298, -80.60, 52.65)),
        (R(E, "146:46:12.8", geodetic_to_geocentric(20.83), -179.89, 65.90, 36221, -6.50, 121.57), None)]
    centers = [E, E, E, M, M, M, E, E]
    g, r, v, c = chain(F, csm, lambda i: centers[i], (g, r, v, E), legs)
    # 突入（表 5-V）: ドローグ・パラシュート 146:54:48（北緯 8.100°・西経 165.013°、最もよい推定）、主パラシュート 146:55:38.9、
    # 着水 147:00:42（SP-4029）。揚抗比は記録がないので、ほかの号と同じ 0.30
    entry(F, csm, legs[-1][0], (8.100, -165.013), hms("146:54:48"), hms("146:55:38.9"), hms("147:00:42"),
          mass=11631 * 0.4536, ld=0.30, gmax_rec=6.84)
    return "Apollo 8", F, [csm]


# ---------------------------------------------------------------- 書き出し
def shared_keep(tracks):
    """同じ道をたどる区間（follow で写した点。ドッキングしている間や切り離しの直後）の点を、両方の機体で同じだけ残すための、
    区間ごとの必ず残す点の番号。別々に間引くと点の時刻がずれ、補間の誤差（20 m まで）で 2 機の位置が数 m〜十数 m ずれて
    互いのまわりを回って見える。共通の点は片方で残ったらもう片方でも残し、共通の点の並びの両端と、その外側で隣り合う
    相手の点も残す（切り離しの時刻ちょうどの点は相手の 2 点の間を補間したものなので、その 2 点が残れば同じ曲線になる）"""
    pieces = [(ti, pi, ref, pts) for ti, trk in enumerate(tracks) for pi, (ref, frame, pts) in enumerate(trk.pieces) if frame == 0]
    index = {(ti, pi): {round(g, 6): k for k, (g, _, _) in enumerate(pts)} for ti, pi, _, pts in pieces}
    pairs = []   # (自分, 相手, 自分の点の番号, 相手の点の番号)
    for a in pieces:
        for b in pieces:
            if a[0] == b[0] or a[2] != b[2]:
                continue
            ib = index[(b[0], b[1])]
            for ka, (g, r, _) in enumerate(a[3]):
                kb = ib.get(round(g, 6))
                if kb is not None and np.linalg.norm(r - b[3][kb][1]) < 1e-9:
                    pairs.append(((a[0], a[1]), (b[0], b[1]), ka, kb))
    forced = {(ti, pi): set() for ti, pi, _, _ in pieces}
    length = {(ti, pi): len(pts) for ti, pi, _, pts in pieces}
    # 相手の点の間から分かれる・合流する点（13 号の月着陸船の切り離しなど、相手の状態を補間して始まる軌跡）: 相手の
    # 両隣の点を残す（間引いた相手の曲線は許容誤差まで離れていて、分かれる瞬間に機体が跳ぶ）
    for a in pieces:
        for ka in (0, len(a[3]) - 1):
            g, r, _ = a[3][ka]
            for b in pieces:
                if a[0] == b[0] or a[2] != b[2]:
                    continue
                pb = b[3]
                for kb in range(len(pb) - 1):
                    if pb[kb][0] < g < pb[kb + 1][0]:
                        rr, _ = hermite_state(pb[kb], pb[kb + 1], g)
                        if np.linalg.norm(rr - r) < 1e-6:
                            forced[(b[0], b[1])] |= {kb, kb + 1}
                        break
    shared = {}
    for a, b, ka, kb in pairs:
        shared.setdefault((a, b), {})[ka] = kb
    for (a, b), m in shared.items():
        ks = sorted(m)
        runs, start = [], ks[0]
        for x, y in zip(ks, ks[1:]):
            if y != x + 1 or m[y] != m[x] + 1:
                runs.append((start, x))
                start = y
        runs.append((start, ks[-1]))
        for s0, s1 in runs:
            forced[a] |= {s0, s1}
            forced[b] |= {k for k in (m[s0] - 1, m[s1] + 1) if 0 <= k < length[b]}
    return forced, shared


def hermite_state(p0, p1, g):
    """点 p0・p1（打ち上げからの秒, 位置, 速度）の間の時刻 g の位置と速度（state_in と同じ式）"""
    return state_in([p0, p1], g)[1:]


def write(path, flights):
    blob = struct.pack("<I", len(flights))
    for name, F, tracks in flights:
        blob += struct.pack("<16sI", name.encode()[:15], len(tracks))
        forced, shared = shared_keep(tracks)
        tol0 = lambda p: max(0.02, 2e-6 * np.linalg.norm(p))
        kept = {}
        for _ in range(10):   # 共通の点で片方だけ残ったものを、もう片方でも残す（変わらなくなるまで）
            for key in forced:
                pts = tracks[key[0]].pieces[key[1]][2]
                kept[key] = decimate_idx([(F.get0 + g / 86400 - J2000, r, v) for g, r, v in pts], tol0, forced[key])
            grew = False
            for (a, b), m in shared.items():
                add = {ka for ka, kb in m.items() if kb in kept[b] and ka not in forced[a]}
                if add:
                    forced[a] |= add
                    grew = True
            if not grew:
                break
        for ti, trk in enumerate(tracks):
            blob += struct.pack("<16sI", trk.name.encode()[:15], len(trk.pieces))
            total = 0
            for pi, (ref, frame, pts) in enumerate(trk.pieces):
                samples = [(F.get0 + g / 86400 - J2000, r, v) for g, r, v in pts]
                if frame == 0:
                    d = [samples[k] for k in kept[(ti, pi)]]
                else:
                    d = decimate(samples, lambda p: 0.002)
                t0, t1 = pts[0][0], pts[-1][0]
                m0 = icrf_to_ecl(F.moon.pos(F.jd(t0)[1]))
                m1 = icrf_to_ecl(F.moon.pos(F.jd(t1)[1]))
                blob += struct.pack("<ii6dI", ref, frame, *m0, *m1, len(d))
                for t, p, v in d:
                    if frame == 0:
                        p, v = icrf_to_ecl(p), icrf_to_ecl(v)
                    blob += struct.pack("<4d3f", t, *p, *v)
                total += len(d)
                print("   %-9s %s %s %9.1f..%9.1f s  %6d -> %5d" % (trk.name, "Moon " if ref == M else "Earth", "fixed" if frame else "inert",
                                                                   t0, t1, len(pts), len(d)))
            print("  %s points %d" % (trk.name, total))
    burns = [(name, F, trk, steady_burn(F, trk, b)) for name, F, tracks in flights for trk in tracks for b in getattr(trk, "burns", [])]
    blob += struct.pack("<I", len(burns))
    for name, F, trk, (g0, g1, eng, dirs) in burns:
        day = lambda g: F.get0 + g / 86400 - J2000
        blob += struct.pack("<16s16sidd I", name.encode()[:15], trk.name.encode()[:15], eng, day(g0), day(g1), len(dirs))
        for g, d in dirs:
            blob += struct.pack("<d3f", day(g), *icrf_to_ecl(d))
    print("burns", len(burns))
    with open(path, "wb") as f:
        f.write(blob)
    print("wrote", len(blob), "bytes")


def steady_burn(F, trk, b):
    """噴射の向きを、噴射全体の平均の推力の向きにする。エルミート曲線の加速度は区間の中で直線的に変わるので、区切りごとの
    向きは数十度揺れ、短い噴射では反転することもある。長い噴射（1 分以上）は、平均の向きを軌道に対して一定（中心天体の向き・
    進む向き・軌道面の法線を軸にした成分が同じ）に保って回す（月遷移軌道投入なども地平線に対して決まった向きで噴射した）"""
    g0, g1, eng, dirs = b[:4]
    if not dirs:
        return b[:4]
    if g1 - g0 < 60:   # 短い噴射は平均の推力の向きのまま（軌道に対する向きもほとんど変わらない）
        m = b[4] / np.linalg.norm(b[4])
        return (g0, g1, eng, [(g0, m), (g1, m)])

    def basis(t):
        st = state_at_time(F, trk, t)
        if st is None:
            return None
        _, (_, r, v) = st
        x = r / np.linalg.norm(r)
        z = np.cross(r, v)
        z /= np.linalg.norm(z)
        return np.array([x, np.cross(z, x), z])
    mid = basis((g0 + g1) / 2)
    if mid is None:
        return b[:4]
    comp = mid @ (b[4] / np.linalg.norm(b[4]))
    out = []
    for t, d in dirs:
        B = basis(t)
        out.append((t, d if B is None else B.T @ comp))
    return (g0, g1, eng, out)


def hermite(p0, v0, p1, v1, t0, t1, t):
    h = t1 - t0
    s = (t - t0) / h
    hs = h * 86400.0
    return (p0 * (2 * s ** 3 - 3 * s ** 2 + 1) + v0 * hs * (s ** 3 - 2 * s ** 2 + s) + p1 * (-2 * s ** 3 + 3 * s ** 2)
            + v1 * hs * (s ** 3 - s ** 2))


def decimate_idx(samples, tol, forced):
    """decimate と同じ。番号 forced の点は必ず残し、その間ごとに間引く。残した点の番号を返す"""
    cut = sorted({0, len(samples) - 1} | set(forced))
    keep = []
    for i0, i1 in zip(cut, cut[1:]):
        part = decimate(list(range(i0, i1 + 1)), tol, samples)
        keep += part if not keep else part[1:]
    return keep if keep else [0]


def decimate(samples, tol, ref=None):
    """エルミート補間で間の点が tol(位置) 以内に収まる範囲で点を減らす（tools/make_probe_assets.py と同じ）。
    ref を渡すと samples は ref の点の番号で、残した番号を返す"""
    if ref is not None:
        idx = samples
        sub = [ref[k] for k in idx]
        if len(sub) <= 2:
            return list(idx)
        return [idx[k] for k in decimate_keep(sub, tol)]
    if len(samples) <= 2:
        return samples
    return [samples[k] for k in decimate_keep(samples, tol)]


def decimate_keep(samples, tol):
    """decimate の残す点の番号"""

    def ok(i, j):
        (t0, p0, v0), (t1, p1, v1) = samples[i], samples[j]
        for k in range(i + 1, j):
            t, p, _ = samples[k]
            if np.linalg.norm(p - hermite(p0, v0, p1, v1, t0, t1, t)) > tol(p):
                return False
        return True
    keep, i, n = [0], 0, len(samples)
    while i < n - 1:
        step = 1
        while i + step * 2 < n and ok(i, i + step * 2):
            step *= 2
        lo, hi = step, min(step * 2, n - 1 - i)
        while lo < hi:
            mid = (lo + hi + 1) // 2
            if ok(i, i + mid):
                lo = mid
            else:
                hi = mid - 1
        i += lo
        keep.append(i)
    return keep


def nominal_descent(F, lm, get_pdi, r, v, site, get_land):
    """降下の途中の目安（高度と速さ）を 11 号の記録から使う（ほかの号は降下の途中の記録を読んでいない）。減速の段階は
    降下の始まりからの時間で（345 秒後に高度 30,000 ft・速さ 2,000 ft/s）、最終進入からは着陸までの時間で
    （ハイゲート 7,129 ft・毎秒 125 ft で降下を着陸の 248 秒前、2,500 ft を 201 秒前、600 ft を 147 秒前、400 ft を 138 秒前）"""
    ft = FT
    return lunar_descent(F, lm, get_pdi, r, v, site, get_land,
                         [(get_pdi + 345, 30000 * ft, None), (get_land - 248, 7129 * ft, -125 * ft), (get_land - 201, 2500 * ft, None),
                          (get_land - 147, 600 * ft, None), (get_land - 138, 400 * ft, None)],
                         [(get_pdi + 345, 2000 * ft), (get_land - 248, None), (get_land - 138, 0.010)])


def impulse(F, trk, center, get, r, v_before, nxt, legs_center=None):
    """停止の記録がない噴射: 点火の時刻に一度に速度を変えたとして、次の記録の位置を通る速度にする"""
    c2 = legs_center or center
    rn = rec_in(F, nxt, c2)[0]
    r2, v2 = F.to_center(r, v_before, get, center, c2)
    v3 = F.shoot(r2, v2, c2, get, nxt[1], rn)
    dv = 1000 * np.linalg.norm(v3 - v2)
    report("   impulse at %.1f: %.2f m/s" % (get, dv))
    # 噴射として記録: 長さは主エンジン（約 5 m/s²。秒速 3 m 未満は小さな噴射口で約 0.1 m/s²、60 秒まで）で変える時間
    T = dv / 5.0 if dv >= 3 else min(60.0, dv / 0.1)
    if dv > 1e-3:
        a = (v3 - v2) / max(T, 1e-3)
        add_burn(trk, get, get + max(T, 0.5), [(get, a), (get + max(T, 0.5), a)])
    return r2, v3


def apollo12(work):
    """アポロ 12 号（1969 年 11 月 14 日 16:22:00 UTC 打ち上げ。嵐の大洋のサーベイヤー3号のそばに着陸）。
    軌道の値は Apollo 12 Mission Report（MSC-01855）の表 5-III と 5-IX、上昇とできごとの時刻は SP-4029"""
    F = Flight(work, (1969, 11, 14, 16, 22, 0.0), 10.5)
    F.site_radius = 1736.0   # 着陸地点の月の半径（月の高度の基準）
    csm, lm = Track("Yankee Clipper"), Track("Intrepid")
    rows = [(0.68, 28.4470, -80.6041, 0.032, 1340.7, 0.07, 90.00), (36.5, 28.4469, -80.6030, 1.053, 1445.7, 15.40, 89.29),
            (52.0, 28.4487, -80.5968, 2.374, 1690.4, 22.74, 87.32), (66.1, 28.4532, -80.5820, 4.215, 2057.7, 27.13, 84.84),
            (81.1, 28.4627, -80.5498, 6.934, 2617.3, 29.02, 82.10), (135.24, 28.5794, -80.1463, 24.158, 6494.4, 23.944, 76.115),
            (161.74, 28.7069, -79.6913, 36.773, 9024.5, 20.513, 75.231), (162.4, 28.7107, -79.6773, 37.118, 9054.2, 20.430, 75.228),
            (460.75, 30.9599, -69.4827, 100.463, 18775.3, 0.502, 79.632), (552.34, 31.7508, -63.9914, 102.801, 22831.7, 0.442, 82.501),
            (553.20, 31.7576, -63.9341, 102.827, 22840.7, 0.432, 82.533), (693.91, 32.4933, -53.8956, 103.093, 25560.2, -0.015, 88.146)]
    earth_ascent(F, csm, rows, -3 * 3600.0)
    g, lat, lon, alt, vel, fpa, hdg = rows[-1]
    r, v = F.state((E, g, lat, lon, alt, vel, fpa, hdg))
    # 地球を回る間: 月へ向かう噴射の点火（2:47:22.8）の位置の記録がないので、そのまま進める
    tli_ign = hms("2:47:22.8")
    r, v, pts = F.propagate(r, v, E, g, tli_ign, record=True)
    add_inertial(F, csm, E, pts)
    tli_cut = R(E, "2:53:03.9", 15.83, -154.98, 192.1, 35427, 8.21, 63.69)
    mcc1 = (R(E, "30:52:44.4", -1.10, -63.02, 116929.1, 4317, 75.83, 120.80), R(E, "30:52:53.6", -1.10, -63.06, 116935.2, 4298, 76.60, 120.05))
    rc, vc = rec_in(F, tli_cut, E)
    vc = F.shoot(rc, vc, E, tli_cut[1], mcc1[0][1], rec_in(F, mcc1[0], E)[0])
    burn(F, csm, E, tli_ign, r, v, tli_cut[1], rc, vc)
    sep_ign = R(M, "108:24:36.8", -6.61, -7.44, 59.2, 5350, -0.18, 305.17)
    legs = [mcc1,
            (R(M, "83:25:23.4", 5.74, 175.61, 82.5, 8175, -8.44, 229.35), R(M, "83:31:15.7", -1.63, 154.04, 61.7, 5470, -0.63, 239.30)),
            (R(M, "87:48:48.1", -1.67, 151.67, 61.6, 5471, -0.66, 239.28), R(M, "87:49:05", -1.89, 150.85, 61.7, 5331, 0.30, 239.51)),
            (sep_ign, None)]
    centers = [E, E, M, M, M]
    g, r, v, c = chain(F, csm, lambda i: centers[i], (tli_cut[1], rc, vc, E), legs)
    sep = (g, r.copy(), v.copy(), c)
    # 司令船: 分離 → 1 回目の軌道面の変更（119:47:13.2、停止の位置の記録なし）→ 2 回目の軌道面の変更 → 地球へ向かう噴射。
    # ドッキングと上昇段の切り離しの記録は位置に使わない（月着陸船は計算した司令船の位置にドッキングする）
    sep_cut = R(M, "108:24:51.2", -6.45, -8.14, 59.2, 5350, -0.20, 305.15)
    pc1 = R(M, "119:47:13.2", -14.01, 77.68, 62.2, 5334, -0.07, 269.27)
    pc2 = (R(M, "159:04:45.5", -6.65, 110.34, 58.7, 5353, -0.20, 241.32), R(M, "159:05:04.8", -6.82, 109.40, 58.9, 5353, -0.20, 245.82))
    tei = (R(M, "172:27:16.8", 8.74, -170.25, 63.3, 5323, -0.21, 244.28), R(M, "172:29:27.1", 7.77, -178.56, 64.6, 8351, 2.69, 243.56))
    mcc2 = (R(E, "188:27:15.8", 15.88, 137.80, 180031.1, 3036, -78.44, 91.35), R(E, "188:27:20.2", 15.88, 137.78, 180028.9, 3036, -78.40, 91.36))
    mcc3 = (R(E, "241:21:59.7", 14.78, 92.40, 25059.0, 12083, -68.54, 96.00), R(E, "241:22:05.4", 14.78, 92.38, 25048.3, 12085, -68.55, 96.01))
    ei = R(E, "244:22:19.1", geodetic_to_geocentric(-13.80), 173.52, 65.8, 36116, -6.48, 98.16)
    rc, vc = rec_in(F, sep_cut, M)
    vc = turn_like(v, rec_in(F, sep_ign, M)[1], vc)
    vc = F.shoot(rc, vc, M, sep_cut[1], pc1[1], rec_in(F, pc1, M)[0])
    burn(F, csm, M, g, r, v, sep_cut[1], rc, vc)
    r, v, _ = coast(F, csm, M, sep_cut[1], rc, vc, pc1[1], rec_in(F, pc1, M)[0])
    r, v = impulse(F, csm, M, pc1[1], r, v, pc2[0])
    g, r, v, c = chain(F, csm, lambda i: [M, M, E, E, E][i], (pc1[1], r, v, M), [pc2, tei, mcc2, mcc3, (ei, None)])
    # 突入（表 5-IX）: ドローグ 244:30:39.7、主パラシュート 244:31:30.2、着水 244:36:25（南緯 15.78°・西経 165.15°、SP-4029）
    entry(F, csm, ei, (-15.78, -165.15), hms("244:30:39.7"), hms("244:31:30.2"), hms("244:36:25"),
          mass=11496.1 * 0.4536, ld=0.291, gmax_rec=6.57)

    # 月着陸船: 切り離し（107:54:02.3）から司令船の分離の噴射までは司令船と同じ道 → 降下軌道 → 動力降下 → 着陸
    follow(lm, csm, hms("107:54:02.3"), sep[0])
    legs = [(R(M, "109:23:39.9", 6.64, 172.21, 60.5, 5343, 0.17, 234.81), R(M, "109:24:08.9", 6.29, 170.76, 61.5, 5268, -0.02, 234.85)),
            (R(M, "110:20:38.1", -6.76, -7.82, 8.0, 5566, -0.02, 305.14), None)]
    g, r, v, c = chain(F, lm, lambda i: M, sep, legs)
    site = (-3.01239, -23.42157)
    land, lift = hms("110:32:36.2"), hms("142:03:47.78")
    site_pos = nominal_descent(F, lm, g, r, v, site, land)
    lm.add(M, 1, still(land, lift, site_pos))
    # 上昇: 軌道投入（142:10:59.9）の位置の記録がないので、CSI の点火の記録から戻した状態を使う
    csi = (R(M, "143:01:51", 5.16, 164.68, 51.5, 5310, 0.06, 234.43), R(M, "143:02:32.1", 4.65, 162.64, 51.5, 5355, 0.02, 234.29))
    ins_t = hms("142:10:59.9")
    rci, vci = rec_in(F, csi[0], M)
    ri, vi = F.propagate(rci, vci, M, csi[0][1], ins_t)
    lunar_ascent(F, lm, site_pos, lift, ins_t, ri, vi)
    g, r, v, c = chain(F, lm, lambda i: M, (ins_t, ri, vi, M), [csi, (R(M, "144:36:26", 14.57, -128.99, 44.5, 5382, 0.05, 257.93), None)])
    # TPI（停止の記録なし）→ ドッキング（145:36:20.2、計算した司令船の位置）
    dock_t = hms("145:36:20.2")
    ref, (gd, rd, vd) = state_at_time(F, csm, dock_t)
    rd = F.to_center(rd, vd, gd, ref, M)[0]
    v2 = F.shoot(r, v, M, g, dock_t, rd)
    report("   TPI: %.2f m/s" % (1000 * np.linalg.norm(v2 - v)))
    coast(F, lm, M, g, r, v2, dock_t, rd)
    # 上昇段の切り離し（148:04:30.9）→ 月面へ落とす噴射 → 月面に衝突（149:55:16.4、南緯 3.94°・西経 21.20°）
    jet = hms("148:04:30.9")
    follow(lm, csm, dock_t, jet)
    ref, (tj, rj, vj) = state_at_time(F, csm, jet)
    rj, vj = F.to_center(rj, vj, tj, ref, M)
    deo = (R(M, "149:28:14.8", -14.32, 62.86, 57.6, 5362, -0.12, 272.27), R(M, "149:29:36.9", -14.47, 58.62, 57.4, 5177, -0.27, 275.90))
    # 切り離しから点火までは位置の記録に合わせず進め、噴射は記録の点火から停止への変化を当てはめる（記録の点火の位置は
    # 計算した司令船の軌道と合わない）。その後は月面に当たるまで進める（記録の衝突は 149:55:16.4、南緯 3.94°・西経 21.20°。
    # 計算した衝突との差を表示する）
    rend, vend, _ = coast(F, lm, M, tj, rj, vj, deo[0][1])
    rc, vc = relative_burn(F, lm, M, deo[0], deo[1], deo[0][1], rend, vend)
    _, _, pts = F.propagate(rc, vc, M, deo[1][1], deo[1][1] + 3 * 3600.0, record=True)
    hit = next((k for k, p in enumerate(pts) if np.linalg.norm(p[1]) < F.site_radius), len(pts) - 1)
    add_inertial(F, lm, M, pts[:hit + 1])
    th, rh = pts[hit][0], pts[hit][1]
    f = moon_fixed_to_icrf(F.jd(th)[1]).T @ rh
    f /= np.linalg.norm(f)
    report("   impact at GET %.1f (record %.1f), %.2f, %.2f (record -3.94, -21.20)" % (
        th, hms("149:55:16.4"), math.degrees(math.asin(f[2])), math.degrees(math.atan2(f[1], f[0]))))
    return "Apollo 12", F, [csm, lm]


def apollo13(work):
    """アポロ 13 号（1970 年 4 月 11 日 19:13:00 UTC 打ち上げ。月へ向かう途中、55:54:53 に機械船の酸素タンクが爆発し、
    着陸を中止して月の裏を回って帰還）。軌道の値は Apollo 13 Mission Report（MSC-02680）の表 4-II、上昇とできごとの時刻は SP-4029"""
    F = Flight(work, (1970, 4, 11, 19, 13, 0.0), 6.5)
    F.site_radius = 1738.09
    csm, lm = Track("Odyssey"), Track("Aquarius")
    csm.lm_drives_after = hms("55:54:53")
    rows = [(0.61, 28.4470, -80.6041, 0.032, 1340.7, 0.04, 90.00), (68.4, 28.4533, -80.5804, 4.394, 2087.5, 27.34, 85.14),
            (81.3, 28.4608, -80.5529, 6.727, 2566.2, 28.98, 82.96), (135.18, 28.5677, -80.1654, 23.464, 6328.2, 23.612, 76.609),
            (163.60, 28.6989, -79.6810, 36.392, 9002.5, 19.480, 75.696), (164.3, 28.7029, -79.6660, 36.739, 9036.3, 19.383, 75.693),
            (330.64, 29.8167, -75.1433, 86.183, 12859.6, 4.158, 76.956), (462.6, 30.8785, -69.8409, 97.450, 16904.3, 0.77, 79.40),
            (592.64, 31.9133, -62.4374, 102.112, 22610.8, 0.657, 83.348), (593.50, 31.9193, -62.3805, 102.150, 22624.5, 0.650, 83.380),
            (749.83, 32.5241, -51.2552, 103.469, 25560.4, 0.004, 89.713)]
    earth_ascent(F, csm, rows, -3 * 3600.0)
    g, lat, lon, alt, vel, fpa, hdg = rows[-1]
    r, v = F.state((E, g, lat, lon, alt, vel, fpa, hdg))
    ei = R(E, "142:40:45.7", geodetic_to_geocentric(-28.23), 173.44, 65.83, 36210.6, -6.269, 77.210)
    legs = [
        (R(E, "2:35:46.4", -22.48, 142.45, 105.39, 25573.1, 0.032, 65.708), R(E, "2:41:37.2", -9.39, 166.45, 175.71, 35562.6, 7.182, 59.443)),
        (R(E, "30:40:49.6", 22.93, -101.85, 121381.93, 4682.5, 77.464, 112.843), R(E, "30:40:53.1", 22.80, -101.86, 121385.43, 4685.6, 77.743, 112.751)),
        # 爆発の後、月着陸船の下降エンジンで自由帰還軌道に戻す
        (R(E, "61:29:43.5", 20.85, 159.70, 188371.38, 3065.8, 79.364, 115.464), R(E, "61:30:17.7", 20.74, 159.56, 188393.19, 3093.2, 79.934, 116.54)),
        # 月の裏を回った 2 時間後、下降エンジンで帰りを早める（PC+2）
        (R(M, "79:27:39.0", 3.73, 65.46, 5465.26, 4547.7, 72.645, -116.308), R(M, "79:32:02.8", 3.62, 64.77, 5658.68, 5020.2, 64.784, -117.886)),
        (R(E, "105:18:28.0", 19.63, -136.84, 152224.32, 4457.8, -79.673, 114.134), R(E, "105:18:42.0", 19.50, -136.90, 152215.52, 4456.6, -79.765, 114.242)),
        (R(E, "137:39:51.5", 11.35, 113.39, 37808.58, 10109.1, -72.369, 118.663), R(E, "137:40:13.0", 11.34, 113.32, 37776.05, 10114.6, -72.373, 118.660)),
        (ei, None)]
    g, r, v, c = chain(F, csm, lambda i: E, (g, r, v, E), legs)
    # 突入: ドローグと主パラシュートの時刻は記録が見当たらないので、ほかの号（8・10・11・12）の平均（着水の約 6 分前、
    # 主パラシュートはその 50 秒後）。着水 142:54:41（南緯 21.63°・西経 165.37°、SP-4029）
    splash = hms("142:54:41")
    entry(F, csm, ei, (-21.63, -165.37), splash - 360.0, splash - 310.0, splash,
          mass=11579.8 * 0.4536, ld=0.280, gmax_rec=5.56)
    # 月着陸船アクエリアス: 救命ボートとして司令船とつながったまま帰り、突入の 1 時間あまり前に切り離した（141:30:00.2）。
    # その後は大気に入って燃え尽きるまで（高度 120 km まで）描く
    undock = hms("141:30:00.2")
    ref, (tu, ru, vu) = state_at_time(F, csm, undock)
    ru, vu = F.to_center(ru, vu, tu, ref, E)
    _, _, pts = F.propagate(ru, vu, E, tu, hms("142:45:00"), record=True)
    pts = [p for p in pts if np.linalg.norm(p[1]) - RE > 120.0]
    add_inertial(F, lm, E, pts)
    return "Apollo 13", F, [csm, lm]


def apollo14(work):
    """アポロ 14 号（1971 年 1 月 31 日 21:03:02 UTC 打ち上げ。フラ・マウロ高地に着陸）。軌道の値は Apollo 14 Mission Report
    （MSC-04112）の表 6-III、上昇とできごとの時刻は SP-4029。降下軌道への噴射を初めて司令船が月着陸船とつながったまま行った"""
    F = Flight(work, (1971, 1, 31, 21, 3, 2.0), 10.0)
    F.site_radius = 1736.0
    F.moon_north = True
    csm, lm = Track("Kitty Hawk"), Track("Antares")
    rows = [(0.57, 28.4470, -80.6041, 0.060, 1340.7, 0.05, 90.00), (68.0, 28.4521, -80.5787, 4.337, 2082.4, 26.80, 86.06),
            (81.0, 28.4580, -80.5509, 6.649, 2540.5, 28.77, 84.61), (135.14, 28.5441, -80.1598, 23.202, 6283.6, 23.554, 79.228),
            (164.10, 28.6516, -79.6634, 36.317, 8972.5, 19.584, 78.468), (164.8, 28.6548, -79.6484, 36.663, 9004.8, 19.489, 78.468),
            (463.09, 30.3347, -69.4425, 98.091, 18554.4, 0.829, 82.809), (559.05, 30.8611, -63.7444, 101.556, 22905.8, 0.621, 85.784),
            (560.00, 30.8654, -63.6810, 101.596, 22917.2, 0.612, 85.818), (700.56, 31.0978, -53.7349, 103.091, 25559.9, -0.004, 91.245)]
    earth_ascent(F, csm, rows, -3 * 3600.0)
    g, lat, lon, alt, vel, fpa, hdg = rows[-1]
    r, v = F.state((E, g, lat, lon, alt, vel, fpa, hdg))
    # 地球を回る間: 月へ向かう噴射の点火（2:28:32.4）の位置の記録がないので、そのまま進める。噴射の後は 2:34:31.9 の記録
    tli_ign = hms("2:28:32.4")
    r, v, pts = F.propagate(r, v, E, g, tli_ign, record=True)
    add_inertial(F, csm, E, pts)
    tli = R(E, "2:34:31.9", -19.53, 141.72, 179.1, 35514.1, 7.48, 65.59)
    mcc1 = (R(E, "30:36:07.9", 28.87, -130.33, 118515.0, 4437.9, 76.47, 101.98), R(E, "30:36:18.1", 28.87, -130.37, 118522.1, 4367.2, 76.95, 102.23))
    rc, vc = rec_in(F, tli, E)
    vc = F.shoot(rc, vc, E, tli[1], mcc1[0][1], rec_in(F, mcc1[0], E)[0])
    burn(F, csm, E, tli_ign, r, v, tli[1], rc, vc)
    # 月へ向かう途中 → 月を回る軌道（停止の方位 338.18 は点火の 257.31 と大きく違うので誤植とみて 258.18）→ つながったまま
    # 降下軌道へ → 月着陸船が離れる（103:47:41.6、噴射は秒速 0.8 m と小さいので入れない）
    sep = R(M, "103:47:41.6", -12.65, 87.76, 30.5, 5435.8, -1.52, 241.64)
    legs = [mcc1,
            (R(M, "76:58:12.0", 0.56, -61.40, 11900.3, 3711.4, -80.1, 295.57), R(M, "76:58:12.6", 0.56, -61.40, 11899.7, 3713.1, -80.1, 295.65)),
            (R(M, "81:56:40.7", 2.83, -174.81, 87.4, 8061.4, -9.97, 257.31), R(M, "82:02:51.5", 0.10, 161.58, 64.2, 5458.5, 1.3, 258.18)),
            (R(M, "86:10:53.0", 6.58, -173.60, 59.2, 5484.8, -0.08, 247.44), R(M, "86:11:13.8", 6.29, -174.65, 59.0, 5279.5, -0.03, 246.94)),
            (sep, None)]
    g, r, v, c = chain(F, csm, lambda i: [E, E, M, M, M][i], (tli[1], rc, vc, E), legs)
    sep_state = (g, r.copy(), v.copy(), c)
    # 司令船: 円軌道へ → 軌道面の変更 → 地球へ向かう噴射 → 中間修正 → 突入。ドッキングと最終分離は位置に使わない
    circ = (R(M, "105:11:46.1", 7.05, 178.56, 60.5, 5271.3, -0.1, 248.58), R(M, "105:11:50.1", 7.04, 178.35, 60.3, 5342.1, 0.22, 248.36))
    pc = (R(M, "117:29:33.1", -10.63, 96.31, 62.1, 5333.1, -0.04, 237.61), R(M, "117:29:51.6", -10.78, 95.40, 62.1, 5333.3, 0.01, 241.79))
    # 地球へ向かう噴射の停止の記録は、地球の自転軸を北として測ると後の記録（突入）とよく合う（月の自転軸では 1 万 km 以上外れる）
    tei = (R(M, "148:36:02.3", 7.41, -81.55, 60.9, 5340.6, -0.17, 260.81),
           R(M, "148:38:31.5", 6.64, 168.85, 66.5, 8505.0, 5.29, 266.89) + ("earth-north",))
    mcc3 = R(E, "165:34:56.7", 25.77, 46.43, 176713.8, 3593.2, -79.61, 124.88)
    ei = R(E, "215:47:45.3", geodetic_to_geocentric(-36.36), 165.80, 66.8, 36170.2, -6.37, 70.84)
    # 地球へ向かう噴射の点火の記録（西経 81.55°）は 2 分 29 秒後の停止（東経 168.85°）と 110° 離れていて誤植とみる。
    # 軌道面の変更の後はそのまま進め、噴射は記録の点火から停止への変化を当てはめ、中間修正の記録の位置へ向ける
    g, r, v, c = chain(F, csm, lambda i: M, sep_state, [circ, (pc[0], None)])
    rc, vc = rec_in(F, pc[1], M)
    vc = turn_like(v, rec_in(F, pc[0], M)[1], vc)
    burn(F, csm, M, pc[0][1], r, v, pc[1][1], rc, vc)
    r, v, _ = coast(F, csm, M, pc[1][1], rc, vc, tei[0][1])
    rc, vc = rec_in(F, tei[1], E)
    rs, vs = F.to_center(r, v, tei[0][1], M, E)
    vc2 = F.shoot(rc, vc, E, tei[1][1], mcc3[1], rec_in(F, mcc3, E)[0])
    report("   TEI: ignition state to cut-off record %.1f km; cut-off velocity adjusted %.2f m/s" % (
        np.linalg.norm(rs + vs * (tei[1][1] - tei[0][1]) - rc), 1000 * np.linalg.norm(vc2 - vc)))
    burn(F, csm, E, tei[0][1], rs, vs, tei[1][1], rc, vc2)
    r, v, _ = coast(F, csm, E, tei[1][1], rc, vc2, mcc3[1], rec_in(F, mcc3, E)[0])
    r, v = impulse(F, csm, E, mcc3[1], r, v, ei)
    g, r, v, c = chain(F, csm, lambda i: E, (mcc3[1], r, v, E), [(ei, None)])
    # 突入: ドローグと主パラシュートの時刻は記録を見ていないので、ほかの号の平均（着水の約 6 分前、その 50 秒後）。
    # 着水 216:01:58.1（南緯 27.02°・西経 172.67°、SP-4029）
    splash = hms("216:01:58.1")
    entry(F, csm, ei, (-27.02, -172.67), splash - 360.0, splash - 310.0, splash, mass=12130.8 * 0.4536, ld=0.290, gmax_rec=6.76)

    # 月着陸船: 離れてから動力降下 → 着陸 → 離陸 → 軌道投入（141:52:52.1）→ バーニア修正 → TPI → ドッキング（計算した司令船の位置）
    g, r, v, c = chain(F, lm, lambda i: M, sep_state, [(R(M, "108:02:26.5", -7.38, -1.57, 7.8, 5565.6, 0.08, 290.84), None)])
    site = (-3.64530, -17.47136)
    land, lift = hms("108:15:09.3"), hms("141:45:40")
    site_pos = nominal_descent(F, lm, g, r, v, site, land)
    lm.add(M, 1, still(land, lift, site_pos))
    vern = R(M, "141:56:49.4", 0.5, -37.1, 11.1, 5548.5, 0.52, 282.1)
    ins_t = hms("141:52:52.1")
    rv, vv = rec_in(F, vern, M)
    ri, vi = F.propagate(rv, vv, M, vern[1], ins_t)
    lunar_ascent(F, lm, site_pos, lift, ins_t, ri, vi)
    tpi = R(M, "142:30:51.1", 11.1, -149.6, 44.8, 5396.6, 0.73, 265.0)
    r, v, _ = coast(F, lm, M, ins_t, ri, vi, tpi[1], rec_in(F, tpi, M)[0])
    g = tpi[1]
    dock_t = hms("143:32:50.5")
    ref, (gd, rd, vd) = state_at_time(F, csm, dock_t)
    rd = F.to_center(rd, vd, gd, ref, M)[0]
    v2 = F.shoot(r, v, M, g, dock_t, rd)
    report("   TPI: %.2f m/s" % (1000 * np.linalg.norm(v2 - v)))
    coast(F, lm, M, g, r, v2, dock_t, rd)
    # ドッキング → 上昇段の切り離し（145:44:58.0）→ 最終分離（145:49:42.5、司令船が離れる）→ 月面へ落とす噴射 → 衝突
    follow(lm, csm, dock_t, hms("145:49:42.5"))
    ref, (tj, rj, vj) = state_at_time(F, csm, hms("145:49:42.5"))
    rj, vj = F.to_center(rj, vj, tj, ref, M)
    deo = (R(M, "147:14:16.9", -11.92, 67.43, 57.2, 5358.7, 0.018, 267.3), R(M, "147:15:33.1", -12.12, 63.53, 57.2, 5177.0, 0.019, 267.7))
    rend, vend, _ = coast(F, lm, M, tj, rj, vj, deo[0][1])
    rc, vc = relative_burn(F, lm, M, deo[0], deo[1], deo[0][1], rend, vend)
    _, _, pts = F.propagate(rc, vc, M, deo[1][1], deo[1][1] + 3 * 3600.0, record=True)
    hit = next((k for k, p in enumerate(pts) if np.linalg.norm(p[1]) < F.site_radius), len(pts) - 1)
    add_inertial(F, lm, M, pts[:hit + 1])
    f = moon_fixed_to_icrf(F.jd(pts[hit][0])[1]).T @ pts[hit][1]
    f /= np.linalg.norm(f)
    report("   impact at GET %.1f (record %.1f), %.2f, %.2f (record -3.42, -19.67)" % (
        pts[hit][0], hms("147:42:23.4"), math.degrees(math.asin(f[2])), math.degrees(math.atan2(f[1], f[0]))))
    return "Apollo 14", F, [csm, lm]


def lm_after_jettison(F, lm, csm, t_free, deo, imp_rec):
    """上昇段: t_free から司令船と別れて進み、月面へ落とす噴射（記録の点火から停止への変化を当てはめる）の後、月面に当たるまで"""
    ref, (tj, rj, vj) = state_at_time(F, csm, t_free)
    rj, vj = F.to_center(rj, vj, tj, ref, M)
    rend, vend, _ = coast(F, lm, M, tj, rj, vj, deo[0][1])
    rc, vc = relative_burn(F, lm, M, deo[0], deo[1], deo[0][1], rend, vend)
    _, _, pts = F.propagate(rc, vc, M, deo[1][1], deo[1][1] + 3 * 3600.0, record=True)
    hit = next((k for k, p in enumerate(pts) if np.linalg.norm(p[1]) < F.site_radius), len(pts) - 1)
    add_inertial(F, lm, M, pts[:hit + 1])
    f = moon_fixed_to_icrf(F.jd(pts[hit][0])[1]).T @ pts[hit][1]
    f /= np.linalg.norm(f)
    report("   impact at GET %.1f (record %.1f), %.2f, %.2f (record %.2f, %.2f)" % (
        pts[hit][0], imp_rec[0], math.degrees(math.asin(f[2])), math.degrees(math.atan2(f[1], f[0])), imp_rec[1], imp_rec[2]))


def apollo15(work):
    """アポロ 15 号（1971 年 7 月 26 日 13:34:00.6 UTC 打ち上げ。ハドリー谷・アペニン山脈に着陸、初めて月面車を使った）。
    軌道の値は Apollo 15 Mission Report（MSC-05161）の表 3-III、上昇とできごとの時刻・着陸地点は SP-4029"""
    F = Flight(work, (1971, 7, 26, 13, 34, 0.6), 12.5)
    F.site_radius = 1735.5
    csm, lm = Track("Endeavour"), Track("Falcon")
    rows = [(0.58, 28.4470, -80.6041, 0.060, 1340.7, 0.07, 90.00), (65.0, 28.4497, -80.5854, 4.224, 2028.1, 27.86, 87.36),
            (82.0, 28.4555, -80.5487, 7.401, 2681.3, 29.80, 85.77),   # 経度は記録では -80.5847（距離 2.97 n mi と合わず誤植とみる） (135.96, 28.5203, -80.1190, 25.271, 6708.5, 24.217, 82.494),
            (159.56, 28.5824, -79.6961, 36.947, 9043.3, 21.266, 82.129), (161.2, 28.5876, -79.6605, 37.830, 9062.2, 21.021, 82.144),
            (549.06, 29.6810, -63.9910, 95.184, 22949.6, 0.059, 89.863), (550.1, 29.6811, -63.9221, 95.187, 22962.5, 0.047, 89.900),
            (694.67, 29.2688, -53.8183, 93.215, 25596.7, 0.013, 95.149)]
    earth_ascent(F, csm, rows, -3 * 3600.0)
    g, lat, lon, alt, vel, fpa, hdg = rows[-1]
    r, v = F.state((E, g, lat, lon, alt, vel, fpa, hdg))
    tli_ign = hms("2:50:02.9")
    r, v, pts = F.propagate(r, v, E, g, tli_ign, record=True)
    add_inertial(F, csm, E, pts)
    tli = R(E, "2:56:03.6", 24.97, -142.13, 173.0, 35582.8, 7.48, 73.19)
    mcc1 = (R(E, "28:40:21.9", -13.36, -19.17, 114783.2, 4849.8, 77.22, 116.83), R(E, "28:40:22.7", -13.36, -19.18, 114784.0, 4845.6, 77.18, 116.76))
    rc, vc = rec_in(F, tli, E)
    vc = F.shoot(rc, vc, E, tli[1], mcc1[0][1], rec_in(F, mcc1[0], E)[0])
    burn(F, csm, E, tli_ign, r, v, tli[1], rc, vc)
    trim = R(M, "95:56:44.7", -25.10, -155.32, 56.4, 5276.9, 0.23, -83.23)
    legs = [mcc1,
            (R(M, "73:31:14.8", -0.94, -55.4, 12618.4, 3963.1, -81.08, -139.68), R(M, "73:31:15.7", -0.95, -55.4, 12617.7, 3966.8, -81.10, -140.00)),
            (R(M, "78:31:46.7", -27.79, -172.05, 86.7, 8188.6, -8.93, -54.45), R(M, "78:38:25.1", -21.03, 160.08, 74.1, 5407.5, 2.17, -50.45)),
            (R(M, "82:39:49.1", -25.26, -149.77, 55.3, 5491.7, 0.00, -81.54), R(M, "82:40:13.6", -25.37, -151.07, 54.9, 5285.0, 0.02, -80.95)),
            (trim, None)]
    g, r, v, c = chain(F, csm, lambda i: [E, E, M, M, M][i], (tli[1], rc, vc, E), legs)
    # 降下軌道の修正（月着陸船の姿勢制御の噴射。停止の記録は 30 秒で経度が 61° 動いていて誤植とみて使わない）→ 月着陸船が離れる
    sep = R(M, "100:39:16.2", 23.49, 33.58, 7.4, 5553.6, -0.53, -86.48)
    r, v = impulse(F, csm, M, trim[1], r, v, sep)
    g, r, v, c = chain(F, csm, lambda i: M, (trim[1], r, v, M), [(sep, None)])
    sep_state = (g, r.copy(), v.copy(), c)
    # 司令船: 円軌道へ（停止の記録は緯度が 3.7 秒で南北反対になっていて誤植とみて使わない）→ 軌道面の変更 → 軌道の調整（子衛星を
    # 放つ前）→ 地球へ向かう噴射 → 中間修正 → 突入
    circ = R(M, "101:38:59.0", -25.03, -157.78, 57.1, 5276.5, 0.32, -83.60)
    pc = (R(M, "165:11:32.7", -14.42, 96.94, 61.8, 5318.1, 0.20, -45.52), R(M, "165:11:51.0", -14.04, 96.00, 62.0, 5318.8, 0.01, -41.83))
    shape = (R(M, "221:20:48.0", 23.21, -87.71, 53.6, 5362.9, -0.34, -131.63), R(M, "221:20:51.4", 23.15, -87.90, 53.7, 5379.2, 1.34, -131.71))
    tei = (R(M, "223:48:45.8", -14.34, -167.97, 67.6, 5305.9, 0.52, -128.90), R(M, "223:51:06.8", -18.30, -176.32, 71.8, 8272.4, 4.43, -129.08))
    mcc3 = (R(E, "291:56:49.9", -38.43, 102.64, 25190.3, 11994.6, -68.47, 103.11), R(E, "291:57:12.2", -38.44, 102.58, 25149.3, 12002.4, -68.49, 103.09))
    ei = R(E, "294:58:54.7", geodetic_to_geocentric(14.23), -175.02, 65.9, 36096.4, -6.51, 52.06)
    r, v, _ = coast(F, csm, M, g, r, v, circ[1], rec_in(F, circ, M)[0])
    r, v = impulse(F, csm, M, circ[1], r, v, pc[0])
    g, r, v, c = chain(F, csm, lambda i: [M, M, M, E, E, E][i], (circ[1], r, v, M), [pc, shape, tei, mcc3, (ei, None)])
    # 突入: ドローグと主パラシュートの時刻は記録を見ていないので、ほかの号の平均。着水 295:11:53.0（北緯 26.13°・西経 158.13°）
    splash = hms("295:11:53.0")
    entry(F, csm, ei, (26.13, -158.13), splash - 360.0, splash - 310.0, splash, mass=12381 * 0.4536, ld=0.286, gmax_rec=6.23)

    # 月着陸船: 離れてから動力降下 → 着陸 → 離陸 → 軌道投入（171:44:34.2、状態は TPI の記録から戻したもの）→ TPI → ドッキング
    g, r, v, c = chain(F, lm, lambda i: M, sep_state, [(R(M, "104:30:09.4", 24.83, 22.50, 5.8, 5560.2, -0.40, -94.80), None)])
    site = (26.13222, 3.63386)
    land, lift = hms("104:42:29.3"), hms("171:37:23.2")
    site_pos = nominal_descent(F, lm, g, r, v, site, land)
    lm.add(M, 1, still(land, lift, site_pos))
    tpi = R(M, "172:29:40.0", -18.02, -150.83, 34.2, 5368.8, 0.11, -122.40)
    ins_t = hms("171:44:34.2")
    rt, vt = rec_in(F, tpi, M)
    ri, vi = F.propagate(rt, vt, M, tpi[1], ins_t)
    lunar_ascent(F, lm, site_pos, lift, ins_t, ri, vi)
    coast(F, lm, M, ins_t, ri, vi, tpi[1])
    dock_t = hms("173:36:25.5")
    ref, (gd, rd, vd) = state_at_time(F, csm, dock_t)
    rd = F.to_center(rd, vd, gd, ref, M)[0]
    v2 = F.shoot(rt, vt, M, tpi[1], dock_t, rd)
    report("   TPI: %.2f m/s" % (1000 * np.linalg.norm(v2 - vt)))
    coast(F, lm, M, tpi[1], rt, v2, dock_t, rd)
    jet = hms("179:30:01.4")
    follow(lm, csm, dock_t, jet)
    deo = (R(M, "181:04:19.8", -7.08, 74.19, 61.5, 5318.9, 0.01, -40.91), R(M, "181:05:42.8", -5.18, 70.37, 61.8, 5196.0, -1.72, -40.53))
    lm_after_jettison(F, lm, csm, jet, deo, (hms("181:29:37.0"), 26.20, 0.28))
    return "Apollo 15", F, [csm, lm]


def dm(d, m):
    """度と分 → 度（16 号の報告書の表は度:分で書かれている）"""
    return math.copysign(abs(d) + m / 60.0, d if d != 0 else 1)


def apollo16(work):
    """アポロ 16 号（1972 年 4 月 16 日 17:54:00 UTC 打ち上げ。デカルト高地に着陸）。軌道の値は Apollo 16 Mission Report
    （MSC-07230）の表 3-III（緯度・経度は度:分）、上昇とできごとの時刻・着陸地点は SP-4029"""
    F = Flight(work, (1972, 4, 16, 17, 54, 0.0), 11.5)
    F.site_radius = 1737.0
    F.moon_north = True
    csm, lm = Track("Casper"), Track("Orion")
    rows = [(0.59, 28.4470, -80.6041, 0.060, 1340.7, 0.05, 90.00), (67.5, 28.4539, -80.5797, 4.282, 2075.5, 26.79, 84.51),
            (86.0, 28.4670, -80.5359, 7.755, 2785.9, 29.12, 81.64), (137.85, 28.5847, -80.1207, 24.548, 6658.8, 23.105, 76.125),
            (161.78, 28.7009, -79.7028, 35.698, 8961.7, 19.914, 75.328), (163.5, 28.7109, -79.6666, 36.560, 8979.2, 19.643, 75.339),
            (461.77, 30.9376, -69.6064, 92.441, 18357.7, 0.116, 79.535), (559.54, 31.7737, -63.8100, 93.445, 22858.7, 0.367, 82.585),
            (560.5, 31.7812, -63.7457, 93.468, 22869.8, 0.358, 82.622), (706.21, 32.5109, -53.2983, 93.374, 25600.0, 0.001, 88.496)]
    earth_ascent(F, csm, rows, -3 * 3600.0)
    g, lat, lon, alt, vel, fpa, hdg = rows[-1]
    r, v = F.state((E, g, lat, lon, alt, vel, fpa, hdg))
    tli_ign = hms("2:33:36.5")
    r, v, pts = F.propagate(r, v, E, g, tli_ign, record=True)
    add_inertial(F, csm, E, pts)
    tli = R(E, "2:39:28", dm(-11, 59), dm(162, 29), 171.0, 35565.7, 7.47, 59.5)
    mcc1 = (R(E, "30:39:01", dm(25, 13), dm(-106, 29), 119343.8, 4514.8, 76.86, 111.56),
            R(E, "30:39:03", dm(25, 13), dm(-106, 30), 119345.3, 4508.1, 76.72, 111.50))
    rc, vc = rec_in(F, tli, E)
    vc = F.shoot(rc, vc, E, tli[1], mcc1[0][1], rec_in(F, mcc1[0], E)[0])
    burn(F, csm, E, tli_ign, r, v, tli[1], rc, vc)
    # 降下軌道への噴射の点火の経度は記録では 136:01 E だが、24 秒後の停止 137:16 W とつながらないので西経とみる
    sep = R(M, "96:13:31", dm(2, 22), dm(121, 55), 33.8, 5417.2, -1.43, -98.83)
    legs = [mcc1,
            (R(M, "74:28:28", dm(8, 9), dm(-166, 38), 93.9, 8105.4, -9.51, -89.95), R(M, "74:34:43", dm(7, 7), dm(169, 19), 75.3, 5399.2, 2.22, -95.5)),
            (R(M, "78:33:45", dm(8, 35), dm(-136, 1), 58.5, 5486.3, -0.40, -87.16), R(M, "78:34:09", dm(8, 37), dm(-137, 16), 58.4, 5281.9, 0.10, -87.31)),
            (sep, None)]
    g, r, v, c = chain(F, csm, lambda i: [E, E, M, M][i], (tli[1], rc, vc, E), legs)
    sep_state = (g, r.copy(), v.copy(), c)
    # 司令船: 円軌道へ（主エンジンの首振り装置の不具合を確かめるため約 6 時間遅れた。停止の記録は使わない）→ 軌道面の変更 →
    # 地球へ向かう噴射 → 中間修正 2 回 → 突入
    circ = R(M, "103:21:43", dm(8, 53), dm(-151, 59), 59.2, 5277.8, -0.06, -87.75)
    pc = (R(M, "169:05:52", dm(5, 36), dm(108, 50), 58.6, 5349.8, 0.26, -97.57), R(M, "169:05:59", dm(5, 34), dm(108, 30), 58.6, 5349.9, 0.28, -98.93))
    tei = (R(M, "200:21:33", dm(9, 43), dm(175, 16), 52.2, 5383.6, -0.15, -85.80), R(M, "200:24:15", dm(10, 58), dm(164, 21), 59.7, 8663.0, 5.12, -82.37))
    mcc2 = (R(E, "214:35:03", dm(-3, 46), dm(74, 32), 183668.0, 3806.8, -75.08, 165.08), R(E, "214:35:25", dm(-3, 46), dm(74, 30), 183664.8, 3807.9, -75.08, 164.99))
    mcc3 = (R(E, "262:37:21", dm(-29, 38), dm(82, 11), 25312.9, 12256.5, -69.02, 157.11), R(E, "262:37:27", dm(-29, 38), dm(82, 10), 25305.2, 12258.3, -69.02, 157.10))
    ei = R(E, "265:37:31", geodetic_to_geocentric(dm(-19, 52)), dm(-162, 8), 65.8, 36196.1, -6.55, 21.08)
    r, v, _ = coast(F, csm, M, g, r, v, circ[1], rec_in(F, circ, M)[0])
    r, v = impulse(F, csm, M, circ[1], r, v, pc[0])
    g, r, v, c = chain(F, csm, lambda i: [M, M, E, E, E][i], (circ[1], r, v, M), [pc, tei, mcc2, mcc3, (ei, None)])
    # 突入: ドローグ 265:45:26、主パラシュート 265:46:16、着水 265:51:05（南緯 0.70°・西経 156.22°、SP-4029）
    entry(F, csm, ei, (-0.70, -156.22), hms("265:45:26"), hms("265:46:16"), hms("265:51:05"), mass=12442 * 0.4536, ld=0.290, gmax_rec=7.19)

    # 月着陸船: 離れてから動力降下 → 着陸 → 離陸 → 軌道投入（175:38:56 の記録）→ TPI → ドッキング → 切り離した後は月を回り続けた
    g, r, v, c = chain(F, lm, lambda i: M, sep_state, [(R(M, "104:17:25", dm(-8, 40), dm(32, 44), 10.5, 5548.8, -0.07, -93.04), None)])
    site = (-8.97301, 15.50019)
    land, lift = hms("104:29:35"), hms("175:31:47.9")
    site_pos = nominal_descent(F, lm, g, r, v, site, land)
    lm.add(M, 1, still(land, lift, site_pos))
    ins = R(M, "175:38:56", dm(-9, 46), dm(5, 26), 9.9, 5523.3, 0.34, -93.88)
    tpi = R(M, "176:26:05", dm(6, 53), dm(-147, 22), 40.2, 5351.6, -0.00, -82.07)
    ri, vi = rec_in(F, ins, M)
    vi = F.shoot(ri, vi, M, ins[1], tpi[1], rec_in(F, tpi, M)[0])
    lunar_ascent(F, lm, site_pos, lift, ins[1], ri, vi)
    r, v, _ = coast(F, lm, M, ins[1], ri, vi, tpi[1], rec_in(F, tpi, M)[0])
    dock_t = hms("177:41:18")
    ref, (gd, rd, vd) = state_at_time(F, csm, dock_t)
    rd = F.to_center(rd, vd, gd, ref, M)[0]
    v2 = F.shoot(r, v, M, tpi[1], dock_t, rd)
    report("   TPI: %.2f m/s" % (1000 * np.linalg.norm(v2 - v)))
    coast(F, lm, M, tpi[1], r, v2, dock_t, rd)
    jet = hms("195:00:12")
    follow(lm, csm, dock_t, jet)
    ref, (tj, rj, vj) = state_at_time(F, csm, jet)
    rj, vj = F.to_center(rj, vj, tj, ref, M)
    coast(F, lm, M, tj, rj, vj, hms("200:21:33"))
    return "Apollo 16", F, [csm, lm]


def apollo17(work):
    """アポロ 17 号（1972 年 12 月 7 日 05:33:00 UTC、夜の打ち上げ。タウルス・リットロウ谷に着陸、アポロ計画の最後の飛行）。
    軌道の値は Apollo 17 Mission Report（JSC-07904）の表 3-III、上昇とできごとの時刻・着陸地点は SP-4029"""
    F = Flight(work, (1972, 12, 7, 5, 33, 0.0), 13.0)
    F.site_radius = 1734.8
    F.moon_north = True
    csm, lm = Track("America"), Track("Challenger")
    rows = [(0.63, 28.4470, -80.6041, 0.060, 1340.6, 0.05, 90.00),
            (67.5, 28.4465, -80.5802, 4.315, 2085.8, 26.91, 90.29),   # 経度は記録では -80.5082（距離 1.265 n mi と合わず誤植とみる）
            (82.5, 28.4457, -80.5460, 6.992, 2650.5, 28.89, 91.04), (139.30, 28.4329, -80.0781, 25.388, 6862.7, 23.199, 91.355),
            (161.20, 28.4211, -79.6741, 35.900, 9012.1, 20.4285, 91.718), (162.9, 28.4200, -79.6369, 36.776, 9036.1, 20.151, 91.741),
            (461.21, 27.5754, -69.4919, 93.420, 18439.6, -0.058, 97.647), (559.66, 26.7251, -63.8908, 93.182, 22933.5, 0.254, 100.395),
            (560.6, 26.7147, -63.8314, 93.195, 22942.1, 0.244, 100.424), (702.65, 24.7139, -54.4952, 92.082, 25598.0, 0.00118, 104.718)]
    earth_ascent(F, csm, rows, -3 * 3600.0)
    g, lat, lon, alt, vel, fpa, hdg = rows[-1]
    r, v = F.state((E, g, lat, lon, alt, vel, fpa, hdg))
    tli_ign = hms("3:12:36.6")
    r, v, pts = F.propagate(r, v, E, g, tli_ign, record=True)
    add_inertial(F, csm, E, pts)
    tli = R(E, "3:18:28", 5.14, -53.86, 162.4, 35589.6, 6.947, 118.040)
    # 第 1 回の中間修正（35:30:00、秒速 3 m ほど）の記録の位置は、前後の記録から進めた軌道と 9,000 km 以上合わないので使わない
    loi = (R(M, "86:14:23", -11.33, 177.38, 76.8, 8110.2, -9.90, 273.70), R(M, "86:20:56", -6.81, 151.84, 51.2, 5512.1, 0.43, 288.89))
    rc, vc = rec_in(F, tli, E)
    vc = F.shoot(rc, vc, E, tli[1], loi[0][1], rec_in(F, loi[0], E)[0])
    burn(F, csm, E, tli_ign, r, v, tli[1], rc, vc)
    r, v = rc, vc
    sep = R(M, "107:47:56", -5.02, 135.91, 47.2, 5342.8, -1.26, 289.41)
    legs = [loi, (R(M, "90:31:37", -11.40, 164.16, 51.1, 5512.7, -0.39, 286.50), R(M, "90:31:59", -11.06, 163.04, 50.9, 5322.1, -0.89, 286.80)),
            (sep, None)]
    g, r, v, c = chain(F, csm, lambda i: [E, M, M][i], (tli[1], r, v, E), legs)
    sep_state = (g, r.copy(), v.copy(), c)
    # 司令船: 円軌道へ → 軌道の調整（点火の記録だけ）→ 軌道面の変更（停止の記録だけ）→ 地球へ向かう噴射 → 中間修正 → 突入
    circ = (R(M, "109:17:29", -20.03, -149.17, 58.6, 5279.9, 0.45, 270.13), R(M, "109:17:33", -20.02, -149.30, 58.8, 5349.9, 0.47, 270.17))
    trim = R(M, "178:54:05", -12.37, 124.36, 64.9, 5315.1, 0.08, 285.74)
    pc = R(M, "179:54:14", 12.01, -57.38, 60.5, 5341.1, -0.001, 250.09)
    tei = (R(M, "234:02:09", -19.65, -170.02, 62.1, 5337.1, -0.18, 257.32), R(M, "234:04:33", -21.52, -179.69, 63.1, 8374.3, 2.46, 259.47))
    mcc2 = (R(E, "298:38:01", 45.67, 78.49, 25016.3, 12021.1, -68.43, 34.63), R(E, "298:38:10", 45.68, 78.46, 24999.7, 12025.8, -68.42, 34.63))
    ei = R(E, "301:38:38", geodetic_to_geocentric(0.71), -173.34, 65.8, 36090.3, -6.49, 156.53)
    g, r, v, c = chain(F, csm, lambda i: M, sep_state, [circ, (trim, None)])
    r, v = impulse(F, csm, M, trim[1], r, v, pc)
    r, v, _ = coast(F, csm, M, trim[1], r, v, pc[1], rec_in(F, pc, M)[0])
    r, v = impulse(F, csm, M, pc[1], r, v, tei[0])
    g, r, v, c = chain(F, csm, lambda i: [M, E, E][i], (pc[1], r, v, M), [tei, mcc2, (ei, None)])
    # 突入: ドローグ 301:46:22、主パラシュート 301:47:13、着水 301:51:59（南緯 17.88°・西経 166.11°、SP-4029）。
    # 揚抗比は記録がないので 16 号と同じ 0.29
    entry(F, csm, ei, (-17.88, -166.11), hms("301:46:22"), hms("301:47:13"), hms("301:51:59"), mass=12567 * 0.4536, ld=0.290, gmax_rec=6.49)

    # 月着陸船: 離れる → 2 回目の降下軌道への噴射 → 動力降下 → 着陸 → 離陸 → 軌道投入（185:28:56 の記録）→ TPI → ドッキング
    legs = [(R(M, "109:22:42", -19.22, -165.78, 59.6, 5274.5, 0.04, 275.74), R(M, "109:23:04", -19.12, -166.77, 59.6, 5267.0, 0.02, 276.06)),
            (R(M, "110:09:53", 19.13, 48.75, 8.7, 5550.3, -0.90, 276.07), None)]
    g, r, v, c = chain(F, lm, lambda i: M, sep_state, legs)
    site = (20.19080, 30.77168)
    land, lift = hms("110:21:58"), hms("185:21:37")
    site_pos = nominal_descent(F, lm, g, r, v, site, land)
    lm.add(M, 1, still(land, lift, site_pos))
    ins = R(M, "185:28:56", 21.91, 19.60, 8.0, 5542.3, 0.34, 277.58)
    tpi = R(M, "186:15:58", -15.12, -129.53, 44.6, 5333.3, 0.29, 252.31)
    ri, vi = rec_in(F, ins, M)
    vi = F.shoot(ri, vi, M, ins[1], tpi[1], rec_in(F, tpi, M)[0])
    lunar_ascent(F, lm, site_pos, lift, ins[1], ri, vi)
    r, v, _ = coast(F, lm, M, ins[1], ri, vi, tpi[1], rec_in(F, tpi, M)[0])
    dock_t = hms("187:37:15")
    ref, (gd, rd, vd) = state_at_time(F, csm, dock_t)
    rd = F.to_center(rd, vd, gd, ref, M)[0]
    v2 = F.shoot(r, v, M, tpi[1], dock_t, rd)
    report("   TPI: %.2f m/s" % (1000 * np.linalg.norm(v2 - v)))
    coast(F, lm, M, tpi[1], r, v2, dock_t, rd)
    jet = hms("191:18:31")
    follow(lm, csm, dock_t, jet)
    deo = (R(M, "192:58:14", -0.24, 86.97, 60.5, 5343.7, -0.03, 293.11), R(M, "193:00:10", 2.02, 81.68, 58.9, 5130.1, -2.14, 293.67))
    lm_after_jettison(F, lm, csm, jet, deo, (hms("193:17:21"), 19.96, 30.50))
    return "Apollo 17", F, [csm, lm]


MISSIONS = {"8": apollo8, "10": apollo10, "11": apollo11, "12": apollo12, "13": apollo13, "14": apollo14, "15": apollo15, "16": apollo16, "17": apollo17}


def build_cached(args):
    """1 つの飛行を組み立て、作業ディレクトリに保存する（飛行ごとに別のプロセスで並べて計算できるように）"""
    work, key = args
    import pickle
    import io
    import contextlib
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        name, F, tracks = MISSIONS[key](work)
    with open(os.path.join(work, "flight_%s.pkl" % key), "wb") as f:
        pickle.dump((name, F, tracks), f)
    return key, buf.getvalue()


def main():
    """使い方: make_apollo_assets.py <作業ディレクトリ> [出力先] [号 ...]（号を指定するとその号だけ計算し直す。ほかは保存したもの。none なら書き出すだけ）"""
    import pickle
    from multiprocessing import Pool
    work = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else "assets"
    keys = sys.argv[3:] or list(MISSIONS)
    if keys == ["none"]:   # 計算し直さず、保存したものから書き出すだけ
        keys = []
    os.makedirs(work, exist_ok=True)
    if keys:
        with Pool(min(len(keys), os.cpu_count() or 1)) as pool:
            for key, log in pool.imap_unordered(build_cached, [(work, k) for k in keys]):
                print("== Apollo", key)
                print(log, end="", flush=True)
    flights = []
    for key in MISSIONS:
        path = os.path.join(work, "flight_%s.pkl" % key)
        if os.path.exists(path):
            with open(path, "rb") as f:
                flights.append(pickle.load(f))
    flights.sort(key=lambda fl: fl[1].get0)
    write(os.path.join(out, "apollo.bin"), flights)


if __name__ == "__main__":
    main()
