// assets/ のファイルをアセンブラの .incbin でそのまま埋め込む（gcc / MinGW 共通）
#include "embedded_assets.h"

// 読み取り専用セクション名と、元のセクションへ戻す方法は ELF と PE (Windows) で異なる
#ifdef _WIN32
#define RODATA_SECTION ".section .rdata,\"dr\"\n"
#define END_SECTION ".text\n"
#else
#define RODATA_SECTION ".section .rodata\n"
#define END_SECTION ".previous\n"
#endif

#define INCBIN(sym, file)                                   \
    __asm__(RODATA_SECTION                                  \
            ".global " #sym "\n"                            \
            ".balign 16\n" #sym ":\n"                       \
            ".incbin \"" ASSET_DIR "/" file "\"\n"          \
            ".global " #sym "_end\n" #sym "_end:\n"         \
            ".byte 0\n"                                     \
            END_SECTION);                                   \
    extern "C" const unsigned char sym[], sym##_end[];

INCBIN(g_earth_color_jpg, "earth_color.jpg")
INCBIN(g_earth_data_png, "earth_data.png")
INCBIN(g_moon_color_jpg, "moon_color.jpg")
INCBIN(g_moon_data_png, "moon_data.png")
INCBIN(g_icon_png, "icon.png")
INCBIN(g_stars_bin, "stars.bin")
INCBIN(g_sky_txt, "sky.txt")
INCBIN(g_probes_bin, "probes.bin")
INCBIN(g_milkyway_jpg, "milkyway.jpg")
INCBIN(g_shapes_bin, "shapes.bin")
INCBIN(g_maps_bin, "maps.bin")
INCBIN(g_music_mercury, "music/holst_mercury.ogg")
INCBIN(g_music_venus, "music/holst_venus.ogg")
INCBIN(g_music_mars, "music/holst_mars.ogg")
INCBIN(g_music_jupiter, "music/holst_jupiter.ogg")
INCBIN(g_music_saturn, "music/holst_saturn.ogg")
INCBIN(g_music_uranus, "music/holst_uranus.ogg")
INCBIN(g_music_neptune, "music/holst_neptune.ogg")
INCBIN(g_music_sun, "music/grieg_morning.ogg")
INCBIN(g_music_earth, "music/beethoven_pastoral1.ogg")
INCBIN(g_music_moon, "music/debussy_clair_de_lune.ogg")

EmbeddedFile EarthColorJpg() { return {g_earth_color_jpg, (size_t)(g_earth_color_jpg_end - g_earth_color_jpg)}; }
EmbeddedFile EarthDataPng() { return {g_earth_data_png, (size_t)(g_earth_data_png_end - g_earth_data_png)}; }
EmbeddedFile MoonColorJpg() { return {g_moon_color_jpg, (size_t)(g_moon_color_jpg_end - g_moon_color_jpg)}; }
EmbeddedFile MoonDataPng() { return {g_moon_data_png, (size_t)(g_moon_data_png_end - g_moon_data_png)}; }
EmbeddedFile IconPng() { return {g_icon_png, (size_t)(g_icon_png_end - g_icon_png)}; }
EmbeddedFile StarsBin() { return {g_stars_bin, (size_t)(g_stars_bin_end - g_stars_bin)}; }
EmbeddedFile ProbesBin() { return {g_probes_bin, (size_t)(g_probes_bin_end - g_probes_bin)}; }
EmbeddedFile MapsBin() { return {g_maps_bin, (size_t)(g_maps_bin_end - g_maps_bin)}; }
EmbeddedFile ShapesBin() { return {g_shapes_bin, (size_t)(g_shapes_bin_end - g_shapes_bin)}; }
EmbeddedFile MilkyWayJpg() { return {g_milkyway_jpg, (size_t)(g_milkyway_jpg_end - g_milkyway_jpg)}; }
EmbeddedFile SkyTxt() { return {g_sky_txt, (size_t)(g_sky_txt_end - g_sky_txt)}; }
EmbeddedFile MusicOgg(int i) {
    static const unsigned char* const B[MUSIC_COUNT] = {g_music_mercury, g_music_venus, g_music_mars, g_music_jupiter,
                                                        g_music_saturn, g_music_uranus, g_music_neptune,
                                                        g_music_sun, g_music_earth, g_music_moon};
    static const unsigned char* const E[MUSIC_COUNT] = {g_music_mercury_end, g_music_venus_end, g_music_mars_end, g_music_jupiter_end,
                                                        g_music_saturn_end, g_music_uranus_end, g_music_neptune_end,
                                                        g_music_sun_end, g_music_earth_end, g_music_moon_end};
    if (i < 0 || i >= MUSIC_COUNT) return {nullptr, 0};
    return {B[i], (size_t)(E[i] - B[i])};
}
