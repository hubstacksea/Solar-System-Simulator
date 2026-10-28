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
EmbeddedFile IconPng();
// 天体の音楽 (OGG Vorbis)。0〜6: ホルスト《惑星》の 水星, 金星, 火星, 木星, 土星, 天王星, 海王星
// 7: 太陽（グリーグ「朝」）, 8: 地球（ベートーヴェン「田園」第1楽章）, 9: 月（ドビュッシー「月の光」）
constexpr int MUSIC_COUNT = 10;
EmbeddedFile MusicOgg(int i);        // ウィンドウのアイコン (256px)
