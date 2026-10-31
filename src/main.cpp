// Solar System Voyager — Copyright (c) 2026 hubstacksea
// MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
// Solar System Voyager — 実スケール太陽系 3D シミュレータ
//
//  * 単位: 距離 km / 速度 km/s / 時間 s（天体暦は日）
//  * 位置はすべて double で保持し、描画時にカメラ相対へ変換してから float にする
//  * 天体は「スケール空間」で描画する: 天体ごとに距離 100 単位へ縮小し、専用の near/far と
//    深度クリアで描画する（遠い順）。これで 30m の自機から数十億 km まで破綻なく描ける
//  * 惑星の位置は PC の時刻 (UTC) から J2000 軌道要素 + ケプラー方程式で計算する
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <string>
#include <vector>
#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "embedded_assets.h"
#include "font_loader.h"
#include "platform_paths.h"
#include "shaders.h"
#include "ship_model.h"
#include "solar_data.h"
#include "sound.h"

#ifdef _WIN32
#define SIM_GLAPI __stdcall
#else
#define SIM_GLAPI
#endif
extern "C" void SIM_GLAPI glClear(unsigned int mask);  // 深度だけをクリアするため直接呼ぶ
extern "C" void SIM_GLAPI glDrawArrays(unsigned int mode, int first, int count);   // 点の描画（rlgl には無い）
static const unsigned int GL_POINTS_MODE = 0x0000;
static const unsigned int GL_DEPTH_BIT = 0x00000100;

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
// ブラウザ版: 設定はブラウザの localStorage に保存する。画面は縦 720 ピクセル未満なら拡大した解像度で描いて
// CSS で縮める（スマホでも HUD が収まる）。指で操作する端末かどうかも調べる
EM_JS_DEPS(solar_voyager, "$stringToNewUTF8");
EM_JS(char*, WebLoadConfig, (), {
    var s = "";
    try { s = localStorage.getItem("solar_voyager.cfg") || ""; } catch (e) {}
    return stringToNewUTF8(s);
});
EM_JS(void, WebSaveConfig, (const char* s), {
    try { localStorage.setItem("solar_voyager.cfg", UTF8ToString(s)); } catch (e) {}
});
EM_JS(int, WebCoarsePointer, (), { return (window.matchMedia && matchMedia("(pointer: coarse)").matches) ? 1 : 0; });
// 描画の画素数とページの大きさ (CSS px) の比。縦が 720 未満のページは縦 720 相当で描いて縮め、高解像度の画面
// (devicePixelRatio > 1) ではその倍率で細かく描く（縦 1440 画素まで）
EM_JS(double, WebPixelScale, (), {
    var h = window.innerHeight, base = Math.max(1, 720 / h);
    return Math.max(base, Math.min(window.devicePixelRatio || 1, 1440 / h));
});
// 縦 720 相当に広げる分を除いた、画面の倍率による分（2D の表示の倍率に使う）
EM_JS(double, WebDpiScale, (), {
    var h = window.innerHeight, base = Math.max(1, 720 / h);
    return Math.max(base, Math.min(window.devicePixelRatio || 1, 1440 / h)) / base;
});
EM_JS(int, WebViewW, (), {
    var h = window.innerHeight, base = Math.max(1, 720 / h);
    return Math.round(window.innerWidth * Math.max(base, Math.min(window.devicePixelRatio || 1, 1440 / h)));
});
EM_JS(int, WebViewH, (), {
    var h = window.innerHeight, base = Math.max(1, 720 / h);
    return Math.round(h * Math.max(base, Math.min(window.devicePixelRatio || 1, 1440 / h)));
});
#endif

// ---------------------------------------------------------------- 各部（src/app/。この順に読み込む）
#include "app/math.h"
#include "app/strings.h"
#include "app/bodies.h"
#include "app/view.h"
#include "app/sky.h"
#include "app/ship_settings.h"
#include "app/app_state.h"
#include "app/places.h"
#include "app/events.h"
#include "app/menus.h"
#include "app/flight.h"
#include "app/render3d.h"
#include "app/hud.h"

// ---------------------------------------------------------------- main
#if !defined(__EMSCRIPTEN__) && !defined(__APPLE__)
// raylib に組み込まれた GLFW の関数（ヘッダはビルドのインクルードパスにないので、使うものだけ宣言する）
extern "C" {
struct GLFWwindow;
int glfwInit(void);
void glfwTerminate(void);
void glfwDefaultWindowHints(void);
void glfwWindowHint(int hint, int value);
GLFWwindow* glfwCreateWindow(int w, int h, const char* title, void* monitor, void* share);
void glfwDestroyWindow(GLFWwindow* w);
}

