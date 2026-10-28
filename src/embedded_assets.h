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
EmbeddedFile IconPng();        // ウィンドウのアイコン (256px)
