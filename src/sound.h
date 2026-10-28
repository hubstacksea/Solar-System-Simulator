// 効果音: エンジン音とワープ音を音声ファイル無しでその場で合成する
#pragma once
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

#include "embedded_assets.h"
#include "raylib.h"

namespace sfx {

constexpr int RATE = 44100;
constexpr float TAU = 6.2831853f;

// エンジンごとの音色: 基本周波数 Hz、ノイズの割合（残りは倍音のうなり）、ノイズの明るさ
struct Voice { float f0, noise, bright; };
constexpr Voice VOICES[] = {
    {48, 0.35f, 0.10f},   // 自動車
    {40, 0.25f, 0.06f},   // 電車
    {65, 0.30f, 0.12f},   // 高速電車
    {85, 0.55f, 0.20f},   // 音速
    {120, 0.65f, 0.30f},  // ジェット機
    {32, 0.90f, 0.18f},   // ロケット
    {180, 0.15f, 0.05f},  // 光速
};

// 音声スレッドと共有するパラメータ（目標値。コールバック内でなめらかに追従する）
inline std::atomic<float> g_vol{0}, g_freq{48}, g_noise{0.3f}, g_bright{0.1f};

// raylib の音声スレッドから呼ばれる（float32 モノラル）
inline void EngineCallback(void* buffer, unsigned int frames) {
    static float vol = 0, freq = 48, noise = 0.3f, bright = 0.1f;
    static float ph1 = 0, ph2 = 0, lp = 0, lp2 = 0;
    static uint32_t rng = 12345;
    float* out = (float*)buffer;
    float tv = g_vol, tf = g_freq, tn = g_noise, tb = g_bright;
    for (unsigned int i = 0; i < frames; i++) {
        vol += (tv - vol) * 0.0004f;
        freq += (tf - freq) * 0.0002f;
        noise += (tn - noise) * 0.0005f;
        bright += (tb - bright) * 0.0005f;
        rng = rng * 1664525u + 1013904223u;
        float w = (float)(rng >> 8) * (2.0f / 16777216.0f) - 1.0f;
        lp += (w - lp) * bright;         // ノイズを低域に絞る（ゴーッという音）
        lp2 += (lp - lp2) * bright;
        ph1 += freq / RATE;
        ph2 += freq * 1.503f / RATE;     // わずかにずらした 5 度上でうなりを作る
        ph1 -= std::floor(ph1);
        ph2 -= std::floor(ph2);
        float tone = std::sin(TAU * ph1) * 0.6f + std::sin(TAU * ph1 * 2.0f) * 0.25f + std::sin(TAU * ph2) * 0.15f;
        out[i] = vol * (lp2 * 3.0f * noise + tone * (1.0f - noise) * 0.5f);
    }
}

struct State {
    bool ready = false, muted = false;
    AudioStream engine{};
    Sound warp{};
};
inline State g;

// ワープ音: 0.9 秒の上昇音 → 閃光の瞬間の衝撃音 → 0.8 秒の余韻（ワープ演出と同じ長さ）
inline std::vector<float> WarpSamples() {
    const float charge = 0.9f, arrive = 0.8f;
    int n = (int)((charge + arrive) * RATE);
    std::vector<float> s(n);
    uint32_t rng = 777;
    float ph = 0, lp = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / RATE;
        rng = rng * 1664525u + 1013904223u;
        float w = (float)(rng >> 8) * (2.0f / 16777216.0f) - 1.0f;
        float v = 0;
        if (t < charge) {
            float k = t / charge;
            float f = 90.0f + 900.0f * k * k;            // 音程が上がっていく
            ph += f / RATE;
            lp += (w - lp) * (0.02f + 0.3f * k);
            v = (std::sin(TAU * ph) * 0.35f + lp * 0.9f) * k * k;
        } else {
            float u = t - charge;
            lp += (w - lp) * 0.08f;
            float boom = std::exp(-u * 7.0f);
            ph += (45.0f + 60.0f * boom) / RATE;         // 低い衝撃音
            v = std::sin(TAU * ph) * boom * 0.9f + lp * std::exp(-u * 4.0f) * 0.9f;
        }
        s[i] = std::fmax(-1.0f, std::fmin(1.0f, v * 0.6f));
    }
    return s;
}
inline Sound MakeWarpSound() {
    std::vector<float> s = WarpSamples();
    Wave wave = {(unsigned int)s.size(), RATE, 32, 1, s.data()};
    return LoadSoundFromWave(wave);
}

