// 実行ファイルに埋め込んだデータ（単体 exe のまま動かすため）
#pragma once
#include <cstddef>

struct EmbeddedFile {
    const unsigned char* data;
    size_t size;
};

EmbeddedFile EarthColorJpg();  // 地球の地表の色 (NASA Blue Marble)
EmbeddedFile EarthDataPng();   // R: 標高・水深, G: 夜景, B: 海陸マスク (NOAA ETOPO 2022 / NASA Black Marble)
EmbeddedFile MoonColorJpg();   // 月の地表の色 (NASA SVS / LROC)
EmbeddedFile MoonDataPng();    // 月の標高 16 bit: グレー=上位, アルファ=下位 (NASA SVS / LOLA)
EmbeddedFile IconPng();         // ウィンドウのアイコン (256px)
EmbeddedFile StarsBin();        // 恒星 (ヒッパルコス星表、8 等級まで)。形式は tools/make_sky_assets.py
EmbeddedFile ProbesBin();       // 探査機の軌跡 (JPL Horizons)。形式は tools/make_probe_assets.py
EmbeddedFile SkyTxt();          // 星座線・星座名・恒星の固有名。形式は tools/make_sky_assets.py
EmbeddedFile ShapesBin();       // 彗星・小惑星の形 (ISAS/JAXA, ESA/Rosetta/NAVCAM)。形式は tools/make_shape_assets.py
EmbeddedFile MilkyWayJpg();     // 天の川の光 (NASA SVS Deep Star Maps 2020)。形式は tools/make_milkyway_asset.py
// 天体の音楽 (OGG Vorbis)。0〜6: ホルスト《惑星》の 水星, 金星, 火星, 木星, 土星, 天王星, 海王星
// 7: 太陽（グリーグ「朝」）, 8: 地球（ベートーヴェン「田園」第1楽章）, 9: 月（ドビュッシー「月の光」）
constexpr int MUSIC_COUNT = 10;
EmbeddedFile MusicOgg(int i);
