#!/usr/bin/env python3
# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
"""天体の音楽 (assets/music/*.ogg) を作る。

ホルスト《惑星》（いずれもパブリックドメイン。Wikimedia Commons より）:
  - 水星: United States Air Force Band (Concert Band), 編曲 Master Sgt. John Romano,
          アルバム "Air Force Blue" (2013)。米国政府の著作物
  - ほか 6 曲: London Symphony Orchestra, 指揮 Gustav Holst, Columbia Records (1922-23 録音)。
          作曲・録音とも保護期間満了

太陽・地球・月（Wikimedia Commons より）:
  - 太陽: グリーグ《ペール・ギュント》第1組曲「朝」 Musopen Symphony (2012)。録音は Musopen がパブリックドメインで公開
  - 地球: ベートーヴェン 交響曲第6番「田園」第1楽章 Skidmore College Orchestra (Musopen)。録音はパブリックドメイン
  - 月:   ドビュッシー「月の光」 ピアノ: Laurens Goedhart (2011)。録音は CC BY 3.0（表記が必要）

出力（OGG Vorbis）:
  holst_mercury.ogg                     ステレオ 44.1 kHz
  holst_{mars,venus,jupiter,saturn,uranus,neptune}.ogg   モノラル 22.05 kHz（SP 盤の音域に合わせる）
  grieg_morning.ogg, beethoven_pastoral1.ogg, debussy_clair_de_lune.ogg  ステレオ 44.1 kHz

使い方: python3 tools/make_music_assets.py <作業ディレクトリ> [出力先=assets/music]
必要なもの: ffmpeg（libvorbis）、ダウンロード 約 350 MB
"""
import os
import subprocess
import sys
import urllib.request

UA = "SolarSystemVoyager-asset-build/1.0 (https://github.com/hubstacksea/Solar-System-Simulator)"
BASE = "https://upload.wikimedia.org/wikipedia/commons/"
COLUMBIA = ("columbia_1922-23.flac", BASE + "5/5f/Holst_-_The_Planets_%28Columbia_1922-23%29.flac")
MERCURY = ("mercury_usaf_2013.mp3",
           BASE + "2/2d/Mercury%2C_the_Winged_Messenger_-_Concert_Band_-_United_States_Air_Force_Band.mp3")

# 太陽・地球・月: (出力名, 保存名, URL)
MODERN = [
    ("grieg_morning", "grieg_morning_musopen.flac",
     BASE + "c/c2/Grieg_-_Peer_Gynt_Suite_No._1%2C_Op._46_-_I._Morning_Mood_%28Musopen_Symphony%29.flac"),
    ("beethoven_pastoral1", "beethoven_pastoral1_musopen.ogg",
     BASE + "d/d5/Ludwig_van_Beethoven_-_symphony_no._6_in_f_major_%27pastoral%27%2C_op._68_-_i._allegro_non_troppo.ogg"),
    ("debussy_clair_de_lune", "clair_de_lune_goedhart.ogg",
     BASE + "b/be/Clair_de_lune_%28Claude_Debussy%29_Suite_bergamasque.ogg"),
]

# 1922-23 録音（全曲 1 ファイル）での各楽章の範囲 [秒]。楽章間の無音から求めた
MOVEMENTS = {
    "mars": (0.0, 372.6),
    "venus": (382.4, 866.0),
    "jupiter": (1101.2, 1524.7),
    "saturn": (1534.6, 1955.0),
    "uranus": (1965.5, 2332.7),
    "neptune": (2342.6, 2673.9),
}
# 雑音を抑え、楽章ごとの音量をそろえる
OLD_FILTER = "highpass=f=70,lowpass=f=7500,afftdn=nf=-30,loudnorm=I=-20:TP=-2:LRA=14"
NEW_FILTER = "loudnorm=I=-20:TP=-2:LRA=14"


def fetch(work, name_url):
    name, url = name_url
    path = os.path.join(work, name)
    if not os.path.exists(path):
        print("download", url)
        req = urllib.request.Request(url, headers={"User-Agent": UA})
        with urllib.request.urlopen(req) as r, open(path + ".part", "wb") as f:
            while True:
                b = r.read(1 << 20)
                if not b:
                    break
                f.write(b)
        os.rename(path + ".part", path)
    return path


def ffmpeg(args):
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y"] + args, check=True)


def main():
    work = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join("assets", "music")
    os.makedirs(work, exist_ok=True)
    os.makedirs(out, exist_ok=True)
    col = fetch(work, COLUMBIA)
    mer = fetch(work, MERCURY)

    for name, (a, b) in MOVEMENTS.items():
        dst = os.path.join(out, "holst_%s.ogg" % name)
        print("encode", dst)
        ffmpeg(["-ss", str(a), "-to", str(b), "-i", col, "-vn", "-af", OLD_FILTER + ",afade=t=in:d=0.5",
                "-ac", "1", "-ar", "22050", "-c:a", "libvorbis", "-q:a", "0", "-map_metadata", "-1", dst])
    dst = os.path.join(out, "holst_mercury.ogg")
    print("encode", dst)
    ffmpeg(["-i", mer, "-vn", "-af", NEW_FILTER, "-ac", "2", "-ar", "44100", "-c:a", "libvorbis", "-q:a", "0",
            "-map_metadata", "-1", dst])
    for name, local, url in MODERN:
        src = fetch(work, (local, url))
        dst = os.path.join(out, name + ".ogg")
        print("encode", dst)
        ffmpeg(["-i", src, "-vn", "-af", NEW_FILTER, "-ac", "2", "-ar", "44100", "-c:a", "libvorbis", "-q:a", "0",
                "-map_metadata", "-1", dst])
    print("done:", out)


if __name__ == "__main__":
    main()
