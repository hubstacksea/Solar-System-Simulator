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
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
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
static const unsigned int GL_DEPTH_BIT = 0x00000100;

// ---------------------------------------------------------------- 定数
static const double AU_KM = 149597870.7;
static const double C_KMS = 299792.458;
static const double J2000_UNIX = 946728000.0;  // 2000-01-01 12:00:00 UTC
static const double DEG = PI / 180.0;

// ---------------------------------------------------------------- double ベクトル
struct V3 {
    double x = 0, y = 0, z = 0;
    V3() {}
    V3(double a, double b, double c) : x(a), y(b), z(c) {}
    V3 operator+(const V3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    V3 operator-(const V3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    V3 operator*(double s) const { return {x * s, y * s, z * s}; }
    V3 operator-() const { return {-x, -y, -z}; }
    V3& operator+=(const V3& o) { x += o.x; y += o.y; z += o.z; return *this; }
};
static inline double Dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline V3 Cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
static inline double Len(V3 a) { return std::sqrt(Dot(a, a)); }
static inline V3 Norm(V3 a) { double l = Len(a); return l > 0 ? a * (1.0 / l) : V3(0, 0, 0); }
static inline Vector3 ToF(V3 a) { return {(float)a.x, (float)a.y, (float)a.z}; }
static inline V3 FromF(Vector3 v) { return {v.x, v.y, v.z}; }
static const V3 Y_UP(0, 1, 0);  // 描画座標の +Y = 黄道北

// 黄道座標 (x, y, z: z=黄道北) → 描画座標 (Y-up)
static V3 EclToRender(double x, double y, double z) { return {x, z, -y}; }
static V3 EquToRender(double raDeg, double decDeg) {
    const double eps = 23.4392911 * DEG;
    double ra = raDeg * DEG, dec = decDeg * DEG;
    double x = std::cos(dec) * std::cos(ra), y = std::cos(dec) * std::sin(ra), z = std::sin(dec);
    return EclToRender(x, y * std::cos(eps) + z * std::sin(eps), -y * std::sin(eps) + z * std::cos(eps));
}

// ---------------------------------------------------------------- 表示文字列
#define UI_STRINGS(X)                                                                        \
    X(ENGINE, "エンジン", "Engine")                                                          \
    X(ACCEL, "加速", "Accel")                                                                \
    X(MAXSPD, "最高", "Max")                                                                 \
    X(SPEED, "速度", "Speed")                                                                \
    X(FRAME, "基準系", "Frame")                                                              \
    X(NEAREST, "最寄り", "Nearest")                                                          \
    X(ALT, "高度", "Alt")                                                                    \
    X(DATE, "日時", "Date")                                                                  \
    X(TSCALE, "時間倍率", "Time")                                                            \
    X(REALTIME, "実時間", "real-time")                                                       \
    X(PAUSED, "停止中", "PAUSED")                                                            \
    X(TARGET, "ターゲット", "Target")                                                        \
    X(DIST, "距離", "Dist")                                                                  \
    X(ETA, "到達予想", "ETA")                                                                \
    X(ETA_MAX, "最高速なら", "at max")                                                       \
    X(LIGHT, "光で", "light")                                                                \
    X(DAY, "日", "d")                                                                        \
    X(YEAR, "年", "y")                                                                       \
    X(HELP_HINT, "[H] 操作説明   [Esc] マウス解放/捕捉", "[H] Help   [Esc] Release/capture mouse") \
    X(VIEW_CHASE, "追尾視点", "Chase view")                                                  \
    X(VIEW_COCKPIT, "コクピット視点", "Cockpit view")                                        \
    X(WARP, "ワープ", "Warp")                                                                \
    X(LAND, "地表付近へワープ", "Surface warp")                                              \
    X(ORBITS_ON, "軌道表示 ON", "Orbits ON")                                                 \
    X(ORBITS_OFF, "軌道表示 OFF", "Orbits OFF")                                              \
    X(LABELS_ON, "衛星マーカー ON", "Moon markers ON")                                       \
    X(LABELS_OFF, "衛星マーカー OFF", "Moon markers OFF")                                    \
    X(INVERT_ON, "マウス上下反転 ON", "Invert mouse ON")                                     \
    X(INVERT_OFF, "マウス上下反転 OFF", "Invert mouse OFF")                                  \
    X(TIME_RESET, "現在時刻・等倍に同期", "Synced to current time")                          \
    X(HEADING, "方位", "Heading")                                                            \
    X(ECL_LON, "黄経", "Ecl.Lon")                                                            \
    X(ECL_LAT, "黄緯", "Ecl.Lat")                                                            \
    X(PITCH, "ピッチ", "Pitch")                                                              \
    X(ROLL, "ロール", "Roll")                                                                \
    X(N, "北", "N")                                                                          \
    X(E, "東", "E")                                                                          \
    X(S, "南", "S")                                                                          \
    X(W, "西", "W")                                                                          \
    X(REVERSE, "後退", "REV")                                                                \
    X(OVERSPEED, "減速中", "SLOWING")                                                        \
    X(HELP_TITLE, "操作説明", "Controls")                                                    \
    X(HELP1, "マウス / 矢印キー : 機首の上下・左右", "Mouse / Arrows : pitch & yaw")         \
    X(HELP2, "A / D : ヨー      Q / E : ロール", "A / D : yaw      Q / E : roll")            \
    X(HELP3, "W : 加速      S : 減速・後退      X : 停止", "W : accelerate   S : brake/reverse   X : stop") \
    X(HELP4, "1〜7 / マウスホイール : エンジン切替", "1-7 / Mouse wheel : select engine")    \
    X(HELP5, "Tab / Shift+Tab : ターゲット切替", "Tab / Shift+Tab : cycle target")           \
    X(HELP6, "F : 機首を向ける    T : 自動操縦    Shift+T : 自動操縦(エンジン自動選択)", "F : face target    T : autopilot    Shift+T : autopilot (auto engine)") \
    X(HELP7, "G : ターゲットへワープ    Shift+G : 地表付近へワープ", "G : warp to target    Shift+G : warp to surface") \
    X(HELP8, "右クリック(押している間) : ズーム    B : 現在地をブックマーク    N : ブックマークへワープ", "Right mouse (hold) : zoom    B : bookmark this spot    N : warp to next bookmark") \
    X(HELP9, "V : 視点切替    O : 軌道表示    L : 衛星マーカー", "V : view    O : orbits    L : moon markers") \
    X(HELP10, "[ / ] : 時間倍率    P : 時間停止    Backspace : 現在時刻に同期", "[ / ] : time scale    P : pause    Backspace : sync to now") \
    X(HELP11, "I : マウス上下反転    M : 効果音    Shift+M : 音楽    F11 : フルスクリーン    F12 : スクリーンショット", "I : invert mouse    M : sound    Shift+M : music    F11 : fullscreen    F12 : screenshot") \
    X(HELP12, "Esc : マウス解放/捕捉    J : 言語切替 (日本語/English)    F9 : 画質    Ctrl+Q : 終了", "Esc : release/capture mouse    J : language (日本語/English)    F9 : quality    Ctrl+Q : quit") \
    X(HELP13, "[パッド] 左スティック : 機首    右スティック左右 / LB・RB : ロール    RT / LT : 加速 / 減速", "[Pad] L-stick : pitch/yaw    R-stick X / LB・RB : roll    RT / LT : throttle / brake") \
    X(HELP14, "[パッド] 十字左右 : エンジン    十字上下 : 時間倍率    A : 自動操縦    B : 停止", "[Pad] D-pad L/R : engine    D-pad U/D : time scale    A : autopilot    B : stop") \
    X(HELP15, "[パッド] X : ターゲット    Y : ワープ    L押込 : 地表ワープ    R押込 : ズーム    View : 視点    Menu : 説明", "[Pad] X : target    Y : warp    L3 : surface warp    R3 : zoom    View : camera    Menu : help") \
    X(REAL_TIME_ETA, "実時間で", "real") \
    X(AP, "自動操縦", "AUTOPILOT") \
    X(AP_ON, "自動操縦 ON", "Autopilot ON") \
    X(AP_ON_AUTO, "自動操縦 ON (エンジン自動選択)", "Autopilot ON (auto engine)") \
    X(AP_OFF, "自動操縦 OFF", "Autopilot OFF") \
    X(AP_ARRIVED, "到着", "Arrived") \
    X(AP_AUTO_ENGINE, "エンジン自動", "auto engine") \
    X(AP_TURN, "旋回中", "turning") \
    X(AP_ACCEL, "加速中", "accelerating") \
    X(AP_CRUISE, "巡航中", "cruising") \
    X(AP_DECEL, "減速中", "decelerating") \
    X(AP_AVOID, "回避中", "avoiding") \
    X(AP_REMAIN, "残り", "remaining") \
    X(LANG_SET, "言語: 日本語", "Language: English") \
    X(BM_SAVED, "ブックマーク %d を保存: %s", "Bookmark %d saved: %s") \
    X(BM_GO, "ブックマーク %d へワープ: %s", "Warp to bookmark %d: %s") \
    X(BM_NONE, "ブックマークがありません（B キーで保存）", "No bookmarks (press B to save)") \
    X(MUSIC_ON, "音楽: オン", "Music: on") \
    X(MUSIC_OFF, "音楽: オフ", "Music: off") \
    X(MUSIC_0, "♪ ホルスト《惑星》より「水星、翼のある使者」 米空軍軍楽隊 (2013)", "Now playing: Holst, The Planets: Mercury, the Winged Messenger - U.S. Air Force Band (2013)") \
    X(MUSIC_1, "♪ ホルスト《惑星》より「金星、平和をもたらす者」 ホルスト指揮 ロンドン響 (1923)", "Now playing: Holst, The Planets: Venus, the Bringer of Peace - LSO, cond. Holst (1923)") \
    X(MUSIC_2, "♪ ホルスト《惑星》より「火星、戦争をもたらす者」 ホルスト指揮 ロンドン響 (1922)", "Now playing: Holst, The Planets: Mars, the Bringer of War - LSO, cond. Holst (1922)") \
    X(MUSIC_3, "♪ ホルスト《惑星》より「木星、快楽をもたらす者」 ホルスト指揮 ロンドン響 (1923)", "Now playing: Holst, The Planets: Jupiter, the Bringer of Jollity - LSO, cond. Holst (1923)") \
    X(MUSIC_4, "♪ ホルスト《惑星》より「土星、老いをもたらす者」 ホルスト指揮 ロンドン響 (1923)", "Now playing: Holst, The Planets: Saturn, the Bringer of Old Age - LSO, cond. Holst (1923)") \
    X(MUSIC_5, "♪ ホルスト《惑星》より「天王星、魔術師」 ホルスト指揮 ロンドン響 (1923)", "Now playing: Holst, The Planets: Uranus, the Magician - LSO, cond. Holst (1923)") \
    X(MUSIC_6, "♪ ホルスト《惑星》より「海王星、神秘主義者」 ホルスト指揮 ロンドン響 (1923)", "Now playing: Holst, The Planets: Neptune, the Mystic - LSO, cond. Holst (1923)") \
    X(MUSIC_7, "♪ グリーグ《ペール・ギュント》より「朝」 Musopen Symphony (2012)", "Now playing: Grieg, Peer Gynt: Morning Mood - Musopen Symphony (2012)") \
    X(MUSIC_8, "♪ ベートーヴェン 交響曲第6番「田園」第1楽章 Skidmore College Orchestra (Musopen)", "Now playing: Beethoven, Symphony No. 6 \"Pastoral\", I - Skidmore College Orchestra (Musopen)") \
    X(MUSIC_9, "♪ ドビュッシー「月の光」 ピアノ: Laurens Goedhart (2011, CC BY 3.0)", "Now playing: Debussy, Clair de lune - Laurens Goedhart, piano (2011, CC BY 3.0)") \
    X(SOUND_ON, "効果音: オン", "Sound: on") \
    X(SOUND_OFF, "効果音: オフ", "Sound: off") \
    X(SHOT_SAVED, "保存しました: ピクチャ/", "Saved: Pictures/") \
    X(SHOT_SAVED_CWD, "保存しました: ", "Saved: ") \
    X(SHOT_FAILED, "保存できませんでした: ", "Could not save: ") \
    X(QUALITY_0, "画質: 低（解像度 60%・細部を省略）", "Quality: Low (60% resolution, less detail)") \
    X(QUALITY_1, "画質: 中（細部を省略）", "Quality: Medium (less detail)") \
    X(QUALITY_2, "画質: 高", "Quality: High") \
    X(LANG_NO_FONT, "日本語フォントが見つかりません", "Japanese font not found") \
    X(PAD_ON, "コントローラ接続", "Controller connected") \
    X(PAD_OFF, "コントローラ切断", "Controller disconnected")

enum UiId {
#define X(id, jp, en) UI_##id,
    UI_STRINGS(X)
#undef X
    UI_COUNT
};
static const char* UI_JP[] = {
#define X(id, jp, en) jp,
    UI_STRINGS(X)
#undef X
};
static const char* UI_EN[] = {
#define X(id, jp, en) en,
    UI_STRINGS(X)
#undef X
};

static bool g_jp = false;       // 表示言語（true: 日本語）
static bool g_jpFont = false;   // 日本語フォントを読み込めたか
static Font g_font;
static inline const char* T(UiId id) { return g_jp ? UI_JP[id] : UI_EN[id]; }
static inline const char* TR(const char* jp, const char* en) { return g_jp ? jp : en; }

// ---------------------------------------------------------------- エンジン
struct Engine {
    const char* jp;
    const char* en;
    double accel;  // m/s^2
    double vmax;   // m/s
    int dialUnit;  // 0: km/h, 1: km/s, 2: c
    Color flame;
};
static const Engine ENGINES[] = {
    {"自動車", "Car", 3.0, 100.0 / 3.6, 0, {255, 160, 60, 255}},
    {"電車", "Train", 0.8, 130.0 / 3.6, 0, {255, 200, 90, 255}},
    {"高速電車", "Bullet train", 0.72, 320.0 / 3.6, 0, {255, 230, 120, 255}},
    {"音速", "Mach 1", 10.0, 343.0, 0, {200, 230, 255, 255}},
    {"ジェット機", "Jet", 15.0, 850.0, 0, {120, 180, 255, 255}},
    {"ロケットエンジン", "Rocket", 30.0, 50000.0, 1, {255, 120, 40, 255}},
    {"光速", "Light speed", 3.0e7, 299792458.0, 2, {180, 120, 255, 255}},
};
static const int ENGINE_COUNT = sizeof(ENGINES) / sizeof(ENGINES[0]);

// ---------------------------------------------------------------- 天体
struct Body {
    BodyDef d;
    V3 pos;               // 太陽中心・描画座標 km
    V3 pole, ex, ez;      // 赤道座標系 (ex, pole, ez) 右手系
    V3 oN, oQ;            // 衛星軌道面の基底
    double spin = 0;
    double soi = 1e300;   // 基準系切替に使う影響圏半径
    double outer = 0;     // 描画上の最大半径（環を含む）
    std::vector<V3> orbit;  // 母天体相対の軌道点
    double orbitT = -1e9;   // 軌道線を計算した時刻（月）
};

static V3 PlanetOrbitPoint(const BodyDef& d, double E) {
    double a = d.a_au * AU_KM, e = d.e;
    double w = (d.varpi - d.node) * DEG, O = d.node * DEG, I = d.inc * DEG;
    double xp = a * (std::cos(E) - e), yp = a * std::sqrt(1 - e * e) * std::sin(E);
    double cw = std::cos(w), sw = std::sin(w), cO = std::cos(O), sO = std::sin(O), cI = std::cos(I), sI = std::sin(I);
    double x = (cw * cO - sw * sO * cI) * xp + (-sw * cO - cw * sO * cI) * yp;
    double y = (cw * sO + sw * cO * cI) * xp + (-sw * sO + cw * cO * cI) * yp;
    double z = (sw * sI) * xp + (cw * sI) * yp;
    return EclToRender(x, y, z);
}

static V3 PlanetPos(const BodyDef& d, double tDays) {
    double n = 360.0 / (365.25 * std::pow(d.a_au, 1.5));  // 平均運動 deg/day
    double M = std::fmod(d.L0 + n * tDays - d.varpi, 360.0) * DEG;
    double E = M + d.e * std::sin(M);
    for (int i = 0; i < 8; i++) E -= (E - d.e * std::sin(E) - M) / (1 - d.e * std::cos(E));
    return PlanetOrbitPoint(d, E);
}

// 月の地心位置 km（J2000 黄道系→描画座標）。Astronomical Almanac の簡易式（精度 約 0.3°）
static V3 MoonGeoPos(double tDays) {
    double T = tDays / 36525.0;
    auto sd = [](double deg) { return std::sin(deg * DEG); };
    auto cd = [](double deg) { return std::cos(deg * DEG); };
    double lam = 218.32 + 481267.881 * T + 6.29 * sd(135.0 + 477198.87 * T) - 1.27 * sd(259.3 - 413335.36 * T) +
                 0.66 * sd(235.7 + 890534.22 * T) + 0.21 * sd(269.9 + 954397.74 * T) -
                 0.19 * sd(357.5 + 35999.05 * T) - 0.11 * sd(186.5 + 966404.03 * T);
    double bet = 5.13 * sd(93.3 + 483202.02 * T) + 0.28 * sd(228.2 + 960400.89 * T) -
                 0.28 * sd(318.3 + 6003.15 * T) - 0.17 * sd(217.6 - 407332.21 * T);
    double par = 0.9508 + 0.0518 * cd(135.0 + 477198.87 * T) + 0.0095 * cd(259.3 - 413335.36 * T) +
                 0.0078 * cd(235.7 + 890534.22 * T) + 0.0028 * cd(269.9 + 954397.74 * T);
    lam -= 1.396971 * T;  // 日付の分点 → J2000 分点（歳差）
    double r = 6378.137 / sd(par);
    return EclToRender(r * cd(bet) * cd(lam), r * cd(bet) * sd(lam), r * sd(bet));
}

static void EquatorBasis(V3 pole, V3* e1, V3* e2) {
    V3 a = Cross(Y_UP, pole);
    if (Len(a) < 1e-6) a = V3(1, 0, 0);
    *e1 = Norm(a);
    *e2 = Cross(*e1, pole);
}

static std::vector<Body> InitBodies() {
    std::vector<Body> B;
    for (const BodyDef& d : BuildSolarSystem()) {
        Body b;
        b.d = d;
        B.push_back(b);
    }
    for (size_t i = 0; i < B.size(); i++) {
        Body& b = B[i];
        const BodyDef& d = b.d;
        b.pole = d.inheritPole ? B[d.parent].pole : EquToRender(d.poleRA, d.poleDec);
        EquatorBasis(b.pole, &b.ex, &b.ez);
        b.outer = d.rings ? d.radius * 2.27 : d.radius;
        if (d.parent == 0) {
            b.soi = d.a_au * AU_KM * std::pow(d.mass / B[0].d.mass, 0.4);
            for (int k = 0; k <= 720; k++) b.orbit.push_back(PlanetOrbitPoint(d, 2 * PI * k / 720.0));
        } else if (d.parent > 0) {
            b.soi = d.moonA * std::pow(d.mass / B[d.parent].d.mass, 0.4);
            V3 ref = d.eclipticRef ? Y_UP : B[d.parent].pole;
            V3 e1, e2;
            EquatorBasis(ref, &e1, &e2);
            e2 = Cross(ref, e1);
            if (d.jpl) {
                // JPL の基準面（Laplace 面か母天体の赤道面）。昇交点経度は基準面と ICRF 赤道の昇交点から測る
                if (!std::isnan(d.framePoleRA)) ref = EquToRender(d.framePoleRA, d.framePoleDec);
                e1 = Norm(Cross(EquToRender(0, 90), ref));
                e2 = Cross(ref, e1);
            }
            V3 N = e1 * std::cos(d.moonNode * DEG) + e2 * std::sin(d.moonNode * DEG);
            V3 M = Cross(ref, N);
            b.oN = N;
            b.oQ = M * std::cos(d.moonIncl * DEG) + ref * std::sin(d.moonIncl * DEG);
            for (int k = 0; k <= 180; k++) {
                double th = 2 * PI * k / 180.0;
                b.orbit.push_back((b.oN * std::cos(th) + b.oQ * std::sin(th)) * d.moonA);
            }
        }
        if (d.parent >= 0) b.soi = std::max(b.soi, d.radius * 5);
    }
    return B;
}

static void UpdateBodies(std::vector<Body>& B, double t) {
    for (Body& b : B) {
        const BodyDef& d = b.d;
        if (d.parent < 0) {
            b.pos = V3();
        } else if (d.parent == 0) {
            b.pos = PlanetPos(d, t);
        } else if (&b == &B[B_MOON]) {
            b.pos = B[d.parent].pos + MoonGeoPos(t);
            // 軌道線は 1 日ごとに、今から 1 公転分の実際の経路で引き直す
            if (std::fabs(t - b.orbitT) > 1.0) {
                b.orbitT = t;
                for (size_t k = 0; k < b.orbit.size(); k++)
                    b.orbit[k] = MoonGeoPos(t + d.moonPeriod * k / (b.orbit.size() - 1));
            }
        } else {
            double th = d.moonPhase * DEG + 2 * PI * t / d.moonPeriod;
            if (d.libA != 0 || d.libB != 0) {   // ミマス・テティスの秤動（周期 70.56 年）
                double w = 2 * PI * t / (70.56 * 365.25);
                th += (d.libA * std::sin(w) + d.libB * std::cos(w)) * DEG;
            }
            b.pos = B[d.parent].pos + (b.oN * std::cos(th) + b.oQ * std::sin(th)) * d.moonA;
        }
        double rp = d.rotPeriod == 0 ? d.moonPeriod : d.rotPeriod;
        b.spin = std::fmod(d.spin0 * DEG + 2 * PI * t / rp, 2 * PI);
        if (d.parent > 0 && d.rotPeriod == 0) {
            // 衛星の自転は公転と同期: 経度 0°（本体 X 軸）を常に母天体へ向ける
            V3 u = B[d.parent].pos - b.pos;
            b.spin = std::atan2(-Dot(u, b.ez), Dot(u, b.ex));
        }
    }
}

// 楕円体表面までの半径（dir は単位ベクトル）
static double SurfaceRadius(const Body& b, V3 dir) {
    double a = b.d.radius, c = a * (1 - b.d.flattening);
    double y = Dot(dir, b.pole);
    return 1.0 / std::sqrt((1 - y * y) / (a * a) + y * y / (c * c));
}
// 点 p（ワールド座標）から楕円体表面までの距離（高度）
static double SurfaceDist(const Body& b, V3 p) {
    V3 rel = p - b.pos;
    double d = Len(rel);
    return d > 0 ? d - SurfaceRadius(b, rel * (1.0 / d)) : -b.d.radius;
}
static double MinAltitude(const Body& b) { return std::max(0.03, b.d.radius * 1e-5); }

static Color BodyColor(const Body& b, float bright = 1.0f) {
    const Col3& c = b.d.c1;
    auto f = [&](float v) { return (unsigned char)std::min(255.0f, (v * 0.7f + 0.3f) * 255 * bright); };
    return {f(c.r), f(c.g), f(c.b), 255};
}

// ---------------------------------------------------------------- メッシュ
static Mesh GenUVSphere(int rings, int slices) {
    Mesh m = {0};
    int vc = (rings + 1) * (slices + 1);
    m.vertexCount = vc;
    m.triangleCount = rings * slices * 2;
    m.vertices = (float*)MemAlloc(vc * 3 * sizeof(float));
    m.normals = (float*)MemAlloc(vc * 3 * sizeof(float));
    m.texcoords = (float*)MemAlloc(vc * 2 * sizeof(float));
    m.indices = (unsigned short*)MemAlloc(m.triangleCount * 3 * sizeof(unsigned short));
    int k = 0;
    for (int r = 0; r <= rings; r++) {
        double phi = PI * r / rings;
        for (int s = 0; s <= slices; s++) {
            double th = 2 * PI * s / slices;
            float x = (float)(std::sin(phi) * std::cos(th)), y = (float)std::cos(phi), z = (float)(std::sin(phi) * std::sin(th));
            m.vertices[k * 3] = m.normals[k * 3] = x;
            m.vertices[k * 3 + 1] = m.normals[k * 3 + 1] = y;
            m.vertices[k * 3 + 2] = m.normals[k * 3 + 2] = z;
            m.texcoords[k * 2] = (float)s / slices;
            m.texcoords[k * 2 + 1] = (float)r / rings;
            k++;
        }
    }
    int t = 0;
    for (int r = 0; r < rings; r++) {
        for (int s = 0; s < slices; s++) {
            unsigned short a = r * (slices + 1) + s, b = (r + 1) * (slices + 1) + s;
            m.indices[t++] = a; m.indices[t++] = b + 1; m.indices[t++] = b;
            m.indices[t++] = a; m.indices[t++] = a + 1; m.indices[t++] = b + 1;
        }
    }
    UploadMesh(&m, false);
    return m;
}

static Mesh GenRing(float inner, float outer, int segs) {
    Mesh m = {0};
    m.vertexCount = (segs + 1) * 2;
    m.triangleCount = segs * 2;
    m.vertices = (float*)MemAlloc(m.vertexCount * 3 * sizeof(float));
    m.normals = (float*)MemAlloc(m.vertexCount * 3 * sizeof(float));
    m.texcoords = (float*)MemAlloc(m.vertexCount * 2 * sizeof(float));
    m.indices = (unsigned short*)MemAlloc(m.triangleCount * 3 * sizeof(unsigned short));
    for (int i = 0; i <= segs; i++) {
        float th = 2 * PI * i / segs, c = std::cos(th), s = std::sin(th);
        float rr[2] = {inner, outer};
        for (int j = 0; j < 2; j++) {
            int v = i * 2 + j;
            m.vertices[v * 3] = c * rr[j]; m.vertices[v * 3 + 1] = 0; m.vertices[v * 3 + 2] = s * rr[j];
            m.normals[v * 3] = 0; m.normals[v * 3 + 1] = 1; m.normals[v * 3 + 2] = 0;
            m.texcoords[v * 2] = (float)j; m.texcoords[v * 2 + 1] = (float)i / segs;
        }
    }
    int t = 0;
    for (int i = 0; i < segs; i++) {
        unsigned short a = i * 2, b = a + 1, c = a + 2, d = a + 3;
        m.indices[t++] = a; m.indices[t++] = b; m.indices[t++] = d;
        m.indices[t++] = a; m.indices[t++] = d; m.indices[t++] = c;
    }
    UploadMesh(&m, false);
    return m;
}

// ---------------------------------------------------------------- カメラ・投影
struct Cam {
    V3 pos, fwd, up, right;
    double fovy = 60 * DEG;
    double fovBase = 60 * DEG;  // ズーム込みの視野角（fovy はこれにワープ演出を加えたもの）
    double focal = 1;  // ピクセル単位の焦点距離
    float W = 1280, H = 720;
};

// 画面座標へ投影。カメラ前方なら true
static bool Project(const Cam& c, V3 rel, Vector2* out) {
    double z = Dot(rel, c.fwd);
    if (z <= 1e-9) return false;
    out->x = (float)(c.W * 0.5 + Dot(rel, c.right) / z * c.focal);
    out->y = (float)(c.H * 0.5 - Dot(rel, c.up) / z * c.focal);
    return true;
}

static Matrix g_view;
static void SetPassProjection(const Cam& c, double n, double f) {
    rlDrawRenderBatchActive();
    rlMatrixMode(RL_PROJECTION);
    rlLoadIdentity();
    double top = n * std::tan(c.fovy * 0.5), right = top * c.W / c.H;
    rlFrustum(-right, right, -top, top, n, f);
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    rlMultMatrixf(MatrixToFloat(g_view));
}
static void ClearDepth() {
    rlDrawRenderBatchActive();
    glClear(GL_DEPTH_BIT);
}

static Quaternion QuatFromBasis(V3 fwd, V3 upHint) {
    V3 f = Norm(fwd);
    V3 r = Cross(f, upHint);
    if (Len(r) < 1e-6) r = Cross(f, V3(1, 0, 0));
    if (Len(r) < 1e-6) r = Cross(f, V3(0, 0, 1));
    r = Norm(r);
    V3 u = Cross(r, f);
    Matrix m = MatrixIdentity();
    m.m0 = (float)r.x; m.m1 = (float)r.y; m.m2 = (float)r.z;
    m.m4 = (float)u.x; m.m5 = (float)u.y; m.m6 = (float)u.z;
    m.m8 = (float)-f.x; m.m9 = (float)-f.y; m.m10 = (float)-f.z;
    return QuaternionNormalize(QuaternionFromMatrix(m));
}

// 天体のモデル行列: 平行移動 * 赤道基底 * 自転(Y軸) * 拡大
static Matrix BodyMatrix(const Body& b, V3 center, double sEq, double sPol, double spin) {
    double cw = std::cos(spin), sw = std::sin(spin);
    V3 X = (b.ex * cw - b.ez * sw) * sEq, Y = b.pole * sPol, Z = (b.ex * sw + b.ez * cw) * sEq;
    Matrix m = {0};
    m.m0 = (float)X.x; m.m1 = (float)X.y; m.m2 = (float)X.z;
    m.m4 = (float)Y.x; m.m5 = (float)Y.y; m.m6 = (float)Y.z;
    m.m8 = (float)Z.x; m.m9 = (float)Z.y; m.m10 = (float)Z.z;
    m.m12 = (float)center.x; m.m13 = (float)center.y; m.m14 = (float)center.z;
    m.m15 = 1;
    return m;
}

// ---------------------------------------------------------------- 書式
static void Txt(const char* s, float x, float y, float size, Color c) {
    float sp = g_jpFont ? 0.5f : size / 10;
    DrawTextEx(g_font, s, {x + 1, y + 1}, size, sp, {0, 0, 0, (unsigned char)(c.a * 0.8f)});
    DrawTextEx(g_font, s, {x, y}, size, sp, c);
}
static float TxtW(const char* s, float size) { return MeasureTextEx(g_font, s, size, g_jpFont ? 0.5f : size / 10).x; }
static void TxtC(const char* s, float cx, float y, float size, Color c) { Txt(s, cx - TxtW(s, size) / 2, y, size, c); }

static std::string Grouped(double v, int decimals) {
    char buf[64];
    snprintf(buf, sizeof buf, "%.*f", decimals, std::fabs(v));
    std::string s(buf), intPart = s.substr(0, s.find('.')), frac = s.find('.') == std::string::npos ? "" : s.substr(s.find('.'));
    std::string out;
    for (size_t i = 0; i < intPart.size(); i++) {
        if (i > 0 && (intPart.size() - i) % 3 == 0) out += ',';
        out += intPart[i];
    }
    return (v < 0 ? "-" : "") + out + frac;
}

static std::string FmtDist(double km) {
    char b[96];
    if (km < 1) snprintf(b, sizeof b, "%.0f m", km * 1000);
    else if (km < 1e7) snprintf(b, sizeof b, "%s km", Grouped(km, km < 100 ? 2 : 0).c_str());
    else snprintf(b, sizeof b, "%.3f AU", km / AU_KM);
    return b;
}

static std::string FmtSpeed(double kms) {
    char b[96];
    double a = std::fabs(kms);
    if (a < 1) snprintf(b, sizeof b, "%s km/h", Grouped(kms * 3600, 1).c_str());
    else snprintf(b, sizeof b, "%s km/s", Grouped(kms, a < 100 ? 3 : 0).c_str());
    std::string s = b;
    if (a > C_KMS * 0.001) {
        snprintf(b, sizeof b, "  (%.4f c)", kms / C_KMS);
        s += b;
    }
    return s;
}

static std::string FmtDuration(double sec) {
    char b[96];
    if (!std::isfinite(sec) || sec < 0 || sec > 1e13) return "---";
    if (sec < 60) snprintf(b, sizeof b, "%.1f s", sec);
    else if (sec < 86400) snprintf(b, sizeof b, "%02d:%02d:%02d", (int)(sec / 3600), (int)std::fmod(sec / 60, 60), (int)std::fmod(sec, 60));
    else if (sec < 365.25 * 86400 * 10) snprintf(b, sizeof b, "%d%s %02d:%02d", (int)(sec / 86400), T(UI_DAY), (int)std::fmod(sec / 3600, 24), (int)std::fmod(sec / 60, 60));
    else snprintf(b, sizeof b, "%s%s", Grouped(sec / (365.25 * 86400), 1).c_str(), T(UI_YEAR));
    return b;
}

static double NowDaysSinceJ2000() { return ((double)std::time(nullptr) - J2000_UNIX) / 86400.0; }

static std::string FmtDate(double tDays) {
    double unixSec = J2000_UNIX + tDays * 86400.0;
    if (unixSec < -62135596800.0 || unixSec > 253402300799.0) return "----";
    time_t tt = (time_t)std::floor(unixSec);
    struct tm* g = std::gmtime(&tt);
    if (!g) return "----";
    char b[64];
    snprintf(b, sizeof b, "%04d-%02d-%02d %02d:%02d:%02d UTC", g->tm_year + 1900, g->tm_mon + 1, g->tm_mday, g->tm_hour, g->tm_min, g->tm_sec);
    return b;
}

// ---------------------------------------------------------------- 背景の星
struct Star { V3 dir; Color col; float size; };
static std::vector<Star> MakeStars() {
    std::vector<Star> s;
    SetRandomSeed(12345);
    auto rnd = [] { return GetRandomValue(0, 1000000) / 1000000.0; };
    auto gauss = [&] { double u = std::max(rnd(), 1e-9), v = rnd(); return std::sqrt(-2 * std::log(u)) * std::cos(2 * PI * v); };
    V3 gp = EquToRender(192.85948, 27.12825), gc = EquToRender(266.405, -28.936);  // 銀河北極・銀河中心
    gc = Norm(gc - gp * Dot(gc, gp));
    V3 gc2 = Cross(gp, gc);
    for (int i = 0; i < 7000; i++) {
        V3 d;
        if (i % 2 == 0) {
            double z = rnd() * 2 - 1, a = rnd() * 2 * PI, r = std::sqrt(1 - z * z);
            d = V3(r * std::cos(a), z, r * std::sin(a));
        } else {
            double l = (i % 6 == 1) ? gauss() * 0.6 : rnd() * 2 * PI;
            double bb = gauss() * (i % 6 == 1 ? 0.06 : 0.12);
            d = (gc * std::cos(l) + gc2 * std::sin(l)) * std::cos(bb) + gp * std::sin(bb);
        }
        double m = std::pow(rnd(), 5.0);
        double tint = rnd();
        unsigned char a = (unsigned char)(50 + 205 * std::min(1.0, m * 3 + 0.15));
        Color c = tint < 0.15 ? Color{180, 200, 255, a} : tint < 0.3 ? Color{255, 225, 190, a} : Color{240, 240, 255, a};
        s.push_back({Norm(d), c, (float)(m > 0.3 ? 1.6 : m > 0.08 ? 1.1 : 0.0)});
    }
    return s;
}

// ---------------------------------------------------------------- 自機
struct Ship {
    int ref = B_EARTH;  // 基準系の天体（自機はこの天体と一緒に公転移動する）
    V3 local;           // 基準天体からの相対位置 km
    Quaternion q = QuaternionIdentity();
    double speed = 0;   // km/s（機首方向）
    int engine = 0;
    bool thrusting = false;
};
static V3 ShipFwd(const Ship& s) { return FromF(Vector3RotateByQuaternion({0, 0, -1}, s.q)); }
static V3 ShipUp(const Ship& s) { return FromF(Vector3RotateByQuaternion({0, 1, 0}, s.q)); }
static V3 ShipRight(const Ship& s) { return FromF(Vector3RotateByQuaternion({1, 0, 0}, s.q)); }

// 噴射炎の 1 層: 根元が明るく先端へ向かって消える円錐（頂点ごとに色を補間）
static void GlowCone(Vector3 o, float r0, float len, Color base) {
    const int segs = 24;
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < segs; i++) {
        float a0 = 2 * PI * i / segs, a1 = 2 * PI * (i + 1) / segs;
        rlColor4ub(base.r, base.g, base.b, base.a);
        rlVertex3f(o.x + cosf(a0) * r0, o.y + sinf(a0) * r0, o.z);
        rlVertex3f(o.x + cosf(a1) * r0, o.y + sinf(a1) * r0, o.z);
        rlColor4ub(base.r, base.g, base.b, 0);
        rlVertex3f(o.x, o.y, o.z + len);
    }
    rlEnd();
}

// 噴射炎: 加算合成のグラデーション円錐を重ねる（単位: m、ノズルは z=+14.4）
static void DrawExhaust(const Ship& s, double glow) {
    if (glow < 0.01) return;
    Color f = ENGINES[s.engine].flame;
    float t = (float)GetTime();
    BeginBlendMode(BLEND_ADDITIVE);
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    for (int sgn = -1; sgn <= 1; sgn += 2) {
        float flick = 1.0f + 0.07f * sinf(t * 43.0f + sgn) + 0.05f * sinf(t * 71.0f);
        float len = (float)(3.0 + 13.0 * glow) * flick;
        Vector3 o = {3.35f * sgn, -0.3f, 14.45f};
        auto a = [&](float k) { return (unsigned char)std::min(255.0, k * (0.3 + 0.7 * glow)); };
        GlowCone(o, 1.0f, len, {f.r, f.g, f.b, a(110)});
        GlowCone(o, 0.8f, len * 0.6f, {f.r, f.g, f.b, a(170)});
        GlowCone(o, 0.55f, len * 0.3f, {255, 245, 230, a(230)});
        // 衝撃波のリング（マッハダイヤモンド風）
        for (int k = 1; k <= 3; k++) {
            float z = len * 0.16f * k, r = 0.5f * (1.0f - 0.22f * k);
            GlowCone({o.x, o.y, o.z + z - 0.3f}, r, 0.9f, {255, 255, 255, a(90.0f / k)});
        }
    }
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
    EndBlendMode();
}

// 実際のフレームバッファサイズで保存する（raylib の TakeScreenshot は DPI スケール時に範囲がずれる）
static bool SaveScreenshot(const char* path) {
    rlDrawRenderBatchActive();
    int w = GetRenderWidth(), h = GetRenderHeight();
    Image img = {rlReadScreenPixels(w, h), w, h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    bool ok = ExportImage(img, path);
    UnloadImage(img);
    return ok;
}

// ---------------------------------------------------------------- 設定ファイル（表示言語）
// 実行ファイルと同じフォルダ（ポータブル版）。書き込めない場所（/usr/bin 等へインストール）なら
// Linux は $XDG_CONFIG_HOME/solar_voyager/ (既定 ~/.config/solar_voyager/)
static std::string ConfigPath() {
    std::string local = std::string(GetApplicationDirectory()) + "solar_voyager.cfg";
#ifndef _WIN32
    if (FileExists(local.c_str()) ? access(local.c_str(), W_OK) == 0 : access(GetApplicationDirectory(), W_OK) == 0)
        return local;
    std::string dir;
    if (const char* x = getenv("XDG_CONFIG_HOME"); x && *x) dir = x;
    else if (const char* h = getenv("HOME"); h && *h) dir = std::string(h) + "/.config";
    else return local;
    mkdir(dir.c_str(), 0700);
    dir += "/solar_voyager";
    mkdir(dir.c_str(), 0755);
    return dir + "/solar_voyager.cfg";
#else
    return local;
#endif
}

// 設定は "キー=値" の行で保存する（lang, quality, ブックマーク bm1〜bm9）
static std::vector<std::pair<std::string, std::string>> g_cfg;

static void LoadSettings() {
    g_cfg.clear();
    std::string path = ConfigPath();
    if (!FileExists(path.c_str())) return;
    char* text = LoadFileText(path.c_str());
    if (!text) return;
    std::string t = text;
    UnloadFileText(text);
    size_t pos = 0;
    while (pos < t.size()) {
        size_t e = t.find('\n', pos);
        if (e == std::string::npos) e = t.size();
        std::string line = t.substr(pos, e - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t eq = line.find('=');
        if (eq != std::string::npos) g_cfg.push_back({line.substr(0, eq), line.substr(eq + 1)});
        pos = e + 1;
    }
}
// 未設定なら空文字
static std::string GetSetting(const std::string& key) {
    for (auto& kv : g_cfg)
        if (kv.first == key) return kv.second;
    return "";
}
// 空文字ならキーを消す
static void SetSetting(const std::string& key, const std::string& value) {
    for (size_t i = 0; i < g_cfg.size(); i++)
        if (g_cfg[i].first == key) {
            if (value.empty()) g_cfg.erase(g_cfg.begin() + i);
            else g_cfg[i].second = value;
            return;
        }
    if (!value.empty()) g_cfg.push_back({key, value});
}
static void SaveSettings() {
    std::string body;
    for (auto& kv : g_cfg) body += kv.first + "=" + kv.second + "\n";
    SaveFileText(ConfigPath().c_str(), (char*)body.c_str());
}

// 言語を切り替える。日本語フォントが無ければ英語のまま
static void SetLanguage(bool jp) {
    g_jp = jp && g_jpFont;
}

// ---------------------------------------------------------------- 起動オプション（自動スクリーンショット用）
struct Options {
    const char* shot = nullptr;
    int frames = 90;
    int target = -1;
    int gotoTarget = -1;  // ワープ後に切り替えるターゲット
    double camYaw = NAN, camPitch = 0, camDist = 60;  // 自機を任意の角度から見る（確認用、度・m）
    double atLat = NAN, atLon = 0, atAlt = 0;          // ターゲット天体の緯度・経度・高度 km に置く（確認用）
    double hours = 0;                                   // 開始時刻をずらす（確認用、時間）
    bool trace = false;  // 1 秒ごとに自機の状態を標準出力へ（動作確認用）
    bool warp = false, land = false, face = false, autopilot = false, autoEngine = false, cockpit = false, help = false;
    int engine = -1;
    double throttleSec = 0;
    double warpAnimAt = -1;  // 指定秒にワープ演出を開始（確認用）
    bool saveBookmark = false;  // 起動直後の位置をブックマーク（確認用）
    int gotoBookmark = -1;      // 1 秒後にブックマークへワープ（確認用、1〜）
    bool dumpMoons = false;     // 衛星の母天体からの相対位置を出力して終了（確認用）
    double timeScale = 1;
    const char* lang = nullptr;  // "ja" / "en"
    int quality = -1;            // 0〜2（確認用）
};
static Options ParseArgs(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&] { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--shot") o.shot = next();
        else if (a == "--frames") o.frames = atoi(next());
        else if (a == "--target") o.target = atoi(next());
        else if (a == "--warp") o.warp = true;
        else if (a == "--warp-anim") o.warpAnimAt = atof(next());
        else if (a == "--save-bookmark") o.saveBookmark = true;
        else if (a == "--dump-moons") o.dumpMoons = true;
        else if (a == "--goto-bookmark") o.gotoBookmark = atoi(next());
        else if (a == "--land") o.land = true;
        else if (a == "--face") o.face = true;
        else if (a == "--autopilot") o.autopilot = true;
        else if (a == "--trace") o.trace = true;
        else if (a == "--goto") o.gotoTarget = atoi(next());
        else if (a == "--cam") { o.camYaw = atof(next()); o.camPitch = atof(next()); o.camDist = atof(next()); }
        else if (a == "--auto-engine") o.autopilot = o.autoEngine = true;
        else if (a == "--cockpit") o.cockpit = true;
        else if (a == "--help-overlay") o.help = true;
        else if (a == "--engine") o.engine = atoi(next());
        else if (a == "--throttle") o.throttleSec = atof(next());
        else if (a == "--timescale") o.timeScale = atof(next());
        else if (a == "--lang") o.lang = next();
        else if (a == "--quality") o.quality = atoi(next());
        else if (a == "--hours") o.hours = atof(next());
        else if (a == "--at") { o.atLat = atof(next()); o.atLon = atof(next()); o.atAlt = atof(next()); }
    }
    return o;
}

// ---------------------------------------------------------------- アプリ本体
struct App {
    std::vector<Body> B;
    std::vector<Star> stars;
    Ship ship;
    Cam cam;
    double tDays = 0;
    int timeIdx = 0;
    bool paused = false;
    int target = B_MOON;
    bool cockpit = false, showOrbits = true, showMoonMarkers = true, showHelp = false, invertY = false;
    bool mouseCaptured = true;
    int pad = -1;                 // 使用中のゲームパッド番号（-1: なし）
    bool padTrigSeen[2] = {false, false};
    bool padZoom = false;
    // 自動操縦
    bool autopilot = false;
    bool apAutoEngine = false;
    int apPhase = 0;        // UI_AP_TURN 〜 UI_AP_AVOID
    double apRemain = 0;    // 到着点までの残り距離 km
    std::string toast;
    double toastTime = 0;
    // ワープ演出
    double warpT = -1;      // 演出開始からの実時間 秒（-1: 停止中）
    int warpTarget = 0;
    bool warpLand = false, warpJumped = false;
    int warpBookmark = -1;                // ブックマークへのワープ中はその番号
    int nextBookmark = 0;                 // N キーで次に向かうブックマーク
    double warpPhase = 0;   // 光の筋の流れ（強さに応じて進む）
    Mesh sphere{}, ring{};
    Mesh shipMesh{};
    Material shipMat{};
    Shader shipShader{};
    int locSunDir = -1, locSunColor = -1, locFillDir = -1, locFillColor = -1, locEngColor = -1, locEngGlow = -1, locNavBlink = -1;
    Material mat{};
    Shader shader{};
    Texture2D earthColor{}, earthData{};  // 地球の実データ（読めなければ手続き生成で描く）
    bool earthTex = false;
    Texture2D moonColor{}, moonData{};    // 月の実データ
    bool moonTex = false;
    int quality = 2;                      // 画質 0: 低, 1: 中, 2: 高
    RenderTexture2D lowRes{};             // 画質「低」で 3D を描く縮小バッファ
    int locLight = -1, locLightObj = -1, locC1 = -1, locC2 = -1, locAtm = -1, locParams = -1, locDetail = -1;
    int locOcc = -1, locOccN = -1, locSunAng = -1;
    std::vector<Vector3> dust;
    double glow = 0;
    double camYaw = NAN, camPitch = 0, camDist = 60;
};

static const double TIME_SCALES[] = {1, 10, 60, 600, 3600, 86400, 864000, 8640000, 31557600};
static const char* TIME_SCALE_NAMES[] = {"x1", "x10", "x60 (1min/s)", "x600 (10min/s)", "x3600 (1h/s)",
                                         "x86400 (1d/s)", "x864000 (10d/s)", "x8.64M (100d/s)", "x31.6M (1y/s)"};
static const int TIME_SCALE_COUNT = sizeof(TIME_SCALES) / sizeof(TIME_SCALES[0]);

static V3 ShipWorld(const App& app) { return app.B[app.ship.ref].pos + app.ship.local; }
static void SetShipWorld(App& app, V3 w) { app.ship.local = w - app.B[app.ship.ref].pos; }
static void Toast(App& app, const std::string& s) { app.toast = s; app.toastTime = 2.5; }

// 基準系の再判定: 影響圏に入っている最も小さい天体を基準にする
static void UpdateReferenceFrame(App& app) {
    V3 w = ShipWorld(app);
    int best = B_SUN;
    double bestSoi = 1e301;
    for (size_t i = 1; i < app.B.size(); i++) {
        const Body& b = app.B[i];
        if (Len(w - b.pos) < b.soi && b.soi < bestSoi) { best = (int)i; bestSoi = b.soi; }
    }
    if (best != app.ship.ref) {
        app.ship.ref = best;
        SetShipWorld(app, w);
    }
}

static void WarpTo(App& app, int ti, bool land) {
    const Body& b = app.B[ti];
    V3 toSun = ti == B_SUN ? Norm(app.B[B_EARTH].pos) : Norm(-b.pos);
    V3 side = Cross(toSun, Y_UP);
    if (Len(side) < 1e-6) side = V3(1, 0, 0);
    side = Norm(side);
    V3 dir = Norm(toSun * 0.6 + side * 0.8 + Y_UP * 0.15);
    app.ship.ref = ti;
    app.ship.speed = 0;
    app.autopilot = false;
    if (!land) {
        double d = std::max(b.d.radius * 4.0, b.outer * 2.4);
        app.ship.local = dir * d;
        app.ship.q = QuatFromBasis(-dir, Y_UP);
    } else {
        dir = Norm(toSun * 0.85 + side * 0.5);
        double r = SurfaceRadius(b, dir) + MinAltitude(b) + 0.15;
        app.ship.local = dir * r;
        V3 east = Cross(b.pole, dir);
        if (Len(east) < 1e-6) east = side;
        // 東向き・やや下向き（地平線が画面中央より少し上に来る）
        V3 fwd = Norm(Norm(east) - dir * 0.08);
        app.ship.q = QuatFromBasis(fwd, dir);
    }
    UpdateReferenceFrame(app);
    Toast(app, std::string(land ? T(UI_LAND) : T(UI_WARP)) + ": " + TR(b.d.jp, b.d.en));
}

// 天体の緯度・経度（東経が正）・高度に置き、北の地平線のやや手前を見下ろす
static void PlaceAt(App& app, int ti, double lat, double lon, double altKm) {
    const Body& b = app.B[ti];
    double cs = std::cos(b.spin), sn = std::sin(b.spin);
    V3 X = b.ex * cs - b.ez * sn, Z = b.ex * sn + b.ez * cs;  // 本体座標の X 軸（経度 0）と Z 軸
    double la = lat * DEG, lo = lon * DEG;
    V3 dir = X * (std::cos(la) * std::cos(lo)) + b.pole * std::sin(la) - Z * (std::cos(la) * std::sin(lo));
    app.ship.ref = ti;
    app.ship.speed = 0;
    double r0 = SurfaceRadius(b, dir), alt = std::max(altKm, MinAltitude(b));
    app.ship.local = dir * (r0 + alt);
    V3 north = b.pole - dir * Dot(b.pole, dir);
    if (Len(north) < 1e-9) north = X;
    north = Norm(north);
    double dip = std::acos(r0 / (r0 + alt)) + 12 * DEG;  // 地平線の見下ろし角 + 12°
    V3 fwd = north * std::cos(dip) - dir * std::sin(dip);
    app.ship.q = QuatFromBasis(fwd, north * std::sin(dip) + dir * std::cos(dip));
    UpdateReferenceFrame(app);
}

// ---------------------------------------------------------------- ブックマーク
// 天体に固定した座標（自転と一緒に回る）で位置と向きを保存する。設定ファイルの bm1〜bm9
// 形式: 天体の英語名,位置 xyz km,機首 xyz,上 xyz
static const int MAX_BOOKMARKS = 9;

static void BodyFixedAxes(const Body& b, V3* X, V3* Z) {
    double cs = std::cos(b.spin), sn = std::sin(b.spin);
    *X = b.ex * cs - b.ez * sn;
    *Z = b.ex * sn + b.ez * cs;
}

struct Bookmark {
    int body = -1;
    V3 pos, fwd, up;   // 天体固定座標
};

static bool ParseBookmark(const App& app, const std::string& v, Bookmark* bm) {
    size_t c = v.find(',');
    if (c == std::string::npos) return false;
    std::string name = v.substr(0, c);
    for (size_t i = 0; i < app.B.size(); i++)
        if (name == app.B[i].d.en) bm->body = (int)i;
    double a[9];
    if (bm->body < 0 || sscanf(v.c_str() + c + 1, "%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf", &a[0], &a[1], &a[2], &a[3], &a[4], &a[5],
                               &a[6], &a[7], &a[8]) != 9)
        return false;
    bm->pos = V3(a[0], a[1], a[2]);
    bm->fwd = V3(a[3], a[4], a[5]);
    bm->up = V3(a[6], a[7], a[8]);
    return true;
}

static std::vector<Bookmark> LoadBookmarks(const App& app) {
    std::vector<Bookmark> v;
    for (int i = 1; i <= MAX_BOOKMARKS; i++) {
        Bookmark bm;
        if (ParseBookmark(app, GetSetting("bm" + std::to_string(i)), &bm)) v.push_back(bm);
    }
    return v;
}

static std::string BookmarkLabel(const App& app, const Bookmark& bm) {
    const Body& b = app.B[bm.body];
    double r = Len(bm.pos);
    V3 dir = bm.pos * (1.0 / r);
    double alt = r - SurfaceRadius(b, b.ex * dir.x + b.pole * dir.y + b.ez * dir.z);  // 楕円体は自転に依らない
    char buf[160];
    snprintf(buf, sizeof buf, "%s  %s %.1f°  %s %.1f°  %s", TR(b.d.jp, b.d.en), dir.y >= 0 ? "N" : "S",
             std::fabs(std::asin(std::max(-1.0, std::min(1.0, dir.y))) / DEG), std::atan2(-dir.z, dir.x) >= 0 ? "E" : "W",
             std::fabs(std::atan2(-dir.z, dir.x) / DEG), FmtDist(std::max(0.0, alt)).c_str());
    return buf;
}

static void SaveBookmark(App& app) {
    std::vector<Bookmark> v = LoadBookmarks(app);
    const Body& b = app.B[app.ship.ref];
    V3 X, Z;
    BodyFixedAxes(b, &X, &Z);
    auto toFixed = [&](V3 w) { return V3(Dot(w, X), Dot(w, b.pole), Dot(w, Z)); };
    Bookmark bm{app.ship.ref, toFixed(app.ship.local), toFixed(ShipFwd(app.ship)), toFixed(ShipUp(app.ship))};
    if ((int)v.size() >= MAX_BOOKMARKS) v.erase(v.begin());   // いっぱいなら一番古いものを消す
    v.push_back(bm);
    for (int i = 1; i <= MAX_BOOKMARKS; i++) {
        std::string val;
        if (i <= (int)v.size()) {
            const Bookmark& m = v[i - 1];
            char buf[400];
            snprintf(buf, sizeof buf, "%s,%.6f,%.6f,%.6f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f", app.B[m.body].d.en, m.pos.x, m.pos.y, m.pos.z,
                     m.fwd.x, m.fwd.y, m.fwd.z, m.up.x, m.up.y, m.up.z);
            val = buf;
        }
        SetSetting("bm" + std::to_string(i), val);
    }
    SaveSettings();
    app.nextBookmark = 0;
    char msg[256];
    snprintf(msg, sizeof msg, T(UI_BM_SAVED), (int)v.size(), BookmarkLabel(app, bm).c_str());
    Toast(app, msg);
}

// ワープの瞬間に呼ぶ: ブックマークの位置・向きへ移す
static void ApplyBookmark(App& app, const Bookmark& bm) {
    const Body& b = app.B[bm.body];
    V3 X, Z;
    BodyFixedAxes(b, &X, &Z);
    auto toWorld = [&](V3 f) { return X * f.x + b.pole * f.y + Z * f.z; };
    app.ship.ref = bm.body;
    app.ship.speed = 0;
    app.ship.local = toWorld(bm.pos);
    app.ship.q = QuatFromBasis(toWorld(bm.fwd), toWorld(bm.up));
    app.target = bm.body;
    UpdateReferenceFrame(app);
}

static void FaceTarget(App& app) {
    V3 d = app.B[app.target].pos - ShipWorld(app);
    app.ship.q = QuatFromBasis(d, ShipUp(app.ship));
}

// ---------------------------------------------------------------- ワープ演出
// 加速（機首を目標へ向けながら光の筋が伸びる）→ 閃光の頂点で移動 → 到着（筋が縮んで消える）
static const double WARP_CHARGE = 0.9, WARP_ARRIVE = 0.8;  // 実時間 秒

static void StartWarp(App& app, int ti, bool land) {
    if (app.warpT >= 0) return;
    sfx::Warp();
    app.warpT = 0;
    app.warpTarget = ti;
    app.warpLand = land;
    app.warpJumped = false;
    app.ship.speed = 0;
    app.autopilot = false;
}

// 演出の強さ 0..1（光の筋の長さ・視野角の広がり）
static double WarpIntensity(const App& app) {
    if (app.warpT < 0) return 0;
    if (app.warpT < WARP_CHARGE) { double k = app.warpT / WARP_CHARGE; return k * k; }
    double k = 1 - std::min(1.0, (app.warpT - WARP_CHARGE) / WARP_ARRIVE);
    return k * k * k;
}

// 移動の瞬間を頂点とする閃光 0..1
static double WarpFlash(const App& app) {
    if (app.warpT < 0) return 0;
    double d = app.warpT - WARP_CHARGE;
    return d < 0 ? std::exp(d * 14.0) : std::exp(-d * 6.0);
}

static double TurnToward(Ship& s, V3 desired, double maxAngle);

static void UpdateWarp(App& app, double dt) {
    if (app.warpT < 0) return;
    app.warpT += dt;
    app.warpPhase += dt * (0.4 + 4.0 * WarpIntensity(app));
    if (!app.warpJumped) {
        V3 d = app.B[app.warpTarget].pos - ShipWorld(app);
        TurnToward(app.ship, d, 3.0 * dt);
        app.ship.thrusting = true;
        if (app.warpT >= WARP_CHARGE) {
            std::vector<Bookmark> bms = app.warpBookmark >= 0 ? LoadBookmarks(app) : std::vector<Bookmark>();
            if (app.warpBookmark >= 0 && app.warpBookmark < (int)bms.size()) ApplyBookmark(app, bms[app.warpBookmark]);
            else WarpTo(app, app.warpTarget, app.warpLand);
            app.warpBookmark = -1;
            app.warpJumped = true;
        }
    } else if (app.warpT >= WARP_CHARGE + WARP_ARRIVE) {
        app.warpT = -1;
    }
}

// ---------------------------------------------------------------- ゲームパッド
struct PadState {
    double pitch = 0, yaw = 0, roll = 0;  // -1..1
    double throttle = 0, brake = 0;       // 0..1
    bool engineUp = false, engineDown = false, timeUp = false, timeDown = false;
    bool autopilot = false, stop = false, nextTarget = false, warp = false, land = false, view = false, help = false, zoom = false;
};

static double StickCurve(double v) {
    const double dead = 0.15;
    double a = std::fabs(v);
    if (a < dead) return 0;
    a = (a - dead) / (1 - dead);
    return (v < 0 ? -1 : 1) * a * a;  // 二乗カーブで微調整しやすくする
}

static PadState ReadPad(App& app) {
    PadState p;
    // 接続の検出（最初に見つかったパッドを使う）
    if (app.pad >= 0 && !IsGamepadAvailable(app.pad)) {
        app.pad = -1;
        Toast(app, T(UI_PAD_OFF));
    }
    if (app.pad < 0) {
        for (int i = 0; i < 4; i++) {
            if (IsGamepadAvailable(i)) {
                app.pad = i;
                app.padTrigSeen[0] = app.padTrigSeen[1] = false;
                Toast(app, std::string(T(UI_PAD_ON)) + ": " + GetGamepadName(i));
                break;
            }
        }
    }
    if (app.pad < 0) return p;
    int g = app.pad;
    p.yaw = -StickCurve(GetGamepadAxisMovement(g, GAMEPAD_AXIS_LEFT_X));
    p.pitch = -StickCurve(GetGamepadAxisMovement(g, GAMEPAD_AXIS_LEFT_Y));
    p.roll = -StickCurve(GetGamepadAxisMovement(g, GAMEPAD_AXIS_RIGHT_X));
    if (IsGamepadButtonDown(g, GAMEPAD_BUTTON_LEFT_TRIGGER_1)) p.roll += 1;
    if (IsGamepadButtonDown(g, GAMEPAD_BUTTON_RIGHT_TRIGGER_1)) p.roll -= 1;

    // トリガーは -1(離す)〜1(押し込み)。ドライバによっては一度動かすまで 0 を返すので、
    // 0 以外の値を観測するまでは未入力として扱う
    GamepadAxis trig[2] = {GAMEPAD_AXIS_RIGHT_TRIGGER, GAMEPAD_AXIS_LEFT_TRIGGER};
    double tv[2];
    for (int k = 0; k < 2; k++) {
        float v = GetGamepadAxisMovement(g, trig[k]);
        if (v != 0.0f) app.padTrigSeen[k] = true;
        tv[k] = app.padTrigSeen[k] ? (v + 1.0) * 0.5 : 0.0;
        if (tv[k] < 0.05) tv[k] = 0;
    }
    p.throttle = std::max(tv[0], IsGamepadButtonDown(g, GAMEPAD_BUTTON_RIGHT_TRIGGER_2) && !app.padTrigSeen[0] ? 1.0 : 0.0);
    p.brake = std::max(tv[1], IsGamepadButtonDown(g, GAMEPAD_BUTTON_LEFT_TRIGGER_2) && !app.padTrigSeen[1] ? 1.0 : 0.0);

    p.engineUp = IsGamepadButtonPressed(g, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
    p.engineDown = IsGamepadButtonPressed(g, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
    p.timeUp = IsGamepadButtonPressed(g, GAMEPAD_BUTTON_LEFT_FACE_UP);
    p.timeDown = IsGamepadButtonPressed(g, GAMEPAD_BUTTON_LEFT_FACE_DOWN);
    p.autopilot = IsGamepadButtonPressed(g, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);  // A / ×
    p.stop = IsGamepadButtonPressed(g, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);      // B / ○
    p.nextTarget = IsGamepadButtonPressed(g, GAMEPAD_BUTTON_RIGHT_FACE_LEFT); // X / □
    p.warp = IsGamepadButtonPressed(g, GAMEPAD_BUTTON_RIGHT_FACE_UP);         // Y / △
    p.land = IsGamepadButtonPressed(g, GAMEPAD_BUTTON_LEFT_THUMB);
    p.zoom = IsGamepadButtonDown(g, GAMEPAD_BUTTON_RIGHT_THUMB);
    p.view = IsGamepadButtonPressed(g, GAMEPAD_BUTTON_MIDDLE_LEFT);
    p.help = IsGamepadButtonPressed(g, GAMEPAD_BUTTON_MIDDLE_RIGHT);
    return p;
}

// 自機の加速・移動に使う経過時間（時間倍率を反映、停止中は 0）
static double SimDt(const App& app, double dt) { return app.paused ? 0.0 : dt * TIME_SCALES[app.timeIdx]; }

static void HandleInput(App& app, double dt) {
    Ship& s = app.ship;
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    PadState pad = ReadPad(app);
    app.padZoom = pad.zoom;

    if (IsKeyPressed(KEY_ESCAPE)) {
        app.mouseCaptured = !app.mouseCaptured;
        if (app.mouseCaptured) DisableCursor(); else EnableCursor();
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !app.mouseCaptured) {
        app.mouseCaptured = true;
        DisableCursor();
    }

    bool warping = app.warpT >= 0;  // ワープ演出中は操縦を受け付けない

    // 回転
    double zoomK = app.cam.fovBase / (60 * DEG);
    double pitch = 0, yaw = 0, roll = 0;
    bool manual = false;  // 手動操作があれば自動操縦を解除する
    if (app.mouseCaptured) {
        Vector2 md = GetMouseDelta();
        if (Vector2Length(md) > 4) manual = true;
        const double sens = 0.0022 * zoomK;
        yaw -= md.x * sens;
        pitch -= md.y * sens * (app.invertY ? -1 : 1);
    }
    double kr = 1.1 * dt * zoomK;
    if (IsKeyDown(KEY_UP)) pitch += kr;
    if (IsKeyDown(KEY_DOWN)) pitch -= kr;
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) yaw += kr;
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) yaw -= kr;
    if (IsKeyDown(KEY_Q)) roll += 1.6 * dt;
    if (IsKeyDown(KEY_E)) roll -= 1.6 * dt;
    pitch += pad.pitch * 1.3 * dt * zoomK * (app.invertY ? -1 : 1);
    yaw += pad.yaw * 1.3 * dt * zoomK;
    roll += std::max(-1.0, std::min(1.0, pad.roll)) * 1.6 * dt;
    if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_A) ||
        IsKeyDown(KEY_D) || IsKeyDown(KEY_Q) || IsKeyDown(KEY_E) || pad.pitch != 0 || pad.yaw != 0 || pad.roll != 0)
        manual = true;
    Quaternion r = QuaternionMultiply(QuaternionFromAxisAngle({0, 1, 0}, (float)yaw), QuaternionFromAxisAngle({1, 0, 0}, (float)pitch));
    r = QuaternionMultiply(r, QuaternionFromAxisAngle({0, 0, 1}, (float)roll));
    if (!warping) s.q = QuaternionNormalize(QuaternionMultiply(s.q, r));

