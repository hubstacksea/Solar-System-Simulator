// Solar System Voyager — Copyright (c) 2026 hubstacksea
// MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
// 実行ファイルに埋め込んだデータ（単体 exe のまま動かすため）
#pragma once
#include <cstddef>

struct EmbeddedFile {
    const unsigned char* data;
    size_t size;
};

EmbeddedFile EarthColorJpg();  // 地球の地表の色 (NASA Blue Marble)
EmbeddedFile EarthDataPng();   // R: 標高・水深, G: 夜景, B: 海陸マスク (NOAA ETOPO 2022 / NASA Black Marble)
EmbeddedFile EarthMonthsJpg();  // 地球の 12 か月の地表の色 (NASA Blue Marble NG)。形式は tools/make_earth_season_assets.py
EmbeddedFile EarthCloudsJpg();  // 地球の雲 (NASA Blue Marble)
EmbeddedFile EarthDetailBin();  // 地球の陸の細かい標高 1 分角 (NOAA ETOPO 2022)。形式は tools/make_earth_detail.py
EmbeddedFile MoonDetailBin();   // 月の細かい標高 (NASA SVS / LOLA)。形式は tools/make_body_detail.py
EmbeddedFile MarsDetailBin();   // 火星の細かい標高 (NASA PDS MGS MOLA)。形式は tools/make_body_detail.py
EmbeddedFile MarsDetail2Bin();  // その後ろ半分の区画（GitHub のファイルの大きさの上限のため分けてある）
EmbeddedFile MarsPatchesBin();  // 地域（オリンポス山・タルシス三山）だけのもっと細かい標高（約 206 m）
EmbeddedFile EarthPatchesBin();  // 地域（エベレスト・富士山など）だけのもっと細かい標高（Copernicus DEM GLO-90、約 90 m）
// 月と火星の極の細かい標高（tools/make_polar_detail.py）。月は緯度 85° より極側（LOLA、1 画素 80 m）、火星は 80° より極側（MOLA、460 m）。
// ブラウザ版では、RequestAsset で取得を頼むまで空
EmbeddedFile MoonSouthBin();
EmbeddedFile MoonNorthBin();
EmbeddedFile MarsSouthBin();
EmbeddedFile MarsNorthBin();
EmbeddedFile MoonPatchesBin();  // 地域（アポロ 15 号・17 号の着陸地点など）だけのもっと細かい標高（SLDEM2015、約 60 m）
EmbeddedFile MoonColorJpg();   // 月の地表の色 (NASA SVS / LROC)
EmbeddedFile MoonDataPng();    // 月の標高 16 bit: グレー=上位, アルファ=下位 (NASA SVS / LOLA)
EmbeddedFile MoonRockPng();    // 月の岩の多さ (NASA PDS / LRO Diviner)。形式は tools/make_surface_props.py
EmbeddedFile MarsTiPng();      // 火星の熱慣性 (NASA PDS / MGS TES) と砂丘地帯 (USGS)。形式は tools/make_surface_props.py
EmbeddedFile PlaceNamesTxt();  // 地名 (Natural Earth の都市、IAU / USGS の惑星の地名)。形式は tools/make_place_names.py
EmbeddedFile IconPng();         // ウィンドウのアイコン (256px)
EmbeddedFile StarsBin();        // 恒星 (ヒッパルコス星表、8 等級まで)。形式は tools/make_sky_assets.py
EmbeddedFile ProbesBin();       // 探査機の軌跡 (JPL Horizons)。形式は tools/make_probe_assets.py
EmbeddedFile ApolloBin();       // アポロ計画の宇宙船の軌跡 (NASA の飛行記録から計算)。形式は tools/make_apollo_assets.py
EmbeddedFile LandersBin();      // 着陸機の降下の道とローバーの走行 (NASA PDS・NAIF・JPL Horizons)。形式は tools/make_lander_tracks.py
EmbeddedFile SkyTxt();          // 星座線・星座名・恒星の固有名。形式は tools/make_sky_assets.py
EmbeddedFile ShapesBin();       // 彗星・小惑星の形 (ISAS/JAXA, ESA/Rosetta/NAVCAM)。形式は tools/make_shape_assets.py
EmbeddedFile MapsBin();         // 惑星・衛星の地図 (USGS / NASA)。形式は tools/make_planet_maps.py
EmbeddedFile AsteroidsBin();    // 小惑星帯・トロヤ群・太陽系外縁天体の軌道要素 (JPL SBDB)。形式は tools/make_asteroid_assets.py
EmbeddedFile SaturnRingsPng();  // 土星の環の不透明度 (Voyager 2 PPS)。形式は tools/make_ring_assets.py
EmbeddedFile MilkyWayJpg();     // 天の川の光 (NASA SVS Deep Star Maps 2020)。形式は tools/make_milkyway_asset.py
// 天体の音楽 (OGG Vorbis)。0〜6: ホルスト《惑星》の 水星, 金星, 火星, 木星, 土星, 天王星, 海王星
// 7: 太陽（グリーグ「朝」）, 8: 地球（ベートーヴェン「田園」第1楽章）, 9: 月（ドビュッシー「月の光」）
constexpr int MUSIC_COUNT = 10;
EmbeddedFile MusicOgg(int i);
#if defined(__EMSCRIPTEN__)
// ブラウザ版: 起動に要らない大きなデータ（地図・形・月・雲など）を裏で取得し始める / まだ届いていない数（-1: 取得しない）
void StartDeferredAssetDownloads();
int DeferredAssetsPending();
#endif
// 近づいた時だけ要るデータ（極の細かい標高）の取得を頼む。ブラウザ版は 1 回目だけ取得を始め（届くまでは DeferredAssetsPending
// にも数える）、ほかの版は何もしない（初めから読める）
void RequestAsset(const char* name);
