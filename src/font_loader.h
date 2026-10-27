// 日本語フォントの読み込み（OS 標準フォントを探す）。見つからなければ英語表示に切り替える。
// raylib は .ttc を直接読めないため、先頭フォントを単体 .ttf として切り出してから読む。
#pragma once
#include "raylib.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace fontload {

inline uint32_t be32(const unsigned char* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
inline uint16_t be16(const unsigned char* p) { return (uint16_t)(p[0] << 8 | p[1]); }
inline void put32(unsigned char* p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }

// TTC → TTF（index 0 のフォント）。TTC でなければそのまま返す。
inline std::vector<unsigned char> TtcToTtf(const unsigned char* d, size_t n) {
    if (n < 16 || std::memcmp(d, "ttcf", 4) != 0) return std::vector<unsigned char>(d, d + n);
    uint32_t off = be32(d + 12);
    if (off + 12 > n) return {};
    uint16_t numTables = be16(d + off + 4);
    size_t hdr = 12 + 16 * (size_t)numTables;
    if (off + hdr > n) return {};
    std::vector<unsigned char> out(d + off, d + off + hdr);
    for (uint16_t i = 0; i < numTables; i++) {
        const unsigned char* rec = d + off + 12 + 16 * i;
        uint32_t toff = be32(rec + 8), len = be32(rec + 12);
        if ((size_t)toff + len > n) return {};
        put32(out.data() + 12 + 16 * i + 8, (uint32_t)out.size());
        out.insert(out.end(), d + toff, d + toff + len);
        while (out.size() % 4) out.push_back(0);
    }
    return out;
}

inline std::vector<std::string> CandidatePaths() {
    std::vector<std::string> p = {"assets/font.ttf", "assets/font.otf", "assets/font.ttc"};
#ifdef _WIN32
    const char* win = std::getenv("WINDIR");
    std::string fonts = std::string(win ? win : "C:\\Windows") + "\\Fonts\\";
    for (const char* f : {"meiryo.ttc", "YuGothM.ttc", "YuGothR.ttc", "msgothic.ttc", "BIZ-UDGothicR.ttc"})
        p.push_back(fonts + f);
#else
    for (const char* f : {"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
                          "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
                          "/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc",
                          "/usr/share/fonts/opentype/noto/NotoSansCJKjp-Regular.otf",
                          "/usr/share/fonts/truetype/fonts-japanese-gothic.ttf",
                          "/usr/share/fonts/opentype/ipafont-gothic/ipagp.ttf",
                          "/usr/share/fonts/truetype/takao-gothic/TakaoPGothic.ttf",
                          "/usr/share/fonts/opentype/noto/NotoSerifCJK-Regular.ttc"})
        p.push_back(f);
#endif
    return p;
}

// 成功したら true（font に日本語フォントが入る）
inline bool LoadJapaneseFont(const std::string& allText, int fontSize, Font* font) {
    int cpCount = 0;
    int* cps = LoadCodepoints(allText.c_str(), &cpCount);
    std::vector<int> uniq;
    for (int c = 32; c < 127; c++) uniq.push_back(c);
    for (int i = 0; i < cpCount; i++) {
        bool found = false;
        for (int u : uniq) if (u == cps[i]) { found = true; break; }
        if (!found) uniq.push_back(cps[i]);
    }
    UnloadCodepoints(cps);

    for (const std::string& path : CandidatePaths()) {
        if (!FileExists(path.c_str())) continue;
        int size = 0;
        unsigned char* data = LoadFileData(path.c_str(), &size);
        if (!data) continue;
        std::vector<unsigned char> ttf = TtcToTtf(data, (size_t)size);
        UnloadFileData(data);
        if (ttf.empty()) continue;
        Font f = LoadFontFromMemory(".ttf", ttf.data(), (int)ttf.size(), fontSize, uniq.data(), (int)uniq.size());
        if (f.texture.id != 0 && f.glyphCount > 0 && f.glyphs[0].image.width >= 0) {
            SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
            *font = f;
            TraceLog(LOG_INFO, "JP font: %s", path.c_str());
            return true;
        }
    }
    return false;
}

}  // namespace fontload