    // エンジン
    int engineBefore = s.engine;
    for (int i = 0; i < ENGINE_COUNT; i++)
        if (IsKeyPressed((KeyboardKey)(KEY_ONE + i)) || IsKeyPressed((KeyboardKey)(KEY_KP_1 + i))) s.engine = i;
    float wheel = GetMouseWheelMove();
    if (wheel > 0) s.engine = std::min(ENGINE_COUNT - 1, s.engine + 1);
    if (wheel < 0) s.engine = std::max(0, s.engine - 1);
    if (pad.engineUp) s.engine = std::min(ENGINE_COUNT - 1, s.engine + 1);
    if (pad.engineDown) s.engine = std::max(0, s.engine - 1);
    if (s.engine != engineBefore) app.apAutoEngine = false;

    const Engine& e = ENGINES[s.engine];
    double acc = e.accel / 1000.0, vmax = e.vmax / 1000.0;
    // スロットル（キーボードは全開、トリガーは押し込み量に比例）。加減速は時間倍率に従う
    double sdt = SimDt(app, dt);
    double throttle = std::max(IsKeyDown(KEY_W) ? 1.0 : 0.0, pad.throttle);
    double brake = std::max(IsKeyDown(KEY_S) ? 1.0 : 0.0, pad.brake);
    if (warping) throttle = brake = 0;
    if (throttle > 0 || brake > 0 || IsKeyPressed(KEY_X) || pad.stop) manual = true;
    if (manual && app.autopilot) {
        app.autopilot = false;
        Toast(app, T(UI_AP_OFF));
    }
    s.thrusting = false;
    if (throttle > 0 && s.speed < vmax) {
        s.speed = std::min(vmax, s.speed + acc * throttle * sdt);
        s.thrusting = true;
    }
    if (brake > 0) {
        s.speed = std::max(-vmax * 0.25, s.speed - acc * brake * sdt);
        s.thrusting = true;
    }
    if (IsKeyPressed(KEY_X) || pad.stop) s.speed = 0;
    // 下位エンジンへ切り替えた時は最高速度まで素早く減速
    double lim = s.speed >= 0 ? vmax : vmax * 0.25;
    if (std::fabs(s.speed) > lim) {
        double sign = s.speed > 0 ? 1 : -1;
        double over = (std::fabs(s.speed) - lim) * std::exp(-3.0 * dt);
        s.speed = sign * (over < lim * 1e-4 ? lim : lim + over);
    }

