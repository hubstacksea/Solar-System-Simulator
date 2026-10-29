// Solar System Voyager — Copyright (c) 2026 hubstacksea
// MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
// ブラウザ版 (Emscripten): 起動に要るファイルは .data にまとめて配り（--preload-file）、起動時に読み込む。
// 大きなデータ（地図・形・月・雲・季節・小惑星）はページの assets/ に置き、起動してから裏で取得する。
// 読めないファイル（まだ届いていないもの・ブラウザ版に入れていない音楽）は空として返す
#include "embedded_assets.h"

#include <emscripten.h>

#include <cstdio>
#include <map>
#include <string>
#include <vector>

static EmbeddedFile Load(const char* name) {
    static std::map<std::string, std::vector<unsigned char>> cache;
    auto it = cache.find(name);
    if (it == cache.end() || it->second.empty()) {   // 読めなかったものは、届いているかもう一度見る
        std::vector<unsigned char> buf;
        if (FILE* f = std::fopen((std::string("/assets/") + name).c_str(), "rb")) {
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

// 後から取得するファイル（CMakeLists.txt の WEB_DEFERRED と同じ）
static const char* const DEFERRED[] = {"maps.bin", "shapes.bin", "moon_data.png", "moon_color.jpg", "earth_clouds.jpg",
                                       "asteroids.bin", "earth_months.jpg"};
static int g_pending = -1;

void StartDeferredAssetDownloads() {
    g_pending = (int)(sizeof DEFERRED / sizeof DEFERRED[0]);
    for (const char* name : DEFERRED)
        emscripten_async_wget((std::string("assets/") + name).c_str(), (std::string("/assets/") + name).c_str(),
                              [](const char*) { g_pending--; }, [](const char*) { g_pending--; });
}
int DeferredAssetsPending() { return g_pending; }

EmbeddedFile EarthColorJpg() { return Load("earth_color.jpg"); }
EmbeddedFile EarthDataPng() { return Load("earth_data.png"); }
EmbeddedFile EarthMonthsJpg() { return Load("earth_months.jpg"); }
EmbeddedFile EarthCloudsJpg() { return Load("earth_clouds.jpg"); }
EmbeddedFile MoonColorJpg() { return Load("moon_color.jpg"); }
EmbeddedFile MoonDataPng() { return Load("moon_data.png"); }
EmbeddedFile IconPng() { return Load("icon.png"); }
EmbeddedFile StarsBin() { return Load("stars.bin"); }
EmbeddedFile ProbesBin() { return Load("probes.bin"); }
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