// OpenGL 3.3 のウィンドウを作れるか（画面がない・ドライバが古いなどでは作れない）
static bool ProbeGlWindow(bool msaa) {
    if (!glfwInit()) return false;
    glfwDefaultWindowHints();
    glfwWindowHint(0x00020004, 0);                   // GLFW_VISIBLE
    glfwWindowHint(0x00022002, 3);                   // GLFW_CONTEXT_VERSION_MAJOR
    glfwWindowHint(0x00022003, 3);                   // GLFW_CONTEXT_VERSION_MINOR
    glfwWindowHint(0x00022008, 0x00032001);          // GLFW_OPENGL_PROFILE = GLFW_OPENGL_CORE_PROFILE
    if (msaa) glfwWindowHint(0x0002100D, 4);         // GLFW_SAMPLES
    GLFWwindow* w = glfwCreateWindow(64, 64, "", nullptr, nullptr);
    if (w) glfwDestroyWindow(w);
    glfwTerminate();
    return w != nullptr;
}
#endif

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IOLBF, 0);   // ログを 1 行ごとに書き出す（ファイルへ出している時に異常終了しても残るように）
    Options opt = ParseArgs(argc, argv);
    if (opt.checkAssets) {   // データの大きさを出し、足りないものがあれば 1 で終わる
        struct { const char* name; EmbeddedFile f; } files[] = {
            {"earth_color.jpg", EarthColorJpg()}, {"earth_data.png", EarthDataPng()}, {"earth_months.jpg", EarthMonthsJpg()},
            {"earth_clouds.jpg", EarthCloudsJpg()}, {"moon_color.jpg", MoonColorJpg()}, {"moon_data.png", MoonDataPng()},
            {"icon.png", IconPng()}, {"stars.bin", StarsBin()}, {"probes.bin", ProbesBin()}, {"sky.txt", SkyTxt()},
            {"shapes.bin", ShapesBin()}, {"maps.bin", MapsBin()}, {"asteroids.bin", AsteroidsBin()},
            {"saturn_rings.png", SaturnRingsPng()}, {"milkyway.jpg", MilkyWayJpg()}};
        int missing = 0;
        for (auto& e : files) {
            printf("%-18s %10zu\n", e.name, e.f.size);
            if (!e.f.data || !e.f.size) missing++;
        }
        int music = 0;
        for (int i = 0; i < MUSIC_COUNT; i++) music += MusicOgg(i).data ? 1 : 0;
        std::vector<Body> B = InitBodies();
        size_t shapes = 0;
        for (const Body& b : B) shapes += b.shape >= 0 ? 1 : 0;
        printf("music %d/%d, bodies %zu, bodies with shapes %zu, missing %d\n", music, MUSIC_COUNT, B.size(), shapes, missing);
        return missing ? 1 : 0;
    }
    if (opt.listEvents) {
        App tmp;
        tmp.B = InitBodies();
        double t = (std::time(nullptr) - J2000_UNIX) / 86400.0 + opt.hours / 24.0;
        UpdateBodies(tmp.B, t);
        for (const AstroEvent& ev : opt.eventsPage == 2 ? FindProbeEvents(tmp) : opt.eventsPage == 1 ? FindCometEvents(tmp) : FindEvents(tmp, t))
            printf("%s  %-34s %s\n", FmtDate(ev.t).c_str(), EventTitle(tmp, ev).c_str(), EventDetail(tmp, ev).c_str());
        return 0;
    }
    if (opt.dumpMoons) {
        // 描画座標（黄道系を並べ替えたもの）のまま、時刻 = 現在 + --hours
        std::vector<Body> B = InitBodies();
        double t = (std::time(nullptr) - J2000_UNIX) / 86400.0 + opt.hours / 24.0;
        UpdateBodies(B, t);
        printf("t %.6f\n", t);
        for (const Body& b : B)
            if (b.d.parent == 0) printf("P %s %.1f %.1f %.1f\n", b.d.en, b.pos.x, b.pos.y, b.pos.z);
        for (const Body& b : B)
            if (b.d.parent > 0) {
                V3 r = b.pos - B[b.d.parent].pos;
                printf("%s %.3f %.3f %.3f\n", b.d.en, r.x, r.y, r.z);
            }
        // 太陽系の外の恒星: 番号, 太陽から見た赤経・赤緯 deg, 距離 光年, 影響圏 AU
        for (size_t i = 0; i < B.size(); i++)
            if (B[i].d.star >= 0) {
                V3 p = B[i].pos;
                const double eps = 23.4392911 * DEG;
                double xe = p.x, ye = -p.z, ze = p.y;   // 黄道座標
                double y = ye * std::cos(eps) - ze * std::sin(eps), z = ye * std::sin(eps) + ze * std::cos(eps), r = Len(p);
                printf("S %zu %s %.7f %.7f %.5f %.0f\n", i, B[i].d.en, std::fmod(std::atan2(y, xe) / DEG + 360, 360), std::asin(z / r) / DEG,
                       r / LY_KM, B[i].soi / AU_KM);
            }
        return 0;
    }
    LoadSettings();
