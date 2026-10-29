// Solar System Voyager — Copyright (c) 2026 hubstacksea
// MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
// windows.h は raylib と名前がぶつかるので、OS 依存の処理はこのファイルに分ける
#include "platform_paths.h"

#include <cstdlib>
#include <cstring>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <direct.h>

std::string ScreenshotDir() {
    PWSTR wpath = nullptr;
    std::string dir;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Pictures, 0, nullptr, &wpath))) {
        // raylib のファイル関数は UTF-8 で受け取る
        int n = WideCharToMultiByte(CP_UTF8, 0, wpath, -1, nullptr, 0, nullptr, nullptr);
        if (n > 1) {
            dir.resize(n - 1);
            WideCharToMultiByte(CP_UTF8, 0, wpath, -1, &dir[0], n, nullptr, nullptr);
        }
    }
    CoTaskMemFree(wpath);
    if (dir.empty()) return "";
    dir += "\\SolarSystemVoyager";
    int wn = MultiByteToWideChar(CP_UTF8, 0, dir.c_str(), -1, nullptr, 0);
    std::wstring wdir(wn, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, dir.c_str(), -1, &wdir[0], wn);
    CreateDirectoryW(wdir.c_str(), nullptr);
    DWORD attr = GetFileAttributesW(wdir.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) return "";
    return dir + "\\";
}

#else
#include <sys/stat.h>

// ~/.config/user-dirs.dirs の XDG_PICTURES_DIR（日本語環境では「ピクチャ」など）。無ければ ~/Pictures
static std::string PicturesDir(const std::string& home) {
    const char* xdg = getenv("XDG_CONFIG_HOME");
    std::string cfg = (xdg && *xdg) ? std::string(xdg) : home + "/.config";
    std::ifstream f(cfg + "/user-dirs.dirs");
    std::string line;
    while (std::getline(f, line)) {
        const char* key = "XDG_PICTURES_DIR=";
        if (line.compare(0, strlen(key), key) != 0) continue;
        std::string v = line.substr(strlen(key));
        if (v.size() >= 2 && v.front() == '"' && v.back() == '"') v = v.substr(1, v.size() - 2);
        if (v.compare(0, 5, "$HOME") == 0) v = home + v.substr(5);
        if (!v.empty() && v != home && v != home + "/") return v;
    }
    return home + "/Pictures";
}

std::string ScreenshotDir() {
    const char* home = getenv("HOME");
    if (!home || !*home) return "";
    std::string pics = PicturesDir(home);
    mkdir(pics.c_str(), 0755);
    std::string dir = pics + "/SolarSystemVoyager";
    mkdir(dir.c_str(), 0755);
    struct stat st;
    if (stat(dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) return "";
    return dir + "/";
}
#endif