    // ターゲット
    int n = (int)app.B.size();
    if (IsKeyPressed(KEY_TAB)) app.target = (app.target + (shift ? n - 1 : 1)) % n;
    if (pad.nextTarget) app.target = (app.target + 1) % n;
    if (IsKeyPressed(KEY_F) && !warping) FaceTarget(app);
    if ((IsKeyPressed(KEY_T) || pad.autopilot) && !warping) {
        if (app.autopilot) {
            app.autopilot = false;
            Toast(app, T(UI_AP_OFF));
        } else {
            app.autopilot = true;
            app.apAutoEngine = shift;
            Toast(app, T(shift ? UI_AP_ON_AUTO : UI_AP_ON));
        }
    }
    if (IsKeyPressed(KEY_G)) StartWarp(app, app.target, shift);
    if (pad.warp || pad.land) StartWarp(app, app.target, pad.land);

    // 表示
    if (IsKeyPressed(KEY_V) || pad.view) { app.cockpit = !app.cockpit; Toast(app, T(app.cockpit ? UI_VIEW_COCKPIT : UI_VIEW_CHASE)); }
    if (IsKeyPressed(KEY_O)) { app.showOrbits = !app.showOrbits; Toast(app, T(app.showOrbits ? UI_ORBITS_ON : UI_ORBITS_OFF)); }
    if (IsKeyPressed(KEY_L)) { app.showMoonMarkers = !app.showMoonMarkers; Toast(app, T(app.showMoonMarkers ? UI_LABELS_ON : UI_LABELS_OFF)); }
    if (IsKeyPressed(KEY_J)) {
        if (!g_jpFont) {
            Toast(app, T(UI_LANG_NO_FONT));
        } else {
            SetLanguage(!g_jp);
            SetSetting("lang", g_jp ? "ja" : "en");
            SaveSettings();
            Toast(app, T(UI_LANG_SET));
        }
    }
    if (IsKeyPressed(KEY_M) && shift) {
        sfx::MusicSetOff(!sfx::gm.off);
        SetSetting("music", sfx::gm.off ? "0" : "1");
        SaveSettings();
        Toast(app, T(sfx::gm.off ? UI_MUSIC_OFF : UI_MUSIC_ON));
    } else if (IsKeyPressed(KEY_M)) {
        sfx::SetMuted(!sfx::g.muted);
        SetSetting("sound", sfx::g.muted ? "0" : "1");
        SaveSettings();
        Toast(app, T(sfx::g.muted ? UI_SOUND_OFF : UI_SOUND_ON));
    }
    if (IsKeyPressed(KEY_B)) SaveBookmark(app);
    if (IsKeyPressed(KEY_N) && app.warpT < 0) {
        std::vector<Bookmark> bms = LoadBookmarks(app);
        if (bms.empty()) {
            Toast(app, T(UI_BM_NONE));
        } else {
            int i = app.nextBookmark % (int)bms.size();
            app.nextBookmark = i + 1;
            StartWarp(app, bms[i].body, false);
            app.warpBookmark = i;
            char msg[256];
            snprintf(msg, sizeof msg, T(UI_BM_GO), i + 1, BookmarkLabel(app, bms[i]).c_str());
            Toast(app, msg);
        }
    }
    if (IsKeyPressed(KEY_F9)) {
        app.quality = (app.quality + 1) % 3;
        SetSetting("quality", std::to_string(app.quality));
        SaveSettings();
        Toast(app, T((UiId)(UI_QUALITY_0 + app.quality)));
    }
    if (IsKeyPressed(KEY_I)) { app.invertY = !app.invertY; Toast(app, T(app.invertY ? UI_INVERT_ON : UI_INVERT_OFF)); }
    if (IsKeyPressed(KEY_H) || pad.help) app.showHelp = !app.showHelp;
    if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();