#if defined(__EMSCRIPTEN__)
    // ブラウザ版: 大きさはページに合わせて毎フレーム自分で変える（raylib の自動の変更は縮小表示に対応しない）
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(WebViewW(), WebViewH(), "Solar System Voyager");
    g_dpiScale = (float)WebDpiScale();
#else
    // アンチエイリアス (MSAA) 付きの OpenGL を作れない環境（仮想マシンなど）では、GLFW がウィンドウを作れないまま
    // raylib が落ちる。初期化の前に印を保存しておき、前回の起動が初期化中に終わっていたら MSAA なしで始める
    bool msaa = !opt.noMsaa && GetSetting("initfail") != "1";
    if (!opt.shot) {
        SetSetting("initfail", "1");
        SaveSettings();
    }
#if !defined(__APPLE__)
    // raylib は GLFW の初期化やウィンドウの作成に失敗しても OpenGL の関数を呼んで落ちるので、先に GLFW で
    // 見えないウィンドウを作って確かめる。MSAA 付きで作れなければ MSAA なしで、それでも作れなければメッセージを出して終わる
    {
        int ok = ProbeGlWindow(msaa);
        if (!ok && msaa) ok = ProbeGlWindow(msaa = false);
        if (!ok) {
            if (!opt.shot) {
                SetSetting("initfail", "");
                SaveSettings();
            }
            fprintf(stderr, "Solar System Voyager: could not create the window (a display and OpenGL 3.3 are required)\n");
            return 1;
        }
    }
#endif
    unsigned int flags = FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | (msaa ? FLAG_MSAA_4X_HINT : 0);
#if defined(__APPLE__)
    flags |= FLAG_WINDOW_HIGHDPI;   // Retina: 座標は今までどおり（ポイント単位）で、描画だけ細かくする
#endif
    SetConfigFlags(flags);
    InitWindow(1280, 720, "Solar System Voyager");
    if (!opt.shot) {
        SetSetting("initfail", "");
        SaveSettings();
    }
#endif
    // 画面（ディスプレイ）につながらない・OpenGL 3.3 が使えないなど。GLFW が初期化できないまま
    // ウィンドウの関数を呼ぶと落ちるので、すぐに終わる
    if (!IsWindowReady()) {
        fprintf(stderr, "Solar System Voyager: could not create the window (a display and OpenGL 3.3 are required)\n");
        return 1;
    }
#if !defined(__EMSCRIPTEN__) && !defined(__APPLE__)
    {   // 高解像度の画面: ウィンドウと 2D の表示を画面の表示倍率に合わせて大きくする（画面の 9 割まで）
        Vector2 dpi = GetWindowScaleDPI();
        g_dpiScale = std::max(1.0f, std::min(3.0f, dpi.y > 0 ? dpi.y : 1.0f));
        if (opt.dpi > 0) g_dpiScale = opt.dpi;
        if (g_dpiScale > 1.01f && (!opt.shot || opt.dpi > 0)) {
            int m = GetCurrentMonitor(), mw = GetMonitorWidth(m), mh = GetMonitorHeight(m);
            float k = std::min(g_dpiScale, std::min(mw * 0.9f / 1280, mh * 0.9f / 720));
            if (k > 1.01f) {
                int w = (int)(1280 * k), h = (int)(720 * k);
                Vector2 pos = GetMonitorPosition(m);
                SetWindowSize(w, h);
                SetWindowPosition((int)pos.x + (mw - w) / 2, (int)pos.y + (mh - h) / 2);
            }
        }
    }
