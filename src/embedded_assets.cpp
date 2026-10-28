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

EmbeddedFile EarthColorJpg() { return {g_earth_color_jpg, (size_t)(g_earth_color_jpg_end - g_earth_color_jpg)}; }
EmbeddedFile EarthDataPng() { return {g_earth_data_png, (size_t)(g_earth_data_png_end - g_earth_data_png)}; }
EmbeddedFile MoonColorJpg() { return {g_moon_color_jpg, (size_t)(g_moon_color_jpg_end - g_moon_color_jpg)}; }
EmbeddedFile MoonDataPng() { return {g_moon_data_png, (size_t)(g_moon_data_png_end - g_moon_data_png)}; }
EmbeddedFile IconPng() { return {g_icon_png, (size_t)(g_icon_png_end - g_icon_png)}; }
