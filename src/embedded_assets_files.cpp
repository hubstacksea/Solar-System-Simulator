// Solar System Voyager — Copyright (c) 2026 hubstacksea
// MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
// データを実行ファイルに埋め込まず、ファイルとして読む版（ブラウザ版と macOS 版）。
// ブラウザ版 (Emscripten): 起動に要るファイルは .data にまとめて配り（--preload-file）、起動時に読み込む。
//   大きなデータ（地図・形・月・雲・季節・小惑星）はページの assets/ に置き、起動してから裏で取得する。
// macOS 版: .app の Contents/Resources/assets/ に置く（Apple Silicon 用と Intel 用で 1 回分を共有する）。
// 読めないファイル（まだ届いていないもの・ブラウザ版に入れていない音楽）は空として返す
#include "embedded_assets.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#else
#include "raylib.h"
#endif

#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

static std::string AssetDir() {
#if defined(__EMSCRIPTEN__)
    return "/assets/";
#else
    return std::string(GetApplicationDirectory()) + "../Resources/assets/";   // 実行ファイルは Contents/MacOS/
#endif
}

static EmbeddedFile Load(const char* name) {
    static std::map<std::string, std::vector<unsigned char>> cache;
    auto it = cache.find(name);
    if (it == cache.end() || it->second.empty()) {   // 読めなかったものは、届いているかもう一度見る
        std::vector<unsigned char> buf;
        if (FILE* f = std::fopen((AssetDir() + name).c_str(), "rb")) {
            std::fseek(f, 0, SEEK_END);
            long n = std::ftell(f);
            std::fseek(f, 0, SEEK_SET);
            if (n > 0) {
                buf.resize((size_t)n + 1);   // 末尾に 0 を 1 バイト（テキストとして読むもの用。埋め込み版と同じ）
                if (std::fread(buf.data(), 1, (size_t)n, f) != (size_t)n) buf.clear();
                else buf.back() = 0;
            }
            std::fclose(f);
        }
        it = cache.insert_or_assign(name, std::move(buf)).first;
    }
    if (it->second.empty()) return {nullptr, 0};
    return {it->second.data(), it->second.size() - 1};
}

#if defined(__EMSCRIPTEN__)
// 後から取得するファイル（CMakeLists.txt の WEB_DEFERRED と同じ）
static const char* const DEFERRED[] = {"maps.bin", "shapes.bin", "moon_data.png", "moon_color.jpg", "earth_clouds.jpg",
                                       "asteroids.bin", "earth_months.jpg", "earth_detail.bin", "earth_patches.bin",
                                       "moon_rock.png", "mars_ti.png", "place_names.txt"};
static int g_pending = -1, g_deferredLeft = 0, g_deferredFailed = 0;

// 届くまでの時間をログに出す（tools/web_smoke.py が読む）。起動時の分は、ページを開いてから全部が届く（か失敗する）まで
static void Arrived(bool ok) {
    g_pending--;
    if (!ok) g_deferredFailed++;
    if (--g_deferredLeft == 0)
        emscripten_log(EM_LOG_CONSOLE, "ASSETS: all %d deferred files arrived %.1f s after the page opened (%d failed)",
                       (int)(sizeof DEFERRED / sizeof DEFERRED[0]), emscripten_get_now() / 1000, g_deferredFailed);
}
void StartDeferredAssetDownloads() {
    g_pending = g_deferredLeft = (int)(sizeof DEFERRED / sizeof DEFERRED[0]);
    for (const char* name : DEFERRED)
        emscripten_async_wget((std::string("assets/") + name).c_str(), (std::string("/assets/") + name).c_str(),
                              [](const char*) { Arrived(true); }, [](const char*) { Arrived(false); });
}
int DeferredAssetsPending() { return g_pending; }