#endif
    SetExitKey(KEY_NULL);
    {
        EmbeddedFile f = IconPng();
        Image icon = LoadImageFromMemory(".png", f.data, (int)f.size);
        SetWindowIcon(icon);
        UnloadImage(icon);
    }

    App app;
    app.B = InitBodies();
    app.sky = LoadSky();
    app.belt = LoadAsteroids();
    if (!SettingsLoaded() && !opt.shot) {   // 初めての起動（設定ファイルがない）: 小惑星などの粒は出さずに始める
        SetSetting("asteroids", "0");
        SaveSettings();
    }
    app.showAsteroids = GetSetting("asteroids") != "0";
    app.showMilkyWay = GetSetting("milkyway") != "0";
    if (std::string rm = GetSetting("resume_mode"); !rm.empty()) app.resumeMode = std::max(0, std::min(2, atoi(rm.c_str())));
    g_uiScaleIdx = std::max(0, std::min(2, atoi(GetSetting("textsize").c_str())));
    app.showOrbits = GetSetting("orbits") != "0";
    app.showMoonMarkers = GetSetting("markers") != "0";
    app.invertY = GetSetting("invert") == "1";
    app.physics = GetSetting("physics") == "1";

    // フォント
    std::string allText = "°²·×〜′″";
    for (int cp = 0x3041; cp <= 0x30FC; cp++) {   // 検索欄に打つひらがな・カタカナ
        int len = 0;
        const char* u = CodepointToUTF8(cp, &len);
        allText.append(u, len);
    }
    for (int i = 0; i < UI_COUNT; i++) allText += UI_JP[i];
    for (const Body& b : app.B) allText += b.d.jp;
    for (const Engine& e : ENGINES) allText += e.jp;
    for (const Constellation& c : app.sky.cons) allText += c.jp;
    for (const City& c : CITIES) allText += c.jp;
    for (const StarName& n : app.sky.names) allText += n.jp;
    for (const TourStop& st : TOUR) allText += st.jp;
    g_jpFont = fontload::LoadJapaneseFont(allText, 36, &g_font);
    if (!g_jpFont) g_font = GetFontDefault();
    // 言語: 起動オプション > 保存された設定 > 日本語（フォントがあれば）
    std::string lang = opt.lang ? opt.lang : GetSetting("lang");
    SetLanguage(lang != "en");
    // 画質: 起動オプション > 保存された設定 > 高
    std::string q = GetSetting("quality");
    if (!q.empty()) app.quality = std::max(0, std::min(2, atoi(q.c_str())));
    if (opt.quality >= 0) app.quality = std::min(2, opt.quality);
#if defined(__EMSCRIPTEN__)
    if (WebCoarsePointer()) {   // スマホ・タブレット: 画面のボタンを出し、画質の設定がなければ低にする
        app.touchUI = true;
        if (q.empty()) app.quality = 0;
    }
#endif
    app.skyMode = std::max(0, std::min(2, atoi(GetSetting("sky").c_str())));
    if (opt.sky >= 0) app.skyMode = std::min(2, opt.sky);
    // 効果音（自動スクリーンショットでは鳴らさない）
    if (!opt.shot) {
        sfx::Init(GetSetting("sound") == "0");
        sfx::MusicInit(GetSetting("music") == "0");
    }

    app.shader = LoadShaderFromMemory(GlslForPlatform(PLANET_VS).c_str(), GlslForPlatform(PLANET_FS).c_str());
    app.locLight = GetShaderLocation(app.shader, "lightDir");
    app.locLightObj = GetShaderLocation(app.shader, "lightDirObj");
    app.locC1 = GetShaderLocation(app.shader, "color1");
    app.locC2 = GetShaderLocation(app.shader, "color2");
    app.locAtm = GetShaderLocation(app.shader, "atmColor");
    app.locParams = GetShaderLocation(app.shader, "params");
    app.locRingInfo = GetShaderLocation(app.shader, "ringInfo");
    app.locDetail = GetShaderLocation(app.shader, "detail");
    app.locOcc = GetShaderLocation(app.shader, "occ");
    app.locOccN = GetShaderLocation(app.shader, "occN");
    app.locSunAng = GetShaderLocation(app.shader, "sunAng");
    app.locAtmBeta = GetShaderLocation(app.shader, "atmBeta");
    app.locAtmMie = GetShaderLocation(app.shader, "atmMie");
    app.locAtmOzone = GetShaderLocation(app.shader, "atmOzone");
    app.locAtmHaze = GetShaderLocation(app.shader, "atmHaze");
    app.locCamUp = GetShaderLocation(app.shader, "camUp");
    app.mat = LoadMaterialDefault();
    LoadEarthTextures(app);
    LoadMoonTextures(app);
    app.milkyWay = LoadEmbeddedTexture(MilkyWayJpg(), ".jpg");
    for (const RingSet& rs : RingSets()) {   // 環の不透明度の表と形
        app.ringTex.push_back(LoadRingTexture(rs));
        double R = app.B[rs.body].d.radius;
        app.ringMesh.push_back(GenRing((float)(rs.inner / R), (float)(rs.outer / R), 720));
    }
    app.shader.locs[SHADER_LOC_MAP_ROUGHNESS] = GetShaderLocation(app.shader, "texture3");   // DrawMesh が texture3 に割り当てる
    LoadBodyMaps(app);
    app.locMapInfo = GetShaderLocation(app.shader, "mapInfo");
    app.locMapInfo2 = GetShaderLocation(app.shader, "mapInfo2");
    app.skyShader = LoadShaderFromMemory(GlslForPlatform(SKY_VS).c_str(), GlslForPlatform(SKY_FS).c_str());
    app.beltShader = LoadShaderFromMemory(GlslForPlatform(BELT_VS).c_str(), GlslForPlatform(BELT_FS).c_str());
    app.locSkyUp = GetShaderLocation(app.skyShader, "up");
    app.locSkySun = GetShaderLocation(app.skyShader, "sunDir");
    app.locSkyBeta = GetShaderLocation(app.skyShader, "beta");
    app.locSkyMie = GetShaderLocation(app.skyShader, "mieTint");
    app.locSkyDensity = GetShaderLocation(app.skyShader, "density");
    app.locSkyOzone = GetShaderLocation(app.skyShader, "ozone");
    app.locSkySunVis = GetShaderLocation(app.skyShader, "sunVis");
    app.mat.shader = app.shader;
    app.sphere = GenUVSphere(128, 256);
    for (const Shape& sh : g_shapes) {
        double r = 1;
        for (const Body& b : app.B)
            if (b.shape >= 0 && g_shapes[b.shape].name == sh.name) r = b.d.radius;
        app.shapeMesh.push_back(ShapeMesh(sh, r));
    }