    // 時間
    if (IsKeyPressed(KEY_RIGHT_BRACKET) || pad.timeUp) app.timeIdx = std::min(TIME_SCALE_COUNT - 1, app.timeIdx + 1);
    if (IsKeyPressed(KEY_LEFT_BRACKET) || pad.timeDown) app.timeIdx = std::max(0, app.timeIdx - 1);
    if (IsKeyPressed(KEY_P)) app.paused = !app.paused;
    if (IsKeyPressed(KEY_BACKSPACE)) {
        app.tDays = NowDaysSinceJ2000();
        app.timeIdx = 0;
        app.paused = false;
        Toast(app, T(UI_TIME_RESET));
    }
    (void)ctrl;
}

// ---------------------------------------------------------------- 自動操縦
static double ArrivalDistance(const Body& b) { return std::max(b.d.radius * 3.0, b.outer * 2.2); }

// 機首を desired 方向へ最大 maxAngle だけ回す（ワールド空間での回転）
static double TurnToward(Ship& s, V3 desired, double maxAngle) {
    V3 f = ShipFwd(s);
    desired = Norm(desired);
    double ang = std::acos(std::max(-1.0, std::min(1.0, Dot(f, desired))));
    if (ang < 1e-6) return 0;
    V3 axis = Cross(f, desired);
    if (Len(axis) < 1e-9) axis = ShipUp(s);  // 真後ろ: 上方向を軸に回る
    double step = std::min(ang, maxAngle);
    Quaternion r = QuaternionFromAxisAngle(ToF(Norm(axis)), (float)step);
    s.q = QuaternionNormalize(QuaternionMultiply(r, s.q));
    return ang - step;
}

