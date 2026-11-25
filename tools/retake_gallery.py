#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""README の画像のうち、地表の見え方（光り方・地図の色・露出）が変わると撮り直すものを、毎回同じ場面で撮る。

日時はすべて固定（PC の時刻によらない）。場所と向きは天体に固定した座標の共有リンク（at=）で指定する。

使い方: python3 tools/retake_gallery.py <実行ファイル> <出力先フォルダ> [名前 ...] [--apply]
  名前を省くとすべて撮る。--apply を付けると docs/screenshots/<名前>.jpg に書き出す（1280x720、JPEG 品質 88）
必要なもの: Pillow（--apply の時だけ）
"""
import datetime
import math
import os
import subprocess
import sys


def at(body, r_km, lat, lon, alt_km, az, pitch, t):
    """天体の緯度・経度（度、球として）・高度に置き、方位 az・俯角 pitch（度）を向く共有リンク（t は J2000 からの日数）"""
    la, lo = math.radians(lat), math.radians(lon)
    up = (math.cos(la) * math.cos(lo), math.sin(la), -math.cos(la) * math.sin(lo))
    north = (-math.sin(la) * math.cos(lo), math.cos(la), math.sin(la) * math.sin(lo))
    east = (-math.sin(lo), 0.0, -math.cos(lo))
    a, p = math.radians(az), math.radians(pitch)
    h = [north[i] * math.cos(a) + east[i] * math.sin(a) for i in range(3)]
    fwd = [h[i] * math.cos(p) + up[i] * math.sin(p) for i in range(3)]
    u2 = [up[i] * math.cos(p) - h[i] * math.sin(p) for i in range(3)]
    pos = [up[i] * (r_km + alt_km) for i in range(3)]
    return "?t=%s&at=%s,%s&p=1" % (t, body, ",".join("%.6f" % x for x in pos + fwd + u2))


def hours(iso):
    """--hours に渡す、今から iso（UTC）までの時間"""
    now = datetime.datetime.now(datetime.timezone.utc)
    return "%.6f" % ((datetime.datetime.fromisoformat(iso).replace(tzinfo=datetime.timezone.utc) - now).total_seconds() / 3600)


def earth_alt(lat, alt_km):
    """地球の楕円体の上 alt_km の点の、半径 6378.137 km の球としての高度"""
    a, b, l = 6378.137, 6356.752, math.radians(lat)
    r = math.sqrt(((a * a * math.cos(l)) ** 2 + (b * b * math.sin(l)) ** 2) / ((a * math.cos(l)) ** 2 + (b * math.sin(l)) ** 2))
    return r + alt_km - a


COCKPIT = ["--cockpit", "--photo", "2"]
# 名前: (起動オプション, 仕上げ（None: そのまま / "half": 2 倍の画素で撮って縮める / "crop": 2 倍の画素で撮って中央を切り出す）)
SHOTS = {
    "sunset": (lambda: ["--frames", "30", "--hours", hours("2026-10-29 02:09"), "--target", "3", "--at", "35", "-134", "0.1",
                        "--face-sun", "3"], None),
    "earth_day": (lambda: ["--frames", "150", "--hours", hours("2026-11-17 03:00"), "--target", "3", "--at", "36", "138", "3000"],
                  None),
    "earth_alps": (lambda: ["--frames", "240", *COCKPIT, "--share", at("Earth", 6378.137, 43, 9, 300, 0, -40, 9816.95)], None),
    "haze": (lambda: ["--frames", "30", "--hours", hours("2026-10-29 02:39"), "--target", "3", "--at", "35", "139", "10",
                      "--photo", "1"], None),   # 日本の上空 10 km
    "mars_sky": (lambda: ["--frames", "30", "--hours", hours("2026-10-29 08:09"), "--target", "4", "--at", "0", "0", "0.1",
                          "--face-sun", "20"], None),   # 太陽高度 約 20°
    "mars_map": (lambda: ["--frames", "220", "--photo", "2", "--hours", hours("2026-10-29 15:43"), "--target", "4", "--warp"],
                 None),   # マリネリス峡谷が正面
    "moon_terrain": (lambda: ["--frames", "300", *COCKPIT, "--share", at("Moon", 1737.4, 25.9, 3.3, 0.5, 50, -3, 9819.0)], None),
    "mars_terrain": (lambda: ["--frames", "300", *COCKPIT, "--share", at("Mars", 3395.4, -12.5, -66, 3, 100, -8, 9816.9)], None),
    "earth_mountains": (lambda: ["--frames", "240", *COCKPIT, "--share",
                                 at("Earth", 6378.137, 27.80, 86.95, earth_alt(27.80, 7.0), 0, 5, 9816.58)], None),
    # エウロパの二重の尾根（太陽高度が低く、尾根の陰影が出る時刻）と、冥王星のスプートニク平原の多角形
    "europa_ridges": (lambda: ["--frames", "300", *COCKPIT, "--share", at("Europa", 1560.8, 10, -90, 15, 0, -15, 9830.55)], None),
    "pluto_sputnik": (lambda: ["--frames", "300", *COCKPIT, "--share", at("Pluto", 1188.3, 15, 178, 12, 270, -12, 9831.35)], None),
    # トリトンのカンタロープ地形（太陽高度が低く、窪みの陰影が出る時刻）
    "triton_cantaloupe": (lambda: ["--frames", "300", *COCKPIT, "--share", at("Triton", 1353.4, 22, -15, 150, 270, -45, 9832.9)], None),
    "apollo15_landing": (lambda: ["--frames", "600", "--photo", "2", "--share", "?apollo=15&scene=12"], None),
    # スカイクレーン: 着地の 8.5 秒前のパーシビアランス（中心から 3391.6015 km）の南 69 m・上 55.5 m から北へ 36° 見下ろす
    # （着陸機は地形の上に降ろすので、地表の高さが変わると合わせ直す）
    "perseverance_skycrane": (lambda: ["--frames", "200", "--zoom", "12", "--dpi", "2", *COCKPIT, "--share",
                                       at("Mars", 3391.6015, 18.444626 - math.degrees(0.069 / 3391.6015), 77.450789, 0.0555, 0, -36,
                                          "%.8f" % (7719.36375947 - 8.5 / 86400)) + "&tg=Perseverance"], "half"),
    "spirit_airbags": (lambda: ["--frames", "200", "--zoom", "16", "--dpi", "2", *COCKPIT, "--share",
                                at("Mars", 3392.30, -14.5747, 175.4785, 0.15, 359.2, 46, "%.8f" % (1463.68439725 - 3 / 86400))
                                + "&tg=Spirit"], "crop"),
}


def main():
    args = [a for a in sys.argv[1:] if a != "--apply"]
    exe, out = os.path.abspath(args[0]), args[1]
    names = args[2:] or list(SHOTS)
    os.makedirs(out, exist_ok=True)
    for n in names:
        opts, finish = SHOTS[n]
        png = os.path.join(out, n + ".png")
        r = subprocess.run([exe, "--shot", png, *opts()], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=600)
        print(n, r.returncode)
        if "--apply" in sys.argv:
            from PIL import Image
            im = Image.open(png).convert("RGB")
            if finish == "half":
                im = im.resize((1280, 720), Image.LANCZOS)
            elif finish == "crop":
                w, h = im.size
                im = im.crop((w // 2 - 640, h // 2 - 360, w // 2 + 640, h // 2 + 360))
            assert im.size == (1280, 720), (n, im.size)
            im.save(os.path.join("docs", "screenshots", n + ".jpg"), quality=88, optimize=True)


if __name__ == "__main__":
    main()
