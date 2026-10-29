#!/usr/bin/env python3
"""天の川の光 (assets/milkyway.jpg) を作る。

データ: NASA/Goddard Space Flight Center Scientific Visualization Studio "Deep Star Maps 2020"
  (https://svs.gsfc.nasa.gov/4851/) の milkyway_2020_4k.exr
  — Hipparcos・Tycho の明るい星を除いた天の川の光（Gaia DR2: ESA/Gaia/DPAC）。赤道座標の正距円筒図法で、
  中央が赤経 0h、左へ行くほど赤経が増える。リニアな色の half float の OpenEXR（ZIP 圧縮）

出力:
  milkyway.jpg  2048x1024。並びは元の画像のまま（中央が赤経 0h、左へ赤経が増える、上が赤緯 +90°）
                明るさは 8 ビット（明るさ^(1/GAMMA)）。黒点 BLACK を 0、白点 WHITE を 255 にする（ゲーム側で薄く重ねる）

使い方: python3 tools/make_milkyway_asset.py <作業ディレクトリ> [出力先=assets]
必要なもの: Pillow, NumPy（ダウンロード 約 35 MB）
"""
import os
import struct
import sys
import urllib.request
import zlib

import numpy as np
from PIL import Image

UA = "SolarSystemVoyager-asset-build/1.0 (https://github.com/hubstacksea/Solar-System-Simulator)"
URL = "https://svs.gsfc.nasa.gov/vis/a000000/a004800/a004851/milkyway_2020_4k.exr"
OUT_W, OUT_H = 2048, 1024
WHITE = 99.5  # この百分位の明るさを 255 にする（銀河中心の一番明るい所だけ飽和させる）
GAMMA = 1.5   # 8 ビットにする時の階調（sRGB の 2.2 より小さくし、銀河中心と反対側の明るさの差を残す）
BLACK = 30    # この百分位の明るさを 0 にする（天の川から離れた所の淡い光を消して、背景を黒く保つ）


def fetch(work):
    path = os.path.join(work, os.path.basename(URL))
    if not os.path.exists(path):
        print("download", URL)
        req = urllib.request.Request(URL, headers={"User-Agent": UA})
        with urllib.request.urlopen(req, timeout=600) as r, open(path + ".part", "wb") as f:
            while True:
                b = r.read(1 << 20)
                if not b:
                    break
                f.write(b)
        os.rename(path + ".part", path)
    return path


def read_exr(path):
    """half float・ZIP 圧縮（16 行ずつ）・スキャンライン形式の OpenEXR を読む。戻り値は (高さ, 幅, チャンネル) と名前"""
    d = open(path, "rb").read()
    assert d[:4] == b"\x76\x2f\x31\x01", "OpenEXR ではありません"
    i, attrs = 8, {}
    while d[i] != 0:
        name = d[i:d.index(b"\0", i)].decode()
        i += len(name) + 1
        typ = d[i:d.index(b"\0", i)].decode()
        i += len(typ) + 1
        size = struct.unpack_from("<i", d, i)[0]
        attrs[name] = d[i + 4:i + 4 + size]
        i += 4 + size
    i += 1
    chans, k, cl = [], 0, attrs["channels"]
    while cl[k] != 0:
        n = cl[k:cl.index(b"\0", k)].decode()
        k += len(n) + 1
        assert struct.unpack_from("<i", cl, k)[0] == 1, "half float 以外のチャンネル"
        chans.append(n)
        k += 16
    assert attrs["compression"] == b"\x03", "ZIP 圧縮以外"
    x0, y0, x1, y1 = struct.unpack("<4i", attrs["dataWindow"])
    w, h = x1 - x0 + 1, y1 - y0 + 1
    nblk = (h + 15) // 16
    offsets = struct.unpack_from("<%dQ" % nblk, d, i)
    img = np.zeros((h, len(chans), w), np.float16)
    for off in offsets:
        y, size = struct.unpack_from("<2i", d, off)
        raw = np.frombuffer(zlib.decompress(d[off + 8:off + 8 + size]), np.uint8)
        t = (np.cumsum(np.concatenate(([raw[0]], raw[1:].astype(np.int64) - 128))) % 256).astype(np.uint8)  # 差分の予測を戻す
        half = (len(t) + 1) // 2
        b = np.empty_like(t)
        b[0::2], b[1::2] = t[:half], t[half:]
        rows = min(16, y1 + 1 - y)
        img[y - y0:y - y0 + rows] = b.view(np.float16).reshape(rows, len(chans), w)
    return img.transpose(0, 2, 1).astype(np.float32), chans


def main():
    work = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else "assets"
    os.makedirs(work, exist_ok=True)
    img, chans = read_exr(fetch(work))
    rgb = np.stack([img[:, :, chans.index(c)] for c in "RGB"], axis=2)
    h, w, _ = rgb.shape
    rgb = rgb.reshape(OUT_H, h // OUT_H, OUT_W, w // OUT_W, 3).mean(axis=(1, 3))   # 平均で縮める
    lum = rgb.max(axis=2)
    black, white = np.percentile(lum, BLACK), np.percentile(lum, WHITE)
    v = np.clip((rgb - black) / (white - black), 0, 1)
    enc = np.power(v, 1 / GAMMA)
    Image.fromarray((enc * 255 + 0.5).astype(np.uint8), "RGB").save(os.path.join(out, "milkyway.jpg"), quality=88,
                                                                    optimize=True)
    print("black %.4g, white %.4g" % (black, white))


if __name__ == "__main__":
    main()