// 経路上の天体を避けるための経由点（なければ目標そのもの）
static V3 AvoidancePoint(const App& app, V3 from, V3 to, bool* avoiding) {
    V3 d = to - from;
    double L = Len(d);
    d = d * (1.0 / L);
    *avoiding = false;
    for (size_t i = 0; i < app.B.size(); i++) {
        if ((int)i == app.target) continue;
        const Body& b = app.B[i];
        double R = b.outer * 1.5;
        V3 oc = b.pos - from;
        double t = Dot(oc, d);
        if (t <= 0 || t >= L) continue;
        V3 off = (from + d * t) - b.pos;
        if (Len(off) >= R) continue;
        if (Len(oc) < R) {
            // 天体のすぐ近く: 目標が地平線の下なら、接線方向へ上昇しながら回り込む
            if (Len(off) >= b.d.radius * 1.02) continue;
            V3 up = Norm(-oc);
            V3 tang = d - up * Dot(d, up);
            if (Len(tang) < 1e-6) tang = Cross(up, Y_UP);
            *avoiding = true;
            return from + Norm(Norm(tang) + up * 0.6) * b.d.radius;
        }
        V3 side = Len(off) > 1e-6 ? Norm(off) : Norm(Cross(d, Y_UP));
        *avoiding = true;
        return b.pos + side * (b.outer * 2.0);
    }
    return to;
}

static void UpdateAutopilot(App& app, double dt) {
    if (!app.autopilot) return;
    Ship& s = app.ship;
    const Body& tb = app.B[app.target];
    V3 sw = ShipWorld(app);
    double dist = Len(tb.pos - sw);
    double remain = dist - ArrivalDistance(tb);
    app.apRemain = std::max(0.0, remain);
    if (remain <= 0.001) {
        s.speed = 0;
        app.autopilot = false;
        Toast(app, std::string(T(UI_AP_ARRIVED)) + ": " + TR(tb.d.jp, tb.d.en));
        return;
    }
    double sdt = SimDt(app, dt);

    // エンジン自動選択: 時間倍率込みで実時間 約 60 秒以内に着く最も遅いエンジン
    if (app.apAutoEngine) {
        double budget = 60.0 * (app.paused ? 1.0 : TIME_SCALES[app.timeIdx]);
        int pick = ENGINE_COUNT - 1;
        for (int i = 0; i < ENGINE_COUNT; i++)
            if (remain / (ENGINES[i].vmax / 1000.0) <= budget) { pick = i; break; }
        s.engine = pick;
    }

    // 旋回（実時間で最大 1.5 rad/s）
    bool avoiding = false;
    V3 aim = AvoidancePoint(app, sw, tb.pos, &avoiding);
    double left = TurnToward(s, aim - sw, 1.5 * dt);

    // 速度: 制動曲線 v = sqrt(2 a d) と最高速度の小さい方。機首が向いていない間は加速しない
    const Engine& e = ENGINES[s.engine];
    double acc = e.accel / 1000.0, vmax = e.vmax / 1000.0;
    double align = std::cos(std::min(left, PI / 2.0));
    double vBrake = std::sqrt(2 * acc * remain);
    double vWant = std::min(vmax, vBrake) * (left < 10 * DEG ? 1.0 : std::pow(std::max(0.0, align), 8));
    double dv = vWant - s.speed;
    double maxDv = acc * sdt;
    s.speed += std::max(-maxDv, std::min(maxDv, dv));
    s.thrusting = std::fabs(dv) > 1e-9 && sdt > 0;

    if (avoiding) app.apPhase = UI_AP_AVOID;
    else if (left > 10 * DEG) app.apPhase = UI_AP_TURN;
    else if (vBrake < vmax * 0.999 && s.speed >= vBrake * 0.98) app.apPhase = UI_AP_DECEL;
    else if (s.speed < vmax * 0.999) app.apPhase = UI_AP_ACCEL;
    else app.apPhase = UI_AP_CRUISE;
}

static void UpdateShip(App& app, double dt) {
    Ship& s = app.ship;
    double step = s.speed * dt;
    if (app.autopilot && step > app.apRemain) step = app.apRemain;  // 大きな時間倍率でも行き過ぎない
    s.local += ShipFwd(s) * step;
    // 地表との衝突: 最低高度まで押し戻す（速度は維持して地表を滑る）
    for (const Body& b : app.B) {
        V3 rel = ShipWorld(app) - b.pos;
        double d = Len(rel);
        if (d > b.d.radius * 1.2 + 1) continue;
        V3 dir = rel * (1.0 / d);
        double rs = SurfaceRadius(b, dir) + MinAltitude(b);
        if (d < rs) SetShipWorld(app, b.pos + dir * rs);
    }
    UpdateReferenceFrame(app);
}

static void UpdateCamera(App& app) {
    Cam& c = app.cam;
    const Ship& s = app.ship;
    c.W = (float)GetScreenWidth();
    c.H = (float)GetScreenHeight();
    double targetFov = (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || app.padZoom) ? 8 * DEG : 60 * DEG;
    c.fovBase += (targetFov - c.fovBase) * std::min(1.0, GetFrameTime() * 10.0);
    double wi = WarpIntensity(app);
    c.fovy = std::min(150 * DEG, c.fovBase * (1.0 + 0.75 * wi));  // ワープ中は視野が広がる
    c.focal = c.H * 0.5 / std::tan(c.fovy * 0.5);
    c.fwd = ShipFwd(s);
    c.up = ShipUp(s);
    c.right = ShipRight(s);
    V3 sw = ShipWorld(app);
    // ワープ中は自機が前へ飛び出し、カメラが置いていかれる。閃光の前後は小刻みに揺れる
    double lag = 0.055 + (app.warpJumped ? 0.12 : 0.2) * wi;
    c.pos = app.cockpit ? sw + c.up * 0.0025 : sw - c.fwd * lag + c.up * 0.013;
    double shake = WarpFlash(app) * 0.0012;
    if (shake > 0) {
        double t = GetTime() * 61.0;
        c.pos = c.pos + c.right * (std::sin(t) * shake) + c.up * (std::sin(t * 1.37 + 1.0) * shake);
    }
    if (!std::isnan(app.camYaw)) {
        // 確認用: 自機の周囲を回る視点（yaw 0 = 真後ろ）
        double y = app.camYaw * DEG, p = app.camPitch * DEG;
        V3 f0 = ShipFwd(s), u0 = ShipUp(s), r0 = ShipRight(s);
        V3 dir = (f0 * -std::cos(y) + r0 * std::sin(y)) * std::cos(p) + u0 * std::sin(p);
        c.pos = sw + dir * (app.camDist / 1000.0);
        c.fwd = Norm(-dir);
        c.right = Norm(Cross(c.fwd, u0));
        c.up = Cross(c.right, c.fwd);
    }
    for (const Body& b : app.B) {
        V3 rel = c.pos - b.pos;
        double d = Len(rel);
        if (d > b.d.radius * 1.2 + 1) continue;
        V3 dir = rel * (1.0 / d);
        double rs = SurfaceRadius(b, dir) + MinAltitude(b) * 0.5;
        if (d < rs) c.pos = b.pos + dir * rs;
    }
    g_view = MatrixLookAt({0, 0, 0}, ToF(c.fwd), ToF(c.up));
}

// ---------------------------------------------------------------- 描画: 3D
static void DrawOrbits(App& app) {
    const Cam& c = app.cam;
    // 地表近くでは軌道線が視界を横切って邪魔なので描かない
    for (const Body& b : app.B)
        if (Len(c.pos - b.pos) < b.d.radius * 1.3) return;
    SetPassProjection(c, 1.0, 1e11);
    rlDisableDepthTest();
    for (size_t i = 1; i < app.B.size(); i++) {
        const Body& b = app.B[i];
        const BodyDef& d = b.d;
        V3 center = d.parent == 0 ? V3() : app.B[d.parent].pos;
        if (d.parent > 0 && Len(c.pos - center) > d.moonA * 60) continue;  // 衛星の軌道は近くでだけ表示
        if (Len(c.pos - b.pos) < d.radius * 40) continue;  // 自分の軌道線が天体を貫いて見えるのを防ぐ
        Color col = BodyColor(b, 0.8f);
        col.a = (int)i == app.target ? 150 : (d.parent == 0 ? 70 : 55);
        rlCheckRenderBatchLimit((int)b.orbit.size() * 2);
        rlBegin(RL_LINES);
        rlColor4ub(col.r, col.g, col.b, col.a);
        for (size_t k = 0; k + 1 < b.orbit.size(); k++) {
            Vector3 p0 = ToF(center + b.orbit[k] - c.pos), p1 = ToF(center + b.orbit[k + 1] - c.pos);
            rlVertex3f(p0.x, p0.y, p0.z);
            rlVertex3f(p1.x, p1.y, p1.z);
        }
        rlEnd();
    }
    rlDrawRenderBatchActive();
    rlEnableDepthTest();
}

// 埋め込みの地球データをテクスチャにする（経度方向は繰り返し、緯度方向は端で止める）
static Texture2D LoadEmbeddedTexture(EmbeddedFile f, const char* ext) {
    Image img = LoadImageFromMemory(ext, f.data, (int)f.size);
    if (!img.data) return Texture2D{};
    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    if (!t.id) return t;
    GenTextureMipmaps(&t);
    SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR);
    rlTextureParameters(t.id, RL_TEXTURE_FILTER_ANISOTROPIC, 8);
    rlTextureParameters(t.id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_REPEAT);
    rlTextureParameters(t.id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);
    return t;
}

static void LoadEarthTextures(App& app) {
    app.earthColor = LoadEmbeddedTexture(EarthColorJpg(), ".jpg");
    app.earthData = LoadEmbeddedTexture(EarthDataPng(), ".png");
    app.earthTex = app.earthColor.id && app.earthData.id;
    if (!app.earthTex) {
        TraceLog(LOG_WARNING, "Earth textures unavailable, using procedural surface");
        return;
    }
}

static void LoadMoonTextures(App& app) {
    app.moonColor = LoadEmbeddedTexture(MoonColorJpg(), ".jpg");
    app.moonData = LoadEmbeddedTexture(MoonDataPng(), ".png");
    app.moonTex = app.moonColor.id && app.moonData.id;
    if (!app.moonTex) TraceLog(LOG_WARNING, "Moon textures unavailable, using procedural surface");
}

// ---------------------------------------------------------------- 音楽の選曲
// 一番近い天体（表面までの距離）の曲。月以外の衛星は母惑星の曲。冥王星とカロンは無音
// （冥王星の曲は後年の別の作曲者の作品で著作権が残っている）
static int MusicTrackOf(const App& app, int body) {
    if (body == B_MOON) return 9;
    if (app.B[body].d.parent > 0) body = app.B[body].d.parent;
    switch (body) {
        case B_SUN: return 7;
        case B_EARTH: return 8;
        case B_MERCURY: return 0;
        case B_VENUS: return 1;
        case B_MARS: return 2;
        case B_JUPITER: return 3;
        case B_SATURN: return 4;
        case B_URANUS: return 5;
        case B_NEPTUNE: return 6;
        default: return -1;
    }
}

// 音量: ワープの到着点（半径の約 3 倍）付近まで最大、影響圏の端で 0（対数で滑らかに減らす）
static void ChooseMusic(const App& app, int* track, float* vol) {
    V3 sw = ShipWorld(app);
    int nearest = -1;
    double best = 1e300;
    for (size_t i = 0; i < app.B.size(); i++) {
        double d = SurfaceDist(app.B[i], sw);
        if (d < best) { best = d; nearest = (int)i; }
    }
    *track = nearest >= 0 ? MusicTrackOf(app, nearest) : -1;
    *vol = 0;
    if (*track < 0) return;
    // 音量の基準は曲の天体（月以外の衛星の近くでは母惑星）
    int planet = (nearest != B_MOON && app.B[nearest].d.parent > 0) ? app.B[nearest].d.parent : nearest;
    const Body& p = app.B[planet];
    double D = Len(p.pos - sw), nearD = ArrivalDistance(p) * 1.5;
    double farD = planet == B_SUN ? AU_KM : std::max(p.soi, nearD * 20);   // 太陽は影響圏が無限なので 1 AU で消す
    double k = D <= nearD ? 1.0 : 1.0 - std::log(D / nearD) / std::log(farD / nearD);
    k = std::max(0.0, std::min(1.0, k));
    *vol = (float)std::pow(k, 1.5);
}

