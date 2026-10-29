#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""ISS とハッブル宇宙望遠鏡の軌道要素を CelesTrak から取得し、src/solar_data.h の SATELLITES 表の行を出力する。

データ: CelesTrak GP（https://celestrak.org/NORAD/elements/gp.php、米宇宙軍 18th SDS の平均軌道要素 TLE を
JSON にしたもの）。元期・平均運動 [周/日]・その変化の半分 [周/日²]・離心率・軌道傾斜角・昇交点赤経・近地点引数・
平均近点角 [deg]（TEME 座標。シミュレータでは J2000 赤道座標とみなす）。

使い方: python3 tools/fetch_tle.py
"""
import calendar
import json
import urllib.request

UA = "SolarSystemVoyager-asset-build/1.0 (https://github.com/hubstacksea/Solar-System-Simulator)"
SATS = [("ISS", 25544), ("Hubble", 20580)]
J2000_UNIX = 946728000.0


def main():
    for name, catnr in SATS:
        url = "https://celestrak.org/NORAD/elements/gp.php?CATNR=%d&FORMAT=json" % catnr
        d = json.load(urllib.request.urlopen(urllib.request.Request(url, headers={"User-Agent": UA}), timeout=60))[0]
        date, _, frac = d["EPOCH"].partition(".")
        sec = calendar.timegm(tuple(int(x) for x in date.replace("T", "-").replace(":", "-").split("-")) + (0, 0, 0))
        t = (sec + float("0." + (frac or "0")) - J2000_UNIX) / 86400.0
        print('        {"%s", %.6f, %.8f, %.8g, %.8f, %.4f, %.4f, %.4f, %.4f},  // %s' % (
            name, t, d["MEAN_MOTION"], d["MEAN_MOTION_DOT"], d["ECCENTRICITY"], d["INCLINATION"],
            d["RA_OF_ASC_NODE"], d["ARG_OF_PERICENTER"], d["MEAN_ANOMALY"], d["EPOCH"]))


if __name__ == "__main__":
    main()
