#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""太陽から 10 光年ほどまでの恒星 (src/nearby_stars.h) を作る。

それぞれの系の重心について、元期の赤経・赤緯・年周視差・固有運動・視線速度を出力する（シミュレータはここから
等速直線運動で位置を求める）。連星の各星は、重心に「相手を回る軌道（実視連星の軌道要素）× 質量の比」を足した所に置く。

データ（ネットからは取らず、下の表に書き写した値を使う）:
  - Gaia DR3 (Gaia Collaboration 2023, 元期 J2016.0): プロキシマ、バーナード星、ウォルフ 359、ラランド 21185、
    ロス 154、エリダヌス座イプシロン星、くじら座 BL 星・UV 星、シリウス B、ルーマン 16 の片方の位置
  - Hipparcos 新整約 (van Leeuwen 2007, 元期 J1991.25): シリウス A、ケンタウルス座アルファ星 A・B（確かめ用）
  - ケンタウルス座アルファ星 A・B: Akeson et al. 2021 (AJ 162, 14) の軌道・質量・重心の位置と固有運動 (J2019.5)
  - シリウス A・B: Bond et al. 2017 (ApJ 840, 70) の軌道・質量
  - くじら座 BL 星・UV 星 (GJ 65 A・B): Kervella et al. 2016 (A&A 593, A127) の軌道・質量・半径・視線速度。
    傾斜角は論文の 307.82° を、観測された位置角（同論文の表 2）と視線速度の差に合う 127.82° に直した
  - ルーマン 16 A・B: Lazorenko & Sahlmann 2018 (A&A 618, A111) の軌道・質量・年周視差・固有運動。昇交点と
    近点引数は、同論文の表 3 の相対位置と Garcia et al. 2017 の視線速度の差に合う向きに直した
  - WISE 0855−0714: Kirkpatrick et al. 2021 の年周視差・固有運動、位置は WISE の名前 (元期 約 2010.5)。
    質量・半径・自転周期は推定
  - 半径・表面温度・自転周期は各星の文献値（Kervella et al. 2017、Ribas et al. 2018 など）。分からない値は推定

惑星（NASA Exoplanet Archive の標準の値、2026 年 10 月に取得）:
  - プロキシマ b・d: Suárez Mascareño et al. 2025 (A&A 700, A11)
  - バーナード星 b〜e: Basant et al. 2025 (ApJL 982, L1)
  - ラランド 21185 b・c: Hurt et al. 2022 (AJ 163, 218)
  - エリダヌス座イプシロン星 b: Thompson et al. 2025 (AJ 170, 301)。視線速度と位置天文で傾斜角 40°・昇交点 186° が分かっている
  視線速度だけで見つかった惑星は、軌道の傾き（下限の質量 m sin i しか分からない）と昇交点が分からないので、傾斜角 60°・
  昇交点 0° とし（同じ星の惑星は同じ面）、質量は m sin i / sin 60°、半径は質量から Chen & Kipping 2017 の関係で推定する。
  視線速度の近点引数は星のものなので、惑星の近点引数は 180° 足す。円軌道で近点引数のないものは、基準の時刻を合
  （惑星が星の手前を通る時）とみなす。平衡温度は反射率 0.3 として星の温度・半径と軌道長半径から求める

軌道要素は (周期 [年], 近点通過 [年], 離心率, 軌道長半径 [秒角], 傾斜角, 昇交点, 近点引数 [deg])。
伴星の主星に対する相対位置は 北 = r(cos(ω+ν)cosΩ − sin(ω+ν)sinΩ cos i), 東 = r(cos(ω+ν)sinΩ + sin(ω+ν)cosΩ cos i),
奥行き（太陽から遠ざかる向き）= r sin(ω+ν) sin i。