// 画質ごとの描画解像度と模様の細かさ（1 画素の大きさに掛ける。大きいほど細部を省いて軽くなる）
static const float QUALITY_SCALE[3] = {0.6f, 1.0f, 1.0f};
static const float QUALITY_DETAIL[3] = {2.5f, 1.8f, 1.0f};

// 天体 i に影を落としうる天体（母天体・兄弟の衛星・自分の衛星）を最大 4 つシェーダへ渡す
static void SetOccluders(App& app, int i, double s) {
    const Body& b = app.B[i];
    float data[16] = {0};
    int n = 0;
    if (i != B_SUN) {
        V3 L = Norm(app.B[B_SUN].pos - b.pos);
        double sunAng = app.B[B_SUN].d.radius / Len(app.B[B_SUN].pos - b.pos);
        struct Cand { int j; double k; };
        std::vector<Cand> cands;
        for (size_t j = 1; j < app.B.size(); j++) {
            const Body& o = app.B[j];
            if ((int)j == i) continue;
            bool related = o.d.parent == i || (b.d.parent > 0 && ((int)j == b.d.parent || o.d.parent == b.d.parent));
            if (!related) continue;
            V3 v = o.pos - b.pos;
            double t = Dot(v, L);
            if (t <= 0) continue;
            double perp = Len(v - L * t), reach = o.d.radius + b.outer + t * sunAng;
            if (perp < reach) cands.push_back({(int)j, perp / reach});
        }
        std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& c) { return a.k < c.k; });
        for (const Cand& cd : cands) {
            if (n == 4) break;
            const Body& o = app.B[cd.j];
            V3 cp = (o.pos - app.cam.pos) * s;
            bool atm = o.d.kind == KIND_EARTH;   // 大気で赤い光が回り込む（月食）
            data[n * 4 + 0] = (float)cp.x; data[n * 4 + 1] = (float)cp.y; data[n * 4 + 2] = (float)cp.z;
            data[n * 4 + 3] = (float)(o.d.radius * s) * (atm ? -1.0f : 1.0f);
            n++;
        }
        float sa = (float)sunAng;
        SetShaderValue(app.shader, app.locSunAng, &sa, SHADER_UNIFORM_FLOAT);
    }
    if (n > 0) SetShaderValueV(app.shader, app.locOcc, data, SHADER_UNIFORM_VEC4, n);
    SetShaderValue(app.shader, app.locOccN, &n, SHADER_UNIFORM_INT);
}

static void DrawBodies(App& app) {
    const Cam& c = app.cam;
    struct Item { int i; double dist; };
    std::vector<Item> items;
    for (size_t i = 0; i < app.B.size(); i++) {
        const Body& b = app.B[i];
        V3 rel = b.pos - c.pos;
        double dist = Len(rel);
        if (Dot(rel, c.fwd) < -b.outer) continue;
        if (b.outer / dist * c.focal < 0.8) continue;  // サブピクセルは 2D の点で描く
        items.push_back({(int)i, dist});
    }
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.dist > b.dist; });

    for (const Item& it : items) {
        const Body& b = app.B[it.i];
        const BodyDef& d = b.d;
        V3 rel = b.pos - c.pos;
        double dist = it.dist;
        double s = 100.0 / dist;
        double rPol = d.radius * (1 - d.flattening);
        // near: 最も近い表面（環があればその外縁）より手前
        double alt = dist - SurfaceRadius(b, rel * (-1.0 / dist));
        double nearKm = (dist > b.outer && b.outer > d.radius) ? std::min(alt, dist - b.outer) * 0.9 : alt * 0.5;
        nearKm = std::max(nearKm, dist * 1e-7);
        SetPassProjection(c, nearKm * s, (dist + b.outer) * s * 1.01);
        ClearDepth();

        V3 L = it.i == B_SUN ? V3(0, 1, 0) : Norm(-b.pos);
        V3 Lobj(Dot(L, b.ex), Dot(L, b.pole), Dot(L, b.ez));
        Vector3 lf = ToF(L), lo = ToF(Lobj);
        Vector3 c1 = {d.c1.r, d.c1.g, d.c1.b}, c2 = {d.c2.r, d.c2.g, d.c2.b}, atm = {d.atm.r, d.atm.g, d.atm.b};
        // 大気の縁の光は外から見た時だけ（地表付近では地面全体が光ってしまう）
        float atmK = (float)std::min(1.0, std::max(0.0, alt / (d.radius * 0.05)));
        float params[4] = {(float)d.kind, d.bands, (float)(it.i * 7.31), d.atmStrength * atmK};
        SetShaderValue(app.shader, app.locLight, &lf, SHADER_UNIFORM_VEC3);
        // 本体の描画では自転込みの座標系での太陽方向（環の影に使う）
        {
            double cw = std::cos(b.spin), sw = std::sin(b.spin);
            V3 X = b.ex * cw - b.ez * sw, Z = b.ex * sw + b.ez * cw;
            Vector3 ls = ToF(V3(Dot(L, X), Dot(L, b.pole), Dot(L, Z)));
            SetShaderValue(app.shader, app.locLightObj, &ls, SHADER_UNIFORM_VEC3);
        }
        SetOccluders(app, it.i, s);
        SetShaderValue(app.shader, app.locC1, &c1, SHADER_UNIFORM_VEC3);
        SetShaderValue(app.shader, app.locC2, &c2, SHADER_UNIFORM_VEC3);
        SetShaderValue(app.shader, app.locAtm, &atm, SHADER_UNIFORM_VEC3);
        SetShaderValue(app.shader, app.locParams, params, SHADER_UNIFORM_VEC4);
        // 模様の細かさは 1 画素の大きさで決める。地球の雲は高度 10〜35 km で消える（地表付近で地面が白く覆われないように）
        float cloudK = (float)std::min(1.0, std::max(0.0, (alt - d.radius * 0.0015) / (d.radius * 0.004)));
        int style = d.style;
        // 実データのある天体はシェーダの texture0 / texture1 に割り当てる
        if (d.style == S_EARTH && app.earthTex) {
            style = S_EARTH_TEX;
            app.mat.maps[MATERIAL_MAP_DIFFUSE].texture = app.earthColor;
            app.mat.maps[MATERIAL_MAP_SPECULAR].texture = app.earthData;
        } else if (d.style == S_MOON && app.moonTex) {
            style = S_MOON_TEX;
            app.mat.maps[MATERIAL_MAP_DIFFUSE].texture = app.moonColor;
            app.mat.maps[MATERIAL_MAP_SPECULAR].texture = app.moonData;
        }
        float detail[4] = {(float)style, (float)(QUALITY_DETAIL[app.quality] / (c.focal * d.radius * s)), (float)app.tDays, cloudK};
        SetShaderValue(app.shader, app.locDetail, detail, SHADER_UNIFORM_VEC4);
        DrawMesh(app.sphere, app.mat, BodyMatrix(b, rel * s, d.radius * s, rPol * s, b.spin));

        if (d.rings) {
            float rp[4] = {4.0f, (float)d.radius, 0, 0};
            Vector3 rc1 = {0.93f, 0.86f, 0.72f}, rc2 = {0.55f, 0.50f, 0.44f};
            SetShaderValue(app.shader, app.locC1, &rc1, SHADER_UNIFORM_VEC3);
            SetShaderValue(app.shader, app.locC2, &rc2, SHADER_UNIFORM_VEC3);
            SetShaderValue(app.shader, app.locParams, rp, SHADER_UNIFORM_VEC4);
            SetShaderValue(app.shader, app.locLightObj, &lo, SHADER_UNIFORM_VEC3);
            rlDisableBackfaceCulling();
            DrawMesh(app.ring, app.mat, BodyMatrix(b, rel * s, d.radius * s, d.radius * s, 0));
            rlEnableBackfaceCulling();
        }
    }
}

static bool Occluded(const App& app, V3 from, V3 to, int self);