inline void Init(bool muted) {
    g.muted = muted;
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return;   // 音声デバイスが無い環境では何もしない
    SetAudioStreamBufferSizeDefault(2048);
    g.engine = LoadAudioStream(RATE, 32, 1);
    SetAudioStreamCallback(g.engine, EngineCallback);
    PlayAudioStream(g.engine);
    g.warp = MakeWarpSound();
    g.ready = true;
}

// thrust: 加速・減速中か, speedFrac: 最高速度に対する割合
inline void Update(int engine, bool thrust, double speedFrac) {
    if (!g.ready) return;
    const Voice& v = VOICES[engine];
    float sf = (float)std::fmin(1.0, std::fmax(0.0, speedFrac));
    float vol = g.muted ? 0.0f : (thrust ? 0.30f : 0.0f) + (sf > 0.001f ? 0.05f + 0.10f * sf : 0.0f);
    g_vol = vol;
    g_freq = v.f0 * (1.0f + 1.2f * sf + (thrust ? 0.15f : 0.0f));
    g_noise = v.noise;
    g_bright = v.bright * (thrust ? 1.6f : 1.0f);
}

inline void Warp() {
    if (g.ready && !g.muted) PlaySound(g.warp);
}

inline void SetMuted(bool m) {
    g.muted = m;
    if (g.ready && m) StopSound(g.warp);
}

// ---------------------------------------------------------------- 音楽（ホルスト《惑星》ほか）
// 一番近い天体の曲を流し、距離で音量を変える。曲が変わる時は今の曲を絞ってから次の曲へ。
// 離れた曲は一時停止して、また近づいた時に続きから流す
struct MusicState {
    bool ready = false, off = false;
    Music m[MUSIC_COUNT]{};
    bool ok[MUSIC_COUNT]{};
    int cur = -1;      // 鳴らしている曲
    float gain = 0;    // 現在の音量（フェード中の値）
};
inline MusicState gm;

inline void MusicInit(bool off) {
    gm.off = off;
    if (!g.ready) return;
    for (int i = 0; i < MUSIC_COUNT; i++) {
        EmbeddedFile f = MusicOgg(i);
        gm.m[i] = LoadMusicStreamFromMemory(".ogg", f.data, (int)f.size);
        gm.ok[i] = IsMusicValid(gm.m[i]);
        if (gm.ok[i]) gm.m[i].looping = true;
    }
    gm.ready = true;
}

// want: 流したい曲（-1 で無音）, vol: 0..1。新しく曲を流し始めたら true
inline bool MusicUpdate(int want, float vol, float dt) {
    if (!gm.ready) return false;
    if (want >= 0 && !gm.ok[want]) want = -1;
    if (gm.off) vol = 0;
    bool started = false;
    if (want != gm.cur) {
        gm.gain -= dt / 1.5f;   // 1.5 秒で絞る
        if (gm.cur < 0 || gm.gain <= 0) {
            if (gm.cur >= 0) PauseMusicStream(gm.m[gm.cur]);
            gm.cur = want;
            gm.gain = 0;
            if (gm.cur >= 0) {
                if (IsMusicStreamPlaying(gm.m[gm.cur])) {
                } else if (GetMusicTimePlayed(gm.m[gm.cur]) > 0) {
                    ResumeMusicStream(gm.m[gm.cur]);
                } else {
                    PlayMusicStream(gm.m[gm.cur]);
                }
                started = vol > 0;
            }
        }
    } else if (gm.cur >= 0) {
        float d = vol - gm.gain;
        float step = dt / 2.0f;   // 2 秒で 0→1
        gm.gain += d > 0 ? std::fmin(d, step) : std::fmax(d, -step);
    }
    if (gm.cur >= 0) {
        gm.gain = std::fmax(0.0f, std::fmin(1.0f, gm.gain));
        SetMusicVolume(gm.m[gm.cur], gm.gain * 0.8f);
        UpdateMusicStream(gm.m[gm.cur]);
    }
    return started;
}

inline void MusicSetOff(bool off) { gm.off = off; }

inline void Shutdown() {
    if (gm.ready) {
        for (int i = 0; i < MUSIC_COUNT; i++)
            if (gm.ok[i]) UnloadMusicStream(gm.m[i]);
        gm.ready = false;
    }
    if (!g.ready) return;
    UnloadSound(g.warp);
    UnloadAudioStream(g.engine);
    CloseAudioDevice();
    g.ready = false;
}

}  // namespace sfx