使い方: python3 tools/make_nearby_stars.py [出力先=src/nearby_stars.h]
"""
import math
import sys

D = math.pi / 180
MAS = 1 / 3.6e6  # mas → deg

# 実視連星の軌道
ORBITS = {
    "acen": (79.762, 1955.564, 0.51947, 17.4930, 79.2430, 205.073, 231.519),
    "sirius": (50.1284, 1994.5715, 0.59142, 7.4957, 136.336, 45.400, 149.161),
    "gj65": (26.284, 1972.115, 0.6185, 2.0584, 127.82, 145.79, 283.27),
    "luh16": (27.54, 2017.78, 0.343, 1.784, 100.26, 139.67, 308.1),
}


def kepler(M, e):
    E = M if e < 0.8 else math.pi
    for _ in range(60):
        E -= (E - e * math.sin(E) - M) / (1 - e * math.cos(E))
    return E


def rel_orbit(el, year):
    """伴星の主星に対する相対位置 (北, 東, 奥行き) [秒角]"""
    P, T, e, a, i, Om, w = el
    M = math.remainder(2 * math.pi * (year - T) / P, 2 * math.pi)
    E = kepler(M, e)
    nu = 2 * math.atan2(math.sqrt(1 + e) * math.sin(E / 2), math.sqrt(1 - e) * math.cos(E / 2))
    r, u = a * (1 - e * math.cos(E)), w * D + nu
    north = r * (math.cos(u) * math.cos(Om * D) - math.sin(u) * math.sin(Om * D) * math.cos(i * D))
    east = r * (math.cos(u) * math.sin(Om * D) + math.sin(u) * math.cos(Om * D) * math.cos(i * D))
    return north, east, r * math.sin(u) * math.sin(i * D)


def rel_rate(el, year):
    """相対位置の変化 [秒角/年]"""
    h = 1e-4
    a, b = rel_orbit(el, year - h), rel_orbit(el, year + h)
    return tuple((q - p) / (2 * h) for p, q in zip(a, b))


def offset(ra, dec, north, east):
    """(ra, dec) を北・東へ [秒角] ずらす（小さな角度）"""
    return ra + east / 3600 / math.cos(dec * D), dec + north / 3600


def sep_pa(north, east):
    return math.hypot(north, east), math.degrees(math.atan2(east, north)) % 360


def report(msg):
    print(msg, file=sys.stderr)


# ---------------------------------------------------------------- 連星の重心
SYSTEMS = {}

# ケンタウルス座アルファ星: Akeson et al. 2021 表 8・9（重心 J2019.5）
SYSTEMS["acen"] = dict(ra=219.85892215, dec=-60.83163195, epoch=2019.5, plx=750.81, pmra=-3639.95, pmdec=700.40, rv=-22.3796)
# 確かめ: Hipparcos (J1991.25) の A と B の位置から、軌道の相対位置と重心を比べる
mA, mB = 1.0788, 0.9092
hipA, hipB = (219.92040813010280, -60.83514521897498), (219.91412459936643, -60.83948046402479)
n, e, _ = rel_orbit(ORBITS["acen"], 1991.25)
obs_n = (hipB[1] - hipA[1]) * 3600
obs_e = (hipB[0] - hipA[0]) * 3600 * math.cos(hipA[1] * D)
report("alf Cen 1991.25: model sep %.2f\" PA %.1f / Hipparcos sep %.2f\" PA %.1f" % (*sep_pa(n, e), *sep_pa(obs_n, obs_e)))

# シリウス: Gaia DR3 の B（J2016.0）から重心を求め、Hipparcos の A（J1991.25）と突き合わせる
mSA, mSB = 2.063, 1.018
fA = mSA / (mSA + mSB)
gB = dict(ra=101.28662552099249, dec=-16.720932526023173, plx=374.48958852876103, pmra=-461.570505530133, pmdec=-914.5196209016666)
n, e, _ = rel_orbit(ORBITS["sirius"], 2016.0)
bra, bdec = offset(gB["ra"], gB["dec"], -fA * n, -fA * e)
dn, de, _ = rel_rate(ORBITS["sirius"], 2016.0)
bpmra, bpmdec = gB["pmra"] - fA * de * 1000, gB["pmdec"] - fA * dn * 1000
SYSTEMS["sirius"] = dict(ra=bra, dec=bdec, epoch=2016.0, plx=gB["plx"], pmra=bpmra, pmdec=bpmdec, rv=-5.50)
# 確かめ: Hipparcos の A の位置 (J1991.25) に軌道の分を足した重心を、Hipparcos の固有運動（長年の平均で、重心の動きに近い）で
# 2016 年へ進めると、Gaia の B から求めた重心と 1 秒角以内で合う
hipSA = dict(ra=101.2885410520664, dec=-16.713143062644765, pmra=-546.01, pmdec=-1223.07)
n, e, _ = rel_orbit(ORBITS["sirius"], 1991.25)
fB = mSB / (mSA + mSB)
pr, pd = offset(*offset(hipSA["ra"], hipSA["dec"], fB * n, fB * e), hipSA["pmdec"] * 24.75 / 1000, hipSA["pmra"] * 24.75 / 1000)
report("Sirius barycenter pm from Gaia B: (%.1f, %.1f) mas/yr; position 2016 vs Hipparcos A: %.2f\" apart"
       % (bpmra, bpmdec, math.hypot((pr - bra) * math.cos(bdec * D), pd - bdec) * 3600))

# くじら座 BL 星・UV 星 (GJ 65 A・B): Gaia DR3 の 2 つの位置・固有運動を質量で平均して重心に
mGA, mGB = 0.1225, 0.1195
gA = dict(ra=24.771554293454546, dec=-17.948299887129313, pmra=3385.315810465071, pmdec=544.3863845619521)
gBB = dict(ra=24.771674208211856, dec=-17.947682860008488, pmra=3178.6942575748953, pmdec=584.0613350385156)
w = mGB / (mGA + mGB)
SYSTEMS["gj65"] = dict(ra=gA["ra"] + (gBB["ra"] - gA["ra"]) * w, dec=gA["dec"] + (gBB["dec"] - gA["dec"]) * w, epoch=2016.0,
                       plx=373.70, pmra=gA["pmra"] + (gBB["pmra"] - gA["pmra"]) * w,
                       pmdec=gA["pmdec"] + (gBB["pmdec"] - gA["pmdec"]) * w, rv=39.04)
n, e, _ = rel_orbit(ORBITS["gj65"], 2016.0)
obs_n = (gBB["dec"] - gA["dec"]) * 3600
obs_e = (gBB["ra"] - gA["ra"]) * 3600 * math.cos(gA["dec"] * D)
report("GJ 65 2016.0: model sep %.2f\" PA %.1f / Gaia sep %.2f\" PA %.1f" % (*sep_pa(n, e), *sep_pa(obs_n, obs_e)))

# ルーマン 16: Gaia DR3 の 2 パラメータ解（片方の星、J2016.0。2016 年の 2 つの星の間は 0.3 秒角ほど）を重心とみなす
SYSTEMS["luh16"] = dict(ra=162.3084022291181, dec=-53.3180447534979, epoch=2016.0, plx=501.557, pmra=-2760.884, pmdec=354.753, rv=19.0)

# ---------------------------------------------------------------- 1 つの星
GAIA = {  # Gaia DR3: ra, dec, 年周視差, μα*, μδ, 視線速度（なければ文献値）
    "proxima": (217.39232147200883, -62.67607511676666, 768.0665391873573, -3781.741008265163, 769.4650146478623, -22.20),
    "barnard": (269.44850252543836, 4.739420051112412, 546.975939730948, -801.5509783684709, 10362.394206546573, -110.47),
    "wolf359": (164.10319030755974, 7.002726940984864, 415.17941567802137, -3866.3382751436793, -2699.214987679166, 19.57),
    "lalande": (165.83095967577933, 35.948653032660104, 392.75294543876464, -580.0570872139048, -4776.588719443488, -85.11),
    "ross154": (282.4587890175222, -23.83709744872712, 336.0266016683708, 639.367850335873, -193.95792507116096, -11.12),
    "epseri": (53.22829341517546, -9.458168216292322, 310.5772928005821, -974.758145249517, 20.875840089774208, 16.38),
}
for k, (ra, dec, plx, pmra, pmdec, rv) in GAIA.items():
    SYSTEMS[k] = dict(ra=ra, dec=dec, epoch=2016.0, plx=plx, pmra=pmra, pmdec=pmdec, rv=rv)
# WISE 0855−0714（Gaia にはない）: 位置は WISE の名前 J085510.83−071442.5、視線速度は分からないので 0
SYSTEMS["wise0855"] = dict(ra=133.795125, dec=-7.2451389, epoch=2010.5, plx=439.0, pmra=-8123.7, pmdec=673.2, rv=0.0)

# ---------------------------------------------------------------- 星
# 系, 連星の何番目 (0: 単独, 1: 主星, 2: 伴星), 日本語名, 英語名, 種類 (0: 恒星, 1: 白色矮星, 2: 褐色矮星), スペクトル型,
# 質量 [太陽], 半径 [太陽], 表面温度 [K], 実視等級 V（太陽から）, 色指数 B−V, 自転周期 [日], 推定の値があるか
STARS = [
    ("acen", 1, "ケンタウルス座アルファ星 A", "Alpha Centauri A", 0, "G2V", 1.0788, 1.2175, 5790, 0.01, 0.71, 22.0, False),
    ("acen", 2, "ケンタウルス座アルファ星 B", "Alpha Centauri B", 0, "K1V", 0.9092, 0.8591, 5260, 1.33, 0.88, 36.7, False),
    ("proxima", 0, "プロキシマ・ケンタウリ", "Proxima Centauri", 0, "M5.5Ve", 0.1221, 0.1542, 3000, 11.13, 1.82, 83.0, False),
    ("barnard", 0, "バーナード星", "Barnard's Star", 0, "M4V", 0.162, 0.187, 3195, 9.51, 1.57, 145.0, False),
    ("luh16", 1, "ルーマン 16 A", "Luhman 16 A", 2, "L7.5", 33.51 / 1047.57, 0.0975, 1350, 99, 2.2, 0.29, True),
    ("luh16", 2, "ルーマン 16 B", "Luhman 16 B", 2, "T0.5", 28.55 / 1047.57, 0.0975, 1210, 99, 2.2, 0.22, True),
    ("wise0855", 0, "WISE 0855-0714", "WISE 0855-0714", 2, "Y4", 6 / 1047.57, 0.0975, 285, 99, 2.2, 0.5, True),
    ("wolf359", 0, "ウォルフ 359", "Wolf 359", 0, "M6V", 0.110, 0.144, 2800, 13.51, 2.03, 2.7, False),
    ("lalande", 0, "ラランド 21185", "Lalande 21185", 0, "M2V", 0.389, 0.392, 3550, 7.52, 1.44, 56.0, False),
    ("sirius", 1, "シリウス A", "Sirius A", 0, "A1V", 2.063, 1.711, 9940, -1.46, 0.00, 5.4, False),
    ("sirius", 2, "シリウス B", "Sirius B", 1, "DA2", 1.018, 0.008098, 25000, 8.44, -0.03, 1.0, True),
    ("gj65", 1, "くじら座 BL 星", "BL Ceti", 0, "M5.5V", 0.1225, 0.165, 2700, 12.54, 1.85, 0.2432, False),
    ("gj65", 2, "くじら座 UV 星", "UV Ceti", 0, "M6V", 0.1195, 0.159, 2650, 12.99, 1.87, 0.2276, False),
    ("ross154", 0, "ロス 154", "Ross 154", 0, "M3.5Ve", 0.17, 0.24, 3250, 10.44, 1.50, 2.85, False),
    ("epseri", 0, "エリダヌス座イプシロン星", "Epsilon Eridani", 0, "K2V", 0.82, 0.735, 5084, 3.73, 0.88, 11.4, False),
]


# ---------------------------------------------------------------- 惑星
JD2000 = 2451545.0
# 星の英語名, 日本語名, 英語名, 周期 [日], 軌道長半径 [AU], 離心率, 星の近点引数 [deg]（None: 円軌道）, 近点通過の時刻 [JD],
# 質量 [地球]（下限 m sin i か、真の質量）, 真の質量か, 傾斜角, 昇交点 [deg]（None: 分からない）, 惑星の近点引数か
PLANETS = [
    ("Proxima Centauri", "プロキシマ・ケンタウリ d", "Proxima Centauri d", 5.12338, 0.02881, 0.0, None, 2460557.55, 0.26, False, None, None, False),
    ("Proxima Centauri", "プロキシマ・ケンタウリ b", "Proxima Centauri b", 11.18465, 0.04848, 0.0, None, 2460548.59, 1.055, False, None, None, False),
    ("Barnard's Star", "バーナード星 d", "Barnard's Star d", 2.3402, 0.0188, 0.04, -51.8, 2460243.70, 0.263, False, None, None, False),
    ("Barnard's Star", "バーナード星 b", "Barnard's Star b", 3.1542, 0.0229, 0.03, 3.8, 2460243.38, 0.299, False, None, None, False),
    ("Barnard's Star", "バーナード星 c", "Barnard's Star c", 4.1244, 0.0274, 0.08, 90.8, 2460242.92, 0.335, False, None, None, False),
    ("Barnard's Star", "バーナード星 e", "Barnard's Star e", 6.7392, 0.0381, 0.04, -27.5, 2460245.30, 0.193, False, None, None, False),
    ("Lalande 21185", "ラランド 21185 b", "Lalande 21185 b", 12.9394, 0.07879, 0.063, -30.0, 2456307.8, 2.69, False, None, None, False),
    ("Lalande 21185", "ラランド 21185 c", "Lalande 21185 c", 2946.0, 2.94, 0.132, 63.0, 2457250.0, 13.6, False, None, None, False),
    ("Epsilon Eridani", "エリダヌス座イプシロン星 b", "Epsilon Eridani b", 2680.0, 3.53, 0.06, 268.0, 2458017.5, 317.83, True, 40.0, 186.0, True),
]
ASSUMED_I = 60.0


def planet_radius(m):
    """質量 [地球] → 半径 [地球]（Chen & Kipping 2017。木星くらいより重いものは木星の半径）"""
    if m < 2.04:
        return 1.008 * m ** 0.279
    if m < 132:
        return 0.808 * m ** 0.589
    return 10.97


def planet_rows():
    rows = []
    for host, jp, en, P, a, e, w, tp, m, true_mass, inc, node, wplanet in PLANETS:
        star = [x for x in STARS if x[3] == host][0]
        teff, rstar = star[8], star[7]
        known = inc is not None
        i = inc if known else ASSUMED_I
        mass = m if true_mass else m / math.sin(i * D)
        wp = (w if wplanet else (90.0 if w is None else w) + 180.0) % 360.0
        rad = planet_radius(mass) * 6371.0
        teq = teff * math.sqrt(rstar * 695700.0 / (2 * a * 1.495978707e8)) * 0.7 ** 0.25
        kind = 0 if mass < 5 else 2 if mass > 100 else 1
        rows.append('    {"%s", "%s", "%s", %d, %.6f, %.6f, %.4f, %.2f, %.2f, %.2f, %.4f, %.5g, %.0f, %.0f, %s},'
                    % (jp, en, host, kind, P, a, e, i, node if node is not None else 0.0, wp, tp - JD2000, mass, rad, teq,
                       "true" if known else "false"))
        report("%-26s %6.2f d  %.4f AU  %.3g Earth masses  R %.0f km  Teq %.0f K" % (en, P, a, mass, rad, teq))
    return rows


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "src/nearby_stars.h"
    rows = []
    for sysk, comp, jp, en, kind, sp, m, r, teff, V, bv, rot, est in STARS:
        s = SYSTEMS[sysk]
        el, k = ORBITS.get(sysk), 0.0
        if comp:
            other = [x[6] for x in STARS if x[0] == sysk and x[1] != comp][0]
            k = -other / (m + other) if comp == 1 else other / (m + other)
        absV = V + 5 + 5 * math.log10(s["plx"] / 1000) if V < 90 else 99.0
        orbit = "%.4f, %.4f, %.5f, %.4f, %.3f, %.3f, %.3f, %.5f" % (*el, k) if el else "0, 0, 0, 0, 0, 0, 0, 0"
        rows.append('    {"%s", "%s", %d, "%s", %d, %.9f, %.9f, %.3f, %.4f, %.4f, %.4f, %.3f,\n     %s,\n     %.5g, %.5g, %.0f, %.2f, %.2f, %.4g, %s},'
                    % (jp, en, kind, sp, list(SYSTEMS).index(sysk), s["ra"], s["dec"], s["epoch"], s["plx"], s["pmra"], s["pmdec"], s["rv"],
                       orbit, m, r, teff, absV, bv, rot, "true" if est else "false"))
        report("%-28s %6.3f ly" % (en, 3.26156 / (s["plx"] / 1000)))
    text = """// Solar System Voyager — Copyright (c) 2026 hubstacksea
// MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
// 太陽から 10 光年ほどまでの恒星（tools/make_nearby_stars.py で作った。データの出典はそちらを参照）
#pragma once

struct NearbyStar {
    const char* jp;
    const char* en;
    int kind;            // 0: 恒星, 1: 白色矮星, 2: 褐色矮星
    const char* spec;    // スペクトル型
    int system;          // 同じ系（連星）の星は同じ番号
    // 系の重心: 元期の赤経・赤緯 (ICRS) [deg], 元期 [年], 年周視差 [mas], 固有運動 μα*・μδ [mas/年], 視線速度 [km/s]
    double ra, dec, epoch, plx, pmra, pmdec, rv;
    // 連星: 伴星の主星に対する軌道（周期 [年], 近点通過 [年], 離心率, 軌道長半径 [秒角], 傾斜角・昇交点・近点引数 [deg]）と、
    // 重心からの位置に掛ける係数（主星 −m伴/m全, 伴星 +m主/m全）。単独の星は P = 0
    double P, T, e, a, i, node, peri, k;
    double mass, radius, teff;  // [太陽], [太陽], [K]
    double absMag, bv;          // 絶対等級 V（99: 可視光ではほぼ見えない）, 色指数 B−V
    double rotPeriod;           // 自転周期 [日]
    bool estimated;             // 質量・半径・自転周期などに推定の値を含む
};

static const NearbyStar NEARBY_STARS[] = {
%s
};

// 惑星。軌道は恒星から見た相対位置で、恒星の連星の軌道と同じ形の要素（天球の北・東・奥の向き）。
// orbitKnown が false の惑星は、傾斜角・昇交点（と質量・半径）が推定
struct NearbyPlanet {
    const char* jp;
    const char* en;
    const char* host;    // 恒星の英語名
    int kind;            // 0: 岩石惑星, 1: 海王星くらい, 2: 木星くらい
    double P, a, e;      // 周期 [日], 軌道長半径 [AU], 離心率
    double i, node, peri;  // 傾斜角・昇交点・惑星の近点引数 [deg]
    double tp;           // 近点通過の時刻 [J2000 からの日数]
    double mass, radius, teq;  // [地球], [km], 平衡温度 [K]
    bool orbitKnown;
};

static const NearbyPlanet NEARBY_PLANETS[] = {
%s
};
""" % ("\n".join(rows), "\n".join(planet_rows()))
    with open(out, "w", encoding="utf-8") as f:
        f.write(text)


if __name__ == "__main__":
    main()