#if defined(__EMSCRIPTEN__)
    StartDeferredAssetDownloads();   // 地図・形・月などは、起動してから裏で読み込む
#endif
    app.shipShader = LoadShaderFromMemory(GlslForPlatform(SHIP_VS).c_str(), GlslForPlatform(SHIP_FS).c_str());
    app.locSunDir = GetShaderLocation(app.shipShader, "sunDir");
    app.locSunColor = GetShaderLocation(app.shipShader, "sunColor");
    app.locFillDir = GetShaderLocation(app.shipShader, "fillDir");
    app.locFillColor = GetShaderLocation(app.shipShader, "fillColor");
    app.locEngColor = GetShaderLocation(app.shipShader, "engineColor");
    app.locEngGlow = GetShaderLocation(app.shipShader, "engineGlow");
    app.locNavBlink = GetShaderLocation(app.shipShader, "navBlink");
    app.shipMat = LoadMaterialDefault();
    app.shipMat.shader = app.shipShader;
    app.shipMesh = GenShipMesh();
    for (int i = 0; i < PM_COUNT; i++) app.probeMesh[i] = GenProbeMesh(i);
    for (int i = 0; i < 260; i++)
        app.dust.push_back({GetRandomValue(-1200, 1200) / 10000.0f, GetRandomValue(-1200, 1200) / 10000.0f, GetRandomValue(-1200, 1200) / 10000.0f});

    // 初期状態: PC 時刻の天体配置、地球の昼側上空から地球を見る
    app.tDays = NowDaysSinceJ2000() + opt.hours / 24.0;
    for (int i = 0; i < TIME_SCALE_COUNT; i++)
        if (TIME_SCALES[i] <= opt.timeScale) app.timeIdx = i;
    UpdateBodies(app.B, app.tDays);
    {
        const Body& earth = app.B[B_EARTH];
        V3 toSun = Norm(-earth.pos);
        V3 side = Norm(Cross(toSun, Y_UP));
        V3 dir = Norm(toSun * 0.55 + side * 0.83 + Y_UP * 0.1);
        app.ship.ref = B_EARTH;
        app.ship.local = dir * (earth.d.radius + 14000);
        app.ship.q = QuatFromBasis(-dir + side * -0.25, dir);
    }
    if (opt.target >= 0 && opt.target < (int)app.B.size()) app.target = opt.target;
    if ((opt.warp || opt.land) && opt.warpAnimAt < 0) WarpTo(app, app.target, opt.land);
    if (opt.gotoTarget >= 0 && opt.gotoTarget < (int)app.B.size()) app.target = opt.gotoTarget;
    if (!std::isnan(opt.atLat)) PlaceAt(app, app.target, opt.atLat, opt.atLon, opt.atAlt);
    if (opt.face) FaceTarget(app);
    if (!std::isnan(opt.faceSunEl)) {
        V3 up = Norm(ShipWorld(app) - app.B[app.ship.ref].pos), sun = app.B[B_SUN].pos - ShipWorld(app);
        V3 h = Norm(sun - up * Dot(sun, up));
        app.ship.q = QuatFromBasis(h * std::cos(opt.faceSunEl * DEG) + up * std::sin(opt.faceSunEl * DEG), up);
    }
    {
        std::string share = opt.share;
#if defined(__EMSCRIPTEN__)
        char* q = WebQuery();
        if (share.empty()) share = q;
        free(q);
#endif
        if (!share.empty()) ApplyShareQuery(app, share);
        else if ((!opt.shot || opt.resume) && app.resumeMode > 0 && !GetSetting("resume").empty())   // 前回の続きから
            ApplyShareQuery(app, ResumeQuery(GetSetting("resume"), app.resumeMode == 2), UI_RESUMED);
    }
    if (opt.engine >= 0) app.ship.engine = std::min(opt.engine, ENGINE_COUNT - 1);
    app.cockpit = opt.cockpit;
    app.camYaw = opt.camYaw; app.camPitch = opt.camPitch; app.camDist = opt.camDist;
    app.autopilot = opt.autopilot;
    app.apAutoEngine = opt.autoEngine;
    app.showHelp = opt.help;
    app.photoMode = std::max(0, std::min(2, opt.photo));
    app.settingsOpen = opt.settings;
    if ((GetSetting("intro") != "1" && !opt.shot) || opt.intro) app.introPage = 0;   // 初回だけ案内を出す
    if (opt.touch) app.touchUI = true;
    if (app.physics || opt.physics) {   // 重力モードで始める時は、今いる天体のまわりの円軌道に乗せる
        app.physics = false;
        SetPhysics(app, true);
    }
    app.probeEvents = FindProbeEvents(app);
    app.cometEvents = FindCometEvents(app);
    app.eventsPage = std::max(0, std::min(2, opt.eventsPage));
    if (opt.showEvents) {
        OpenEventsPage(app);
        if (app.eventsPage == 0) app.eventSel = 5;
        app.eventsOpen = true;
    }
    if (opt.showDate) {
        app.dateDigits = DigitsOfTime(app.tDays).substr(0, 9);
        app.dateOpen = true;
    }
    if (!opt.shot) DisableCursor(); else app.mouseCaptured = false;
    if (opt.map) OpenMap(app);
    if (opt.mapStars) app.mapStars = true;
    app.showInfo = opt.info;
    if (opt.city >= 0 && opt.city < CITY_COUNT) GoToCity(app, opt.city, std::isnan(opt.cityEl) ? 35 : opt.cityEl);
    if (opt.frameLock) SetFrameLock(app, true, true);
    if (opt.search) {
        OpenSearch(app);
        app.searchText = opt.search;
        app.searchTab = std::max(0, std::min(SEARCH_TABS - 1, opt.searchTab));
    }

    int frame = 0;
    while (!WindowShouldClose()) {
        double dt = std::min((double)GetFrameTime(), 0.1);
        if (opt.shot) dt = 1.0 / 60.0;
        if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_Q)) break;

        HandleInput(app, dt);
        if (opt.warpAnimAt >= 0 && frame == (int)(opt.warpAnimAt * 60)) StartWarp(app, app.target, opt.land);
        if (opt.saveBookmark && frame == 1) SaveBookmark(app);
        if (opt.gotoEvent > 0 && frame == 60) {
            app.events = app.eventsPage == 0 ? FindEvents(app, app.tDays) : app.eventsPage == 1 ? app.cometEvents : app.probeEvents;
            if (opt.gotoEvent <= (int)app.events.size()) {
                const AstroEvent& ev = app.events[opt.gotoEvent - 1];
                StartWarp(app, EventBody(ev), false);
                app.warpEvent = opt.gotoEvent - 1;
            }
        }
        if (opt.probeView > 0 && frame > 180 && app.B[app.target].d.probe >= 0) {
            const Body& pb = app.B[app.target];
            V3 toSun = Norm(app.B[B_SUN].pos - pb.pos), side = Norm(Cross(toSun, Y_UP));
            V3 dir = Norm(toSun * 0.6 + side * 0.7 + Y_UP * 0.4);
            SetShipWorld(app, pb.pos + dir * (opt.probeView / 1000.0));
            app.ship.speed = 0;
            app.ship.q = QuatFromBasis(dir * -1.0 - Y_UP * (0.0025 / (opt.probeView / 1000.0)), Y_UP);  // 操縦席の高さの分だけ下を向く
            app.padZoom = true;
        }
        if (opt.tour > 0 && frame == 60) {
            StartTour(app);
            if (app.tourOn && opt.tour > 1 && opt.tour <= (int)app.tourEvents.size()) {
                app.warpT = -1;   // 1 つ目へのワープを取り消して指定の場面へ
                GoTourStop(app, opt.tour - 1);
            }
        }
        if (opt.gotoBookmark > 0 && frame == 60) {
            std::vector<Bookmark> bms = LoadBookmarks(app);
            if (opt.gotoBookmark <= (int)bms.size()) {
                StartWarp(app, bms[opt.gotoBookmark - 1].body, false);
                app.warpBookmark = opt.gotoBookmark - 1;
            }
        }
        UpdateWarp(app, dt);
        UpdateTour(app, dt);
        UpdateAutopilot(app, dt);
        if (opt.shot && frame * dt < opt.throttleSec) {
            const Engine& e = ENGINES[app.ship.engine];
            app.ship.speed = std::min(e.vmax / 1000.0, app.ship.speed + e.accel / 1000.0 * SimDt(app, dt));
            app.ship.thrusting = true;
        }
        double simDt = SimDt(app, dt);
        if (app.physics && !app.autopilot && app.warpT < 0) {
            // 重力の計算の刻みが上限を超えるほど速い時は、時間の進みを抑える
            double maxDt = PhysicsStep(app, ShipWorld(app)) * PHYS_MAX_STEPS;
            if (simDt > maxDt) {
                simDt = maxDt;
                if (app.toastTime <= 0) Toast(app, T(UI_PHYS_SLOW));
            }
        }
        app.tDays += (app.timeReverse ? -simDt : simDt) / 86400.0;   // 逆再生は天体の時刻だけ（自機はそのまま進む）
        int spinRef = app.ship.ref;
        double spin0 = app.B[spinRef].spin;
        UpdateBodies(app.B, app.tDays);
        CoRotateWithGround(app, spinRef, spin0);
        UpdateMeteors(app, dt, simDt);
        UpdateShip(app, simDt);
        sfx::Update(app.ship.engine, app.ship.thrusting && app.warpT < 0,
                    std::fabs(app.ship.speed) * 1000.0 / ENGINES[app.ship.engine].vmax);
        {
            int track;
            float vol;
            ChooseMusic(app, &track, &vol);
            if (sfx::MusicUpdate(track, vol, (float)dt)) Toast(app, T((UiId)(UI_MUSIC_0 + track)));
        }
        UpdateCamera(app);
        if (app.toastTime > 0) app.toastTime -= dt;

