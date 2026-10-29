#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""アプリアイコンを生成する（Pillow のみ使用）。

出力: assets/icon.png (256px, ウィンドウ・Linux 用), assets/icon.ico (16〜256px, Windows exe 用)
使い方: python3 tools/make_icon.py
"""
import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter

S = 1024  # 作業解像度（最後に縮小してアンチエイリアス）
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(len(a)))


def background():
    """角丸の宇宙背景 + 星"""
    bg = Image.new("RGBA", (S, S))
    px = bg.load()
    c0, c1 = (8, 12, 40), (2, 3, 12)
    for y in range(S):
        for x in range(S):
            d = math.hypot(x - S * 0.3, y - S * 0.25) / (S * 1.1)
            px[x, y] = lerp(c0, c1, min(d, 1.0)) + (255,)
    d = ImageDraw.Draw(bg)
    rnd = random.Random(7)
    for _ in range(90):
        x, y = rnd.uniform(0, S), rnd.uniform(0, S)
        r = rnd.choice([2, 2, 3, 4, 5])
        v = rnd.randint(150, 255)
        d.ellipse([x - r, y - r, x + r, y + r], fill=(v, v, min(255, v + 20), 255))
    mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, S - 1, S - 1], radius=int(S * 0.2), fill=255)
    out = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    out.paste(bg, (0, 0), mask)
    return out, mask


def sun_glow(img, mask):
    """左上の太陽の光"""
    glow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    g = ImageDraw.Draw(glow)
    cx, cy = S * 0.16, S * 0.16
    for r, a in [(330, 40), (220, 70), (140, 120), (90, 200)]:
        g.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(255, 210, 120, a))
    glow = glow.filter(ImageFilter.GaussianBlur(60))
    g = ImageDraw.Draw(glow)
    g.ellipse([cx - 70, cy - 70, cx + 70, cy + 70], fill=(255, 250, 225, 255))
    glow = glow.filter(ImageFilter.GaussianBlur(6))
    clip = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    clip.paste(glow, (0, 0), mask)
    return Image.alpha_composite(img, clip)


def ring_layer(cx, cy, rx, ry, tilt, front):
    """楕円の環。front=True なら手前半分のみ"""
    layer = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    bands = [(1.00, (200, 180, 140, 230)), (0.93, (230, 210, 170, 240)), (0.86, (0, 0, 0, 0)),
             (0.82, (215, 195, 155, 235)), (0.70, (170, 150, 115, 200)), (0.62, (0, 0, 0, 0))]
    for k, col in bands:
        d.ellipse([cx - rx * k, cy - ry * k, cx + rx * k, cy + ry * k], fill=col)
    if front:
        half = Image.new("L", (S, S), 0)
        ImageDraw.Draw(half).rectangle([0, cy, S, S], fill=255)
        layer.putalpha(Image.composite(layer.getchannel("A"), Image.new("L", (S, S), 0), half))
    return layer.rotate(tilt, center=(cx, cy), resample=Image.BICUBIC)


def planet(cx, cy, r):
    """青い惑星（光は左上から）"""
    layer = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    px = layer.load()
    lx, ly, lz = -0.55, -0.6, 0.58
    n = math.sqrt(lx * lx + ly * ly + lz * lz)
    lx, ly, lz = lx / n, ly / n, lz / n
    for y in range(int(cy - r) - 1, int(cy + r) + 2):
        for x in range(int(cx - r) - 1, int(cx + r) + 2):
            u, v = (x - cx) / r, (y - cy) / r
            q = u * u + v * v
            if q > 1:
                continue
            w = math.sqrt(1 - q)
            diff = max(0.0, u * lx + v * ly + w * lz)
            # 雲の帯
            band = 0.5 + 0.5 * math.sin((v * 7.0 + 0.6 * math.sin(u * 5 + v * 3)) * 1.7)
            base = lerp((30, 90, 190), (90, 170, 235), band * 0.6)
            cloud = max(0.0, math.sin(u * 9 + v * 4) * math.sin(v * 11 - u * 2)) ** 3
            base = lerp(base, (240, 245, 255), cloud * 0.7)
            sh = 0.08 + 0.92 * diff ** 0.9
            rim = (1 - w) ** 3 * 0.6  # 大気のふち
            col = tuple(min(255, int(base[i] * sh + (120, 190, 255)[i] * rim)) for i in range(3))
            edge = min(1.0, (1 - math.sqrt(q)) * r / 1.5)
            px[x, y] = col + (int(255 * edge),)
    return layer


def ship(img):
    """右下を飛ぶ小さな宇宙船と噴射"""
    layer = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    ang = math.radians(-35)
    ox, oy = S * 0.74, S * 0.78

    def tr(px_, py_):
        return (ox + px_ * math.cos(ang) - py_ * math.sin(ang), oy + px_ * math.sin(ang) + py_ * math.cos(ang))

    flame = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    fd = ImageDraw.Draw(flame)
    fd.polygon([tr(-60, -22), tr(-250, 0), tr(-60, 22)], fill=(110, 190, 255, 230))
    fd.polygon([tr(-60, -11), tr(-160, 0), tr(-60, 11)], fill=(235, 250, 255, 255))
    flame = flame.filter(ImageFilter.GaussianBlur(10))
    d.polygon([tr(110, 0), tr(-60, -36), tr(-40, 0), tr(-60, 36)], fill=(235, 238, 245, 255))
    d.polygon([tr(110, 0), tr(-40, 0), tr(-60, 36)], fill=(160, 168, 185, 255))
    d.polygon([tr(70, -4), tr(30, -14), tr(30, 0)], fill=(90, 190, 255, 255))
    return Image.alpha_composite(Image.alpha_composite(img, flame), layer)


def main():
    img, mask = background()
    img = sun_glow(img, mask)
    cx, cy, r = S * 0.47, S * 0.5, S * 0.25
    tilt = 18
    back = ring_layer(cx, cy, r * 1.95, r * 0.5, tilt, False)
    img = Image.alpha_composite(img, back)
    img = Image.alpha_composite(img, planet(cx, cy, r))
    img = Image.alpha_composite(img, ring_layer(cx, cy, r * 1.95, r * 0.5, tilt, True))
    img = ship(img)
    # 角丸の外へはみ出した部分を消す
    a = Image.composite(img.getchannel("A"), Image.new("L", (S, S), 0), mask)
    img.putalpha(a)

    os.makedirs(os.path.join(ROOT, "assets"), exist_ok=True)
    big = img.resize((256, 256), Image.LANCZOS)
    big.save(os.path.join(ROOT, "assets", "icon.png"), optimize=True)
    big.save(os.path.join(ROOT, "assets", "icon.ico"),
             sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    print("wrote assets/icon.png, assets/icon.ico")


if __name__ == "__main__":
    main()
