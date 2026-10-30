#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""ブラウザ版の日本語フォント (web/NotoSansJP-subset.ttf) を作る。

ブラウザでは OS のフォントを読めないので、Noto Sans JP（SIL Open Font License 1.1、Google Fonts）から、
ゲームで表示する文字（src/ の文字列・assets/sky.txt の星座名と恒星名・ASCII）だけを残した静的なフォントを作る。
可変フォントは標準の太さ（400）に固定する。

使い方: python3 tools/make_web_font.py <作業ディレクトリ>
必要なもの: fontTools（ダウンロード 約 10 MB）
"""
import glob
import os
import sys
import urllib.request

from fontTools import subset
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer

UA = "SolarSystemVoyager-asset-build/1.0 (https://github.com/hubstacksea/Solar-System-Simulator)"
URL = "https://github.com/google/fonts/raw/main/ofl/notosansjp/NotoSansJP%5Bwght%5D.ttf"
LICENSE_URL = "https://github.com/google/fonts/raw/main/ofl/notosansjp/OFL.txt"


def fetch(work, url, name):
    path = os.path.join(work, name)
    if not os.path.exists(path):
        print("download", url)
        req = urllib.request.Request(url, headers={"User-Agent": UA})
        with urllib.request.urlopen(req, timeout=300) as r, open(path, "wb") as f:
            f.write(r.read())
    return path


def main():
    work = sys.argv[1]
    os.makedirs(work, exist_ok=True)
    chars = set(chr(c) for c in range(32, 127)) | set("°²·×〜…（）「」、。・")
    for f in glob.glob("src/**/*.cpp", recursive=True) + glob.glob("src/**/*.h", recursive=True) + ["assets/sky.txt"]:
        chars |= set(open(f, encoding="utf-8").read())
    chars = {c for c in chars if c.isprintable()}
    font = TTFont(fetch(work, URL, "NotoSansJP-wght.ttf"))
    font = instancer.instantiateVariableFont(font, {"wght": 400})
    opts = subset.Options()
    opts.layout_features = []
    opts.name_IDs = ["*"]
    opts.notdef_outline = True
    sub = subset.Subsetter(opts)
    sub.populate(text="".join(sorted(chars)))
    sub.subset(font)
    os.makedirs("web", exist_ok=True)
    font.save("web/NotoSansJP-subset.ttf")
    with open(fetch(work, LICENSE_URL, "OFL.txt"), encoding="utf-8") as src, open("web/OFL.txt", "w", encoding="utf-8") as dst:
        dst.write(src.read())
    print("chars", len(chars), "size", os.path.getsize("web/NotoSansJP-subset.ttf"))


if __name__ == "__main__":
    main()