// 近づいた時だけ要るファイル（CMakeLists.txt の WEB_ON_DEMAND。月・火星の細かい標高）: 頼まれた時刻（ms）
static std::map<std::string, double> g_requested;
static void OnDemandArrived(const char* file) {
    g_pending--;
    std::string name = std::string(file).substr(8);   // "/assets/" の後
    emscripten_log(EM_LOG_CONSOLE, "ASSETS: %s arrived %.1f s after it was requested", name.c_str(),
                   (emscripten_get_now() - g_requested[name]) / 1000);
}
void RequestAsset(const char* name) {
    if (g_requested.count(name)) return;
    g_requested[name] = emscripten_get_now();
    g_pending = std::max(g_pending, 0) + 1;
    emscripten_async_wget((std::string("assets/") + name).c_str(), (std::string("/assets/") + name).c_str(), OnDemandArrived,
                          [](const char*) { g_pending--; });
}
#else
void RequestAsset(const char*) {}   // macOS 版: 初めから .app の中にある
#endif

EmbeddedFile EarthColorJpg() { return Load("earth_color.jpg"); }
EmbeddedFile EarthDataPng() { return Load("earth_data.png"); }
EmbeddedFile EarthMonthsJpg() { return Load("earth_months.jpg"); }
EmbeddedFile EarthCloudsJpg() { return Load("earth_clouds.jpg"); }
EmbeddedFile EarthDetailBin() { return Load("earth_detail.bin"); }
EmbeddedFile MoonDetailBin() { return Load("moon_detail.bin"); }
EmbeddedFile MarsDetailBin() { return Load("mars_detail.bin"); }
EmbeddedFile MarsDetail2Bin() { return Load("mars_detail2.bin"); }
EmbeddedFile MarsPatchesBin() { return Load("mars_patches.bin"); }
EmbeddedFile EarthPatchesBin() { return Load("earth_patches.bin"); }
EmbeddedFile MoonPatchesBin() { return Load("moon_patches.bin"); }
EmbeddedFile MoonSouthBin() { return Load("moon_south.bin"); }
EmbeddedFile MoonNorthBin() { return Load("moon_north.bin"); }
EmbeddedFile MarsSouthBin() { return Load("mars_south.bin"); }
EmbeddedFile MarsNorthBin() { return Load("mars_north.bin"); }
EmbeddedFile MoonColorJpg() { return Load("moon_color.jpg"); }
EmbeddedFile MoonDataPng() { return Load("moon_data.png"); }
EmbeddedFile MoonRockPng() { return Load("moon_rock.png"); }
EmbeddedFile MarsTiPng() { return Load("mars_ti.png"); }
EmbeddedFile PlaceNamesTxt() { return Load("place_names.txt"); }
EmbeddedFile IconPng() { return Load("icon.png"); }
EmbeddedFile StarsBin() { return Load("stars.bin"); }
EmbeddedFile ProbesBin() { return Load("probes.bin"); }
EmbeddedFile ApolloBin() { return Load("apollo.bin"); }
EmbeddedFile LandersBin() { return Load("landers.bin"); }
EmbeddedFile SkyTxt() { return Load("sky.txt"); }
EmbeddedFile MilkyWayJpg() { return Load("milkyway.jpg"); }
EmbeddedFile ShapesBin() { return Load("shapes.bin"); }
EmbeddedFile MapsBin() { return Load("maps.bin"); }
EmbeddedFile AsteroidsBin() { return Load("asteroids.bin"); }
EmbeddedFile SaturnRingsPng() { return Load("saturn_rings.png"); }
EmbeddedFile MusicOgg(int i) {
    static const char* const N[MUSIC_COUNT] = {"music/holst_mercury.ogg", "music/holst_venus.ogg", "music/holst_mars.ogg",
                                               "music/holst_jupiter.ogg", "music/holst_saturn.ogg", "music/holst_uranus.ogg",
                                               "music/holst_neptune.ogg", "music/grieg_morning.ogg",
                                               "music/beethoven_pastoral1.ogg", "music/debussy_clair_de_lune.ogg"};
    if (i < 0 || i >= MUSIC_COUNT) return {nullptr, 0};
    return Load(N[i]);
}