// 自機の照明: 太陽（天体の影に入ると消える）と、近くの天体からの照り返し
static void SetShipLighting(App& app, V3 shipWorld) {
    V3 sun = Norm(app.B[B_SUN].pos - shipWorld);
    float sunK = Occluded(app, shipWorld, app.B[B_SUN].pos, B_SUN) ? 0.0f : 1.05f;
    Vector3 sunDir = ToF(sun), sunColor = {sunK, sunK * 0.98f, sunK * 0.94f};
    // 見かけの大きさが最大の天体（太陽以外）
    int best = -1;
    double bestK = 0;
    for (size_t i = 1; i < app.B.size(); i++) {
        double d = Len(app.B[i].pos - shipWorld);
        double k = app.B[i].d.radius / d;
        if (k > bestK) { bestK = k; best = (int)i; }
    }
    Vector3 fillDir = {0, 1, 0}, fillColor = {0, 0, 0};
    if (best > 0) {
        const Body& b = app.B[best];
        V3 toBody = Norm(b.pos - shipWorld);
        // 天体の昼側がこちらを向いているほど明るい
        double phase = 0.5 * (1 + Dot(Norm(app.B[B_SUN].pos - b.pos), -toBody));
        double k = std::min(0.45, bestK * bestK * 1.5) * phase;
        fillDir = ToF(toBody);
        fillColor = {(float)(b.d.c1.r * k), (float)(b.d.c1.g * k), (float)(b.d.c1.b * k)};
    }
    SetShaderValue(app.shipShader, app.locSunDir, &sunDir, SHADER_UNIFORM_VEC3);
    SetShaderValue(app.shipShader, app.locSunColor, &sunColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(app.shipShader, app.locFillDir, &fillDir, SHADER_UNIFORM_VEC3);
    SetShaderValue(app.shipShader, app.locFillColor, &fillColor, SHADER_UNIFORM_VEC3);
}

static void DrawShipPass(App& app, double dt) {
    const Cam& c = app.cam;
    const Ship& s = app.ship;
    SetPassProjection(c, 0.0005, 5.0);
    ClearDepth();

    // 宇宙塵: 速度感を出すための演出（対数スケール）
    double v = std::fabs(s.speed) * 1000.0;
    double vis = 0.012 * std::pow(std::log10(1 + v), 1.3) * (s.speed < 0 ? -1 : 1);
    V3 f = c.fwd;
    const float box = 0.12f;
    Vector3 ff = ToF(f);
    float streak = (float)std::min(0.03, std::fabs(vis) * 0.06 + 0.0004) * (vis < 0 ? -1.0f : 1.0f);
    for (Vector3& p : app.dust) {
        p = Vector3Subtract(p, Vector3Scale(ff, (float)(vis * dt)));
        for (int k = 0; k < 3; k++) {
            float* q = k == 0 ? &p.x : k == 1 ? &p.y : &p.z;
            if (*q > box) *q -= 2 * box;
            if (*q < -box) *q += 2 * box;
        }
        DrawLine3D(p, Vector3Add(p, Vector3Scale(ff, streak)), {200, 210, 230, 90});
    }

    if (!app.cockpit) {
        V3 sw = ShipWorld(app);
        V3 rel = sw - c.pos;
        double target = s.thrusting ? 1.0 : (std::fabs(s.speed) > 0 ? 0.25 : 0.0);
        app.glow += (target - app.glow) * std::min(1.0, dt * 6);

        SetShipLighting(app, sw);
        Color f = ENGINES[s.engine].flame;
        Vector3 engColor = {f.r / 255.0f, f.g / 255.0f, f.b / 255.0f};
        float glow = (float)app.glow;
        float blink = std::fmod(GetTime(), 1.4) < 0.12 ? 1.0f : 0.0f;  // 航法灯の点滅
        SetShaderValue(app.shipShader, app.locEngColor, &engColor, SHADER_UNIFORM_VEC3);
        SetShaderValue(app.shipShader, app.locEngGlow, &glow, SHADER_UNIFORM_FLOAT);
        SetShaderValue(app.shipShader, app.locNavBlink, &blink, SHADER_UNIFORM_FLOAT);
        Matrix model = MatrixMultiply(MatrixMultiply(MatrixScale(0.001f, 0.001f, 0.001f), QuaternionToMatrix(s.q)),
                                      MatrixTranslate((float)rel.x, (float)rel.y, (float)rel.z));
        rlDisableBackfaceCulling();  // 薄い翼の裏面も描く（シェーダ側で法線を反転）
        DrawMesh(app.shipMesh, app.shipMat, model);
        rlEnableBackfaceCulling();

        rlPushMatrix();
        rlTranslatef((float)rel.x, (float)rel.y, (float)rel.z);
        rlMultMatrixf(MatrixToFloat(QuaternionToMatrix(s.q)));
        rlScalef(0.001f, 0.001f, 0.001f);
        DrawExhaust(s, app.glow);
        rlPopMatrix();
    }
    rlDrawRenderBatchActive();
}

// ---------------------------------------------------------------- 描画: 2D
static void DrawStars(const App& app) {
    const Cam& c = app.cam;
    for (const Star& st : app.stars) {
        Vector2 p;
        if (!Project(c, st.dir, &p)) continue;
        if (p.x < 0 || p.y < 0 || p.x >= c.W || p.y >= c.H) continue;
        if (st.size > 0) DrawCircleV(p, st.size, st.col);
        else DrawPixelV(p, st.col);
    }
}

// 天体が他の天体に隠れていないか（太陽のグロー用）
static bool Occluded(const App& app, V3 from, V3 to, int self) {
    V3 d = to - from;
    double L = Len(d);
    d = d * (1.0 / L);
    for (size_t i = 0; i < app.B.size(); i++) {
        if ((int)i == self) continue;
        V3 oc = app.B[i].pos - from;
        double t = Dot(oc, d);
        if (t < 0 || t > L) continue;
        if (Len(oc - d * t) < app.B[i].d.radius) return true;
    }
    return false;
}

static void DrawSunGlowAndDots(const App& app) {
    const Cam& c = app.cam;
    for (size_t i = 0; i < app.B.size(); i++) {
        const Body& b = app.B[i];
        V3 rel = b.pos - c.pos;
        Vector2 p;
        if (!Project(c, rel, &p)) continue;
        double px = b.d.radius / Len(rel) * c.focal;
        if (i == B_SUN) {
            if (Occluded(app, c.pos, b.pos, B_SUN)) continue;
            float r = (float)(px * 1.8 + 14);
            BeginBlendMode(BLEND_ADDITIVE);
            DrawCircleGradient((int)p.x, (int)p.y, r, {255, 240, 200, 200}, {255, 200, 120, 0});
            DrawCircleGradient((int)p.x, (int)p.y, r * 3, {255, 200, 140, 35}, {255, 160, 80, 0});
            EndBlendMode();
            if (px < 0.8) DrawCircleV(p, 2.5f, {255, 250, 230, 255});
        } else if (b.outer / Len(rel) * c.focal < 0.8) {
            DrawCircleV(p, b.d.parent == 0 ? 1.8f : 1.2f, BodyColor(b));
        }
    }
}

// ラベルの重なり回避: 既に置いた矩形（HUD パネル含む）と重ならない位置まで縦にずらす
static std::vector<Rectangle> g_placed;
static void PlaceLabel(float* x, float* y, float w, float h, float screenH) {
    float y0 = *y;
    for (int attempt = 0; attempt < 16; attempt++) {
        Rectangle r = {*x, *y, w, h};
        bool hit = false;
        for (const Rectangle& o : g_placed) {
            if (CheckCollisionRecs(r, o)) {
                hit = true;
                // 下へずらし、画面下端に達したら上方向へ
                *y = (y0 > screenH * 0.5f) ? o.y - h - 1 : o.y + o.height + 1;
                break;
            }
        }
        if (!hit) break;
    }
    g_placed.push_back({*x, *y, w, h});
}

// 天体方向マーカー（画面外なら画面端に矢印）
static void DrawMarker(const App& app, int idx) {
    const Cam& c = app.cam;
    const Body& b = app.B[idx];
    V3 rel = b.pos - c.pos;
    double dist = Len(rel);
    double surf = SurfaceDist(b, c.pos);
    bool isTarget = idx == app.target;
    Color col = isTarget ? Color{255, 220, 80, 255} : BodyColor(b, 1.1f);
    std::string label = std::string(TR(b.d.jp, b.d.en)) + "  " + FmtDist(std::max(0.0, surf));
    float fs = isTarget ? 18.0f : 15.0f;

    // 矢印を置く枠（上は方位計、下はエンジン表示を避ける）
    const float mL = 30, mR = 30, mT = 100, mB = 76;
    Vector2 p;
    bool front = Project(c, rel, &p);
    bool on = front && p.x >= mL && p.x <= c.W - mR && p.y >= mT && p.y <= c.H - mB;
    double px = b.d.radius / dist * c.focal;
    if (on) {
        if (px > 260 && !isTarget) return;  // 視界いっぱいの天体にはラベル不要
        float r = (float)std::max(7.0, std::min(px + 6, 60.0));
        if (isTarget) {
            float k = r * 0.45f;
            Color tc = col;
            DrawLineEx({p.x - r, p.y - r}, {p.x - r + k, p.y - r}, 2, tc); DrawLineEx({p.x - r, p.y - r}, {p.x - r, p.y - r + k}, 2, tc);
            DrawLineEx({p.x + r, p.y - r}, {p.x + r - k, p.y - r}, 2, tc); DrawLineEx({p.x + r, p.y - r}, {p.x + r, p.y - r + k}, 2, tc);
            DrawLineEx({p.x - r, p.y + r}, {p.x - r + k, p.y + r}, 2, tc); DrawLineEx({p.x - r, p.y + r}, {p.x - r, p.y + r - k}, 2, tc);
            DrawLineEx({p.x + r, p.y + r}, {p.x + r - k, p.y + r}, 2, tc); DrawLineEx({p.x + r, p.y + r}, {p.x + r, p.y + r - k}, 2, tc);
        } else {
            DrawCircleLinesV(p, r, Fade(col, 0.6f));
        }
        float lx = p.x + r + 4, ly = p.y - fs * 0.5f;
        PlaceLabel(&lx, &ly, TxtW(label.c_str(), fs), fs + 2, c.H);
        Txt(label.c_str(), lx, ly, fs, col);
        return;
    }
    // 画面外: 方向を画面端へ投影
    double x = Dot(rel, c.right), y = -Dot(rel, c.up);
    if (!front && std::fabs(x) < 1e-12 && std::fabs(y) < 1e-12) y = 1;
    double l = std::sqrt(x * x + y * y);
    x /= l; y /= l;
    double cx = (mL + c.W - mR) * 0.5, cy = (mT + c.H - mB) * 0.5;
    double hw = (c.W - mR - mL) * 0.5, hh = (c.H - mB - mT) * 0.5;
    double t = std::min(hw / std::max(std::fabs(x), 1e-9), hh / std::max(std::fabs(y), 1e-9));
    Vector2 e = {(float)(cx + x * t), (float)(cy + y * t)};
    Vector2 dir = {(float)x, (float)y}, nrm = {(float)-y, (float)x};
    float as = isTarget ? 13.0f : 9.0f;
    Vector2 tip = {e.x + dir.x * as, e.y + dir.y * as};
    Vector2 b1 = {e.x - dir.x * as * 0.6f + nrm.x * as * 0.7f, e.y - dir.y * as * 0.6f + nrm.y * as * 0.7f};
    Vector2 b2 = {e.x - dir.x * as * 0.6f - nrm.x * as * 0.7f, e.y - dir.y * as * 0.6f - nrm.y * as * 0.7f};
    DrawTriangle(tip, b1, b2, col);
    DrawTriangle(tip, b2, b1, col);
    float tw = TxtW(label.c_str(), fs);
    float lx = e.x - dir.x * (tw * 0.5f + 16) - tw * 0.5f, ly = e.y - dir.y * 22 - fs * 0.5f;
    lx = std::max(4.0f, std::min(lx, c.W - tw - 4));
    ly = std::max(4.0f, std::min(ly, c.H - fs - 4));
    PlaceLabel(&lx, &ly, tw, fs + 2, c.H);
    Txt(label.c_str(), lx, ly, fs, Fade(col, 0.9f));
}

static void DrawMarkers(const App& app) {
    const Cam& c = app.cam;
    float R = std::min(105.0f, c.H * 0.16f);
    g_placed = {
        {0, 0, 432, 158},                                   // 左上パネル
        {c.W - 390, 0, 390, 112},                           // ターゲット情報
        {c.W * 0.5f - 290, 0, 580, 100},                    // 方位計
        {c.W - 2 * R - 44, c.H - 2 * R - 48, 2 * R + 44, 2 * R + 48},  // 速度計
        {0, c.H - 64, c.W, 64},                             // エンジン表示
    };
    if (app.autopilot) g_placed.push_back({c.W * 0.5f - 320, c.H - 118, 640, 40});  // 自動操縦表示
    if (!app.cockpit) {  // 自機の上にラベルを重ねない
        Vector2 sp;
        V3 rel = ShipWorld(app) - c.pos;
        if (Project(c, rel, &sp)) {
            float r = (float)(0.017 / Len(rel) * c.focal);
            g_placed.push_back({sp.x - r, sp.y - r * 0.7f, 2 * r, r * 1.5f});
        }
    }
    DrawMarker(app, app.target);  // ターゲットを優先して配置
    for (size_t i = 0; i < app.B.size(); i++) {
        const BodyDef& d = app.B[i].d;
        if ((int)i == app.target) continue;
        if (d.parent > 0) {
            if (!app.showMoonMarkers) continue;
            // 衛星: 母天体の近くにいる時、または画面上で母天体から十分離れて見える時だけ
            const Body& par = app.B[d.parent];
            bool near = Len(c.pos - par.pos) < par.soi;
            Vector2 p0, p1;
            bool sep = Project(c, app.B[i].pos - c.pos, &p1) && Project(c, par.pos - c.pos, &p0) && Vector2Distance(p0, p1) > 45;
            if (!near && !sep) continue;
            if (!near) {
                Vector2 p;
                if (!Project(c, app.B[i].pos - c.pos, &p) || p.x < 0 || p.y < 0 || p.x > c.W || p.y > c.H) continue;
            }
        }
        DrawMarker(app, (int)i);
    }
}

// 方位計（コンパス・テープ）
static void DrawCompass(const App& app) {
    const Cam& c = app.cam;
    const Ship& s = app.ship;
    const Body& ref = app.B[s.ref];
    V3 rel = ShipWorld(app) - ref.pos;
    bool local = s.ref != B_SUN && Len(rel) < ref.d.radius * 30;
    V3 upRef, north, east;
    if (local) {
        upRef = Norm(rel);
        east = Cross(ref.pole, upRef);
        if (Len(east) < 1e-9) east = Cross(V3(1, 0, 0), upRef);
        east = Norm(east);
        north = Cross(upRef, east);
    } else {
        upRef = Y_UP;
        north = V3(1, 0, 0);   // 黄経 0°（春分点方向）
        east = V3(0, 0, -1);   // 黄経 90°
    }
    auto headingOf = [&](V3 v) { return std::atan2(Dot(v, east), Dot(v, north)) / DEG; };
    double heading = headingOf(c.fwd);
    if (heading < 0) heading += 360;
    double pitch = std::asin(std::max(-1.0, std::min(1.0, Dot(c.fwd, upRef)))) / DEG;
    double roll = std::atan2(-Dot(c.right, upRef), Dot(c.up, upRef)) / DEG;

    // 左右の情報パネル（幅 420 / 380）と重ならない幅にする
    float W = std::max(240.0f, std::min(560.0f, c.W - 2 * 432)), cx = c.W * 0.5f, y0 = 10, h = 34;
    const double range = 60;  // 表示幅 ±60°
    DrawRectangleRounded({cx - W / 2, y0, W, h}, 0.3f, 6, {0, 10, 25, 150});
    BeginScissorMode((int)(cx - W / 2), (int)y0, (int)W, (int)h + 30);
    for (int deg = -180; deg <= 540; deg += 5) {
        double dd = deg - heading;
        if (dd < -range || dd > range) continue;
        float x = (float)(cx + dd / range * W / 2);
        bool major = deg % 15 == 0;
        DrawLineEx({x, y0 + h}, {x, y0 + h - (major ? 12.0f : 6.0f)}, 1.5f, {170, 200, 230, 200});
        if (deg % 45 == 0) {
            int d360 = ((deg % 360) + 360) % 360;
            char b[16];
            const char* lab = b;
            if (local && d360 % 90 == 0) lab = T(d360 == 0 ? UI_N : d360 == 90 ? UI_E : d360 == 180 ? UI_S : UI_W);
            else snprintf(b, sizeof b, "%d", d360);
            TxtC(lab, x, y0 + 2, 16, (local && d360 % 90 == 0) ? Color{255, 220, 120, 255} : Color{200, 220, 240, 255});
        }
    }
    // ターゲット方位
    V3 tdir = app.B[app.target].pos - c.pos;
    double th = headingOf(tdir) - heading;
    while (th > 180) th -= 360;
    while (th < -180) th += 360;
    float tx = (float)(cx + std::max(-range, std::min(range, th)) / range * W / 2);
    DrawTriangle({tx, y0 + h + 1}, {tx + 7, y0 + h + 10}, {tx - 7, y0 + h + 10}, {255, 220, 80, 255});
    EndScissorMode();
    DrawTriangle({cx, y0 + h - 2}, {cx - 8, y0 + h + 8}, {cx + 8, y0 + h + 8}, {255, 255, 255, 255});
    DrawTriangle({cx, y0 + h - 2}, {cx + 8, y0 + h + 8}, {cx - 8, y0 + h + 8}, {255, 255, 255, 255});

    char line[160];
    snprintf(line, sizeof line, "%s %05.1f°   %s %+05.1f°   %s %+06.1f°",
             local ? T(UI_HEADING) : T(UI_ECL_LON), heading, local ? T(UI_PITCH) : T(UI_ECL_LAT), pitch, T(UI_ROLL), roll);
    TxtC(line, cx, y0 + h + 12, 16, {200, 225, 255, 255});
    if (local) {
        snprintf(line, sizeof line, "(%s)", TR(ref.d.jp, ref.d.en));
        TxtC(line, cx, y0 + h + 31, 14, {150, 170, 200, 255});
    }

    // 簡易姿勢指示: 中央の水平線をロールに合わせて傾ける
    float rr = (float)(roll * DEG);
    Vector2 ctr = {c.W * 0.5f, c.H * 0.5f};
    float len = 70, gap = 26;
    Vector2 dx = {std::cos(rr), -std::sin(rr)};
    Color hc = {120, 255, 160, 140};
    DrawLineEx({ctr.x - dx.x * (gap + len), ctr.y - dx.y * (gap + len)}, {ctr.x - dx.x * gap, ctr.y - dx.y * gap}, 1.5f, hc);
    DrawLineEx({ctr.x + dx.x * gap, ctr.y + dx.y * gap}, {ctr.x + dx.x * (gap + len), ctr.y + dx.y * (gap + len)}, 1.5f, hc);
}

// 速度計（アナログメーター）
static void DrawSpeedometer(const App& app) {
    const Cam& c = app.cam;
    const Ship& s = app.ship;
    const Engine& e = ENGINES[s.engine];
    float R = std::min(105.0f, c.H * 0.16f);
    Vector2 ctr = {c.W - R - 26, c.H - R - 30};
    DrawCircleV(ctr, R + 8, {0, 10, 25, 170});
    DrawRing(ctr, R - 4, R, 135, 405, 90, {90, 120, 160, 200});

    // 目盛の単位
    double unitScale;  // km/s → 表示単位
    const char* unit;
    if (e.dialUnit == 0) { unitScale = 3600; unit = "km/h"; }
    else if (e.dialUnit == 1) { unitScale = 1; unit = "km/s"; }
    else { unitScale = 1.0 / C_KMS; unit = "c"; }
    double vmax = e.vmax / 1000.0;
    double vmaxU = vmax * unitScale;

    for (int i = 0; i <= 10; i++) {
        double u = i / 10.0;
        float a = (float)((135 + 270 * u) * DEG);
        Vector2 dir = {std::cos(a), std::sin(a)};
        float l = i % 2 == 0 ? 14.0f : 7.0f;
        DrawLineEx({ctr.x + dir.x * (R - 4), ctr.y + dir.y * (R - 4)}, {ctr.x + dir.x * (R - 4 - l), ctr.y + dir.y * (R - 4 - l)}, 2, {200, 220, 240, 230});
        if (i % 2 == 0) {
            char b[32];
            double v = vmaxU * u;
            if (e.dialUnit == 2) snprintf(b, sizeof b, "%.1f", v);
            else if (v >= 10000) snprintf(b, sizeof b, "%.0fk", v / 1000);
            else snprintf(b, sizeof b, "%.0f", v);
            float fs = 13;
            float tw = TxtW(b, fs);
            Txt(b, ctr.x + dir.x * (R - 32) - tw / 2, ctr.y + dir.y * (R - 32) - fs / 2, fs, {180, 200, 225, 255});
        }
    }
    // 現在速度の円弧と針
    double u = std::min(1.08, std::fabs(s.speed) / vmax);
    bool over = std::fabs(s.speed) > vmax * 1.001;
    Color needle = over ? Color{255, 80, 60, 255} : (s.speed < 0 ? Color{255, 170, 60, 255} : Color{90, 230, 255, 255});
    if (u > 0.002) DrawRing(ctr, R - 3, R + 4, 135, (float)(135 + 270 * std::min(1.0, u)), 60, Fade(needle, 0.8f));
    float a = (float)((135 + 270 * u) * DEG);
    DrawLineEx(ctr, {ctr.x + std::cos(a) * (R - 10), ctr.y + std::sin(a) * (R - 10)}, 3, needle);
    DrawCircleV(ctr, 6, needle);

    // デジタル表示
    char b[64];
    double v = s.speed * unitScale;
    if (e.dialUnit == 2) snprintf(b, sizeof b, "%.5f", v);
    else snprintf(b, sizeof b, "%s", Grouped(v, std::fabs(v) < 100 ? 1 : 0).c_str());
    TxtC(b, ctr.x, ctr.y + R * 0.28f, 24, WHITE);
    TxtC(unit, ctr.x, ctr.y + R * 0.28f + 24, 14, {170, 190, 215, 255});
    if (over) TxtC(T(UI_OVERSPEED), ctr.x, ctr.y - R * 0.45f, 14, needle);
    else if (s.speed < 0) TxtC(T(UI_REVERSE), ctr.x, ctr.y - R * 0.45f, 14, needle);
    TxtC(TR(e.jp, e.en), ctr.x, ctr.y + R + 10, 15, e.flame);
}

static void DrawEngineBar(const App& app) {
    const Cam& c = app.cam;
    float bw = std::min(118.0f, (c.W - 300) / ENGINE_COUNT), bh = 40;
    float total = bw * ENGINE_COUNT, x0 = (c.W - total) * 0.5f - 60, y0 = c.H - bh - 14;
    if (x0 < 10) x0 = 10;
    for (int i = 0; i < ENGINE_COUNT; i++) {
        const Engine& e = ENGINES[i];
        bool cur = i == app.ship.engine;
        Rectangle r = {x0 + i * bw + 2, y0, bw - 4, bh};
        DrawRectangleRounded(r, 0.2f, 6, cur ? Color{40, 70, 110, 220} : Color{0, 10, 25, 150});
        if (cur) DrawRectangleRoundedLinesEx(r, 0.2f, 6, 2, e.flame);
        char b[64];
        snprintf(b, sizeof b, "%d", i + 1);
        Txt(b, r.x + 6, r.y + 3, 13, cur ? WHITE : Color{140, 160, 190, 255});
        float fs = TxtW(TR(e.jp, e.en), 15) > bw - 14 ? 12.0f : 15.0f;
        TxtC(TR(e.jp, e.en), r.x + r.width / 2, r.y + 18, fs, cur ? e.flame : Color{170, 185, 205, 255});
    }
}

// ワープの 2D 演出: 星が放射状の光の筋になり、奥から光の粒が流れ、周辺が青く染まり、移動の瞬間に閃光
static void DrawWarpEffect(const App& app) {
    if (app.warpT < 0) return;
    const Cam& c = app.cam;
    double wi = WarpIntensity(app), flash = WarpFlash(app);
    Vector2 ctr = {c.W * 0.5f, c.H * 0.5f};
    float halfDiag = std::sqrt(c.W * c.W + c.H * c.H) * 0.5f;

    BeginBlendMode(BLEND_ADDITIVE);
    // 星の光の筋（画面中央から外へ伸びる）
    if (wi > 0.01) {
        float len = (float)(1.6 * wi);
        for (size_t i = 0; i < app.stars.size(); i++) {
            const Star& st = app.stars[i];
            if (st.size <= 0 && i % 2) continue;  // 暗い星は半分に間引く（画面が白飛びしないように）
            Vector2 p;
            if (!Project(c, st.dir, &p)) continue;
            Vector2 v = {p.x - ctr.x, p.y - ctr.y};
            float r = std::sqrt(v.x * v.x + v.y * v.y) / halfDiag;
            if (r > 1.2f) continue;
            Vector2 q = {p.x + v.x * len * r, p.y + v.y * len * r};
            Color col = st.col;
            col.a = (unsigned char)(std::min(1.0, 0.25 + wi) * col.a);
            DrawLineEx(p, q, st.size > 0 ? 2.0f : 1.0f, col);
        }
    }
    // 奥から手前へ流れる光の粒（トンネル）
    float tunnel = (float)std::min(1.0, wi * 1.6);
    if (tunnel > 0.01) {
        for (int i = 0; i < 220; i++) {
            double h1 = std::fmod(std::sin(i * 12.9898) * 43758.5453, 1.0);
            double h2 = std::fmod(std::sin(i * 78.233) * 12543.123, 1.0);
            double ang = std::fabs(h1) * 2 * PI;
            double z = 1.0 - std::fmod(std::fabs(h2) + app.warpPhase * (0.6 + 0.4 * std::fabs(h1)), 1.0);  // 1(奥)→0(手前)
            double z2 = std::min(1.0, z + 0.05 + 0.25 * wi);
            float r1 = (float)(halfDiag * 0.06 / (z + 0.05)), r2 = (float)(halfDiag * 0.06 / (z2 + 0.05));
            Vector2 dir = {(float)std::cos(ang), (float)std::sin(ang)};
            Vector2 a = {ctr.x + dir.x * r2, ctr.y + dir.y * r2};
            Vector2 b = {ctr.x + dir.x * r1, ctr.y + dir.y * r1};
            float fade = (float)(tunnel * std::min(1.0, (1.0 - z) * 3.0));
            Color col = i % 3 == 0 ? Color{255, 220, 170, 255} : Color{140, 190, 255, 255};
            DrawLineEx(a, b, 1.5f + 2.5f * (float)(1.0 - z), Fade(col, fade * 0.8f));
        }
    }
    EndBlendMode();

    // 周辺の青い滲みと閃光
    DrawCircleGradient((int)ctr.x, (int)ctr.y, halfDiag, {40, 90, 200, 0}, Fade({60, 120, 255, 255}, (float)(0.55 * wi)));
    if (flash > 0.003) DrawRectangle(0, 0, (int)c.W, (int)c.H, Fade({225, 238, 255, 255}, (float)std::min(1.0, flash)));
}

static void DrawHUD(App& app) {
    const Cam& c = app.cam;
    const Ship& s = app.ship;
    const Engine& e = ENGINES[s.engine];
    V3 sw = ShipWorld(app);

    // 中央の照準
    Vector2 ctr = {c.W * 0.5f, c.H * 0.5f};
    DrawCircleLinesV(ctr, 12, {120, 255, 160, 160});
    DrawLineEx({ctr.x - 4, ctr.y}, {ctr.x + 4, ctr.y}, 1, {120, 255, 160, 200});
    DrawLineEx({ctr.x, ctr.y - 4}, {ctr.x, ctr.y + 4}, 1, {120, 255, 160, 200});

    // 最寄りの天体
    int nearest = 0;
    double nearestAlt = 1e300;
    for (size_t i = 0; i < app.B.size(); i++) {
        V3 rel = sw - app.B[i].pos;
        double d = Len(rel);
        double alt = d - SurfaceRadius(app.B[i], rel * (1.0 / d));
        if (alt < nearestAlt) { nearestAlt = alt; nearest = (int)i; }
    }

    // 左上パネル
    float x = 14, y = 12, fs = 17, lh = 22;
    DrawRectangleRounded({6, 6, 420, lh * 6 + 14}, 0.06f, 6, {0, 10, 25, 150});
    char b[256];
    snprintf(b, sizeof b, "%s [%d] %s", T(UI_ENGINE), s.engine + 1, TR(e.jp, e.en));
    Txt(b, x, y, fs + 2, e.flame); y += lh + 2;
    std::string vmaxS = FmtSpeed(e.vmax / 1000.0);
    snprintf(b, sizeof b, "%s %s m/s%s   %s %s", T(UI_ACCEL), Grouped(e.accel, e.accel < 100 ? 2 : 0).c_str(), g_jpFont ? "²" : "^2", T(UI_MAXSPD), vmaxS.c_str());
    Txt(b, x, y, fs - 1, {190, 205, 225, 255}); y += lh;
    snprintf(b, sizeof b, "%s %s", T(UI_SPEED), FmtSpeed(s.speed).c_str());
    Txt(b, x, y, fs + 1, WHITE); y += lh;
    snprintf(b, sizeof b, "%s %s    %s %s  %s %s", T(UI_FRAME), TR(app.B[s.ref].d.jp, app.B[s.ref].d.en), T(UI_NEAREST),
             TR(app.B[nearest].d.jp, app.B[nearest].d.en), T(UI_ALT), FmtDist(std::max(0.0, nearestAlt)).c_str());
    Txt(b, x, y, fs, {200, 220, 240, 255}); y += lh;
    snprintf(b, sizeof b, "%s %s", T(UI_DATE), FmtDate(app.tDays).c_str());
    Txt(b, x, y, fs, {200, 220, 240, 255}); y += lh;
    std::string ts = app.paused ? T(UI_PAUSED) : (app.timeIdx == 0 ? std::string("x1 (") + T(UI_REALTIME) + ")" : TIME_SCALE_NAMES[app.timeIdx]);
    snprintf(b, sizeof b, "%s %s", T(UI_TSCALE), ts.c_str());
    Txt(b, x, y, fs, app.paused ? Color{255, 160, 80, 255} : (app.timeIdx == 0 ? Color{140, 230, 160, 255} : Color{255, 210, 120, 255}));

    // 右上: ターゲット情報
    const Body& tb = app.B[app.target];
    double tdist = std::max(0.0, SurfaceDist(tb, sw));
    float pw = 380, px = c.W - pw - 6;
    double scale = TIME_SCALES[app.timeIdx];
    // 時間倍率をかけている時は、実際に待つ時間も併記
    auto eta = [&](double simSec) {
        std::string r = FmtDuration(simSec);
        if (scale > 1 && std::isfinite(simSec) && r != "---")
            r += std::string("  (") + T(UI_REAL_TIME_ETA) + " " + FmtDuration(simSec / scale) + ")";
        return r;
    };
    DrawRectangleRounded({px, 6, pw, lh * 4 + 14}, 0.06f, 6, {0, 10, 25, 150});
    x = px + 10; y = 12;
    snprintf(b, sizeof b, "%s: %s  (Tab)", T(UI_TARGET), TR(tb.d.jp, tb.d.en));
    Txt(b, x, y, fs + 1, {255, 220, 80, 255}); y += lh;
    snprintf(b, sizeof b, "%s %s   (%s %s)", T(UI_DIST), FmtDist(tdist).c_str(), T(UI_LIGHT), FmtDuration(tdist / C_KMS).c_str());
    Txt(b, x, y, fs, WHITE); y += lh;
    snprintf(b, sizeof b, "%s %s", T(UI_ETA), s.speed > 0 ? eta(tdist / s.speed).c_str() : "---");
    Txt(b, x, y, fs - 1, {200, 220, 240, 255}); y += lh;
    snprintf(b, sizeof b, "%s %s", T(UI_ETA_MAX), eta(tdist / (e.vmax / 1000.0)).c_str());
    Txt(b, x, y, fs - 1, {200, 220, 240, 255});

    DrawCompass(app);
    DrawSpeedometer(app);
    DrawEngineBar(app);

    if (app.autopilot) {
        std::string ap = std::string(T(UI_AP)) + " >> " + TR(tb.d.jp, tb.d.en) + "   " + T((UiId)app.apPhase) + "   " +
                         T(UI_AP_REMAIN) + " " + FmtDist(app.apRemain);
        if (app.apAutoEngine) ap += std::string("   [") + T(UI_AP_AUTO_ENGINE) + "]";
        float tw = TxtW(ap.c_str(), 18);
        float ax = c.W * 0.5f - tw * 0.5f - 12, ay = c.H - 112;
        float blink = 0.75f + 0.25f * std::sin((float)GetTime() * 4);
        DrawRectangleRounded({ax, ay, tw + 24, 28}, 0.4f, 6, {0, 40, 20, 190});
        DrawRectangleRoundedLinesEx({ax, ay, tw + 24, 28}, 0.4f, 6, 1.5f, Fade({120, 255, 160, 255}, blink));
        Txt(ap.c_str(), ax + 12, ay + 4, 18, {150, 255, 180, 255});
    }

    Txt(T(UI_HELP_HINT), 12, c.H - 24, 15, {150, 170, 200, 220});
    snprintf(b, sizeof b, "%d FPS", GetFPS());
    Txt(b, 12, c.H - 44, 13, {110, 130, 160, 200});

    if (app.toastTime > 0) {
        float a = (float)std::min(1.0, app.toastTime);
        TxtC(app.toast.c_str(), c.W * 0.5f, c.H * 0.24f, 24, Fade({255, 240, 200, 255}, a));
    }

    if (app.showHelp) {
        const int lines = UI_HELP15 - UI_HELP1 + 1;
        float w = std::min(c.W - 20, 860.0f), h = 44 + lines * 26.0f, hx = (c.W - w) / 2, hy = (c.H - h) / 2;
        DrawRectangleRounded({hx, hy, w, h}, 0.04f, 6, {0, 8, 20, 225});
        DrawRectangleRoundedLinesEx({hx, hy, w, h}, 0.04f, 6, 1.5f, {90, 130, 180, 255});
        TxtC(T(UI_HELP_TITLE), c.W * 0.5f, hy + 10, 22, {255, 220, 120, 255});
        for (int i = 0; i < lines; i++)
            Txt(T((UiId)(UI_HELP1 + i)), hx + 24, hy + 44 + i * 26.0f, i >= 12 ? 15 : 17, i >= 12 ? Color{170, 220, 190, 255} : Color{215, 228, 245, 255});
    }
}

// ---------------------------------------------------------------- main
int main(int argc, char** argv) {
    Options opt = ParseArgs(argc, argv);
    if (opt.dumpMoons) {
        // 描画座標（黄道系を並べ替えたもの）のまま、時刻 = 現在 + --hours
        std::vector<Body> B = InitBodies();
        double t = (std::time(nullptr) - J2000_UNIX) / 86400.0 + opt.hours / 24.0;
        UpdateBodies(B, t);
        printf("t %.6f\n", t);
        for (const Body& b : B)
            if (b.d.parent > 0) {
                V3 r = b.pos - B[b.d.parent].pos;
                printf("%s %.3f %.3f %.3f\n", b.d.en, r.x, r.y, r.z);
            }
        return 0;
    }
    LoadSettings();
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "Solar System Voyager");
    SetExitKey(KEY_NULL);
    {
        EmbeddedFile f = IconPng();
        Image icon = LoadImageFromMemory(".png", f.data, (int)f.size);
        SetWindowIcon(icon);
        UnloadImage(icon);
    }

    App app;
    app.B = InitBodies();

    // フォント
    std::string allText = "°²·×〜";
    for (int i = 0; i < UI_COUNT; i++) allText += UI_JP[i];
    for (const Body& b : app.B) allText += b.d.jp;
    for (const Engine& e : ENGINES) allText += e.jp;
    g_jpFont = fontload::LoadJapaneseFont(allText, 36, &g_font);
    if (!g_jpFont) g_font = GetFontDefault();
    // 言語: 起動オプション > 保存された設定 > 日本語（フォントがあれば）
    std::string lang = opt.lang ? opt.lang : GetSetting("lang");
    SetLanguage(lang != "en");
    // 画質: 起動オプション > 保存された設定 > 高
    std::string q = GetSetting("quality");
    if (!q.empty()) app.quality = std::max(0, std::min(2, atoi(q.c_str())));
    if (opt.quality >= 0) app.quality = std::min(2, opt.quality);
    // 効果音（自動スクリーンショットでは鳴らさない）
    if (!opt.shot) {
        sfx::Init(GetSetting("sound") == "0");
        sfx::MusicInit(GetSetting("music") == "0");
    }

    app.shader = LoadShaderFromMemory(PLANET_VS, PLANET_FS);
    app.locLight = GetShaderLocation(app.shader, "lightDir");
    app.locLightObj = GetShaderLocation(app.shader, "lightDirObj");
    app.locC1 = GetShaderLocation(app.shader, "color1");
    app.locC2 = GetShaderLocation(app.shader, "color2");
    app.locAtm = GetShaderLocation(app.shader, "atmColor");
    app.locParams = GetShaderLocation(app.shader, "params");
    app.locDetail = GetShaderLocation(app.shader, "detail");
    app.locOcc = GetShaderLocation(app.shader, "occ");
    app.locOccN = GetShaderLocation(app.shader, "occN");
    app.locSunAng = GetShaderLocation(app.shader, "sunAng");
    app.mat = LoadMaterialDefault();
    LoadEarthTextures(app);
    LoadMoonTextures(app);
    app.mat.shader = app.shader;
    app.sphere = GenUVSphere(128, 256);
    app.ring = GenRing(74658.0f / 60268.0f, 136775.0f / 60268.0f, 360);
    app.shipShader = LoadShaderFromMemory(SHIP_VS, SHIP_FS);
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
    app.stars = MakeStars();
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
    if (opt.engine >= 0) app.ship.engine = std::min(opt.engine, ENGINE_COUNT - 1);
    app.cockpit = opt.cockpit;
    app.camYaw = opt.camYaw; app.camPitch = opt.camPitch; app.camDist = opt.camDist;
    app.autopilot = opt.autopilot;
    app.apAutoEngine = opt.autoEngine;
    app.showHelp = opt.help;
    if (!opt.shot) DisableCursor(); else app.mouseCaptured = false;

    int frame = 0;
    while (!WindowShouldClose()) {
        double dt = std::min((double)GetFrameTime(), 0.1);
        if (opt.shot) dt = 1.0 / 60.0;
        if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_Q)) break;

        HandleInput(app, dt);
        if (opt.warpAnimAt >= 0 && frame == (int)(opt.warpAnimAt * 60)) StartWarp(app, app.target, opt.land);
        if (opt.saveBookmark && frame == 1) SaveBookmark(app);
        if (opt.gotoBookmark > 0 && frame == 60) {
            std::vector<Bookmark> bms = LoadBookmarks(app);
            if (opt.gotoBookmark <= (int)bms.size()) {
                StartWarp(app, bms[opt.gotoBookmark - 1].body, false);
                app.warpBookmark = opt.gotoBookmark - 1;
            }
        }
        UpdateWarp(app, dt);
        UpdateAutopilot(app, dt);
        if (opt.shot && frame * dt < opt.throttleSec) {
            const Engine& e = ENGINES[app.ship.engine];
            app.ship.speed = std::min(e.vmax / 1000.0, app.ship.speed + e.accel / 1000.0 * SimDt(app, dt));
            app.ship.thrusting = true;
        }
        double simDt = SimDt(app, dt);
        app.tDays += simDt / 86400.0;
        UpdateBodies(app.B, app.tDays);
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

        BeginDrawing();
        // 画質「低」: 星空と 3D を縮小バッファに描いて拡大する（HUD は等倍）
        bool lowRes = QUALITY_SCALE[app.quality] < 1.0f;
        float rk = QUALITY_SCALE[app.quality] * GetRenderWidth() / std::max(1, GetScreenWidth());
        if (lowRes) {
            int w = std::max(1, (int)(GetScreenWidth() * rk)), h = std::max(1, (int)(GetScreenHeight() * rk));
            if (app.lowRes.texture.width != w || app.lowRes.texture.height != h) {
                if (app.lowRes.id) UnloadRenderTexture(app.lowRes);
    sfx::Shutdown();
                app.lowRes = LoadRenderTexture(w, h);
                SetTextureFilter(app.lowRes.texture, TEXTURE_FILTER_BILINEAR);
            }
            BeginTextureMode(app.lowRes);
            // rlPushMatrix は rlgl 内部の変換行列を使い続けて毎フレーム縮小が重なるので、modelview に直接掛ける
            rlLoadIdentity();
            rlScalef(rk, rk, 1);
        }
        ClearBackground({1, 2, 6, 255});
        DrawStars(app);
        DrawSunGlowAndDots(app);  // 3D より先に描き、手前の天体や自機で隠れるようにする

        Camera3D dummy = {{0, 0, 0}, {0, 0, -1}, {0, 1, 0}, 60, CAMERA_PERSPECTIVE};
        BeginMode3D(dummy);
        if (app.showOrbits) DrawOrbits(app);
        DrawBodies(app);
        DrawShipPass(app, dt);
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

        if (app.warpT < 0) DrawMarkers(app);
        DrawWarpEffect(app);
        DrawHUD(app);

        frame++;
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

    UnloadMesh(app.sphere);
    UnloadMesh(app.ring);
    UnloadMesh(app.shipMesh);
    UnloadShader(app.shipShader);
    UnloadShader(app.shader);
    if (app.earthTex) { UnloadTexture(app.earthColor); UnloadTexture(app.earthData); }
    if (app.moonTex) { UnloadTexture(app.moonColor); UnloadTexture(app.moonData); }
    if (app.lowRes.id) UnloadRenderTexture(app.lowRes);
    sfx::Shutdown();
    if (g_jpFont) UnloadFont(g_font);
    CloseWindow();
    return 0;
}