#if defined(__EMSCRIPTEN__)
        if (static int lastPending = -1; DeferredAssetsPending() != lastPending) {   // ファイルが届くたびに組み込む
            lastPending = DeferredAssetsPending();
            RefreshDeferredAssets(app);
        }
        {   // ページの大きさが変わったら描画の大きさも合わせる
            int w = WebViewW(), h = WebViewH();
            if (w != GetScreenWidth() || h != GetScreenHeight()) SetWindowSize(w, h);
            g_dpiScale = (float)WebDpiScale();
        }
#endif
        BeginDrawing();
        // 画質「低」: 星空と 3D を縮小バッファに描いて拡大する（HUD は等倍）
        bool lowRes = QUALITY_SCALE[app.quality] < 1.0f;
        float rk = QUALITY_SCALE[app.quality] * GetRenderWidth() / std::max(1, GetScreenWidth());
#if defined(__EMSCRIPTEN__)
        if (lowRes) rk /= g_dpiScale;   // ブラウザ版の画質「低」: 3D は画面の倍率を掛けない画素数で（2D の表示だけ細かく）
#endif
        if (lowRes) {
            int w = std::max(1, (int)(GetScreenWidth() * rk)), h = std::max(1, (int)(GetScreenHeight() * rk));
            if (app.lowRes.texture.width != w || app.lowRes.texture.height != h) {
                if (app.lowRes.id) UnloadRenderTexture(app.lowRes);
                app.lowRes = LoadRenderTexture(w, h);
                SetTextureFilter(app.lowRes.texture, TEXTURE_FILTER_BILINEAR);
            }
            BeginTextureMode(app.lowRes);
            // rlPushMatrix は rlgl 内部の変換行列を使い続けて毎フレーム縮小が重なるので、modelview に直接掛ける
            rlLoadIdentity();
            rlScalef(rk, rk, 1);
        }
        ClearBackground({1, 2, 6, 255});
        {
            Camera3D sky = {{0, 0, 0}, {0, 0, -1}, {0, 1, 0}, 60, CAMERA_PERSPECTIVE};
            BeginMode3D(sky);
            DrawMilkyWay(app);
            EndMode3D();
            if (lowRes) rlScalef(rk, rk, 1);   // EndMode3D で modelview が単位行列に戻るので、縮小を掛け直す
        }
        UpdateSkyParallax(app.sky, (app.cam.pos - app.B[0].pos) * (1 / PC_KM));
        DrawStars(app);
        DrawAsteroids(app);
        {
            Camera3D sky = {{0, 0, 0}, {0, 0, -1}, {0, 1, 0}, 60, CAMERA_PERSPECTIVE};
            BeginMode3D(sky);
            DrawSkyAtmosphere(app);   // 昼は星を隠す
            EndMode3D();
            if (lowRes) rlScalef(rk, rk, 1);
        }
        DrawSunGlowAndDots(app);  // 3D より先に描き、手前の天体や自機で隠れるようにする

        Camera3D dummy = {{0, 0, 0}, {0, 0, -1}, {0, 1, 0}, 60, CAMERA_PERSPECTIVE};
        BeginMode3D(dummy);
        if (app.showOrbits) DrawOrbits(app);
        DrawCometTails(app);
        DrawBodies(app);
        DrawProbes(app);
        if (app.photoMode < 2) DrawShipPass(app, dt);
        EndMode3D();
        if (lowRes) {
            EndTextureMode();
            // 半透明の描画でバッファのアルファが下がっているので、ブレンドせずにそのまま写す
            Texture2D t = app.lowRes.texture;
            rlDrawRenderBatchActive();
            rlDisableColorBlend();
            DrawTexturePro(t, {0, 0, (float)t.width, -(float)t.height}, {0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()},
                           {0, 0}, 0, WHITE);
            rlDrawRenderBatchActive();
            rlEnableColorBlend();
        }

        {   // 2D の表示: 文字の大きさの設定に合わせて拡大して描く
            Cam real = app.cam;
            float k = UiScale(real);
            app.cam = UiCam(real);
            g_uiK = k;
            rlPushMatrix();
            rlScalef(k, k, 1);
            DrawSkyLabels(app);
            if (app.warpT < 0 && app.photoMode == 0) DrawMarkers(app);
            DrawWarpEffect(app);
            DrawHUD(app);
            rlPopMatrix();
            g_uiK = 1;
            app.cam = real;
        }

        frame++;
        if (!opt.shot && frame % 1800 == 0) SaveResume(app);   // 30 秒ごと（ブラウザ版は閉じる時に保存できないので）
        if (opt.trace && frame % 60 == 0) {
            const Body& tb = app.B[app.target];
            int mt;
            float mv;
            ChooseMusic(app, &mt, &mv);
            printf("t=%3ds engine=%d speed=%.6g km/s dist=%s ap=%d phase=%s remain=%s music=%d vol=%.2f\n", frame / 60, app.ship.engine + 1, app.ship.speed,
                   FmtDist(SurfaceDist(tb, ShipWorld(app))).c_str(), app.autopilot ? 1 : 0,
                   app.autopilot ? UI_EN[app.apPhase] : "-", FmtDist(app.apRemain).c_str(), mt, mv);
            fflush(stdout);
        }
        bool quitAfterShot = opt.shot && frame == opt.frames;
        if (quitAfterShot) SaveScreenshot(opt.shot);
        if (IsKeyPressed(KEY_F12)) {
            std::time_t now = std::time(nullptr);
            char name[64];
            std::strftime(name, sizeof name, "screenshot_%Y%m%d_%H%M%S.png", std::localtime(&now));
            // ピクチャ/SolarSystemVoyager に保存。できなければカレントフォルダ
            std::string dir = ScreenshotDir();
            std::string msg;
            if (!dir.empty() && SaveScreenshot((dir + name).c_str())) msg = std::string(T(UI_SHOT_SAVED)) + "SolarSystemVoyager/" + name;
            else if (SaveScreenshot(name)) msg = std::string(T(UI_SHOT_SAVED_CWD)) + name;
            else msg = std::string(T(UI_SHOT_FAILED)) + name;
            Toast(app, msg.c_str());
        }
        EndDrawing();
        if (quitAfterShot) break;
    }

    if (!opt.shot) SaveResume(app);
    UnloadMesh(app.sphere);
    for (Mesh& m : app.ringMesh) UnloadMesh(m);
    for (Mesh& m : app.shapeMesh) UnloadMesh(m);
    UnloadMesh(app.shipMesh);
    for (Mesh& m : app.probeMesh) UnloadMesh(m);
    UnloadShader(app.shipShader);
    UnloadShader(app.shader);
    UnloadShader(app.skyShader);
    UnloadShader(app.beltShader);
    if (app.beltVao) rlUnloadVertexArray(app.beltVao);
    if (app.beltVbo) rlUnloadVertexBuffer(app.beltVbo);
    if (app.earthTex) { UnloadTexture(app.earthColor); UnloadTexture(app.earthData); }
    if (app.earthMonths.id) UnloadTexture(app.earthMonths);
    if (app.earthClouds.id) UnloadTexture(app.earthClouds);
    if (app.moonTex) { UnloadTexture(app.moonColor); UnloadTexture(app.moonData); }
    if (app.milkyWay.id) UnloadTexture(app.milkyWay);
    for (Texture2D& t : app.ringTex)
        if (t.id) UnloadTexture(t);
    for (App::BodyMap& m : app.maps) {
        UnloadTexture(m.color);
        UnloadTexture(m.mask);
        if (m.data.id) UnloadTexture(m.data);
    }
    if (app.lowRes.id) UnloadRenderTexture(app.lowRes);
    sfx::Shutdown();
    if (g_jpFont) UnloadFont(g_font);
    CloseWindow();
    return 0;
}
