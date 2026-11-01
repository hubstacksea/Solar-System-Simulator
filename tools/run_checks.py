#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""リリース前の確認をまとめて実行する（ビルド済みの実行ファイルを使う。外部のライブラリは不要）。

  1. データ: --check-assets ですべての埋め込みデータを読めるか
  2. 日食: 9 件の日食（紀元前 763 年〜2950 年）が NASA の日食表（Espenak & Meeus, Five Millennium Catalog of
     Solar Eclipses）と、時刻 6 分・食の中心 6° 以内で合うか
  3. 歴史: イベント一覧の「歴史」のページの件数
  4. ワープ（画面が要る）: すべての天体へワープ演出を通してワープし、着いた後にターゲットが正面（2.5° 以内）にあり、
     1 秒間で機首が動かず、位置が数値として壊れていないか
  5. 地表すれすれの光速飛行（画面が要る）: 扁平な惑星の北緯 45° の高度 20 km から光速エンジンで飛び出し、
     まだ地表すれすれ（高度 30 km 以下）にいる間に、画面の下側の明るさが急に落ちる（表面が消える）フレームがないか。
     （上昇して地平線が画面から外れたことによる暗さは数えない）

使い方: python3 tools/run_checks.py [実行ファイル=build/solar_voyager] [--no-display] [--jobs N]
画面が要る確認は DISPLAY（Linux）が必要。画面のない環境では --no-display で飛ばす。
"""
import concurrent.futures
import os
import re
import struct
import subprocess
import sys
import tempfile
import time
import zlib

# NASA の日食表の値（世界時 = 表の力学時 − ΔT）: (探し始める年, 日付, 世界時の時:分, 緯度, 経度)
ECLIPSES = [
    (2027.5, "2027-08-02", "10:06", 26, 33),
    (2027.5, "2028-01-26", "15:07", 3, -52),
    (1599.9, "1600-07-10", "12:34", 38, -3),
    (999.9, "1000-04-07", "08:55", 19, 41),
    (999.9, "1000-09-30", "11:19", -11, 4),
    (0.9, "0001-06-10", "03:48", 44, 121),
    (-584.0, "-0584-05-28", "14:22", 38, -45),
    (-762.3, "-0762-06-15", "08:14", 39, 54),
    (2950.0, "2950-01-09", "13:57", -13, -28),
]
HISTORY_COUNT = 18
OBLATE = [4, 5, 6, 7, 8]   # 火星・木星・土星・天王星・海王星


def run(exe, args, timeout=180, env=None):
    e = dict(os.environ)
    if env:
        e.update(env)
    return subprocess.run([exe] + args, capture_output=True, text=True, timeout=timeout, env=e).stdout


def hours_until(year):
    now = (time.time() - 946728000) / 86400
    return ((year - 2000) * 365.25 - now) * 24


def check_assets(exe):
    out = run(exe, ["--check-assets"])
    m = re.search(r"bodies (\d+).*missing (\d+)", out)
    ok = bool(m) and m.group(2) == "0"
    return ok, out.strip().splitlines()[-1] if out.strip() else "no output"


def check_eclipses(exe):
    bad, notes = [], []
    for start, date, hm, lat, lon in ECLIPSES:
        out = run(exe, ["--list-events", "--hours", "%.3f" % hours_until(start)])
        line = next((l for l in out.splitlines() if l.startswith(date) and "solar eclipse " in l.lower() and "ground" not in l), None)
        if not line:
            bad.append("%s: not found" % date)
            continue
        t = line.split()[1]
        dt = (int(t[:2]) * 60 + int(t[3:5])) - (int(hm[:2]) * 60 + int(hm[3:5]))
        m = re.search(r"(\d+)°([NS]) (\d+)°([EW])", line)
        la = int(m.group(1)) * (1 if m.group(2) == "N" else -1)
        lo = int(m.group(3)) * (1 if m.group(4) == "E" else -1)
        dlon = (lo - lon + 180) % 360 - 180
        notes.append("%s %+d min, %+d°/%+d°" % (date, dt, la - lat, dlon))
        if abs(dt) > 6 or abs(la - lat) > 6 or abs(dlon) > 6:
            bad.append(notes[-1])
    return not bad, "; ".join(bad) if bad else "all within 6 min / 6° (" + ", ".join(notes) + ")"


def check_history(exe):
    out = run(exe, ["--list-events", "--events-page", "3"])
    n = len([l for l in out.splitlines() if re.match(r"^-?\d{4}-\d\d-\d\d ", l)])
    return n == HISTORY_COUNT, "%d events" % n


def body_count(exe):
    out = run(exe, ["--check-assets"])
    m = re.search(r"bodies (\d+)", out)
    return int(m.group(1)) if m else 0


def warp_one(exe, t, tmp):
    out = run(exe, ["--shot", os.path.join(tmp, "w%d.png" % t), "--frames", "400", "--target", str(t),
                    "--warp-anim", "0.5", "--trace"], timeout=240)
    line = next((l for l in out.splitlines() if l.startswith("CHECK ")), None)
    if not line:
        return t, False, "no CHECK line"
    f = dict(kv.split("=", 1) for kv in line[6:].split("|")[2:] if "=" in kv)
    ok = f.get("warp") == "-1.00" and f.get("finite") == "1" and float(f.get("ang", 99)) <= 2.5 \
        and float(f.get("drift", 99)) <= 0.05
    return t, ok, line[6:]


def check_warps(exe, jobs):
    n = body_count(exe)
    bad = []
    with tempfile.TemporaryDirectory() as tmp, concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        for t, ok, info in ex.map(lambda t: warp_one(exe, t, tmp), range(n)):
            if not ok:
                bad.append(info)
    return not bad, ("%d bodies OK" % n) if not bad else "; ".join(bad[:5])


def png_lower_mean(path):
    """PNG（8 ビット RGB/RGBA）の下側（高さ 62〜89%）の明るさの平均（外部ライブラリなしで読む）"""
    data = open(path, "rb").read()
    pos, w, h, ctype, idat = 8, 0, 0, 0, b""
    while pos < len(data):
        ln = struct.unpack(">I", data[pos:pos + 4])[0]
        typ, body = data[pos + 4:pos + 8], data[pos + 8:pos + 8 + ln]
        if typ == b"IHDR":
            w, h, _, ctype = struct.unpack(">IIBB", body[:10])
        elif typ == b"IDAT":
            idat += body
        pos += 12 + ln
    bpp = 4 if ctype == 6 else 3
    raw, stride = zlib.decompress(idat), w * bpp
    prev, rows, i = bytearray(stride), [], 0
    for y in range(h):
        ft, line = raw[i], bytearray(raw[i + 1:i + 1 + stride])
        i += 1 + stride
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b, c = prev[x], prev[x - bpp] if x >= bpp else 0
            if ft == 1: line[x] = (line[x] + a) & 255
            elif ft == 2: line[x] = (line[x] + b) & 255
            elif ft == 3: line[x] = (line[x] + (a + b) // 2) & 255
            elif ft == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(line)
        prev = line
    sel = rows[int(h * 0.62):int(h * 0.89)]
    tot = sum(sum(r[x] + r[x + 1] + r[x + 2] for x in range(0, stride, bpp * 8)) for r in sel)
    return tot / (3 * len(sel) * (stride // (bpp * 8)))


def skim_one(exe, t, tmp):
    base = os.path.join(tmp, "s%d.png" % t)
    out = run(exe, ["--shot", base, "--shot-every", "6", "--frames", "120", "--target", str(t), "--at", "45", "0", "20",
                    "--engine", "6", "--throttle", "0.15", "--trace"], timeout=240)
    alt = {int(m.group(1)): float(m.group(2)) for m in re.finditer(r"FRAME (\d+) alt ([-\d.]+)", out)}
    files = sorted(f for f in os.listdir(tmp) if f.startswith("s%d_" % t))
    frames = [int(re.search(r"_(\d+)\.png$", f).group(1)) for f in files]
    v = [png_lower_mean(os.path.join(tmp, f)) for f in files]
    low = lambda i: alt.get(frames[i], 1e9) <= 30
    drops = [frames[i] for i in range(1, len(v)) if v[i] < v[i - 1] * 0.6 and v[i - 1] > 25 and low(i) and low(i - 1)]
    return t, not drops and len(v) > 5, "body %d: %d frames, drops at %s" % (t, len(v), drops)


def check_skim(exe, jobs):
    with tempfile.TemporaryDirectory() as tmp, concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        res = list(ex.map(lambda t: skim_one(exe, t, tmp), OBLATE))
    bad = [r[2] for r in res if not r[1]]
    return not bad, "; ".join(bad) if bad else "no surface loss on %d oblate planets" % len(res)


def main():
    args = sys.argv[1:]
    exe = next((a for a in args if not a.startswith("--") and not a.isdigit()), "build/solar_voyager")
    display = "--no-display" not in args
    jobs = int(args[args.index("--jobs") + 1]) if "--jobs" in args else 3
    checks = [("assets", check_assets), ("eclipses", check_eclipses), ("history", check_history)]
    if display:
        checks += [("warps", lambda e: check_warps(e, jobs)), ("skim", lambda e: check_skim(e, jobs))]
    failed = 0
    for name, fn in checks:
        t0 = time.time()
        try:
            ok, info = fn(exe)
        except Exception as ex:   # 実行ファイルが落ちた・時間切れなど
            ok, info = False, repr(ex)
        failed += not ok
        print("%-9s %s  (%.0f s)  %s" % (name, "PASS" if ok else "FAIL", time.time() - t0, info), flush=True)
    if not display:
        print("warps / skim skipped (--no-display)")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
