#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""決まった場面の描画の速さを測る（変更で重くなっていないかを、前の版の実行ファイルと比べる）。

場面ごとに 50 フレームと 250 フレームを撮るのにかかった時間の差（200 フレームを描く時間。起動と読み込みの時間を除く）を
測る。PC のほかの仕事でばらつくので、それぞれ数回測って一番短いものを使う。日時と場所は固定（共有リンク）。
画面の大きさは既定（1280x720）、--photo 1 --cockpit で HUD は最小。

使い方: python3 tools/bench_render.py <実行ファイル> [<比べる実行ファイル> ...] [--runs 回数=3] [--scenes 名前,名前,...]
  例: python3 tools/bench_render.py build/solar_voyager old/solar_voyager --scenes pluto,europa
"""
import os
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from retake_gallery import at, earth_alt  # noqa: E402

# 名前: 共有リンク（地表の近くで、その天体の描き方が画面の大部分を占める場面）
SCENES = {
    "pluto": at("Pluto", 1188.3, 20, 180, 30, 270, -30, 9831.35),        # スプートニク平原の溝
    "europa": at("Europa", 1560.8, 10, -90, 20, 270, -25, 9830.5),       # 二重の尾根
    "triton": at("Triton", 1353.4, 22, -15, 150, 90, -25, 9832.8),       # カンタロープ地形
    "moon": at("Moon", 1737.4, 25.9, 3.3, 0.5, 50, -3, 9819.0),          # 地形の格子・岩塊
    "mars": at("Mars", 3395.4, -12.5, -66, 3, 100, -8, 9816.9),          # 地形の格子・崖・砂丘
    # オリンポス断崖の東の急な所を、15 km 東の地表の 400 m 上から見上げる（地域だけのもっと細かい標高。mars_patches.bin）
    "olympus": at("Mars", 3396.2, 17.88, -128.85, -0.569, 275, 8, 9830.15),
    "earth": at("Earth", 6378.137, 27.80, 86.95, earth_alt(27.80, 7.0), 0, 5, 9816.58),   # 地形の格子・大気
}


def shot_time(exe, query, frames, out):
    t = time.monotonic()
    subprocess.run([exe, "--shot", out, "--frames", str(frames), "--photo", "1", "--cockpit", "--share", query],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=600, check=True)
    return time.monotonic() - t


def main():
    args, runs, names = [], 3, list(SCENES)
    it = iter(sys.argv[1:])
    for a in it:
        if a == "--runs":
            runs = int(next(it))
        elif a == "--scenes":
            names = next(it).split(",")
        else:
            args.append(os.path.abspath(a))
    if not args:
        sys.exit(__doc__)
    out = os.path.join(tempfile.mkdtemp(), "bench.png")
    print("%-8s" % "scene" + "".join("%12s" % ("exe%d" % (k + 1)) for k in range(len(args))) + "   (s / 200 frames)")
    for n in names:
        row = []
        for exe in args:
            t = {f: min(shot_time(exe, SCENES[n], f, out) for _ in range(runs)) for f in (50, 250)}
            row.append(t[250] - t[50])
        print("%-8s" % n + "".join("%12.2f" % x for x in row)
              + ("" if len(row) < 2 else "   " + " ".join("%+.0f%%" % (100 * (x / row[0] - 1)) for x in row[1:])), flush=True)
    for k, exe in enumerate(args):
        print("exe%d = %s" % (k + 1, exe))


if __name__ == "__main__":
    main()
