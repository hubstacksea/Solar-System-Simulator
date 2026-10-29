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
    X(SKY_OFF, "星座 OFF", "Constellations OFF")                                            \
    X(SKY_LINES, "星座線", "Constellation lines")                                           \
    X(SKY_NAMES, "星座線と名前", "Constellation lines & names")                             \
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
    X(HELP9, "V : 視点切替    O : 軌道表示    L : 衛星マーカー    C : 星座    Y : 日時指定    K : イベント一覧    U : 自動ツアー", "V : view    O : orbits    L : moon markers    C : constellations    Y : set date    K : events    U : guided tour") \
    X(HELP10, "[ / ] : 時間倍率    P : 時間停止    Backspace : 現在時刻に同期    Z : 写真モード    R : 重力モード", "[ / ] : time scale    P : pause    Backspace : sync to now    Z : photo mode    R : gravity mode") \
    X(HELP11, "I : マウス上下反転    M : 効果音    Shift+M : 音楽    F11 : フルスクリーン    F12 : スクリーンショット", "I : invert mouse    M : sound    Shift+M : music    F11 : fullscreen    F12 : screenshot") \
    X(HELP12, "Esc : マウス解放/捕捉    J : 言語切替 (日本語/English)    F9 : 画質    F10 : 設定    Ctrl+Q : 終了", "Esc : release/capture mouse    J : language (日本語/English)    F9 : quality    F10 : settings    Ctrl+Q : quit") \
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
    X(EV_TITLE, "天文イベント", "Astronomical events") \
    X(EV_KEYS, "↑↓ : 選択    Enter : その日時・場所へワープ    Tab : 天文 / 彗星 / 探査機    Esc / K : 閉じる", "Up/Down : select    Enter : warp to that time and place    Tab : sky / comets / spacecraft    Esc / K : close") \
    X(EV_PROBE_TITLE, "探査機のできごと", "Spacecraft events") \
    X(EV_COMET_TITLE, "彗星", "Comets") \
    X(EV_COMET_NOTE, "1980〜2070 年の近日点と地球への最接近。尾の長さは見た目の演出です", "Perihelia and closest approaches to Earth, 1980-2070. Tail lengths are illustrative") \
    X(EV_PERIHELION, "%sが近日点を通過", "%s at perihelion") \
    X(EV_COMET_EARTH, "%sが地球に最接近", "%s: closest approach to Earth") \
    X(EV_FROM_SUN, "太陽から %s", "%s from the Sun") \
    X(EV_FROM_EARTH, "地球から %s", "%s from Earth") \
    X(EV_TRANSIT, "%sの太陽面通過", "Transit of %s") \
    X(EV_TRANSIT_D, "太陽の中心から半径の %.2f 倍", "%.2f solar radii from the center") \
    X(EV_CONJ, "%sと%sが接近", "Conjunction: %s and %s") \
    X(EV_CONJ_D, "角距離 %.2f°", "separation %.2f°") \
    X(EV_OPPOSITION, "%sが衝", "Opposition of %s") \
    X(EV_PROBE_NOTE, "軌跡は JPL Horizons のデータ。惑星への接近は実際の距離と向きで再現します", "Trajectories from JPL Horizons. Planetary encounters are reproduced at their real distance and direction") \
    X(EV_FLYBY, "%sが%sに最接近", "%s: closest approach to %s") \
    X(EV_ARRIVE, "%sが%sに到着（周回開始）", "%s arrives at %s (orbit insertion)") \
    X(EV_END, "%sの運用終了（%sへ突入）", "%s: end of mission (enters %s)") \
    X(EV_ARRIVE_SMALL, "%sが%sに到着", "%s arrives at %s") \
    X(EV_END_LAND, "%sの運用終了（%sに降下）", "%s: end of mission (descends to %s)") \
    X(EV_IMPACT, "%sが%sに衝突", "%s impacts %s") \
    X(EV_GROUND, "%s（地上から）", "%s (from the ground)") \
    X(EV_IMPACT_D, "公転周期 11.92 → 11.37 時間", "orbital period 11.92 → 11.37 h") \
    X(EV_ALT, "高度 %s", "altitude %s") \
    X(EV_NOTE, "簡易計算のため、時刻は数分〜数十分、日食の位置は数百 km ずれることがあります。太陽面通過は 150 年先まで", "Simplified calculation: times may be off by minutes, solar eclipse locations by a few hundred km. Transits up to 150 years ahead") \
    X(EV_SEARCHING, "計算中…", "Calculating...") \
    X(EV_NONE, "見つかりませんでした", "No events found") \
    X(EV_SOLAR_TOTAL, "皆既日食", "Total solar eclipse") \
    X(EV_SOLAR_ANNULAR, "金環日食", "Annular solar eclipse") \
    X(EV_SOLAR_PARTIAL, "部分日食", "Partial solar eclipse") \
    X(EV_LUNAR_TOTAL, "皆既月食", "Total lunar eclipse") \
    X(EV_LUNAR_PARTIAL, "部分月食", "Partial lunar eclipse") \
    X(EV_SHADOW, "木星面を%sの影が通過", "Shadow of %s crosses Jupiter") \
    X(EV_CENTER, "食の中心 %s付近", "centered near %s") \
    X(EV_GO, "ワープ: %s  %s", "Warp: %s  %s") \
    X(TOUR, "ツアー %d / %d", "Tour %d / %d") \
    X(TOUR_KEYS, "Enter : 次へ    Shift+Enter : 前へ    U : ツアー終了", "Enter : next    Shift+Enter : previous    U : end tour") \
    X(TOUR_END, "ツアー終了", "Tour ended") \
    X(TOUR_NONE, "ツアーの場面が見つかりません", "No tour stops found") \
    X(PHOTO_0, "写真モード: 解除", "Photo mode: off") \
    X(SET_TITLE, "設定", "Settings") \
    X(SET_KEYS, "↑↓ : 選択    ←→ / Enter : 変更    Esc / F10 : 閉じる（変更はすぐ保存されます）", "Up/Down : select    Left/Right / Enter : change    Esc / F10 : close (changes are saved at once)") \
    X(SET_LANG, "言語", "Language") \
    X(SET_QUALITY, "画質", "Quality") \
    X(SET_SOUND, "効果音", "Sound effects") \
    X(SET_MUSIC, "音楽", "Music") \
    X(SET_SKY, "星座", "Constellations") \
    X(SET_ORBITS, "軌道表示", "Orbit lines") \
    X(SET_ASTEROIDS, "小惑星帯・トロヤ群・外縁天体の粒", "Asteroids, Trojans and TNOs") \
    X(SET_MARKERS, "衛星・探査機のマーカー", "Moon and spacecraft markers") \
    X(SET_INVERT, "マウス上下反転", "Invert mouse") \
    X(SET_FULLSCREEN, "フルスクリーン", "Fullscreen") \
    X(SET_ON, "オン", "On") \
    X(SET_PHYSICS, "飛行モード", "Flight model") \
    X(SET_ARCADE, "アーケード", "Arcade") \
    X(SET_GRAVITY, "重力", "Gravity") \
    X(PHYS_ON, "重力モード: 天体の重力を受けて飛びます（W / S で機首の向きに加減速、X で相対停止）", "Gravity mode: you now fall under the bodies' gravity (W / S thrust along the nose, X stops relative motion)") \
    X(PHYS_OFF, "アーケードモード: 機首の向きへ進みます", "Arcade mode: you fly where the nose points") \
    X(PHYS_ORBIT, "近点 高度 %s / 遠点 高度 %s / 周期 %s（%s のまわり）", "periapsis %s / apoapsis %s / period %s (around %s)") \
    X(PHYS_ESCAPE, "%s の重力から脱出する軌道", "escape trajectory from %s") \
    X(PHYS_SLOW, "重力の計算が追いつかないので、時間の進みを抑えています", "Time is slowed so the gravity calculation can keep up") \
    X(SET_OFF, "オフ", "Off") \
    X(SET_Q0, "低（解像度 60%）", "Low (60% resolution)") \
    X(SET_Q1, "中", "Medium") \
    X(SET_Q2, "高", "High") \
    X(SET_SKY0, "表示しない", "Off") \
    X(SET_SKY1, "星座線", "Lines") \
    X(SET_SKY2, "星座線と名前", "Lines and names") \
    X(ASTEROIDS_ON, "小惑星などの粒 ON", "Asteroids ON") \
    X(ASTEROIDS_OFF, "小惑星などの粒 OFF", "Asteroids OFF") \
    X(PHOTO_1, "写真モード: 表示を隠す（Z で自機も隠す）", "Photo mode: overlays hidden (Z again to hide the ship)") \
    X(PHOTO_2, "写真モード: 表示と自機を隠す", "Photo mode: overlays and ship hidden") \
    X(DATE_TITLE, "日時を指定（UTC）", "Set date and time (UTC)") \
    X(DATE_KEYS, "数字を入力    Backspace : 1 文字消す    Enter : 決定    Esc / Y : 取消", "Type digits    Backspace : delete    Enter : apply    Esc / Y : cancel") \
    X(DATE_SET, "日時: %s", "Date: %s") \
    X(DATE_BAD, "日時が正しくありません（1800〜2200 年）", "Invalid date (years 1800-2200)") \
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

#ifndef APP_VERSION
#define APP_VERSION "dev"
#endif
// 操作説明・設定画面の下に出す表記
static const char* CREDIT = "Solar System Voyager " APP_VERSION "   (c) 2026 hubstacksea   MIT License   "
                            "https://github.com/hubstacksea/Solar-System-Simulator";

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
    bool active = true;     // 探査機: その時刻に飛んでいるか（打ち上げ前・運用終了後は false で、表示しない）
    int shape = -1;         // 実際の形（g_shapes の番号。-1 は楕円体）
    int map = -1;           // 探査機の画像による地図（App::maps の番号。-1 はなし）
    double primeOffset = 0; // 自転角 0 の X 軸から IAU の基準（赤道と ICRF 赤道の昇交点）までの角度 rad
};

// ---------------------------------------------------------------- 彗星・小惑星の実際の形（assets/shapes.bin）
// 頂点は本体の座標系（X, 自転軸, Z）で、天体の半径を 1 とする。外側の表面までの半径の表で高度や衝突を求める
struct Shape {
    std::string name;
    std::vector<float> vert, norm;          // 頂点・法線（xyz の並び）
    std::vector<unsigned short> idx;
    int nlon = 0, nlat = 0;
    std::vector<float> rad;                 // [緯度][経度] km（緯度 -90〜90°、経度はモデルの x 軸から反時計回り）
    float rmax = 0;
};
static std::vector<Shape> g_shapes;

static void LoadShapes() {
    EmbeddedFile f = ShapesBin();
    const unsigned char* p = f.data;
    const unsigned char* end = f.data + f.size;
    auto rd = [&](void* dst, size_t n) { if (p + n > end) return false; std::memcpy(dst, p, n); p += n; return true; };
    uint32_t count = 0;
    if (!rd(&count, 4)) return;
    for (uint32_t k = 0; k < count; k++) {
        char name[33] = {0};
        float scale;
        uint32_t nv, nf;
        uint16_t nlon, nlat;
        if (!rd(name, 32) || !rd(&scale, 4) || !rd(&nv, 4) || !rd(&nf, 4) || !rd(&nlon, 2) || !rd(&nlat, 2)) return;
        Shape sh;
        sh.name = name;
        std::vector<int16_t> v(nv * 3);
        std::vector<uint16_t> fi(nf * 3);
        sh.rad.resize((size_t)nlon * nlat);
        if (!rd(v.data(), v.size() * 2) || !rd(fi.data(), fi.size() * 2) || !rd(sh.rad.data(), sh.rad.size() * 4)) return;
        sh.nlon = nlon;
        sh.nlat = nlat;
        // モデルの (x, y, z: 北) → 本体の座標系 (X = x, Y = z, Z = -y)。km のまま（描画時に半径で割る）
        sh.vert.resize(nv * 3);
        for (uint32_t i = 0; i < nv; i++) {
            sh.vert[i * 3] = v[i * 3] * scale;
            sh.vert[i * 3 + 1] = v[i * 3 + 2] * scale;
            sh.vert[i * 3 + 2] = -v[i * 3 + 1] * scale;
        }
        sh.idx.assign(fi.begin(), fi.end());
        sh.norm.assign(nv * 3, 0.0f);
        for (uint32_t t = 0; t < nf; t++) {
            const float* a = &sh.vert[fi[t * 3] * 3];
            const float* b = &sh.vert[fi[t * 3 + 1] * 3];
            const float* c = &sh.vert[fi[t * 3 + 2] * 3];
            Vector3 n = Vector3CrossProduct({b[0] - a[0], b[1] - a[1], b[2] - a[2]}, {c[0] - a[0], c[1] - a[1], c[2] - a[2]});
            for (int j = 0; j < 3; j++) {
                float* q = &sh.norm[fi[t * 3 + j] * 3];
                q[0] += n.x; q[1] += n.y; q[2] += n.z;
            }
        }
        for (uint32_t i = 0; i < nv; i++) {
            Vector3 n = Vector3Normalize({sh.norm[i * 3], sh.norm[i * 3 + 1], sh.norm[i * 3 + 2]});
            sh.norm[i * 3] = n.x; sh.norm[i * 3 + 1] = n.y; sh.norm[i * 3 + 2] = n.z;
        }
        for (float r : sh.rad) sh.rmax = std::max(sh.rmax, r);
        g_shapes.push_back(std::move(sh));
    }
}

// 描画用のメッシュ（半径 1 の単位）
static Mesh ShapeMesh(const Shape& sh, double radius) {
    Mesh m = {0};
    int nv = (int)sh.vert.size() / 3;
    m.vertexCount = nv;
    m.triangleCount = (int)sh.idx.size() / 3;
    m.vertices = (float*)MemAlloc(nv * 3 * sizeof(float));
    m.normals = (float*)MemAlloc(nv * 3 * sizeof(float));
    m.texcoords = (float*)MemAlloc(nv * 2 * sizeof(float));
    m.indices = (unsigned short*)MemAlloc(sh.idx.size() * sizeof(unsigned short));
    for (int i = 0; i < nv * 3; i++) {
        m.vertices[i] = (float)(sh.vert[i] / radius);
        m.normals[i] = sh.norm[i];
    }
    std::memset(m.texcoords, 0, nv * 2 * sizeof(float));
    std::memcpy(m.indices, sh.idx.data(), sh.idx.size() * sizeof(unsigned short));
    UploadMesh(&m, false);
    return m;
}

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

static V3 PlanetPos(const BodyDef& d0, double tDays) {
    // 要素を時刻 t の値へ（JPL の変化率。平均経度の変化率が公転の速さ）
    double T = tDays / 36525.0;
    BodyDef d = d0;
    d.a_au += d0.da * T; d.e += d0.de * T; d.inc += d0.dinc * T;
    d.varpi += d0.dvarpi * T; d.node += d0.dnode * T;
    double L = d0.L0 + (d0.dL != 0 ? d0.dL * T : 360.0 / (365.25 * std::pow(d0.a_au, 1.5)) * tDays);
    double M = std::remainder(L - d.varpi, 360.0) * DEG;  // -π〜π
    // ケプラー方程式（ハレー彗星の e = 0.97 でも収束する初期値）
    double E = M + 0.85 * d.e * (M >= 0 ? 1 : -1);
    for (int i = 0; i < 50; i++) {
        double dE = (E - d.e * std::sin(E) - M) / (1 - d.e * std::cos(E));
        E -= dE;
        if (std::fabs(dE) < 1e-12) break;
    }
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

// 地球の中心。軌道要素は地球と月の重心のものなので、月の位置の分だけずらす（月の質量比 0.0121505）
static V3 EarthPos(const BodyDef& d, double tDays) { return PlanetPos(d, tDays) - MoonGeoPos(tDays) * 0.0121505; }

// ---------------------------------------------------------------- 探査機の軌跡
// assets/probes.bin（tools/make_probe_assets.py、JPL Horizons）。区間ごとに太陽中心か惑星中心の位置・速度の点列を持ち、
// 点の間は 3 次エルミート補間。惑星中心の区間は「シミュレータの惑星 + 相対位置」なので、惑星の位置の誤差があっても
// フライバイは実際の距離と向きで起きる。前後の太陽中心の区間には、境目でのずれ（シミュレータの惑星 − 実際の惑星）を
// 境目から PROBE_BLEND_DAYS かけて足し込み、位置が跳ばないようにつなぐ
struct ProbeSeg {
    int body = 0;             // 0: 太陽中心, それ以外: 惑星の番号
    V3 trueStart, trueEnd;    // 区間の始まり・終わりでの実際の惑星の位置
    V3 deltaStart, deltaEnd;  // 同じ時刻のシミュレータの惑星 − 実際の惑星
    std::vector<double> t;    // J2000 からの日数
    std::vector<V3> p, v;     // 描画座標 km, km/日
};
struct ProbeTrack { std::vector<ProbeSeg> segs; };
static std::vector<ProbeTrack> g_probes;
static const double PROBE_BLEND_DAYS = 20;

static void LoadProbeTracks() {
    EmbeddedFile f = ProbesBin();
    const unsigned char* q = f.data;
    const unsigned char* end = f.data + f.size;
    auto rd = [&](void* dst, size_t n) {
        if (q + n > end) return false;
        std::memcpy(dst, q, n);
        q += n;
        return true;
    };
    auto ecl = [](const double* a) { return EclToRender(a[0], a[1], a[2]); };
    uint32_t np = 0;
    if (!rd(&np, 4)) return;
    for (uint32_t i = 0; i < np; i++) {
        char name[32];
        uint32_t ns = 0;
        if (!rd(name, 32) || !rd(&ns, 4)) return;
        ProbeTrack tr;
        for (uint32_t k = 0; k < ns; k++) {
            ProbeSeg sg;
            int32_t body;
            double e[6];
            uint32_t n;
            if (!rd(&body, 4) || !rd(e, sizeof e) || !rd(&n, 4)) return;
            sg.body = body;
            sg.trueStart = ecl(e);
            sg.trueEnd = ecl(e + 3);
            for (uint32_t j = 0; j < n; j++) {
                double tp[4];
                float v[3];
                if (!rd(tp, sizeof tp) || !rd(v, sizeof v)) return;
                sg.t.push_back(tp[0]);
                sg.p.push_back(ecl(tp + 1));
                sg.v.push_back(EclToRender(v[0] * 86400.0, v[1] * 86400.0, v[2] * 86400.0));
            }
            if (sg.t.size() >= 2) tr.segs.push_back(sg);
        }
        g_probes.push_back(tr);
    }
}

// 区間内の位置（基準天体から）
static V3 SegPos(const ProbeSeg& s, double t) {
    size_t i = std::upper_bound(s.t.begin(), s.t.end(), t) - s.t.begin();
    i = std::max<size_t>(1, std::min(i, s.t.size() - 1));
    double t0 = s.t[i - 1], h = s.t[i] - t0, u = (t - t0) / h;
    double h00 = 2 * u * u * u - 3 * u * u + 1, h10 = u * u * u - 2 * u * u + u, h01 = -2 * u * u * u + 3 * u * u, h11 = u * u * u - u * u;
    return s.p[i - 1] * h00 + s.v[i - 1] * (h * h10) + s.p[i] * h01 + s.v[i] * (h * h11);
}

static double BlendIn(double x) {   // x = 0 で 1、x >= 1 で 0（滑らかに）
    if (x >= 1) return 0;
    x = std::max(0.0, x);
    return 1 - x * x * (3 - 2 * x);
}

// 探査機の位置。planetAt(番号) はその時刻のシミュレータの惑星の位置。データの範囲外では *active = false
// （脱出する機体はデータの終わりの速度のまま進む）
template <class F>
static V3 ProbePosAt(const BodyDef& d, double t, F planetAt, bool* active, int* segOut = nullptr) {
    *active = false;
    if (d.probe < 0 || d.probe >= (int)g_probes.size() || g_probes[d.probe].segs.empty()) return V3(1e15, 0, 0);
    const std::vector<ProbeSeg>& S = g_probes[d.probe].segs;
    double t0 = S.front().t.front(), t1 = S.back().t.back(), coast = 0;
    *active = true;
    if (t < t0) { *active = false; t = t0; }
    if (t > t1) {
        if (d.probeCoast) coast = t - t1; else *active = false;
        t = t1;
    }
    size_t k = 0;
    while (k + 1 < S.size() && S[k].t.back() < t) k++;
    const ProbeSeg& s = S[k];
    if (segOut) *segOut = (int)k;
    V3 pos = SegPos(s, t);
    if (s.body > 0) return pos + planetAt(s.body);
    if (k > 0 && S[k - 1].body > 0) pos = pos + S[k - 1].deltaEnd * BlendIn((t - s.t.front()) / PROBE_BLEND_DAYS);
    if (k + 1 < S.size() && S[k + 1].body > 0) pos = pos + S[k + 1].deltaStart * BlendIn((s.t.back() - t) / PROBE_BLEND_DAYS);
    return pos + s.v.back() * coast;
}

// 地球を回る人工衛星の地球中心の位置 km（描画座標）。TLE の平均軌道要素を 2 体問題で進め、地球の扁平（J2）による
// 昇交点と近地点の移動と、平均運動の変化（大気の抵抗）を加える。SGP4 より粗いが、元期の近くでは数 km〜数十 km
static V3 SatellitePos(const BodyDef& d, double t) {
    const double MU = 398600.4418, J2 = 1.08263e-3, RE = 6378.137;
    double dt = t - d.tleEpoch, dts = dt * 86400.0;
    double n0 = d.tleN * 2 * PI / 86400.0;   // rad/s
    double a = std::cbrt(MU / (n0 * n0)), e = d.tleE, inc = d.tleI * DEG;
    double p = a * (1 - e * e), k = 1.5 * J2 * (RE / p) * (RE / p) * n0;
    double node = d.tleNode * DEG - k * std::cos(inc) * dts;
    double argp = d.tleArgp * DEG + k * (2 - 2.5 * std::sin(inc) * std::sin(inc)) * dts;
    double dd = std::max(-365.0, std::min(365.0, dt));   // 平均運動の変化は 1 年分まで
    double M = std::remainder(d.tleM * DEG + n0 * dts + 2 * PI * d.tleNdot * dd * dd, 2 * PI);
    double E = M;
    for (int i = 0; i < 8; i++) E -= (E - e * std::sin(E) - M) / (1 - e * std::cos(E));
    double xp = a * (std::cos(E) - e), yp = a * std::sqrt(1 - e * e) * std::sin(E);
    double cw = std::cos(argp), sw = std::sin(argp), cO = std::cos(node), sO = std::sin(node), cI = std::cos(inc), sI = std::sin(inc);
    double x = (cw * cO - sw * sO * cI) * xp + (-sw * cO - cw * sO * cI) * yp;
    double y = (cw * sO + sw * cO * cI) * xp + (-sw * sO + cw * cO * cI) * yp;
    double z = (sw * sI) * xp + (cw * sI) * yp;
    const double eps = 23.4392911 * DEG;   // 赤道座標 → 黄道座標 → 描画座標
    return EclToRender(x, y * std::cos(eps) + z * std::sin(eps), -y * std::sin(eps) + z * std::cos(eps));
}

static void EquatorBasis(V3 pole, V3* e1, V3* e2) {
    V3 a = Cross(Y_UP, pole);
    if (Len(a) < 1e-6) a = V3(1, 0, 0);
    *e1 = Norm(a);
    *e2 = Cross(*e1, pole);
}

// 惑星のシミュレータ上の位置（任意の時刻）
static V3 SimPlanetPos(const std::vector<Body>& B, int i, double t) { return i == B_EARTH ? EarthPos(B[i].d, t) : PlanetPos(B[i].d, t); }

static std::vector<Body> InitBodies() {
    std::vector<Body> B;
    for (const BodyDef& d : BuildSolarSystem()) {
        Body b;
        b.d = d;
        B.push_back(b);
    }
    if (g_shapes.empty()) LoadShapes();
    for (size_t i = 0; i < B.size(); i++) {
        Body& b = B[i];
        const BodyDef& d = b.d;
        b.pole = d.inheritPole ? B[d.parent].pole : EquToRender(d.poleRA, d.poleDec);
        EquatorBasis(b.pole, &b.ex, &b.ez);
        if (!std::isnan(d.w0)) {
            // IAU の W は、天体の赤道が ICRF の赤道と交わる昇交点 Q（赤経 = 自転軸の赤経 + 90°）から測る
            V3 Q = Norm(Cross(EquToRender(0, 90), b.pole));
            b.primeOffset = std::atan2(-Dot(Q, b.ez), Dot(Q, b.ex));
        }
        b.outer = d.rings ? d.radius * 2.37 : d.radius;
        if (d.probe >= 0 || d.tleN > 0) {
            b.soi = 2000;    // 近くにいる自機は探査機と一緒に移動する
            b.outer = d.tleN > 0 ? 0.06 : 0.03;  // 機体の大きさ（ISS は端から端まで 109 m。ワープの到着距離に使う）
        } else if (d.parent == 0) {
            b.soi = d.a_au * AU_KM * std::pow(d.mass / B[0].d.mass, 0.4);
            // 離心率の大きい軌道は、離心近点角の等間隔だけでは近日点付近が粗いので、真近点角の等間隔の点も混ぜる
            std::vector<double> Es;
            for (int k = 0; k <= 720; k++) Es.push_back(2 * PI * k / 720.0);
            if (d.e > 0.3)
                for (int k = 1; k < 720; k++) {
                    double nu = -PI + 2 * PI * k / 720.0;
                    double E = 2 * std::atan(std::sqrt((1 - d.e) / (1 + d.e)) * std::tan(nu / 2));
                    Es.push_back(E < 0 ? E + 2 * PI : E);
                }
            std::sort(Es.begin(), Es.end());
            for (double E : Es) b.orbit.push_back(PlanetOrbitPoint(d, E));
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
        for (size_t k = 0; k < g_shapes.size(); k++)
            if (g_shapes[k].name == d.en) {
                b.shape = (int)k;
                b.outer = g_shapes[k].rmax;
            }
    }
    // 探査機: 惑星中心の区間の境目での、シミュレータの惑星と実際の惑星の差
    if (g_probes.empty()) LoadProbeTracks();
    // 人工衛星は軌跡の代わりに軌道要素で動く。探査機と同じ扱いにするため、空の軌跡の番号を割り当てる
    for (Body& b : B)
        if (b.d.tleN > 0 && b.d.probe < 0) {
            b.d.probe = (int)g_probes.size();
            g_probes.emplace_back();
        }
    // ディモルフォスの公転の位置: 衝突の瞬間に、DART の最後の点から衝突までまっすぐ進んだ所にいるように合わせる
    for (Body& m : B) {
        if (m.d.moonPeriod2 <= 0) continue;
        for (const Body& pr : B) {
            if (pr.d.probe < 0 || pr.d.probe >= (int)g_probes.size() || !pr.d.probeEnd || g_probes[pr.d.probe].segs.empty()) continue;
            const ProbeSeg& sg = g_probes[pr.d.probe].segs.back();
            if (sg.body != m.d.parent) continue;
            V3 r = sg.p.back() + sg.v.back() * (m.d.moonChangeT - sg.t.back());
            double th = std::atan2(Dot(r, m.oQ), Dot(r, m.oN));
            m.d.moonPhase = (th - 2 * PI * m.d.moonChangeT / m.d.moonPeriod) / DEG;
        }
    }
    for (ProbeTrack& tr : g_probes)
        for (ProbeSeg& sg : tr.segs) {
            if (sg.body <= 0 || sg.body >= (int)B.size()) continue;
            sg.deltaStart = SimPlanetPos(B, sg.body, sg.t.front()) - sg.trueStart;
            sg.deltaEnd = SimPlanetPos(B, sg.body, sg.t.back()) - sg.trueEnd;
        }
    return B;
}

static void UpdateBodies(std::vector<Body>& B, double t) {
    for (Body& b : B) {
        const BodyDef& d = b.d;
        if (d.parent < 0) {
            b.pos = V3();
        } else if (d.tleN > 0) {
            b.pos = B[B_EARTH].pos + SatellitePos(d, t);
            b.active = t >= d.launch;
        } else if (d.probe >= 0) {
            // 惑星は先に更新済み（探査機は配列の最後）
            b.pos = ProbePosAt(d, t, [&](int i) { return B[i].pos; }, &b.active);
        } else if (d.parent == 0) {
            b.pos = &b == &B[B_EARTH] ? EarthPos(d, t) : PlanetPos(d, t);
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
            if (d.moonPeriod2 > 0 && t > d.moonChangeT)   // ディモルフォス: DART の衝突で公転周期が縮んだ
                th = d.moonPhase * DEG + 2 * PI * (d.moonChangeT / d.moonPeriod + (t - d.moonChangeT) / d.moonPeriod2);
            if (d.libA != 0 || d.libB != 0) {   // ミマス・テティスの秤動（周期 70.56 年）
                double w = 2 * PI * t / (70.56 * 365.25);
                th += (d.libA * std::sin(w) + d.libB * std::cos(w)) * DEG;
            }
            b.pos = B[d.parent].pos + (b.oN * std::cos(th) + b.oQ * std::sin(th)) * d.moonA;
        }
        double rp = d.rotPeriod == 0 ? d.moonPeriod : d.rotPeriod;
        b.spin = std::fmod(d.spin0 * DEG + 2 * PI * t / rp, 2 * PI);
        if (!std::isnan(d.w0)) b.spin = std::fmod(b.primeOffset + d.w0 * DEG + 2 * PI * t / rp, 2 * PI);
        if (d.parent > 0 && d.rotPeriod == 0) {
            // 衛星の自転は公転と同期: 経度 0°（本体 X 軸）を常に母天体へ向ける
            V3 u = B[d.parent].pos - b.pos;
            b.spin = std::atan2(-Dot(u, b.ez), Dot(u, b.ex));
        }
    }
}

// 楕円体表面までの半径（dir は単位ベクトル）
static double SurfaceRadius(const Body& b, V3 dir) {
    if (b.shape >= 0) {
        // 実際の形: 本体の座標系での向きから、外側の表面までの半径の表を補間する
        const Shape& sh = g_shapes[b.shape];
        double cs = std::cos(b.spin), sn = std::sin(b.spin);
        V3 X = b.ex * cs - b.ez * sn, Z = b.ex * sn + b.ez * cs;
        double x = Dot(dir, X), y = -Dot(dir, Z), z = Dot(dir, b.pole);
        double fl = (std::asin(std::max(-1.0, std::min(1.0, z))) / PI + 0.5) * (sh.nlat - 1);
        double fo = std::fmod(std::atan2(y, x) / (2 * PI) + 1.0, 1.0) * sh.nlon;
        int i0 = std::min((int)fl, sh.nlat - 2), j0 = (int)fo % sh.nlon, j1 = (j0 + 1) % sh.nlon;
        double u = fl - i0, w = fo - std::floor(fo);
        auto R = [&](int i, int j) { return (double)sh.rad[(size_t)i * sh.nlon + j]; };
        return (R(i0, j0) * (1 - w) + R(i0, j1) * w) * (1 - u) + (R(i0 + 1, j0) * (1 - w) + R(i0 + 1, j1) * w) * u;
    }
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
// ヒッパルコス星表の 8 等級までの恒星と、88 星座の線・名前（tools/make_sky_assets.py で作った埋め込みデータ）
struct Star { V3 dir; Color col; float size; float mag; };  // size 0 は 1 ピクセルの点。明るい順に並ぶ
struct Constellation { V3 label; std::string jp, en; std::vector<std::vector<V3>> lines; };
struct StarName { V3 dir; float mag; std::string jp, en; };
struct Sky {
    std::vector<Star> stars;
    std::vector<Constellation> cons;
    std::vector<StarName> names;
};

// 色指数 B-V → 見た目の色（青白い星〜赤い星。白に寄せて淡くする）
static Color StarColor(double bv, unsigned char a) {
    static const double T[][4] = {{-0.4, 155, 178, 255}, {0.0, 200, 214, 255}, {0.4, 248, 246, 255},
                                  {0.8, 255, 236, 214}, {1.2, 255, 212, 160}, {1.6, 255, 190, 128}, {2.2, 255, 170, 100}};
    const int n = sizeof T / sizeof T[0];
    bv = std::max(T[0][0], std::min(T[n - 1][0], bv));
    int i = 0;
    while (i < n - 2 && bv > T[i + 1][0]) i++;
    double f = (bv - T[i][0]) / (T[i + 1][0] - T[i][0]), rgb[3];
    for (int k = 0; k < 3; k++) rgb[k] = (T[i][k + 1] + (T[i + 1][k + 1] - T[i][k + 1]) * f) * 0.7 + 255 * 0.3;
    return {(unsigned char)rgb[0], (unsigned char)rgb[1], (unsigned char)rgb[2], a};
}

static Sky LoadSky() {
    Sky sky;
    EmbeddedFile sb = StarsBin();
    uint32_t n = 0;
    if (sb.size >= 4) std::memcpy(&n, sb.data, 4);
    n = (uint32_t)std::min<size_t>(n, (sb.size - 4) / 12);
    sky.stars.reserve(n);
    for (uint32_t i = 0; i < n; i++) {
        const unsigned char* p = sb.data + 4 + 12 * (size_t)i;
        float ra, dec;
        int16_t mag100, bv1000;
        std::memcpy(&ra, p, 4);
        std::memcpy(&dec, p + 4, 4);
        std::memcpy(&mag100, p + 8, 2);
        std::memcpy(&bv1000, p + 10, 2);
        float mag = mag100 / 100.0f;
        // 4.5 等より明るい星は円で、暗い星は 1 ピクセルの点の濃さで明るさを表す
        float size = mag < 4.5f ? std::min(3.2f, 1.0f + (4.5f - mag) * 0.38f) : 0.0f;
        float a = mag < 5.0f ? 255.0f : std::max(30.0f, 235.0f - (mag - 5.0f) * 65.0f);
        sky.stars.push_back({EquToRender(ra, dec), StarColor(bv1000 / 1000.0, (unsigned char)a), size, mag});
    }

    EmbeddedFile st = SkyTxt();
    std::string text((const char*)st.data, st.size);
    size_t pos = 0;
    while (pos < text.size()) {
        size_t eol = text.find('\n', pos);
        if (eol == std::string::npos) eol = text.size();
        std::vector<std::string> f;
        for (size_t q = pos; q <= eol;) {
            size_t tab = std::min(text.find('\t', q), eol);
            f.push_back(text.substr(q, tab - q));
            q = tab + 1;
        }
        pos = eol + 1;
        if (f[0] == "C" && f.size() >= 5) {
            sky.cons.push_back({EquToRender(atof(f[1].c_str()), atof(f[2].c_str())), f[3], f[4], {}});
        } else if (f[0] == "L" && !sky.cons.empty()) {
            std::vector<V3> line;
            for (size_t k = 1; k + 1 < f.size(); k += 2) line.push_back(EquToRender(atof(f[k].c_str()), atof(f[k + 1].c_str())));
            sky.cons.back().lines.push_back(line);
        } else if (f[0] == "S" && f.size() >= 6) {
            sky.names.push_back({EquToRender(atof(f[1].c_str()), atof(f[2].c_str())), (float)atof(f[3].c_str()), f[4], f[5]});
        }
    }
    return sky;
}

// ---------------------------------------------------------------- 小惑星帯などの粒（assets/asteroids.bin）
// 軌道要素から 2 体問題で位置を求め、点で描く（軌道表示 O と、設定の「小惑星」で表示）
struct AsteroidBelt {
    std::vector<float> a, e, n, m0;   // km, 離心率, 平均運動 rad/日, J2000 での平均近点角 rad
    std::vector<V3> P, Q;             // 軌道面の基底（近日点の向きと、それに直交する向き。描画座標）
    std::vector<unsigned char> kind;  // 0: 小惑星帯, 1: 地球近傍, 2: トロヤ群, 3: ケンタウルス族・太陽系外縁天体
    std::vector<V3> pos;              // 今の位置 km（太陽中心）
    double posT = NAN;                // pos を計算した時刻
    int pending = -1;                 // 計算し直している途中の組（0: 偶数番, 1: 奇数番, -1: 終わっている）
};

static float HalfToFloat(uint16_t h) {
    uint32_t sign = (h >> 15) & 1, exp = (h >> 10) & 31, man = h & 1023, f;
    if (exp == 0) {
        float v = std::ldexp((float)man, -24);
        return sign ? -v : v;
    }
    if (exp == 31) f = (sign << 31) | 0x7f800000 | (man << 13);
    else f = (sign << 31) | ((exp + 112) << 23) | (man << 13);
    float v;
    std::memcpy(&v, &f, 4);
    return v;
}

static AsteroidBelt LoadAsteroids() {
    AsteroidBelt ab;
    EmbeddedFile f = AsteroidsBin();
    uint32_t n = 0;
    if (f.size >= 4) std::memcpy(&n, f.data, 4);
    n = (uint32_t)std::min<size_t>(n, (f.size - 4) / 15);
    for (uint32_t k = 0; k < n; k++) {
        const unsigned char* p = f.data + 4 + 15 * (size_t)k;
        float a;
        uint16_t h[5];
        std::memcpy(&a, p, 4);
        std::memcpy(h, p + 4, 10);
        double e = HalfToFloat(h[0]), inc = HalfToFloat(h[1]) * DEG, om = HalfToFloat(h[2]) * DEG, w = HalfToFloat(h[3]) * DEG;
        double cw = std::cos(w), sw = std::sin(w), cO = std::cos(om), sO = std::sin(om), cI = std::cos(inc), sI = std::sin(inc);
        ab.P.push_back(EclToRender(cw * cO - sw * sO * cI, cw * sO + sw * cO * cI, sw * sI));
        ab.Q.push_back(EclToRender(-sw * cO - cw * sO * cI, -sw * sO + cw * cO * cI, cw * sI));
        ab.a.push_back((float)(a * AU_KM));
        ab.e.push_back((float)e);
        ab.n.push_back((float)(0.01720209895 / std::pow(a, 1.5)));
        ab.m0.push_back((float)(HalfToFloat(h[4]) * DEG));
        ab.kind.push_back(p[14]);
    }
    ab.pos.resize(ab.a.size());
    return ab;
}

// 時刻 t の位置。半日以上進んだら計算し直す。数が多いので、偶数番と奇数番を 1 フレームずつ交互に計算し、
// float で計算する（ケプラー方程式は 2 次の初期値から、離心率の小さいものは 2 回の反復で足りる）
static void UpdateAsteroids(AsteroidBelt& ab, double t) {
    if (ab.pending < 0) {
        if (!std::isnan(ab.posT) && std::fabs(t - ab.posT) < 0.5) return;
        ab.posT = t;
        ab.pending = 0;
    }
    size_t start = (size_t)ab.pending;
    ab.pending = ab.pending == 0 ? 1 : -1;
    const float TWO_PI = 6.2831853f;
    for (size_t k = start; k < ab.a.size(); k += 2) {
        float e = ab.e[k];
        double Md = std::fmod(ab.m0[k] + ab.n[k] * t, 2 * PI);
        float M = (float)Md;
        if (M > 3.14159265f) M -= TWO_PI;
        if (M < -3.14159265f) M += TWO_PI;
        float sM = sinf(M), cM = cosf(M);
        float E = M + e * sM * (1 + e * cM);
        int iters = e < 0.3f ? 2 : 6;
        float sE = 0, cE = 1;
        for (int it = 0; it < iters; it++) {
            sE = sinf(E); cE = cosf(E);
            E -= (E - e * sE - M) / (1 - e * cE);
        }
        sE = sinf(E); cE = cosf(E);
        float a = ab.a[k];
        ab.pos[k] = ab.P[k] * (double)(a * (cE - e)) + ab.Q[k] * (double)(a * sqrtf(1 - e * e) * sE);
    }
}

// ---------------------------------------------------------------- 自機
struct Ship {
    int ref = B_EARTH;  // 基準系の天体（自機はこの天体と一緒に公転移動する）
    V3 local;           // 基準天体からの相対位置 km
    Quaternion q = QuaternionIdentity();
    double speed = 0;   // km/s（機首方向。重力モードでは速度ベクトルの大きさ）
    V3 vel;             // 重力モード: 基準天体に対する速度 km/s
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
    bool listEvents = false;    // 天文イベントを出力して終了（確認用）
    int gotoEvent = -1;         // 1 秒後に天文イベントへワープ（確認用、1〜）
    double probeView = 0;       // ワープの後、ターゲットの探査機をこの距離 m から斜めに見る（確認用）
    double faceSunEl = NAN;     // 太陽の方位を向き、この仰角 deg を見る（--at と使う確認用）
    int tour = 0;               // 1 秒後に自動ツアーをこの場面から始める（確認用、1〜）
    int photo = 0;              // 写真モード 0〜2（確認用）
    bool settings = false;      // 設定画面を開いた状態で始める（確認用）
    bool physics = false;       // 重力モードで始める（確認用）
    bool showEvents = false, showDate = false;  // 一覧・日時入力を開いた状態で始める（確認用）
    int eventsPage = 0;                         // 一覧のページ 0: 天文, 1: 彗星, 2: 探査機（確認用）
    double timeScale = 1;
    const char* lang = nullptr;  // "ja" / "en"
    int quality = -1;            // 0〜2（確認用）
    int sky = -1;                // 星座の表示 0〜2（確認用）
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
        else if (a == "--list-events") o.listEvents = true;
        else if (a == "--show-events") o.showEvents = true;
        else if (a == "--events-page") o.eventsPage = atoi(next());
        else if (a == "--show-date") o.showDate = true;
        else if (a == "--goto-event") o.gotoEvent = atoi(next());
        else if (a == "--probe-view") o.probeView = atof(next());
        else if (a == "--face-sun") o.faceSunEl = atof(next());
        else if (a == "--tour") o.tour = atoi(next());
        else if (a == "--photo") o.photo = atoi(next());
        else if (a == "--settings") o.settings = true;
        else if (a == "--physics") o.physics = true;
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
        else if (a == "--sky") o.sky = atoi(next());
        else if (a == "--hours") o.hours = atof(next());
        else if (a == "--at") { o.atLat = atof(next()); o.atLon = atof(next()); o.atAlt = atof(next()); }
    }
    return o;
}

// ---------------------------------------------------------------- アプリ本体
// 天文イベント（日食・月食・木星面の衛星の影の通過・太陽面通過・惑星の接近・衝）、彗星、探査機のできごと
enum EventKind { EV_SOLAR_TOTAL, EV_SOLAR_ANNULAR, EV_SOLAR_PARTIAL, EV_LUNAR_TOTAL, EV_LUNAR_PARTIAL, EV_SHADOW,
                 EV_TRANSIT, EV_CONJ, EV_OPPOSITION, EV_PERIHELION, EV_COMET_EARTH, EV_ECLIPSE_GROUND,
                 EV_FLYBY, EV_ARRIVE, EV_END, EV_IMPACT };
struct AstroEvent {
    int kind = 0;
    double t = 0;          // 最大の時刻（J2000 からの日数）
    int moon = -1;         // 影の通過: 影を落とす衛星
    double lat = 0, lon = 0;  // 日食: 食の中心（地球の緯度・経度 deg）
    int probe = -1, body = -1;  // 探査機のできごと: 探査機と惑星の番号。ほかは主な天体（太陽面通過・衝の惑星、彗星）
    int body2 = -1;             // 惑星の接近: もう一方の惑星。地上から見る日食: 元の日食の種類
    double dist = 0;            // 最接近: 惑星の中心からの距離 km。彗星: 太陽か地球からの距離 km、
                                // 太陽面通過: 太陽の中心からの距離（太陽の半径が 1）、接近: 角距離 deg、衝: 地球からの距離 km
};

struct App {
    std::vector<Body> B;
    Sky sky;
    int skyMode = 0;  // 0: 星のみ, 1: 星座線, 2: 星座線と名前
    AsteroidBelt belt;
    bool showAsteroids = true;  // 小惑星帯などの粒（軌道表示 O が有効な時に表示）
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
    bool eventZoom = false;       // 太陽面通過・惑星の接近へワープした後のズーム（右クリック・移動・ワープで解除）
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
    bool eventsOpen = false;              // 天文イベント一覧（K キー）
    int eventsPage = 0;                   // 0: 天文イベント, 1: 彗星, 2: 探査機のできごと（Tab で切り替え）
    std::vector<AstroEvent> probeEvents;  // 探査機のできごと（起動時に軌跡から求める）
    std::vector<AstroEvent> cometEvents;  // 彗星の近日点と地球への最接近（起動時に求める）
    std::vector<AstroEvent> events;
    int eventSel = 0;
    int warpEvent = -1;                   // イベントへのワープ中はその番号
    int photoMode = 0;                    // 写真モード（Z キー）0: 通常, 1: 表示を隠す, 2: 表示と自機を隠す
    bool physics = false;                 // 重力モード（R キー）: 天体の重力を受けて飛ぶ（オフ: 機首の向きへ進むアーケード式）
    bool tourOn = false;                  // 自動ツアー（U キー）
    int tourIdx = 0;
    double tourTime = 0;                  // 今の場面に着いてからの実時間 秒
    std::vector<AstroEvent> tourEvents;
    std::vector<int> tourStops;           // tourEvents の各場面の TOUR の番号
    bool settingsOpen = false;            // 設定画面（F10 キー）
    int settingsSel = 0;
    bool dateOpen = false;                // 日時指定（Y キー）
    std::string dateDigits;               // 入力中の数字（YYYYMMDDhhmm）
    double warpPhase = 0;   // 光の筋の流れ（強さに応じて進む）
    Mesh sphere{}, ring{};
    std::vector<Mesh> shapeMesh;          // 実際の形（g_shapes と同じ並び）
    Mesh shipMesh{};
    Mesh probeMesh[PM_COUNT]{};
    Material shipMat{};
    Shader shipShader{};
    int locSunDir = -1, locSunColor = -1, locFillDir = -1, locFillColor = -1, locEngColor = -1, locEngGlow = -1, locNavBlink = -1;
    Material mat{};
    Shader shader{};
    Texture2D earthColor{}, earthData{};  // 地球の実データ（読めなければ手続き生成で描く）
    Texture2D earthMonths{}, earthClouds{};  // 地球の 12 か月の地表の色と雲（読めなければ 4 月の色と手続きの雲）
    bool earthTex = false;
    Texture2D moonColor{}, moonData{};    // 月の実データ
    bool moonTex = false;
    Texture2D milkyWay{};                 // 天の川の光（赤道座標の全天画像）
    Texture2D ringTex{};                  // 土星の環の不透明度（半径方向）
    struct BodyMap { std::string name; int kind = 0; bool height = false; float mean = 0.5f; Texture2D color{}, mask{}, data{}; };
    std::vector<BodyMap> maps;            // 探査機の画像による地図（assets/maps.bin）
    int locMapInfo = -1, locMapInfo2 = -1;
    Shader skyShader{};                   // 大気の中から見た空
    int locSkyUp = -1, locSkySun = -1, locSkyBeta = -1, locSkyMie = -1, locSkyOzone = -1, locSkyDensity = -1, locSkySunVis = -1;
    int quality = 2;                      // 画質 0: 低, 1: 中, 2: 高
    RenderTexture2D lowRes{};             // 画質「低」で 3D を描く縮小バッファ
    int locLight = -1, locLightObj = -1, locC1 = -1, locC2 = -1, locAtm = -1, locParams = -1, locDetail = -1;
    int locOcc = -1, locOccN = -1, locSunAng = -1;
    int locAtmBeta = -1, locAtmMie = -1, locAtmOzone = -1, locAtmHaze = -1, locCamUp = -1;
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
        if (!b.active) continue;
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

// ---------------------------------------------------------------- 天文イベント
// 太陽 → 影を落とす天体 の延長線（影の軸）に対する、相手の天体の中心の位置。
// along: 影を落とす天体から軸に沿った距離、perp: 軸からの距離（km）
static void ShadowAxis(V3 sun, V3 caster, V3 target, double* along, double* perp) {
    V3 u = Norm(caster - sun);
    V3 d = target - caster;
    *along = Dot(d, u);
    *perp = Len(d - u * *along);
}

// 地球の天体固定座標での緯度・経度 deg
static void LatLonOf(const Body& b, V3 dir, double* lat, double* lon) {
    V3 X, Z;
    BodyFixedAxes(b, &X, &Z);
    *lat = std::asin(std::max(-1.0, std::min(1.0, Dot(dir, b.pole)))) / DEG;
    *lon = std::atan2(-Dot(dir, Z), Dot(dir, X)) / DEG;
}

// 日食・月食の判定に使う量（地球と月だけ計算するので速い）
struct EclipseGeom { double solarPerp, solarAlong, lunarPerp, lunarAlong; bool newSide; };
static EclipseGeom EclipseAt(const std::vector<Body>& B, double t) {
    V3 E = EarthPos(B[B_EARTH].d, t), M = E + MoonGeoPos(t), S;
    EclipseGeom g;
    ShadowAxis(S, M, E, &g.solarAlong, &g.solarPerp);   // 月の影と地球
    ShadowAxis(S, E, M, &g.lunarAlong, &g.lunarPerp);   // 地球の影と月
    g.newSide = Dot(M - E, S - E) > 0;                   // 月が太陽側（新月の側）
    return g;
}

// f(t) の極小を [a, b] で黄金分割探索
template <class F> static double MinimizeT(F f, double a, double b) {
    const double r = 0.6180339887;
    double c = b - (b - a) * r, d = a + (b - a) * r, fc = f(c), fd = f(d);
    for (int i = 0; i < 40; i++) {
        if (fc < fd) { b = d; d = c; fd = fc; c = b - (b - a) * r; fc = f(c); }
        else { a = c; c = d; fc = fd; d = a + (b - a) * r; fd = f(d); }
    }
    return (a + b) * 0.5;
}

// 日食の種類と中心位置（食の最大の時刻 t）
static bool ClassifySolar(std::vector<Body>& B, double t, AstroEvent* ev) {
    UpdateBodies(B, t);
    const double Rs = B[B_SUN].d.radius, Rm = B[B_MOON].d.radius, Re = B[B_EARTH].d.radius;
    V3 S = B[B_SUN].pos, E = B[B_EARTH].pos, M = B[B_MOON].pos;
    double along, perp;
    ShadowAxis(S, M, E, &along, &perp);
    if (along <= 0) return false;
    double dSM = Len(M - S);
    double rPen = Rm + along * (Rs + Rm) / dSM;          // 地球の位置での半影の半径
    if (perp > Re + rPen) return false;
    V3 u = Norm(M - S), c = M + u * along;               // 軸上で地球中心に最も近い点
    V3 hit;
    if (perp < Re) {                                     // 影の軸が地球に当たる（中心食）
        double k = std::sqrt(Re * Re - perp * perp);
        hit = c - u * k;
        double sSurf = Dot(hit - M, u);
        double rUmb = Rm - sSurf * (Rs - Rm) / dSM;      // 地表での本影の半径（負なら金環）
        ev->kind = rUmb > 0 ? EV_SOLAR_TOTAL : EV_SOLAR_ANNULAR;
    } else {
        hit = E + Norm(c - E) * Re;
        ev->kind = EV_SOLAR_PARTIAL;
    }
    ev->t = t;
    LatLonOf(B[B_EARTH], Norm(hit - E), &ev->lat, &ev->lon);
    return true;
}

static bool ClassifyLunar(std::vector<Body>& B, double t, AstroEvent* ev) {
    UpdateBodies(B, t);
    const double Rs = B[B_SUN].d.radius, Rm = B[B_MOON].d.radius, Re = B[B_EARTH].d.radius;
    V3 S = B[B_SUN].pos, E = B[B_EARTH].pos, M = B[B_MOON].pos;
    double along, perp;
    ShadowAxis(S, E, M, &along, &perp);
    if (along <= 0) return false;
    double rUmb = Re * 1.02 - along * (Rs - Re) / Len(E - S);   // 月の位置での本影（大気で 2% 広がる）
    if (perp - Rm >= rUmb) return false;                        // 半影月食は目立たないので含めない
    ev->kind = perp + Rm < rUmb ? EV_LUNAR_TOTAL : EV_LUNAR_PARTIAL;
    ev->t = t;
    return true;
}

// 2 つの向きの間の角度 rad（小さい角度でも精度が落ちない式）
static double AngleBetween(V3 a, V3 b) { return std::atan2(Len(Cross(a, b)), Dot(a, b)); }

// [a, b] を step ごとに調べ、f の極小（f < limit）の時刻を細かく求めて返す
template <class F> static std::vector<double> FindMinima(F f, double a, double b, double step, double limit) {
    std::vector<double> out;
    double p0 = f(a - step), p1 = f(a);
    for (double t = a; t < b; t += step) {
        double p2 = f(t + step);
        if (p1 < p0 && p1 <= p2 && p1 < limit) out.push_back(MinimizeT(f, t - step, t + step));
        p0 = p1; p1 = p2;
    }
    return out;
}

// 惑星のイベント（地球の中心から見る）:
//  - 水星・金星の太陽面通過: 150 年先までの、それぞれ次の 2 回
//  - 惑星どうしの接近: 3 年分。角距離 1° 未満で、どちらも太陽から 15° 以上離れているもの
//  - 衝（地球をはさんで太陽の反対側）: 火星〜海王星の 3 年分
static void FindPlanetEvents(const std::vector<Body>& B, double t0, std::vector<AstroEvent>* out) {
    auto pos = [&](int i, double t) { return SimPlanetPos(B, i, t); };
    for (int p : {B_MERCURY, B_VENUS}) {
        auto f = [&](double t) {
            V3 e = pos(B_EARTH, t), q = pos(p, t) - e;
            return Len(q) < Len(e) && Dot(q, e) < 0 ? AngleBetween(q, e * -1.0) : 1e9;
        };
        int found = 0;
        for (double tm : FindMinima(f, t0, t0 + 150 * 365.25, 1.0, 0.02)) {
            V3 e = pos(B_EARTH, tm), q = pos(p, tm) - e;
            double sunR = std::asin(B[B_SUN].d.radius / Len(e)), plR = std::asin(B[p].d.radius / Len(q));
            if (f(tm) >= sunR + plR) continue;
            AstroEvent ev;
            ev.kind = EV_TRANSIT;
            ev.t = tm;
            ev.body = p;
            ev.dist = f(tm) / sunR;
            out->push_back(ev);
            if (++found == 2) break;
        }
    }
    const int P[] = {B_MERCURY, B_VENUS, B_MARS, B_JUPITER, B_SATURN, B_URANUS, B_NEPTUNE};
    const double span = 3 * 365.25;
    for (int i = 0; i < 7; i++)
        for (int j = i + 1; j < 7; j++) {
            auto sep = [&](double t) {
                V3 e = pos(B_EARTH, t);
                return AngleBetween(pos(P[i], t) - e, pos(P[j], t) - e);
            };
            for (double tm : FindMinima(sep, t0, t0 + span, 0.25, 1.0 * DEG)) {
                V3 e = pos(B_EARTH, tm);
                if (AngleBetween(pos(P[i], tm) - e, e * -1.0) < 15 * DEG || AngleBetween(pos(P[j], tm) - e, e * -1.0) < 15 * DEG)
                    continue;
                AstroEvent ev;
                ev.kind = EV_CONJ;
                ev.t = tm;
                ev.body = P[i];
                ev.body2 = P[j];
                ev.dist = sep(tm) / DEG;
                out->push_back(ev);
            }
        }
    for (int p : {B_MARS, B_JUPITER, B_SATURN, B_URANUS, B_NEPTUNE}) {
        auto f = [&](double t) { V3 e = pos(B_EARTH, t); return AngleBetween(pos(p, t) - e, e); };
        for (double tm : FindMinima(f, t0, t0 + span, 1.0, 0.1)) {
            AstroEvent ev;
            ev.kind = EV_OPPOSITION;
            ev.t = tm;
            ev.body = p;
            ev.dist = Len(pos(p, tm) - pos(B_EARTH, tm));
            out->push_back(ev);
        }
    }
}

// 彗星: 1980〜2070 年（軌道を合わせた期間）の近日点と、その前後 200 日の地球への最接近（2 AU 以内）
static std::vector<AstroEvent> FindCometEvents(const App& app) {
    std::vector<AstroEvent> out;
    const double t0 = -20 * 365.25, t1 = 70 * 365.25;
    for (size_t i = 0; i < app.B.size(); i++) {
        const BodyDef& d = app.B[i].d;
        if (!d.comet) continue;
        auto r = [&](double t) { return Len(PlanetPos(d, t)); };
        auto de = [&](double t) { return Len(PlanetPos(d, t) - SimPlanetPos(app.B, B_EARTH, t)); };
        for (double tp : FindMinima(r, t0, t1, 1.0, 1e300)) {
            AstroEvent ev;
            ev.kind = EV_PERIHELION;
            ev.t = tp;
            ev.body = (int)i;
            ev.dist = r(tp);
            out.push_back(ev);
            double best = 1e300, bt = tp;
            for (double tm : FindMinima(de, tp - 200, tp + 200, 0.25, 2 * AU_KM))
                if (de(tm) < best) { best = de(tm); bt = tm; }
            if (best < 2 * AU_KM) {
                ev.kind = EV_COMET_EARTH;
                ev.t = bt;
                ev.dist = best;
                out.push_back(ev);
            }
        }
    }
    std::sort(out.begin(), out.end(), [](const AstroEvent& a, const AstroEvent& b) { return a.t < b.t; });
    return out;
}

// t0 から先のイベントを探す: 日食・月食は 3 年分、木星面の影の通過は 4 衛星それぞれ次の 1 回、ほかは FindPlanetEvents
static std::vector<AstroEvent> FindEvents(const App& app, double t0) {
    std::vector<Body> B = app.B;
    std::vector<AstroEvent> out;
    const double step = 0.05, span = 3 * 365.25;
    auto solarF = [&](double t) { EclipseGeom g = EclipseAt(B, t); return g.newSide ? g.solarPerp : 1e12; };
    auto lunarF = [&](double t) { EclipseGeom g = EclipseAt(B, t); return g.newSide ? 1e12 : g.lunarPerp; };
    double ps[3] = {solarF(t0 - step), solarF(t0), 0}, pl[3] = {lunarF(t0 - step), lunarF(t0), 0};
    for (double t = t0; t < t0 + span; t += step) {
        ps[2] = solarF(t + step);
        pl[2] = lunarF(t + step);
        AstroEvent ev;
        if (ps[1] < ps[0] && ps[1] <= ps[2] && ps[1] < 2.0e4) {
            double tm = MinimizeT(solarF, t - step, t + step);
            if (ClassifySolar(B, tm, &ev)) {
                out.push_back(ev);
                if (ev.kind != EV_SOLAR_PARTIAL) {   // 皆既・金環は、食の中心の地上から見る場面も並べる
                    AstroEvent g = ev;
                    g.kind = EV_ECLIPSE_GROUND;
                    g.body2 = ev.kind;
                    out.push_back(g);
                }
            }
        }
        if (pl[1] < pl[0] && pl[1] <= pl[2] && pl[1] < 2.0e4) {
            double tm = MinimizeT(lunarF, t - step, t + step);
            if (ClassifyLunar(B, tm, &ev)) out.push_back(ev);
        }
        ps[0] = ps[1]; ps[1] = ps[2];
        pl[0] = pl[1]; pl[1] = pl[2];
    }
    // 木星面の影: 衛星が太陽側にあり、影の軸が木星に当たる時刻
    const int galilean[4] = {B_MOON + 3, B_MOON + 4, B_MOON + 5, B_MOON + 6};
    for (int m : galilean) {
        auto f = [&](double t) {
            UpdateBodies(B, t);
            double along, perp;
            ShadowAxis(B[B_SUN].pos, B[m].pos, B[B_JUPITER].pos, &along, &perp);
            return along > 0 ? perp : 1e12;
        };
        const double st = 0.01;
        double p0 = f(t0 - st), p1 = f(t0);
        for (double t = t0; t < t0 + 30; t += st) {
            double p2 = f(t + st);
            if (p1 < p0 && p1 <= p2 && p1 < B[B_JUPITER].d.radius) {
                AstroEvent ev;
                ev.kind = EV_SHADOW;
                ev.moon = m;
                ev.t = MinimizeT(f, t - st, t + st);
                out.push_back(ev);
                break;
            }
            p0 = p1; p1 = p2;
        }
    }
    FindPlanetEvents(B, t0, &out);
    std::sort(out.begin(), out.end(), [](const AstroEvent& a, const AstroEvent& b) { return a.t < b.t; });
    return out;
}

// 探査機のできごと: 惑星への最接近（フライバイ）、周回機の到着（周回軌道に入る最初の最接近）、運用終了（突入の 10 分前）
static std::vector<AstroEvent> FindProbeEvents(const App& app) {
    std::vector<AstroEvent> out;
    for (size_t i = 0; i < app.B.size(); i++) {
        const BodyDef& d = app.B[i].d;
        if (d.probe < 0 || d.probe >= (int)g_probes.size()) continue;
        const std::vector<ProbeSeg>& S = g_probes[d.probe].segs;
        for (const ProbeSeg& sg : S) {
            if (sg.body <= 0) continue;
            bool orbiter = sg.t.back() - sg.t.front() > 200;
            if (orbiter && app.B[sg.body].d.kind == KIND_ROCKY && app.B[sg.body].d.a_au > 0 && sg.body > B_MOON) {
                // 彗星・小惑星への到着は区間の始まり（周回や待機の開始）
                AstroEvent ev;
                ev.kind = EV_ARRIVE;
                ev.t = sg.t.front();
                ev.probe = (int)i;
                ev.body = sg.body;
                ev.dist = Len(sg.p.front());
                out.push_back(ev);
                continue;
            }
            // フライバイは区間で一番近い点、周回機は最初の近点（軌道投入。2 周目以降の方が近いことがある）
            size_t best = 0;
            for (size_t j = 0; j < sg.t.size(); j++) {
                if (orbiter && best > 0 && Len(sg.p[j]) > Len(sg.p[best])) break;
                if (Len(sg.p[j]) < Len(sg.p[best])) best = j;
            }
            if (best == 0) continue;  // 打ち上げ直後の地球からの出発
            if (best + 1 == sg.t.size() && &sg == &S.back() && d.probeEnd) continue;  // 突入・衝突の直前（運用終了として扱う）
            auto f = [&](double t) { return Len(SegPos(sg, t)); };
            AstroEvent ev;
            ev.kind = orbiter ? EV_ARRIVE : EV_FLYBY;
            ev.t = MinimizeT(f, sg.t[best - 1], sg.t[std::min(best + 1, sg.t.size() - 1)]);
            ev.probe = (int)i;
            ev.body = sg.body;
            ev.dist = f(ev.t);
            out.push_back(ev);
        }
        if (d.probeEnd && !S.empty() && S.back().body > 0) {
            AstroEvent ev;
            ev.kind = EV_END;
            ev.t = S.back().t.back() - 10.0 / 1440;
            ev.probe = (int)i;
            ev.body = S.back().body;
            // 衝突で公転周期の変わった衛星があれば、その衛星への衝突（1 分前から見る）
            for (size_t k = 0; k < app.B.size(); k++)
                if (app.B[k].d.parent == ev.body && app.B[k].d.moonPeriod2 > 0) {
                    ev.kind = EV_IMPACT;
                    ev.body = (int)k;
                    ev.t = app.B[k].d.moonChangeT - 1.0 / 1440;
                }
            out.push_back(ev);
        }
    }
    std::sort(out.begin(), out.end(), [](const AstroEvent& a, const AstroEvent& b) { return a.t < b.t; });
    return out;
}

static std::string EventTitle(const App& app, const AstroEvent& ev) {
    auto name = [&](int i) { return TR(app.B[i].d.jp, app.B[i].d.en); };
    char b[200];
    switch (ev.kind) {
        case EV_SOLAR_TOTAL: return T(UI_EV_SOLAR_TOTAL);
        case EV_ECLIPSE_GROUND:
            snprintf(b, sizeof b, T(UI_EV_GROUND), T(ev.body2 == EV_SOLAR_TOTAL ? UI_EV_SOLAR_TOTAL : UI_EV_SOLAR_ANNULAR));
            break;
        case EV_SOLAR_ANNULAR: return T(UI_EV_SOLAR_ANNULAR);
        case EV_SOLAR_PARTIAL: return T(UI_EV_SOLAR_PARTIAL);
        case EV_LUNAR_TOTAL: return T(UI_EV_LUNAR_TOTAL);
        case EV_LUNAR_PARTIAL: return T(UI_EV_LUNAR_PARTIAL);
        case EV_SHADOW: snprintf(b, sizeof b, T(UI_EV_SHADOW), name(ev.moon)); break;
        case EV_TRANSIT: snprintf(b, sizeof b, T(UI_EV_TRANSIT), name(ev.body)); break;
        case EV_CONJ: snprintf(b, sizeof b, T(UI_EV_CONJ), name(ev.body), name(ev.body2)); break;
        case EV_OPPOSITION: snprintf(b, sizeof b, T(UI_EV_OPPOSITION), name(ev.body)); break;
        case EV_PERIHELION: snprintf(b, sizeof b, T(UI_EV_PERIHELION), name(ev.body)); break;
        case EV_COMET_EARTH: snprintf(b, sizeof b, T(UI_EV_COMET_EARTH), name(ev.body)); break;
        case EV_FLYBY: snprintf(b, sizeof b, T(UI_EV_FLYBY), name(ev.probe), name(ev.body)); break;
        case EV_ARRIVE:
            snprintf(b, sizeof b, T(ev.body > B_PLUTO ? UI_EV_ARRIVE_SMALL : UI_EV_ARRIVE), name(ev.probe), name(ev.body));
            break;
        case EV_IMPACT: snprintf(b, sizeof b, T(UI_EV_IMPACT), name(ev.probe), name(ev.body)); break;
        default: snprintf(b, sizeof b, T(ev.body > B_PLUTO ? UI_EV_END_LAND : UI_EV_END), name(ev.probe), name(ev.body)); break;
    }
    return b;
}

static std::string EventDetail(const App& app, const AstroEvent& ev) {
    char b[160];
    switch (ev.kind) {
        case EV_FLYBY:
        case EV_ARRIVE: snprintf(b, sizeof b, T(UI_EV_ALT), FmtDist(ev.dist - app.B[ev.body].d.radius).c_str()); return b;
        case EV_TRANSIT: snprintf(b, sizeof b, T(UI_EV_TRANSIT_D), ev.dist); return b;
        case EV_CONJ: snprintf(b, sizeof b, T(UI_EV_CONJ_D), ev.dist); return b;
        case EV_OPPOSITION:
        case EV_COMET_EARTH: snprintf(b, sizeof b, T(UI_EV_FROM_EARTH), FmtDist(ev.dist).c_str()); return b;
        case EV_PERIHELION: snprintf(b, sizeof b, T(UI_EV_FROM_SUN), FmtDist(ev.dist).c_str()); return b;
        case EV_IMPACT: return T(UI_EV_IMPACT_D);
        default: break;
    }
    if (ev.kind > EV_SOLAR_PARTIAL && ev.kind != EV_ECLIPSE_GROUND) return "";
    char ll[96];
    if (g_jp)
        snprintf(ll, sizeof ll, "%s%.0f° %s%.0f°", ev.lat >= 0 ? "北緯" : "南緯", std::fabs(ev.lat), ev.lon >= 0 ? "東経" : "西経", std::fabs(ev.lon));
    else
        snprintf(ll, sizeof ll, "%.0f°%s %.0f°%s", std::fabs(ev.lat), ev.lat >= 0 ? "N" : "S", std::fabs(ev.lon), ev.lon >= 0 ? "E" : "W");
    snprintf(b, sizeof b, T(UI_EV_CENTER), ll);
    return b;
}

// 彗星の活動の強さ（0〜1）と尾の長さ km。太陽から 4 AU（cometScale 倍の彗星は √倍）より内側で現れ、
// 近いほど長い（見た目の演出で、長さは実際の尾と同じ桁）
static double CometActivity(const Body& b) {
    double r = Len(b.pos) / AU_KM, r0 = 4.0 * std::sqrt(b.d.cometScale);
    double act = std::max(0.0, std::min(1.0, (r0 - r) / (r0 - 1.0)));
    return act * act * (3 - 2 * act);
}
static double CometTailLength(const Body& b) {
    return 2.0e7 * b.d.cometScale * CometActivity(b) * std::min(2.5, AU_KM / Len(b.pos));
}

static double ArrivalDistance(const Body& b);

// ワープの瞬間に呼ぶ: イベントの時刻へ移り、よく見える位置に自機を置く
static void ApplyEvent(App& app, const AstroEvent& ev) {
    app.tDays = ev.t;
    UpdateBodies(app.B, app.tDays);
    const Body& sun = app.B[B_SUN];
    const Body& earth = app.B[B_EARTH];
    app.ship.speed = 0;
    if (ev.kind == EV_TRANSIT) {
        // 地球と惑星を結ぶ線の上から、ズームして太陽を見る。この線の上ならどこでも、太陽面のどこを惑星が通るかは
        // 地球から見た時と同じ。惑星の見かけの半径が太陽の 1/10 になる距離に置く（地球からは 1/30〜1/160）
        const Body& pl = app.B[ev.body];
        double D = pl.d.radius * Len(pl.pos - sun.pos) / (0.1 * sun.d.radius);
        app.ship.ref = B_SUN;
        app.ship.local = pl.pos + Norm(earth.pos - pl.pos) * D;
        app.ship.q = QuatFromBasis(sun.pos - app.ship.local, Y_UP);
        app.target = ev.body;
        app.eventZoom = true;
    } else if (ev.kind == EV_CONJ) {
        // 地球のすぐ外（地球が視界に入らないよう前方へ 4 半径）から、2 つの惑星の中間の方向を見る
        V3 m = Norm(Norm(app.B[ev.body].pos - earth.pos) + Norm(app.B[ev.body2].pos - earth.pos));
        app.ship.ref = B_EARTH;
        app.ship.local = m * (earth.d.radius * 4);
        app.ship.q = QuatFromBasis(m, Y_UP);
        app.target = ev.body;
        app.eventZoom = true;
    } else if (ev.kind == EV_OPPOSITION) {
        // 惑星の地球側（ワープの到着距離）から見る。衝の惑星は満月のように全面が照らされる
        const Body& pl = app.B[ev.body];
        V3 d = Norm(earth.pos - pl.pos);
        app.ship.ref = ev.body;
        app.ship.local = d * ArrivalDistance(pl);
        app.ship.q = QuatFromBasis(d * -1.0, pl.pole);
        app.target = ev.body;
    } else if (ev.kind == EV_PERIHELION) {
        // 軌道面の上方から尾を横に見る（ダストの尾が曲がるのが分かる）。尾は画面の横向きに、核は中央の少し左に
        const Body& cm = app.B[ev.body];
        V3 anti = Norm(cm.pos), vel = PlanetPos(cm.d, app.tDays + 0.01) - PlanetPos(cm.d, app.tDays);
        V3 n = Norm(Cross(cm.pos, vel));
        double len = std::max(CometTailLength(cm), 2e6);
        V3 aim = cm.pos + anti * (len * 0.3);
        app.ship.ref = B_SUN;
        app.ship.local = aim + n * (len * 0.8);
        app.ship.q = QuatFromBasis(n * -1.0, Cross(anti, n * -1.0) * -1.0);
        app.target = ev.body;
    } else if (ev.kind == EV_COMET_EARTH) {
        // 地球のすぐ外から彗星を見る（地球から見た尾の向きと長さ）
        V3 d = Norm(app.B[ev.body].pos - earth.pos);
        app.ship.ref = B_EARTH;
        app.ship.local = d * (earth.d.radius * 4);
        app.ship.q = QuatFromBasis(d, Y_UP);
        app.target = ev.body;
    } else if (ev.kind == EV_ECLIPSE_GROUND) {
        // 食の中心の地上（高度 300 m）から、ズームして太陽を見上げる
        PlaceAt(app, B_EARTH, ev.lat, ev.lon, 0.3);
        V3 sw = ShipWorld(app);
        app.ship.q = QuatFromBasis(sun.pos - sw, Norm(sw - earth.pos));
        app.target = B_MOON;
        app.eventZoom = true;
    } else if (ev.kind == EV_IMPACT) {
        // 衝突の 1 分前: 衛星のすぐ横（探査機が来る向きから 70° ずらした 1.5 km 先）から衛星と母天体を見る
        const Body& m = app.B[ev.body];
        const Body& host = app.B[m.d.parent];
        V3 u = Norm(app.B[ev.probe].pos - m.pos);
        V3 side = Norm(Cross(u, host.pole));
        V3 dir = Norm(u * 0.35 + side * 0.94);
        app.ship.ref = ev.body;
        app.ship.local = dir * 1.5;
        V3 aim = m.pos + (host.pos - m.pos) * 0.3;
        app.ship.q = QuatFromBasis(aim - (m.pos + app.ship.local), host.pole);
        app.target = ev.body;
    } else if (ev.kind >= EV_FLYBY) {
        // 探査機の斜め後ろ（惑星と反対側）から惑星の方を見る。探査機は画面の横に、惑星は正面に見える。
        // 惑星の夜側を通る時は、昼側の縁（三日月形に光る部分）を正面にする
        const Body& pr = app.B[ev.probe];
        const Body& pl = app.B[ev.body];
        V3 out = Norm(pr.pos - pl.pos);
        V3 side = Cross(out, Y_UP);
        side = Len(side) > 1e-6 ? Norm(side) : V3(1, 0, 0);
        V3 up = Cross(side, out);
        V3 toSun = Norm(sun.pos - pl.pos), aim = pl.pos;
        if (Dot(out, toSun) < 0) aim = pl.pos + Norm(toSun - out * Dot(toSun, out)) * pl.d.radius;
        app.ship.ref = ev.probe;
        app.ship.speed = 0;
        app.ship.local = out * 0.035 - side * 0.022 + up * 0.006;
        app.ship.q = QuatFromBasis(aim - (pr.pos + app.ship.local), up);
        app.target = ev.probe;
        app.eventZoom = ev.body > B_PLUTO;   // 彗星・小惑星は小さいのでズームして見る（探査機はマーカーで示す）
    } else if (ev.kind <= EV_SOLAR_PARTIAL) {
        // 食の中心の上空 12,000 km から見下ろす
        PlaceAt(app, B_EARTH, ev.lat, ev.lon, 12000);
        app.target = B_EARTH;
    } else if (ev.kind <= EV_LUNAR_PARTIAL) {
        // 月を横から見る（地球の影の軸に直交する方向へ 4 半径）
        const Body& m = app.B[B_MOON];
        V3 u = Norm(m.pos - sun.pos);
        V3 side = Norm(Cross(u, m.pole));
        app.ship.ref = B_MOON;
        app.ship.speed = 0;
        app.ship.local = (side * 4.0 - u * 1.0) * m.d.radius;
        app.ship.q = QuatFromBasis(app.ship.local * -1.0, m.pole);
        app.target = B_MOON;
    } else {   // EV_SHADOW
        // 木星面に落ちた影の真上（太陽側）から見下ろす
        const Body& j = app.B[B_JUPITER];
        const Body& mo = app.B[ev.moon];
        V3 u = Norm(mo.pos - sun.pos);
        double along, perp;
        ShadowAxis(sun.pos, mo.pos, j.pos, &along, &perp);
        double R = j.d.radius, k = std::sqrt(std::max(0.0, R * R - perp * perp));
        V3 spot = mo.pos + u * (along - k) - j.pos;   // 影の中心（木星中心から）
        app.ship.ref = B_JUPITER;
        app.ship.speed = 0;
        app.ship.local = spot - u * (R * 0.9);
        app.ship.q = QuatFromBasis(u, j.pole);
        app.target = ev.moon;
    }
    UpdateReferenceFrame(app);
}

// ---------------------------------------------------------------- 日時の指定
// 年月日 → 1970-01-01 からの日数（グレゴリオ暦）
static long long DaysFromCivil(int y, int m, int d) {
    y -= m <= 2;
    long long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long long)doe - 719468;
}

// "YYYYMMDD" または "YYYYMMDDhhmm"（UTC）→ J2000 からの日数。不正なら NaN
static double ParseDateDigits(const std::string& s) {
    if (s.size() != 8 && s.size() != 12) return NAN;
    int y = std::stoi(s.substr(0, 4)), mo = std::stoi(s.substr(4, 2)), d = std::stoi(s.substr(6, 2));
    int h = s.size() == 12 ? std::stoi(s.substr(8, 2)) : 0, mi = s.size() == 12 ? std::stoi(s.substr(10, 2)) : 0;
    static const int MD[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (y < 1800 || y > 2200 || mo < 1 || mo > 12 || d < 1 || d > MD[mo - 1] || h > 23 || mi > 59) return NAN;
    bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    if (mo == 2 && d == 29 && !leap) return NAN;
    double unixSec = (double)DaysFromCivil(y, mo, d) * 86400.0 + h * 3600.0 + mi * 60.0;
    return (unixSec - J2000_UNIX) / 86400.0;
}

// 入力中の数字を "2026-10-28 14:05" の形に（未入力は _）
static std::string FormatDateDigits(const std::string& dg) {
    const char* pat = "____-__-__ __:__";
    std::string out = pat;
    size_t k = 0;
    for (size_t i = 0; i < out.size() && k < dg.size(); i++)
        if (out[i] == '_') out[i] = dg[k++];
    return out;
}

static std::string DigitsOfTime(double tDays) {
    std::string f = FmtDate(tDays);   // "YYYY-MM-DD hh:mm:ss UTC"
    std::string d;
    for (size_t i = 0; i < 16 && i < f.size(); i++)
        if (f[i] >= '0' && f[i] <= '9') d += f[i];
    return d;
}

static void StartWarp(App& app, int ti, bool land);

// イベントへワープする時に向かう天体
static int EventBody(const AstroEvent& ev) {
    if (ev.kind == EV_IMPACT) return ev.body;
    if (ev.kind >= EV_FLYBY) return ev.probe;
    if (ev.kind == EV_ECLIPSE_GROUND) return B_EARTH;
    if (ev.kind >= EV_TRANSIT) return ev.body;
    return ev.kind <= EV_SOLAR_PARTIAL ? B_EARTH : ev.kind <= EV_LUNAR_PARTIAL ? B_MOON : B_JUPITER;
}

// 一覧の中身をページに合わせて用意する。彗星・探査機のページでは、今の時刻に一番近いできごとを選ぶ
static void OpenEventsPage(App& app) {
    if (app.eventsPage == 0) {
        app.events = FindEvents(app, app.tDays);
        app.eventSel = 0;
        return;
    }
    app.events = app.eventsPage == 1 ? app.cometEvents : app.probeEvents;
    app.eventSel = 0;
    for (size_t i = 0; i < app.events.size(); i++)
        if (std::fabs(app.events[i].t - app.tDays) < std::fabs(app.events[app.eventSel].t - app.tDays)) app.eventSel = (int)i;
}

// ---------------------------------------------------------------- 自動ツアー（U キー）
// イベント一覧の名場面を年代順に巡る。各場面は一覧（0: 天文, 1: 彗星, 2: 探査機）の種類・天体名・日付で探す
struct TourStop {
    int page, kind;
    const char* who;    // 探査機か彗星の英語名（天文イベントは nullptr）
    const char* body;   // 探査機の相手の天体の英語名（nullptr: 問わない）
    const char* date;   // "YYYYMMDD"（UTC、±2 日で探す）
    const char* jp;
    const char* en;     // 説明（\n で改行）
};
static const TourStop TOUR[] = {
    {2, EV_FLYBY, "Pioneer 10", "Jupiter", "19731204",
     "人類で初めて木星に接近した探査機。小惑星帯を越えて飛べることを確かめ、\n木星を取り巻く強い放射線帯を測った",
     "The first spacecraft to fly past Jupiter. It proved that a probe could cross the asteroid belt\nand measured Jupiter's intense radiation belts"},
    {2, EV_FLYBY, "Voyager 1", "Jupiter", "19790305",
     "イオの活火山と木星の細い環を発見した。このあと土星を経て、\n2012 年に人工物で初めて太陽圏の外へ出た",
     "It discovered Io's active volcanoes and Jupiter's faint ring. After Saturn it went on to become,\nin 2012, the first human-made object to leave the heliosphere"},
    {1, EV_PERIHELION, "Halley", nullptr, "19860209",
     "76 年ごとに戻ってくる周期彗星。1986 年の回帰では、\nジオットなど 5 機の探査機が核に迫った",
     "The periodic comet that returns every 76 years. During its 1986 return,\nfive spacecraft including Giotto flew past its nucleus"},
    {2, EV_FLYBY, "Voyager 2", "Neptune", "19890825",
     "海王星を訪れた唯一の探査機。北極の上空 約 5,000 km をかすめ、\n大暗斑やトリトンの間欠泉を撮影した",
     "The only spacecraft to visit Neptune. It skimmed about 5,000 km above the north pole\nand imaged the Great Dark Spot and the geysers of Triton"},
    {1, EV_COMET_EARTH, "Hyakutake", nullptr, "19960325",
     "地球から 0.1 AU（約 1,500 万 km）を通り、空に長い尾を引いた大彗星。\n日本のアマチュア天文家、百武裕司さんが発見した",
     "A great comet that passed 0.1 AU (15 million km) from Earth with a long tail across the sky.\nDiscovered by the Japanese amateur astronomer Yuji Hyakutake"},
    {1, EV_PERIHELION, "Hale-Bopp", nullptr, "19970401",
     "核の直径が約 60 km もある巨大な彗星。1 年半近く肉眼で見え、\n20 世紀で最も多くの人に見られた彗星と言われる",
     "A giant comet with a nucleus about 60 km across. Visible to the naked eye for a record 18 months,\nit was perhaps the most widely seen comet of the 20th century"},
    {2, EV_ARRIVE, "Cassini", "Saturn", "20040701",
     "環の隙間をくぐって土星を回る軌道に入った。13 年間の観測で、\nエンケラドゥスの地下の海から噴き出す水を見つけた",
     "It passed through a gap in the rings into orbit around Saturn. Over 13 years\nit found water jets erupting from the subsurface ocean of Enceladus"},
    {2, EV_ARRIVE, "Rosetta", "Churyumov-Gerasimenko", "20140806",
     "彗星を初めて周回した探査機。11 月には着陸機フィラエを\n核の表面に降ろした",
     "The first spacecraft to orbit a comet. In November it sent\nthe lander Philae down to the surface of the nucleus"},
    {2, EV_FLYBY, "New Horizons", "Pluto", "20150714",
     "冥王星を初めて間近に見た探査機。ハート形に広がる窒素の氷の平原\n「スプートニク平原」を撮影した",
     "The first close look at Pluto. It revealed the heart-shaped\nnitrogen-ice plain, Sputnik Planitia"},
    {2, EV_ARRIVE, "Juno", "Jupiter", "20160705",
     "北極と南極の上を通る軌道から木星の雲のすぐ上をかすめ、\n極に並ぶ巨大な渦の群れや、木星の内部を調べている",
     "From a polar orbit it skims just above Jupiter's clouds, studying the clusters\nof giant cyclones at the poles and the planet's interior"},
    {2, EV_END, "Cassini", "Saturn", "20170915",
     "燃料が尽きる前に、生命がいるかもしれない衛星を汚さないよう、\n土星の大気へ自ら突入して運用を終えた",
     "Before its fuel ran out, it plunged into Saturn's atmosphere\nso that it could never contaminate moons that might harbour life"},
    {2, EV_ARRIVE, "Hayabusa2", "Ryugu", "20180627",
     "直径 約 900 m のそろばん玉形の小惑星に到着。2 回の着地で岩石を採り、\n2020 年 12 月に地球へ届けた",
     "It reached this 900 m spinning-top-shaped asteroid, collected samples in two touchdowns\nand delivered them to Earth in December 2020"},
    {2, EV_ARRIVE, "OSIRIS-REx", "Bennu", "20181203",
     "炭素に富む小惑星ベンヌに到着。2020 年に表面の砂を採り、\n2023 年 9 月に地球へ届けた（いまは小惑星アポフィスへ向かう OSIRIS-APEX）",
     "It arrived at the carbon-rich asteroid Bennu, collected surface material in 2020\nand delivered it to Earth in September 2023 (now OSIRIS-APEX, bound for Apophis)"},
    {2, EV_IMPACT, "DART", "Dimorphos", "20220926",
     "小惑星の軌道を変えられるかを確かめる、人類初の惑星防衛の実験。秒速 6 km で衝突し、\nディモルフォスの公転周期を 33 分縮めた",
     "The first planetary-defense test: could a spacecraft change an asteroid's orbit? Hitting at 6 km/s,\nit shortened Dimorphos's orbital period by 33 minutes"},
    {0, EV_ECLIPSE_GROUND, nullptr, nullptr, "20270802",
     "北アフリカから中東を通る皆既日食を、食の中心の地上から。皆既は最長 6 分 23 秒続き、\n今世紀でも有数の長さ。太陽の周りにコロナが広がり、空が暗くなって明るい星が見える",
     "The total eclipse crossing North Africa and the Middle East, seen from the ground at its center. Totality lasts\nup to 6 min 23 s, one of the longest this century: the corona appears and the sky darkens enough to show bright stars"},
    {0, EV_TRANSIT, nullptr, nullptr, "20321113",
     "水星が太陽の前を横切る太陽面通過。地球から見えるのは 100 年に 13 回ほどで、\n次は 2039 年",
     "Mercury crosses the face of the Sun. Seen from Earth this happens about 13 times a century;\nthe next one is in 2039"},
    {1, EV_PERIHELION, "Halley", nullptr, "20610728",
     "ハレー彗星の次の近日点。今度は地球から見やすい位置関係になり、\n1986 年より明るく見えると予想されている",
     "Halley's next perihelion. This time Earth is better placed,\nand the comet is expected to look brighter than in 1986"},
};
static const double TOUR_STOP_SEC = 15;

// ツアーの場面をイベント一覧から探す（見つからない場面は飛ばす）
static void BuildTour(App& app) {
    app.tourEvents.clear();
    app.tourStops.clear();
    for (int i = 0; i < (int)(sizeof TOUR / sizeof TOUR[0]); i++) {
        const TourStop& st = TOUR[i];
        double t = ParseDateDigits(st.date);
        std::vector<AstroEvent> list = st.page == 0 ? FindEvents(app, t - 3) : st.page == 1 ? app.cometEvents : app.probeEvents;
        for (const AstroEvent& ev : list) {
            if (ev.kind != st.kind || std::fabs(ev.t - t) > 2) continue;
            int who = ev.probe >= 0 ? ev.probe : ev.body;
            if (st.who && (who < 0 || std::strcmp(app.B[who].d.en, st.who) != 0)) continue;
            if (st.body && ev.probe >= 0 && std::strcmp(app.B[ev.body].d.en, st.body) != 0) continue;
            app.tourEvents.push_back(ev);
            app.tourStops.push_back(i);
            break;
        }
    }
}

static void GoTourStop(App& app, int i) {
    if (i < 0 || i >= (int)app.tourEvents.size() || app.warpT >= 0) return;
    app.tourIdx = i;
    app.tourTime = 0;
    app.paused = false;
    app.timeIdx = 0;
    const AstroEvent& ev = app.tourEvents[i];
    StartWarp(app, EventBody(ev), false);
    app.events.assign(1, ev);   // ワープの瞬間に ApplyEvent で使う
    app.warpEvent = 0;
}

static void StartTour(App& app) {
    BuildTour(app);
    if (app.tourEvents.empty()) {
        Toast(app, T(UI_TOUR_NONE));
        return;
    }
    app.tourOn = true;
    app.autopilot = false;
    GoTourStop(app, 0);
}

static void StopTour(App& app) {
    app.tourOn = false;
    Toast(app, T(UI_TOUR_END));
}

// 場面に着いてから TOUR_STOP_SEC 秒たったら次へ。最後の場面のあとは終了
static void UpdateTour(App& app, double dt) {
    if (!app.tourOn || app.warpT >= 0) return;
    app.tourTime += dt;
    if (app.tourTime < TOUR_STOP_SEC) return;
    if (app.tourIdx + 1 < (int)app.tourEvents.size()) GoTourStop(app, app.tourIdx + 1);
    else StopTour(app);
}

static void DrawTourPanel(const App& app) {
    if (!app.tourOn || app.tourIdx >= (int)app.tourEvents.size()) return;
    const Cam& c = app.cam;
    const AstroEvent& ev = app.tourEvents[app.tourIdx];
    const TourStop& st = TOUR[app.tourStops[app.tourIdx]];
    std::string desc = TR(st.jp, st.en);
    std::vector<std::string> lines;
    for (size_t a = 0; a <= desc.size();) {
        size_t b = std::min(desc.find('\n', a), desc.size());
        lines.push_back(desc.substr(a, b - a));
        a = b + 1;
    }
    // 右下の速度計に重ならないよう、左寄せで置く
    float w = std::min(c.W - 300, 900.0f), h = 84 + lines.size() * 22.0f, x = 12, y = c.H - 96 - h;
    DrawRectangleRounded({x, y, w, h}, 0.08f, 6, {0, 8, 20, 210});
    DrawRectangleRoundedLinesEx({x, y, w, h}, 0.08f, 6, 1.5f, {90, 130, 180, 255});
    char head[96];
    snprintf(head, sizeof head, T(UI_TOUR), app.tourIdx + 1, (int)app.tourEvents.size());
    Txt((std::string(head) + "    " + FmtDate(ev.t).substr(0, 16) + " UTC").c_str(), x + 18, y + 10, 15, {170, 200, 230, 255});
    Txt(EventTitle(app, ev).c_str(), x + 18, y + 30, 21, {255, 220, 120, 255});
    for (size_t i = 0; i < lines.size(); i++) Txt(lines[i].c_str(), x + 18, y + 60 + i * 22.0f, 16, {215, 228, 245, 255});
    Txt(T(UI_TOUR_KEYS), x + w - 18 - TxtW(T(UI_TOUR_KEYS), 13), y + 12, 13, {150, 170, 190, 255});
    if (app.warpT < 0) {   // 次の場面までの残り
        float k = (float)std::min(1.0, app.tourTime / TOUR_STOP_SEC);
        DrawRectangle((int)(x + 18), (int)(y + h - 10), (int)((w - 36) * k), 3, {255, 220, 120, 200});
    }
}

static void SetPhysics(App& app, bool on);
static V3 CircularVelocity(const App& app);

// ---------------------------------------------------------------- 設定（F10 キー。それぞれのキー操作も同じ関数を使う）
enum SettingItem { SET_LANG, SET_QUALITY, SET_SOUND, SET_MUSIC, SET_SKY, SET_ORBITS, SET_ASTEROIDS, SET_MARKERS,
                   SET_INVERT, SET_PHYSICS, SET_FULLSCREEN, SET_COUNT };

// 項目を dir（+1 / -1）の向きに変える。変えたらすぐ保存し、知らせを出すかは toast で選ぶ
static void ChangeSetting(App& app, int item, int dir, bool toast) {
    auto onoff = [](bool v) { return v ? "1" : "0"; };
    switch (item) {
        case SET_LANG:
            if (!g_jpFont) {
                Toast(app, T(UI_LANG_NO_FONT));
                return;
            }
            SetLanguage(!g_jp);
            SetSetting("lang", g_jp ? "ja" : "en");
            if (toast) Toast(app, T(UI_LANG_SET));
            break;
        case SET_QUALITY:
            app.quality = (app.quality + dir + 3) % 3;
            SetSetting("quality", std::to_string(app.quality));
            if (toast) Toast(app, T((UiId)(UI_QUALITY_0 + app.quality)));
            break;
        case SET_SOUND:
            sfx::SetMuted(!sfx::g.muted);
            SetSetting("sound", sfx::g.muted ? "0" : "1");
            if (toast) Toast(app, T(sfx::g.muted ? UI_SOUND_OFF : UI_SOUND_ON));
            break;
        case SET_MUSIC:
            sfx::MusicSetOff(!sfx::gm.off);
            SetSetting("music", sfx::gm.off ? "0" : "1");
            if (toast) Toast(app, T(sfx::gm.off ? UI_MUSIC_OFF : UI_MUSIC_ON));
            break;
        case SET_SKY:
            app.skyMode = (app.skyMode + dir + 3) % 3;
            SetSetting("sky", std::to_string(app.skyMode));
            if (toast) Toast(app, T((UiId)(UI_SKY_OFF + app.skyMode)));
            break;
        case SET_ORBITS:
            app.showOrbits = !app.showOrbits;
            SetSetting("orbits", onoff(app.showOrbits));
            if (toast) Toast(app, T(app.showOrbits ? UI_ORBITS_ON : UI_ORBITS_OFF));
            break;
        case SET_ASTEROIDS:
            app.showAsteroids = !app.showAsteroids;
            SetSetting("asteroids", onoff(app.showAsteroids));
            if (toast) Toast(app, T(app.showAsteroids ? UI_ASTEROIDS_ON : UI_ASTEROIDS_OFF));
            break;
        case SET_MARKERS:
            app.showMoonMarkers = !app.showMoonMarkers;
            SetSetting("markers", onoff(app.showMoonMarkers));
            if (toast) Toast(app, T(app.showMoonMarkers ? UI_LABELS_ON : UI_LABELS_OFF));
            break;
        case SET_INVERT:
            app.invertY = !app.invertY;
            SetSetting("invert", onoff(app.invertY));
            if (toast) Toast(app, T(app.invertY ? UI_INVERT_ON : UI_INVERT_OFF));
            break;
        case SET_PHYSICS:
            SetPhysics(app, !app.physics);
            SetSetting("physics", onoff(app.physics));
            if (toast) Toast(app, T(app.physics ? UI_PHYS_ON : UI_PHYS_OFF));
            break;
        case SET_FULLSCREEN:
            ToggleBorderlessWindowed();
            break;
    }
    SaveSettings();
}

static std::string SettingValue(const App& app, int item) {
    auto onoff = [](bool v) { return T(v ? UI_SET_ON : UI_SET_OFF); };
    switch (item) {
        case SET_LANG: return g_jp ? "日本語" : "English";
        case SET_QUALITY: return T((UiId)(UI_SET_Q0 + app.quality));
        case SET_SOUND: return onoff(!sfx::g.muted);
        case SET_MUSIC: return onoff(!sfx::gm.off);
        case SET_SKY: return T((UiId)(UI_SET_SKY0 + app.skyMode));
        case SET_ORBITS: return onoff(app.showOrbits);
        case SET_ASTEROIDS: return onoff(app.showAsteroids);
        case SET_MARKERS: return onoff(app.showMoonMarkers);
        case SET_INVERT: return onoff(app.invertY);
        case SET_PHYSICS: return T(app.physics ? UI_SET_GRAVITY : UI_SET_ARCADE);
        default: return onoff(IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE));
    }
}

// 一覧・入力欄が開いている間のキー操作。開いていれば true（ほかの操作は止める）
static bool HandleModalInput(App& app) {
    if (app.settingsOpen) {
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN)) app.settingsSel = (app.settingsSel + 1) % SET_COUNT;
        if (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP)) app.settingsSel = (app.settingsSel + SET_COUNT - 1) % SET_COUNT;
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) ChangeSetting(app, app.settingsSel, 1, false);
        if (IsKeyPressed(KEY_LEFT)) ChangeSetting(app, app.settingsSel, -1, false);
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_F10)) app.settingsOpen = false;
        return true;
    }
    if (app.dateOpen) {
        for (int ch = GetCharPressed(); ch > 0; ch = GetCharPressed())
            if (ch >= '0' && ch <= '9' && app.dateDigits.size() < 12) app.dateDigits += (char)ch;
        if (IsKeyPressed(KEY_BACKSPACE) && !app.dateDigits.empty()) app.dateDigits.pop_back();
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Y)) app.dateOpen = false;
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
            double t = ParseDateDigits(app.dateDigits);
            if (std::isnan(t)) {
                Toast(app, T(UI_DATE_BAD));
            } else {
                app.tDays = t;
                UpdateBodies(app.B, app.tDays);
                UpdateReferenceFrame(app);
                app.dateOpen = false;
                char b[96];
                snprintf(b, sizeof b, T(UI_DATE_SET), FmtDate(t).c_str());
                Toast(app, b);
            }
        }
        return true;
    }
    if (app.eventsOpen) {
        int n = (int)app.events.size();
        if (n > 0) {
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN)) app.eventSel = (app.eventSel + 1) % n;
            if (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP)) app.eventSel = (app.eventSel + n - 1) % n;
        }
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_K)) app.eventsOpen = false;
        if (IsKeyPressed(KEY_TAB)) {
            app.eventsPage = (app.eventsPage + (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) ? 2 : 1)) % 3;
            OpenEventsPage(app);
            return true;
        }
        if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) && n > 0 && app.warpT < 0) {
            const AstroEvent& ev = app.events[app.eventSel];
            StartWarp(app, EventBody(ev), false);
            app.warpEvent = app.eventSel;
            app.eventsOpen = false;
            std::string when = FmtDate(ev.t).substr(0, 16) + " UTC";
            char b[256];
            snprintf(b, sizeof b, T(UI_EV_GO), when.c_str(), EventTitle(app, ev).c_str());
            Toast(app, b);
        }
        return true;
    }
    return false;
}

static void DrawModalPanels(const App& app) {
    const Cam& c = app.cam;
    if (app.settingsOpen) {
        static const UiId NAME[SET_COUNT] = {UI_SET_LANG, UI_SET_QUALITY, UI_SET_SOUND, UI_SET_MUSIC, UI_SET_SKY, UI_SET_ORBITS,
                                             UI_SET_ASTEROIDS, UI_SET_MARKERS, UI_SET_INVERT, UI_SET_PHYSICS, UI_SET_FULLSCREEN};
        float w = std::min(c.W - 20, 760.0f), h = 120 + SET_COUNT * 30.0f, x = (c.W - w) / 2, y = (c.H - h) / 2;
        DrawRectangleRounded({x, y, w, h}, 0.04f, 6, {0, 8, 20, 230});
        DrawRectangleRoundedLinesEx({x, y, w, h}, 0.04f, 6, 1.5f, {90, 130, 180, 255});
        TxtC(T(UI_SET_TITLE), c.W * 0.5f, y + 10, 22, {255, 220, 120, 255});
        TxtC(T(UI_SET_KEYS), c.W * 0.5f, y + 40, 14, {170, 200, 230, 255});
        for (int i = 0; i < SET_COUNT; i++) {
            float ry = y + 72 + i * 30.0f;
            bool sel = i == app.settingsSel;
            if (sel) DrawRectangle((int)x + 12, (int)ry - 4, (int)w - 24, 28, {60, 90, 140, 200});
            Color col = sel ? Color{255, 240, 200, 255} : Color{215, 228, 245, 255};
            Txt(T(NAME[i]), x + 28, ry, 18, col);
            std::string v = SettingValue(app, i);
            std::string shown = sel ? "<  " + v + "  >" : v;
            Txt(shown.c_str(), x + w - 28 - TxtW(shown.c_str(), 18), ry, 18, sel ? Color{255, 220, 120, 255} : Color{170, 200, 230, 255});
        }
        TxtC(CREDIT, c.W * 0.5f, y + h - 26, 13, {150, 170, 190, 255});
    }
    if (app.eventsOpen) {
        int n = (int)app.events.size();
        int rows = std::max(1, std::min(n, 14));
        float w = std::min(c.W - 20, 900.0f), h = 96 + rows * 28.0f, x = (c.W - w) / 2, y = (c.H - h) / 2;
        DrawRectangleRounded({x, y, w, h}, 0.04f, 6, {0, 8, 20, 230});
        DrawRectangleRoundedLinesEx({x, y, w, h}, 0.04f, 6, 1.5f, {90, 130, 180, 255});
        static const UiId TITLE[3] = {UI_EV_TITLE, UI_EV_COMET_TITLE, UI_EV_PROBE_TITLE};
        static const UiId NOTE[3] = {UI_EV_NOTE, UI_EV_COMET_NOTE, UI_EV_PROBE_NOTE};
        TxtC(T(TITLE[app.eventsPage]), c.W * 0.5f, y + 10, 22, {255, 220, 120, 255});
        TxtC(T(UI_EV_KEYS), c.W * 0.5f, y + 40, 15, {170, 200, 230, 255});
        if (n == 0) TxtC(T(UI_EV_NONE), c.W * 0.5f, y + 72, 18, {215, 228, 245, 255});
        int first = std::max(0, std::min(app.eventSel - rows / 2, n - rows));
        for (int i = 0; i < rows && first + i < n; i++) {
            const AstroEvent& ev = app.events[first + i];
            float ry = y + 68 + i * 28.0f;
            bool sel = first + i == app.eventSel;
            if (sel) DrawRectangle((int)x + 12, (int)ry - 3, (int)w - 24, 26, {60, 90, 140, 200});
            Color col = sel ? Color{255, 240, 200, 255} : Color{215, 228, 245, 255};
            Txt((FmtDate(ev.t).substr(0, 16) + " UTC").c_str(), x + 24, ry, 17, col);
            Txt(EventTitle(app, ev).c_str(), x + 230, ry, 17, col);
            Txt(EventDetail(app, ev).c_str(), x + 520, ry, 15, {170, 200, 230, 255});
        }
        TxtC(T(NOTE[app.eventsPage]), c.W * 0.5f, y + h - 26, 13, {150, 170, 190, 255});
    }
    if (app.dateOpen) {
        float w = std::min(c.W - 20, 620.0f), h = 150, x = (c.W - w) / 2, y = (c.H - h) / 2;
        DrawRectangleRounded({x, y, w, h}, 0.06f, 6, {0, 8, 20, 235});
        DrawRectangleRoundedLinesEx({x, y, w, h}, 0.06f, 6, 1.5f, {90, 130, 180, 255});
        TxtC(T(UI_DATE_TITLE), c.W * 0.5f, y + 12, 22, {255, 220, 120, 255});
        TxtC((FormatDateDigits(app.dateDigits) + " UTC").c_str(), c.W * 0.5f, y + 52, 32, {255, 255, 255, 255});
        TxtC(T(UI_DATE_KEYS), c.W * 0.5f, y + 108, 15, {170, 200, 230, 255});
    }
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
    app.eventZoom = false;
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
            if (app.warpEvent >= 0 && app.warpEvent < (int)app.events.size()) ApplyEvent(app, app.events[app.warpEvent]);
            else if (app.warpBookmark >= 0 && app.warpBookmark < (int)bms.size()) ApplyBookmark(app, bms[app.warpBookmark]);
            else WarpTo(app, app.warpTarget, app.warpLand);
            app.warpBookmark = -1;
            app.warpEvent = -1;
            app.warpJumped = true;
            if (app.physics) app.ship.vel = CircularVelocity(app);   // 重力モード: 着いた天体のまわりの円軌道に乗せる
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
    if (HandleModalInput(app)) return;
    if (IsKeyPressed(KEY_Z)) {
        app.photoMode = (app.photoMode + 1) % 3;
        Toast(app, T((UiId)(UI_PHOTO_0 + app.photoMode)));
    }
    if (IsKeyPressed(KEY_U)) {
        if (app.tourOn) StopTour(app);
        else StartTour(app);
    }
    if (app.tourOn && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)))
        GoTourStop(app, app.tourIdx + (shift ? -1 : 1));
    if (IsKeyPressed(KEY_K)) {
        OpenEventsPage(app);
        app.eventsOpen = true;
    }
    if (IsKeyPressed(KEY_Y)) {
        app.dateDigits = DigitsOfTime(app.tDays);
        while (GetCharPressed() > 0) {}   // 押した Y が入力欄に入らないように捨てる
        app.dateOpen = true;
    }

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
    if (app.physics && !app.autopilot && !warping) {
        // 重力モード: 噴射は機首の向き（S は逆噴射）に速度を変える。X は基準天体に対して止まる
        V3 f = ShipFwd(s);
        s.vel += f * (acc * (throttle - brake) * sdt);
        if (IsKeyPressed(KEY_X) || pad.stop) s.vel = V3();
        double v = Len(s.vel);
        if (v > C_KMS * 0.999) s.vel = s.vel * (C_KMS * 0.999 / v);
        s.speed = Len(s.vel);
    }
    if (IsKeyPressed(KEY_R)) ChangeSetting(app, SET_PHYSICS, 1, true);

    // ターゲット
    int n = (int)app.B.size();
    auto cycle = [&](int step) {   // 飛んでいない探査機は飛ばす
        for (int k = 0; k < n; k++) {
            app.target = (app.target + step + n) % n;
            if (app.B[app.target].active) break;
        }
    };
    if (IsKeyPressed(KEY_TAB)) cycle(shift ? -1 : 1);
    if (pad.nextTarget) cycle(1);
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
    if (IsKeyPressed(KEY_O)) ChangeSetting(app, SET_ORBITS, 1, true);
    if (IsKeyPressed(KEY_C)) ChangeSetting(app, SET_SKY, 1, true);
    if (IsKeyPressed(KEY_L)) ChangeSetting(app, SET_MARKERS, 1, true);
    if (IsKeyPressed(KEY_J)) ChangeSetting(app, SET_LANG, 1, true);
    if (IsKeyPressed(KEY_M)) ChangeSetting(app, shift ? SET_MUSIC : SET_SOUND, 1, true);
    if (IsKeyPressed(KEY_F10)) {
        app.settingsOpen = true;
        app.settingsSel = 0;
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
    if (IsKeyPressed(KEY_F9)) ChangeSetting(app, SET_QUALITY, 1, true);
    if (IsKeyPressed(KEY_I)) ChangeSetting(app, SET_INVERT, 1, true);
    if (IsKeyPressed(KEY_H) || pad.help) app.showHelp = !app.showHelp;
    if (IsKeyPressed(KEY_F11)) ChangeSetting(app, SET_FULLSCREEN, 1, false);

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

// ---------------------------------------------------------------- 重力モード
static const double G_KM = 6.674e-20;   // 万有引力定数 km³/(kg s²)

// 重力を及ぼす天体（10²⁰ kg 以上: 太陽・惑星・大きな衛星・冥王星・カロン・準惑星の一部）
static bool Massive(const Body& b) { return b.d.probe < 0 && b.d.mass > 1e20; }

// 基準天体 ref の中心から見た位置 r の点の加速度 km/s²（各天体の位置は今の時刻のまま。ref 自身が受ける加速度を
// 差し引くので、ref と一緒に動く座標系での運動になる）
static V3 GravityAccel(const App& app, int ref, V3 r) {
    V3 a, aref;
    const V3 p0 = app.B[ref].pos;
    for (const Body& b : app.B) {
        if (!Massive(b)) continue;
        double gm = G_KM * b.d.mass;
        V3 d = (b.pos - p0) - r;
        double L = Len(d);
        if (L > 1e-3) a += d * (gm / (L * L * L));
        if (&b != &app.B[ref]) {
            V3 e = b.pos - p0;
            double M = Len(e);
            aref += e * (gm / (M * M * M));
        }
    }
    return a - aref;
}

// 計算の刻み s: 近い天体ほど細かく（その天体のまわりの自由落下の時間の 1/50）
static double PhysicsStep(const App& app, V3 w) {
    double best = 3600.0 * 24;
    for (const Body& b : app.B) {
        if (!Massive(b)) continue;
        double r = std::max(Len(w - b.pos), b.d.radius);
        best = std::min(best, 0.02 * std::sqrt(r * r * r / (G_KM * b.d.mass)));
    }
    return std::max(best, 0.05);
}
static const int PHYS_MAX_STEPS = 3000;   // 1 フレームの刻みの上限（超える時は時間の進みを抑える）

// 基準天体のまわりの円軌道の速度（重力モードに入った時・ワープで着いた時）。地表近くでは止まる
static V3 CircularVelocity(const App& app) {
    const Ship& s = app.ship;
    const Body& b = app.B[s.ref];
    if (!Massive(b)) return V3();
    double r = Len(s.local);
    if (r < SurfaceRadius(b, Norm(s.local)) + 1.0) return V3();
    V3 radial = Norm(s.local), dir = ShipFwd(s) - radial * Dot(ShipFwd(s), radial);
    if (Len(dir) < 1e-6) dir = Cross(radial, Y_UP);
    if (Len(dir) < 1e-6) dir = V3(1, 0, 0);
    return Norm(dir) * std::sqrt(G_KM * b.d.mass / r);
}

// 天体の速度 km/s（1 分後の位置との差。基準天体が変わった時の速度の付け替えに使う）
static V3 BodyVelocity(const App& app, int i) {
    std::vector<Body> B = app.B;
    UpdateBodies(B, app.tDays + 60.0 / 86400);
    return (B[i].pos - app.B[i].pos) * (1.0 / 60.0);
}

// 重力と噴射のもとで dt 秒進める（速度ベルレ法）。地表に当たったら止まる
static void IntegrateShip(App& app, double dt) {
    Ship& s = app.ship;
    if (dt <= 0) return;
    int n = std::min(PHYS_MAX_STEPS, std::max(1, (int)std::ceil(dt / PhysicsStep(app, ShipWorld(app)))));
    double h = dt / n;
    V3 a = GravityAccel(app, s.ref, s.local);
    for (int k = 0; k < n; k++) {
        s.vel += a * (h * 0.5);
        s.local += s.vel * h;
        a = GravityAccel(app, s.ref, s.local);
        s.vel += a * (h * 0.5);
        for (const Body& b : app.B) {
            V3 rel = ShipWorld(app) - b.pos;
            double d = Len(rel);
            if (d > b.d.radius * 1.2 + 1 || b.d.probe >= 0) continue;
            V3 dir = rel * (1.0 / d);
            double rs = SurfaceRadius(b, dir) + MinAltitude(b);
            if (d < rs) {
                SetShipWorld(app, b.pos + dir * rs);
                double vin = Dot(s.vel, dir);
                if (vin < 0) s.vel = s.vel - dir * vin;   // 地面に向かう成分を消し、地面の上を滑らせて摩擦で止める
                s.vel = s.vel * std::exp(-2.0 * h);
            }
        }
    }
}

// 今の時刻の 2 体問題の軌道（基準天体のまわり）: 近点・遠点の高度と周期。脱出軌道なら false
static bool OrbitInfo(const App& app, double* peri, double* apo, double* period) {
    const Ship& s = app.ship;
    const Body& b = app.B[s.ref];
    if (!Massive(b)) return false;
    double mu = G_KM * b.d.mass, r = Len(s.local), v2 = Dot(s.vel, s.vel);
    double energy = v2 / 2 - mu / r;
    if (energy >= 0) return false;
    double a = -mu / (2 * energy);
    V3 hvec = Cross(s.local, s.vel);
    double e = std::sqrt(std::max(0.0, 1 - Dot(hvec, hvec) / (mu * a)));
    *peri = a * (1 - e) - b.d.radius;
    *apo = a * (1 + e) - b.d.radius;
    *period = 2 * PI * std::sqrt(a * a * a / mu);
    return true;
}

static void SetPhysics(App& app, bool on) {
    if (on == app.physics) return;
    app.physics = on;
    if (on) {
        app.ship.vel = ShipFwd(app.ship) * app.ship.speed;
        if (Len(app.ship.vel) < 1e-6) app.ship.vel = CircularVelocity(app);
    } else {
        app.ship.speed = std::max(0.0, Dot(app.ship.vel, ShipFwd(app.ship)));
    }
    app.autopilot = false;
}

static void UpdateShip(App& app, double dt) {
    Ship& s = app.ship;
    if (app.physics && !app.autopilot && app.warpT < 0) {
        int oldRef = s.ref;
        IntegrateShip(app, dt);
        s.speed = Len(s.vel);
        UpdateReferenceFrame(app);
        if (s.ref != oldRef) s.vel += BodyVelocity(app, oldRef) - BodyVelocity(app, s.ref);   // 速度を新しい基準天体に対する値へ
        return;
    }
    if (app.physics) s.vel = s.speed > 1e-9 ? ShipFwd(s) * s.speed : CircularVelocity(app);   // 自動操縦・ワープの後に引き継ぐ
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
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || (app.ship.thrusting && app.warpT < 0)) app.eventZoom = false;
    double targetFov = (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || app.padZoom || app.eventZoom) ? 8 * DEG : 60 * DEG;
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
    // 重力モード: これから飛ぶ軌道の予測（基準天体と一緒に動く座標で、軌道 1 周分か 30 日分。天体の位置は今のまま）
    if (app.physics && app.warpT < 0 && !app.autopilot && Len(app.ship.vel) > 1e-9) {
        double peri, apo, period;
        double T = OrbitInfo(app, &peri, &apo, &period) ? std::min(period * 1.02, 2 * 365.25 * 86400.0) : 30 * 86400.0;
        V3 r = app.ship.local, v = app.ship.vel;
        const Body& rb = app.B[app.ship.ref];
        const int N = 720, MAXSUB = 12000;
        int used = 0;
        rlCheckRenderBatchLimit(N * 2 + 2);
        rlBegin(RL_LINES);
        V3 prev = rb.pos + r;
        for (int k = 1; k <= N && used < MAXSUB; k++) {
            double left = T / N;
            while (left > 0 && used < MAXSUB) {
                double h = std::min(left, PhysicsStep(app, rb.pos + r) * 2);
                V3 a = GravityAccel(app, app.ship.ref, r);
                v += a * (h * 0.5);
                r += v * h;
                v += GravityAccel(app, app.ship.ref, r) * (h * 0.5);
                left -= h;
                used++;
            }
            bool hit = false;
            for (const Body& b : app.B)
                if (b.d.probe < 0 && Len(rb.pos + r - b.pos) < b.d.radius) hit = true;
            V3 pnt = rb.pos + r;
            unsigned char al = (unsigned char)(200 * (1.0 - 0.7 * k / N));
            rlColor4ub(120, 230, 255, al);
            Vector3 p0 = ToF(prev - c.pos), p1 = ToF(pnt - c.pos);
            rlVertex3f(p0.x, p0.y, p0.z);
            rlVertex3f(p1.x, p1.y, p1.z);
            prev = pnt;
            if (hit) break;   // 天体に当たる所まで
        }
        rlEnd();
    }
    // 地球を回る人工衛星の軌道（今の軌道要素で 1 周分。地球の近く 10 万 km 以内でだけ）
    for (size_t i = 0; i < app.B.size(); i++) {
        const Body& b = app.B[i];
        if (b.d.tleN <= 0 || !b.active || Len(c.pos - app.B[B_EARTH].pos) > 1e5) continue;
        if (Len(c.pos - b.pos) < 0.1) continue;
        const int N = 180;
        double P = 1.0 / b.d.tleN;
        rlCheckRenderBatchLimit(N * 2 + 2);
        rlBegin(RL_LINES);
        rlColor4ub(255, 196, 120, (unsigned char)((int)i == app.target ? 150 : 60));
        V3 prev = app.B[B_EARTH].pos + SatellitePos(b.d, app.tDays);
        for (int k = 1; k <= N; k++) {
            V3 pnt = app.B[B_EARTH].pos + SatellitePos(b.d, app.tDays + P * k / N);
            Vector3 p0 = ToF(prev - c.pos), p1 = ToF(pnt - c.pos);
            rlVertex3f(p0.x, p0.y, p0.z);
            rlVertex3f(p1.x, p1.y, p1.z);
            prev = pnt;
        }
        rlEnd();
    }
    // 探査機の軌跡（通ってきた部分は明るく、これから通る部分は暗く。ターゲット以外は控えめに）。太陽中心の区間は全体を、
    // 惑星中心の区間は今の惑星の位置を中心に前後 60 日分を描く（周回中の何年分もの軌道が重ならないように）
    for (size_t i = 0; i < app.B.size(); i++) {
        const Body& b = app.B[i];
        if (b.d.probe < 0 || !b.active || b.d.probe >= (int)g_probes.size()) continue;
        if (Len(c.pos - b.pos) < 0.1) continue;  // 寄り添っている時は、軌跡が視界の中央を貫くので描かない
        bool tgt = (int)i == app.target;
        auto planetAt = [&](int k) { return app.B[k].pos; };
        for (const ProbeSeg& sg : g_probes[b.d.probe].segs) {
            double ta = sg.t.front(), tb = sg.t.back();
            if (sg.body > 0) {
                ta = std::max(ta, app.tDays - 60);
                tb = std::min(tb, app.tDays + 60);
                if (ta >= tb) continue;
            }
            rlCheckRenderBatchLimit((int)sg.t.size() * 8 + 8);
            rlBegin(RL_LINES);
            bool have = false;
            V3 prev;
            double prevT = 0;
            for (size_t j = 0; j + 1 < sg.t.size(); j++) {
                if (sg.t[j + 1] < ta || sg.t[j] > tb) continue;
                for (int k = 0; k < 4; k++) {
                    double tt = std::max(ta, std::min(tb, sg.t[j] + (sg.t[j + 1] - sg.t[j]) * k / 4.0));
                    bool act;
                    V3 pnt = sg.body > 0 ? app.B[sg.body].pos + SegPos(sg, tt) : ProbePosAt(b.d, tt, planetAt, &act);
                    if (have) {
                        unsigned char a = (unsigned char)(tgt ? (prevT <= app.tDays ? 150 : 60) : (prevT <= app.tDays ? 45 : 18));
                        rlColor4ub(255, 196, 120, a);
                        Vector3 p0 = ToF(prev - c.pos), p1 = ToF(pnt - c.pos);
                        rlVertex3f(p0.x, p0.y, p0.z);
                        rlVertex3f(p1.x, p1.y, p1.z);
                    }
                    prev = pnt; prevT = tt; have = true;
                }
            }
            if (have) {   // 区間の最後の点まで
                bool act;
                V3 pnt = sg.body > 0 ? app.B[sg.body].pos + SegPos(sg, tb) : ProbePosAt(b.d, tb, planetAt, &act);
                rlColor4ub(255, 196, 120, (unsigned char)(tgt ? (prevT <= app.tDays ? 150 : 60) : (prevT <= app.tDays ? 45 : 18)));
                Vector3 p0 = ToF(prev - c.pos), p1 = ToF(pnt - c.pos);
                rlVertex3f(p0.x, p0.y, p0.z);
                rlVertex3f(p1.x, p1.y, p1.z);
            }
            rlEnd();
        }
    }
    rlDrawRenderBatchActive();
    rlEnableDepthTest();
}

// 彗星のコマと尾。大きさと明るさは CometActivity・CometTailLength
//  - イオンの尾: 太陽と反対方向へまっすぐ伸びる青い尾
//  - ダストの尾: 公転の向きと逆側へ曲がる、幅の広い黄白色の尾
// 天体より先に加算合成で描くので、手前の天体には隠れる
static void DrawCometTails(const App& app) {
    const Cam& c = app.cam;
    SetPassProjection(c, 0.01, 1e11);
    rlDisableDepthTest();
    rlDisableBackfaceCulling();
    BeginBlendMode(BLEND_ADDITIVE);
    for (const Body& b : app.B) {
        if (!b.d.comet) continue;
        double r = Len(b.pos) / AU_KM, k = b.d.cometScale, act = CometActivity(b);
        if (act <= 0) continue;
        double len = CometTailLength(b);
        V3 anti = Norm(b.pos);                                 // 太陽と反対の向き
        V3 vel = Norm(PlanetPos(b.d, app.tDays + 0.01) - PlanetPos(b.d, app.tDays));
        V3 lag = Norm(vel * -1.0 - anti * Dot(vel * -1.0, anti));  // 公転の後ろ側（反太陽方向に垂直な成分）
        V3 rel0 = b.pos - c.pos;

        // 中心線に沿った帯（中心が明るく縁は透明）。軌道面とそれに垂直な面の 2 枚を十字に重ね、どの向きから見ても
        // 厚みが見えるようにする（カメラに向けると、尾の近くで帯がねじれて筋になる）。w0 / w1: 根元と先端の半幅 km
        V3 normal = Norm(Cross(anti, lag));
        auto sheet = [&](auto point, int k, double w0, double w1, Vector3 col, double alpha) {
            const int N = 64;
            auto vtx = [&](V3 q, unsigned char aa) {
                rlColor4ub((unsigned char)(col.x * 255), (unsigned char)(col.y * 255), (unsigned char)(col.z * 255), aa);
                rlVertex3f((float)q.x, (float)q.y, (float)q.z);
            };
            rlBegin(RL_TRIANGLES);
            V3 pl, pc, pr;
            unsigned char pa = 0;
            for (int i = 0; i <= N; i++) {
                double u = (double)i / N;
                V3 p = point(u), t = Norm(point(std::min(1.0, u + 0.01)) - point(std::max(0.0, u - 0.01)));
                V3 side = k == 0 ? Norm(Cross(normal, t)) : normal;
                double w = (w0 + (w1 - w0) * u) * (k == 0 ? 1.0 : 0.5);
                V3 C = p - c.pos, L = C + side * w, R = C - side * w;
                unsigned char a = (unsigned char)(255 * alpha * act * std::pow(1 - u, 1.6) * std::min(1.0, u * 12));
                if (i > 0) {
                    vtx(pl, 0); vtx(pc, pa); vtx(C, a);
                    vtx(pl, 0); vtx(C, a); vtx(L, 0);
                    vtx(pc, pa); vtx(pr, 0); vtx(R, 0);
                    vtx(pc, pa); vtx(R, 0); vtx(C, a);
                }
                pl = L; pc = C; pr = R; pa = a;
            }
            rlEnd();
        };
        auto ion = [&](double u) { return b.pos + anti * (len * u); };
        auto dust = [&](double u) { return b.pos + anti * (len * 0.6 * u) + lag * (len * 0.35 * u * u); };
        for (int k = 0; k < 2; k++) {
            sheet(ion, k, 2e4, len * 0.03, Vector3{0.45f, 0.65f, 1.0f}, 0.55);
            sheet(dust, k, 4e4, len * 0.12, Vector3{1.0f, 0.88f, 0.68f}, 0.45);
        }

        // コマ: 核を中心に、カメラを向く円（中心が明るい）。内側に入ったら薄くする
        double comaR = 3e5 * k * act * std::min(2.0, 1.0 / r);
        double dist = Len(rel0);
        double inside = std::min(1.0, dist / comaR);
        V3 ax = Norm(Cross(c.fwd, c.up)), ay = c.up;
        if (Len(rel0) > 1e-9) {
            V3 vdir = Norm(rel0);
            ax = Norm(Cross(vdir, c.up));
            if (Len(Cross(vdir, c.up)) < 1e-6) ax = c.right;
            ay = Cross(ax, vdir);
        }
        const int SEG = 32;
        const double ringR[3] = {0, 0.3, 1.0};
        const double ringA[3] = {0.9, 0.35, 0};
        rlBegin(RL_TRIANGLES);
        for (int ring = 0; ring < 2; ring++)
            for (int j = 0; j < SEG; j++) {
                double a0 = 2 * PI * j / SEG, a1 = 2 * PI * (j + 1) / SEG;
                auto pt = [&](int rr, double ang) {
                    V3 q = rel0 + (ax * std::cos(ang) + ay * std::sin(ang)) * (comaR * ringR[rr]);
                    rlColor4ub(220, 235, 255, (unsigned char)(255 * ringA[rr] * act * inside));
                    rlVertex3f((float)q.x, (float)q.y, (float)q.z);
                };
                pt(ring, a0); pt(ring + 1, a0); pt(ring + 1, a1);
                pt(ring, a0); pt(ring + 1, a1); pt(ring, a1);
            }
        rlEnd();
    }
    rlDrawRenderBatchActive();
    EndBlendMode();
    rlEnableBackfaceCulling();
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
    app.earthMonths = LoadEmbeddedTexture(EarthMonthsJpg(), ".jpg");
    app.earthClouds = LoadEmbeddedTexture(EarthCloudsJpg(), ".jpg");
    if (!app.earthTex) {
        TraceLog(LOG_WARNING, "Earth textures unavailable, using procedural surface");
        return;
    }
}

// 木星の大赤斑の経度（体系 II、西経 deg、360° を超えても続けて数える）。Project Pluto がまとめた観測値
// （https://www.projectpluto.com/grs_lon.txt、1964 年以降は JUPOS）を年 1 点に間引いたもの。2025 年 12 月より後は
// 年 16° で増えるとして延ばし、1962 年より前は 1962 年の値のまま
static double GreatRedSpotLon(double tDays) {
    static const double T[][2] = {
        {1962.66, 14}, {1964.73, 18}, {1966.03, 22}, {1967.20, 28}, {1968.44, 21}, {1969.34, 21}, {1970.58, 22},
        {1971.70, 7}, {1972.66, 0}, {1973.79, 7}, {1974.40, 14}, {1975.47, 35}, {1976.20, 49}, {1977.11, 48},
        {1978.29, 55}, {1979.81, 61}, {1980.36, 58}, {1981.93, 53}, {1983.57, 39}, {1984.42, 28}, {1986.50, 19},
        {1987.92, 14}, {1988.92, 16}, {1989.84, 30}, {1990.92, 27}, {1991.42, 32}, {1992.50, 33}, {1993.33, 45},
        {1994.42, 40}, {1995.67, 49}, {1996.67, 61}, {1997.33, 62}, {1998.84, 64}, {1999.92, 66}, {2000.84, 76},
        {2001.84, 77}, {2002.17, 80}, {2003.84, 84}, {2004.92, 94}, {2006.42, 109}, {2007.33, 114}, {2008.54, 127},
        {2009.54, 138}, {2010.77, 156}, {2011.58, 168}, {2012.58, 182}, {2013.75, 202}, {2014.67, 216},
        {2015.92, 236}, {2016.92, 258}, {2017.75, 280}, {2018.84, 296}, {2019.84, 320}, {2020.92, 347},
        {2021.92, 365}, {2022.84, 386}, {2024.00, 410}, {2024.75, 421}, {2025.92, 439},
    };
    const int n = sizeof T / sizeof T[0];
    double y = 2000.0 + tDays / 365.25;
    if (y <= T[0][0]) return T[0][1];
    if (y >= T[n - 1][0]) return T[n - 1][1] + 16.0 * (y - T[n - 1][0]);
    int i = 0;
    while (T[i + 1][0] < y) i++;
    return T[i][1] + (T[i + 1][1] - T[i][1]) * (y - T[i][0]) / (T[i + 1][0] - T[i][0]);
}

// 惑星・衛星の地図（形式は tools/make_planet_maps.py）。読めた地図を名前の同じ天体に割り当てる
static void LoadBodyMaps(App& app) {
    EmbeddedFile f = MapsBin();
    const unsigned char* p = f.data;
    const unsigned char* end = f.data + f.size;
    auto rd = [&](void* dst, size_t n) { if (p + n > end) return false; std::memcpy(dst, p, n); p += n; return true; };
    auto blob = [&](EmbeddedFile* out) {
        uint32_t n;
        if (!rd(&n, 4) || p + n > end) return false;
        *out = {p, n};
        p += n;
        return true;
    };
    uint32_t count = 0;
    if (!rd(&count, 4)) return;
    for (uint32_t k = 0; k < count; k++) {
        char name[33] = {0};
        uint8_t kind, height;
        uint16_t pad;
        float mean;
        EmbeddedFile jpg, mask, data{};
        if (!rd(name, 32) || !rd(&kind, 1) || !rd(&height, 1) || !rd(&pad, 2) || !rd(&mean, 4) || !blob(&jpg) || !blob(&mask)) return;
        if (height && !blob(&data)) return;
        App::BodyMap m;
        m.name = name;
        m.kind = kind;
        m.mean = mean;
        m.color = LoadEmbeddedTexture(jpg, ".jpg");
        m.mask = LoadEmbeddedTexture(mask, ".png");
        if (height) m.data = LoadEmbeddedTexture(data, ".png");
        m.height = m.data.id != 0;
        if (!m.color.id || !m.mask.id) continue;
        for (Body& b : app.B)
            if (m.name == b.d.en && (b.d.kind == KIND_ROCKY || b.d.kind == KIND_GAS)) b.map = (int)app.maps.size();
        app.maps.push_back(m);
    }
}

static void LoadMoonTextures(App& app) {
    app.moonColor = LoadEmbeddedTexture(MoonColorJpg(), ".jpg");
    app.moonData = LoadEmbeddedTexture(MoonDataPng(), ".png");
    app.moonTex = app.moonColor.id && app.moonData.id;
    if (!app.moonTex) TraceLog(LOG_WARNING, "Moon textures unavailable, using procedural surface");
}

// 天の川の光: 赤道座標の全天画像を、カメラを中心とする球の内側に薄く貼る（星より先に描く）。
// 画像は中央が赤経 0h で、左へ行くほど赤経が増える
static void DrawMilkyWay(const App& app) {
    if (!app.milkyWay.id) return;
    const Cam& c = app.cam;
    // ズームすると画像の粗さが目立つので、視野が狭いほど薄くする（視野 40° 以上で最大、12° 以下で消える）
    double k = std::max(0.0, std::min(1.0, (c.fovy / DEG - 12.0) / 28.0));
    // 地球の大気の中（高度 100 km 未満）で太陽が地平線より上にある時は、昼の空なので消す
    const Body& e = app.B[B_EARTH];
    double alt = Len(c.pos - e.pos) - e.d.radius;
    if (alt < 100) {
        double sunEl = std::asin(Dot(Norm(c.pos - e.pos), Norm(app.B[B_SUN].pos - c.pos)));
        double day = std::max(0.0, std::min(1.0, (sunEl / DEG + 6.0) / 8.0));
        k *= 1.0 - day * std::min(1.0, (100 - alt) / 50);
    }
    if (k <= 0) return;
    unsigned char br = (unsigned char)(170 * k);
    SetPassProjection(c, 1.0, 1e6);
    rlDisableDepthTest();
    rlDisableBackfaceCulling();
    const int NU = 72, NV = 36;
    const double R = 1e5;
    rlSetTexture(app.milkyWay.id);
    rlBegin(RL_QUADS);
    rlColor4ub(br, br, br, 255);
    auto vtx = [&](int i, int j) {
        double u = (double)i / NU, v = (double)j / NV;
        V3 p = EquToRender((0.5 - u) * 360.0, 90.0 - v * 180.0) * R;
        rlTexCoord2f((float)u, (float)v);
        rlVertex3f((float)p.x, (float)p.y, (float)p.z);
    };
    for (int j = 0; j < NV; j++)
        for (int i = 0; i < NU; i++) {
            vtx(i, j); vtx(i, j + 1); vtx(i + 1, j + 1); vtx(i + 1, j);
        }
    rlEnd();
    rlSetTexture(0);
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    rlEnableDepthTest();
}

// 大気のある天体（地球・火星）の空: 高さ 0 から top km まで、スケールハイト H km で薄くなる
// 点 from から見て、太陽の円盤がほかの天体に隠されている割合（0〜1。日食）。2 つの円の重なりの面積から求める
static double SunCover(const App& app, V3 from) {
    V3 ds = app.B[B_SUN].pos - from;
    double Ls = Len(ds), rs = std::asin(std::min(1.0, app.B[B_SUN].d.radius / Ls));
    double best = 0;
    for (size_t i = 1; i < app.B.size(); i++) {
        const Body& b = app.B[i];
        if (b.d.probe >= 0 || !b.active) continue;
        V3 db = b.pos - from;
        double Lb = Len(db);
        if (Lb >= Ls || Lb < b.d.radius) continue;
        double rb = std::asin(std::min(1.0, b.d.radius / Lb));
        double d = AngleBetween(ds, db);
        if (d >= rs + rb) continue;
        double area;
        if (d <= std::fabs(rs - rb)) {
            area = PI * std::min(rs, rb) * std::min(rs, rb);
        } else {
            double a1 = std::acos(std::max(-1.0, std::min(1.0, (d * d + rs * rs - rb * rb) / (2 * d * rs))));
            double a2 = std::acos(std::max(-1.0, std::min(1.0, (d * d + rb * rb - rs * rs) / (2 * d * rb))));
            area = rs * rs * (a1 - std::sin(2 * a1) / 2) + rb * rb * (a2 - std::sin(2 * a2) / 2);
        }
        best = std::max(best, area / (PI * rs * rs));
    }
    return std::min(1.0, best);
}

// beta: レイリー散乱（火星は塵）, mie: 太陽のまわりの光の色, ozone: オゾンの吸収（地球だけ）
struct SkyAtmosphere { int body; double top, H; Vector3 beta, mie, ozone; };
static const SkyAtmosphere SKY_ATM[] = {
    {B_EARTH, 120, 8.5, {0.10f, 0.23f, 0.56f}, {1.0f, 0.95f, 0.9f}, {0.05f, 0.08f, 0.003f}},
    {B_MARS, 80, 11.1, {0.16f, 0.11f, 0.07f}, {0.45f, 0.65f, 1.0f}, {0, 0, 0}},   // 塵で昼は黄褐色、夕方は太陽のまわりが青白い
};

// カメラのいる大気（なければ nullptr）と、空気の濃さ・その場所の真上・太陽の高さ（sin）
static const SkyAtmosphere* CameraAtmosphere(const App& app, double* density, V3* up, double* sunEl) {
    const Cam& c = app.cam;
    for (const SkyAtmosphere& a : SKY_ATM) {
        const Body& b = app.B[a.body];
        V3 rel = c.pos - b.pos;
        double alt = Len(rel) - SurfaceRadius(b, Norm(rel));
        if (alt > a.top) continue;
        *density = std::exp(-std::max(0.0, alt) / a.H) * std::min(1.0, (a.top - alt) / (a.top * 0.3));
        *up = Norm(rel);
        *sunEl = Dot(*up, Norm(app.B[B_SUN].pos - c.pos));
        return &a;
    }
    return nullptr;
}

// 昼の空の明るさ（0: 宇宙・夜、1: 昼の地表）。星座名などの表示を消すのに使う
static double SkyDaylight(const App& app) {
    double density, sunEl;
    V3 up;
    if (!CameraAtmosphere(app, &density, &up, &sunEl)) return 0;
    double light = std::max(0.0, std::min(1.0, (sunEl + 0.17) / 0.2));
    return std::min(1.0, density * 2.0) * light;
}

static void DrawSkyAtmosphere(const App& app) {
    double density, sunEl;
    V3 up;
    const SkyAtmosphere* a = CameraAtmosphere(app, &density, &up, &sunEl);
    if (!a || !app.skyShader.id || density < 1e-3) return;
    const Cam& c = app.cam;
    SetPassProjection(c, 1.0, 1e6);
    rlDisableDepthTest();
    rlDisableBackfaceCulling();
    Vector3 u = ToF(up), sd = ToF(Norm(app.B[B_SUN].pos - c.pos));
    float dens = (float)density;
    BeginShaderMode(app.skyShader);
    SetShaderValue(app.skyShader, app.locSkyUp, &u, SHADER_UNIFORM_VEC3);
    SetShaderValue(app.skyShader, app.locSkySun, &sd, SHADER_UNIFORM_VEC3);
    SetShaderValue(app.skyShader, app.locSkyBeta, &a->beta, SHADER_UNIFORM_VEC3);
    SetShaderValue(app.skyShader, app.locSkyMie, &a->mie, SHADER_UNIFORM_VEC3);
    SetShaderValue(app.skyShader, app.locSkyOzone, &a->ozone, SHADER_UNIFORM_VEC3);
    float vis = (float)(1.0 - SunCover(app, c.pos));
    SetShaderValue(app.skyShader, app.locSkySunVis, &vis, SHADER_UNIFORM_FLOAT);
    SetShaderValue(app.skyShader, app.locSkyDensity, &dens, SHADER_UNIFORM_FLOAT);
    const int NU = 48, NV = 24;
    const double R = 1e5;
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, 255);
    auto vtx = [&](int i, int j) {
        double lo = 2 * PI * i / NU, la = PI * (0.5 - (double)j / NV);
        V3 p = V3(std::cos(la) * std::cos(lo), std::sin(la), std::cos(la) * std::sin(lo)) * R;
        rlVertex3f((float)p.x, (float)p.y, (float)p.z);
    };
    for (int j = 0; j < NV; j++)
        for (int i = 0; i < NU; i++) {
            vtx(i, j); vtx(i, j + 1); vtx(i + 1, j + 1); vtx(i + 1, j);
        }
    rlEnd();
    EndShaderMode();
    rlEnableBackfaceCulling();
    rlEnableDepthTest();
}

// ---------------------------------------------------------------- 音楽の選曲
// 一番近い天体（表面までの距離）の曲。月以外の衛星は母惑星の曲。冥王星とカロン・準惑星・小惑星・彗星は無音
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
        if (app.B[i].d.probe >= 0) continue;  // 探査機に寄り添っていても、近くの惑星の曲を流す
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
        if (b.d.probe >= 0) continue;  // 探査機は DrawProbes で描く
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
            bool season = app.earthMonths.id && app.earthClouds.id;
            if (season) {
                app.mat.maps[MATERIAL_MAP_NORMAL].texture = app.earthClouds;
                app.mat.maps[MATERIAL_MAP_ROUGHNESS].texture = app.earthMonths;
            }
            // 今の月（各月の画像はその月の中ごろ）: 1 年の中の位置から
            double year = 2000.0 + (app.tDays + 0.5) / 365.2425;
            double month = std::fmod((year - std::floor(year)) * 12.0 - 0.5 + 12.0, 12.0);
            float info2[4] = {0, 0, (float)month, season ? 1.0f : 0.0f};
            SetShaderValue(app.shader, app.locMapInfo2, info2, SHADER_UNIFORM_VEC4);
        } else if (d.style == S_MOON && app.moonTex) {
            style = S_MOON_TEX;
            app.mat.maps[MATERIAL_MAP_DIFFUSE].texture = app.moonColor;
            app.mat.maps[MATERIAL_MAP_SPECULAR].texture = app.moonData;
        } else if (b.map >= 0) {
            const App::BodyMap& m = app.maps[b.map];
            style = S_MAPPED;
            app.mat.maps[MATERIAL_MAP_DIFFUSE].texture = m.color;
            app.mat.maps[MATERIAL_MAP_SPECULAR].texture = m.height ? m.data : m.color;
            app.mat.maps[MATERIAL_MAP_NORMAL].texture = m.mask;
            float info[4] = {1.0f / std::max(0.05f, m.mean), (float)m.kind, m.height ? 1.0f : 0.0f, (float)d.radius};
            // 木星の地図は大赤斑が経度 0° にあるので、大赤斑の今の西経（体系 II）だけ回す
            float shift = it.i == B_JUPITER ? (float)(GreatRedSpotLon(app.tDays) * DEG) : 0.0f;
            float info2[4] = {(float)d.style, shift, 0, 0};
            SetShaderValue(app.shader, app.locMapInfo, info, SHADER_UNIFORM_VEC4);
            SetShaderValue(app.shader, app.locMapInfo2, info2, SHADER_UNIFORM_VEC4);
        }
        float detail[4] = {(float)style, (float)(QUALITY_DETAIL[app.quality] / (c.focal * d.radius * s)), (float)app.tDays, cloudK};
        SetShaderValue(app.shader, app.locDetail, detail, SHADER_UNIFORM_VEC4);
        // 大気のある天体: 日光の色と、カメラが同じ大気の中にいる時のかすみ
        {
            Vector3 beta = {0, 0, 0}, mie = {0, 0, 0}, ozone = {0, 0, 0}, up = {0, 1, 0};
            float haze[4] = {0, (float)(1.0 / s), 1, 1};
            for (const SkyAtmosphere& a : SKY_ATM)
                if (a.body == it.i) {
                    beta = a.beta;
                    mie = a.mie;
                    ozone = a.ozone;
                    double density, sunEl;
                    V3 u;
                    if (CameraAtmosphere(app, &density, &u, &sunEl) == &a) {
                        haze[0] = (float)density;
                        haze[2] = (float)a.H;
                        haze[3] = (float)(1.0 - SunCover(app, app.cam.pos));
                        up = ToF(u);
                    }
                }
            SetShaderValue(app.shader, app.locAtmBeta, &beta, SHADER_UNIFORM_VEC3);
            SetShaderValue(app.shader, app.locAtmMie, &mie, SHADER_UNIFORM_VEC3);
            SetShaderValue(app.shader, app.locAtmOzone, &ozone, SHADER_UNIFORM_VEC3);
            SetShaderValue(app.shader, app.locAtmHaze, haze, SHADER_UNIFORM_VEC4);
            SetShaderValue(app.shader, app.locCamUp, &up, SHADER_UNIFORM_VEC3);
        }
        if (d.rings) app.mat.maps[MATERIAL_MAP_ROUGHNESS].texture = app.ringTex;   // 本体に落ちる環の影と環の描画
        DrawMesh(b.shape >= 0 ? app.shapeMesh[b.shape] : app.sphere, app.mat, BodyMatrix(b, rel * s, d.radius * s, rPol * s, b.spin));

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
        if (app.B[i].d.probe >= 0) continue;
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
        double target = s.thrusting ? 1.0 : (!app.physics && std::fabs(s.speed) > 0 ? 0.25 : 0.0);   // 重力モードの慣性飛行では噴射しない
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

// 探査機の機体（自機と同じシェーダ）。天体に隠れている時は描かない
static const double GALILEO_PROBE_RELEASE = -1633.27;  // 1995-07-13 05:30 UTC: ガリレオが大気圏突入機を切り離す
static const double HUYGENS_RELEASE = 1819.58;         // 2004-12-25 02:00 UTC: カッシーニがホイヘンスを切り離す
static void DrawProbes(App& app) {
    const Cam& c = app.cam;
    for (size_t i = 0; i < app.B.size(); i++) {
        const Body& b = app.B[i];
        if (b.d.probe < 0 || !b.active) continue;
        V3 rel = b.pos - c.pos;
        double dist = Len(rel);
        if (0.015 / dist * c.focal < 0.5 || Dot(rel, c.fwd) < -0.02) continue;
        if (Occluded(app, c.pos, b.pos, (int)i)) continue;
        SetPassProjection(c, std::max(1e-5, (dist - 0.02) * 0.5), dist + 0.02);
        ClearDepth();
        SetShipLighting(app, b.pos);
        // 太陽の反対側から見ると真っ黒な影絵になるので、視点側から弱い補助光を当てる
        Vector3 fillDir = ToF(Norm(c.pos - b.pos)), fillColor = {0.3f, 0.3f, 0.32f};
        SetShaderValue(app.shipShader, app.locFillDir, &fillDir, SHADER_UNIFORM_VEC3);
        SetShaderValue(app.shipShader, app.locFillColor, &fillColor, SHADER_UNIFORM_VEC3);
        Vector3 zero = {0, 0, 0};
        float off = 0;
        SetShaderValue(app.shipShader, app.locEngColor, &zero, SHADER_UNIFORM_VEC3);
        SetShaderValue(app.shipShader, app.locEngGlow, &off, SHADER_UNIFORM_FLOAT);
        SetShaderValue(app.shipShader, app.locNavBlink, &off, SHADER_UNIFORM_FLOAT);
        // パーカーは耐熱シールドを太陽へ、ほかは高利得アンテナを地球へ向ける
        // パーカーと JWST は太陽へ、地球を回る人工衛星は進む向き（上が天頂）、ほかは高利得アンテナを地球へ向ける
        int pm = b.d.probeModel;
        V3 aim = (pm == PM_PARKER || pm == PM_JWST ? app.B[B_SUN].pos : app.B[B_EARTH].pos) - b.pos, upHint = Y_UP;
        if (b.d.tleN > 0) {
            aim = SatellitePos(b.d, app.tDays + 1.0 / 86400) - SatellitePos(b.d, app.tDays);
            upHint = Norm(b.pos - app.B[B_EARTH].pos);
        }
        if (pm == PM_GALILEO && app.tDays < GALILEO_PROBE_RELEASE) pm = PM_GALILEO_PROBE;
        if (pm == PM_CASSINI && app.tDays < HUYGENS_RELEASE) pm = PM_CASSINI_HUYGENS;
        Quaternion q = QuatFromBasis(Len(aim) > 1e-6 ? aim : V3(1, 0, 0), upHint);
        Matrix model = MatrixMultiply(MatrixMultiply(MatrixScale(0.001f, 0.001f, 0.001f), QuaternionToMatrix(q)),
                                      MatrixTranslate((float)rel.x, (float)rel.y, (float)rel.z));
        rlDisableBackfaceCulling();
        DrawMesh(app.probeMesh[pm], app.shipMat, model);
        rlEnableBackfaceCulling();
    }
    rlDrawRenderBatchActive();
}

// ---------------------------------------------------------------- 描画: 2D
static void DrawStars(const App& app) {
    const Cam& c = app.cam;
    if (app.skyMode > 0) {
        // 星座線は星より奥に、暗く細く描く
        const Color lc = {90, 130, 200, 120};
        for (const Constellation& con : app.sky.cons)
            for (const std::vector<V3>& line : con.lines)
                for (size_t i = 0; i + 1 < line.size(); i++) {
                    Vector2 p, q;
                    if (Project(c, line[i], &p) && Project(c, line[i + 1], &q)) DrawLineEx(p, q, 1.2f, lc);
                }
    }
    for (const Star& st : app.sky.stars) {
        Vector2 p;
        if (!Project(c, st.dir, &p)) continue;
        if (p.x < -4 || p.y < -4 || p.x >= c.W + 4 || p.y >= c.H + 4) continue;
        if (st.size > 0) DrawCircleV(p, st.size, st.col);
        else DrawPixelV(p, st.col);
    }
}

// 小惑星帯などの粒: 1 ピクセルの点。種類ごとに色を変える（地表近くでは軌道線と同じく描かない）
static void DrawAsteroids(App& app) {
    if (!app.showOrbits || !app.showAsteroids || app.belt.a.empty()) return;
    const Cam& c = app.cam;
    for (const Body& b : app.B)
        if (Len(c.pos - b.pos) < b.d.radius * 1.3) return;
    UpdateAsteroids(app.belt, app.tDays);
    static const unsigned char COL[4][4] = {{205, 190, 165, 120}, {235, 150, 120, 150}, {160, 215, 150, 130}, {150, 185, 235, 140}};
    rlCheckRenderBatchLimit(4);
    rlBegin(RL_QUADS);
    for (size_t k = 0; k < app.belt.pos.size(); k++) {
        Vector2 p;
        if (!Project(c, app.belt.pos[k] - c.pos, &p) || p.x < 0 || p.y < 0 || p.x >= c.W || p.y >= c.H) continue;
        const unsigned char* col = COL[app.belt.kind[k] & 3];
        rlCheckRenderBatchLimit(4);
        rlColor4ub(col[0], col[1], col[2], col[3]);
        rlVertex2f(p.x, p.y);
        rlVertex2f(p.x, p.y + 1);
        rlVertex2f(p.x + 1, p.y + 1);
        rlVertex2f(p.x + 1, p.y);
    }
    rlEnd();
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

// 星座名と恒星の名前。画質を下げてもにじまないよう、縮小描画の後に重ねる（天体に隠れる名前は出さない）
static void DrawSkyLabels(const App& app) {
    if (app.skyMode != 2 || SkyDaylight(app) > 0.3) return;
    const Cam& c = app.cam;
    auto hidden = [&](V3 dir) { return Occluded(app, c.pos, c.pos + dir * 1e13, -1); };
    for (const Constellation& con : app.sky.cons) {
        Vector2 p;
        if (Project(c, con.label, &p) && p.x > -200 && p.x < c.W + 200 && p.y > 0 && p.y < c.H && !hidden(con.label))
            TxtC(TR(con.jp.c_str(), con.en.c_str()), p.x, p.y - 8, 15, {120, 160, 225, 190});
    }
    for (const StarName& n : app.sky.names) {
        Vector2 p;
        if (Project(c, n.dir, &p) && p.x > -100 && p.x < c.W && p.y > 0 && p.y < c.H && !hidden(n.dir))
            Txt(TR(n.jp.c_str(), n.en.c_str()), p.x + 6, p.y - 6, 13, {225, 215, 180, 190});
    }
}

// 皆既日食のコロナ: 太陽の縁から放射状に伸びる淡い光の筋（赤道方向に長い流線、極の方向に短い羽毛状の筋）
static void DrawCorona(Vector2 c, float rs, float k) {
    rs = std::max(rs, 3.0f);
    BeginBlendMode(BLEND_ADDITIVE);
    DrawCircleGradient((int)c.x, (int)c.y, rs * 2.6f, {235, 238, 255, (unsigned char)(110 * k)}, {200, 210, 255, 0});
    const int N = 240;
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < N; i++) {
        double a0 = 2 * PI * i / N, a1 = 2 * PI * (i + 1) / N;
        // 角度ごとの筋の長さ: 赤道（画面の左右）方向に長く、いくつかの流線が特に長い
        auto len = [&](double a) {
            double eq = 0.55 + 0.45 * std::pow(std::fabs(std::cos(a)), 1.5);
            double h = std::sin(a * 7.0 + 1.3) * 0.25 + std::sin(a * 13.0 + 0.4) * 0.15 + std::sin(a * 29.0 + 2.1) * 0.08;
            return rs * (1.4 + 2.6 * eq * (0.75 + h));
        };
        float l0 = (float)len(a0), l1 = (float)len(a1);
        unsigned char al = (unsigned char)(150 * k);
        rlColor4ub(240, 242, 255, al); rlVertex2f(c.x + (float)std::cos(a0) * rs, c.y + (float)std::sin(a0) * rs);
        rlColor4ub(200, 210, 255, 0); rlVertex2f(c.x + (float)std::cos(a0) * l0, c.y + (float)std::sin(a0) * l0);
        rlColor4ub(240, 242, 255, al); rlVertex2f(c.x + (float)std::cos(a1) * rs, c.y + (float)std::sin(a1) * rs);
        rlColor4ub(240, 242, 255, al); rlVertex2f(c.x + (float)std::cos(a1) * rs, c.y + (float)std::sin(a1) * rs);
        rlColor4ub(200, 210, 255, 0); rlVertex2f(c.x + (float)std::cos(a0) * l0, c.y + (float)std::sin(a0) * l0);
        rlColor4ub(200, 210, 255, 0); rlVertex2f(c.x + (float)std::cos(a1) * l1, c.y + (float)std::sin(a1) * l1);
    }
    rlEnd();
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    EndBlendMode();
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
            // 日食: 欠けた分だけまぶしさを弱め、皆既の直前からコロナを描く（月の円盤は後で 3D に重なって中央を隠す）
            double cover = SunCover(app, c.pos);
            double k = std::max(0.0, std::min(1.0, (cover - 0.985) / 0.015));
            if (k > 0) DrawCorona(p, (float)px, (float)k);
            if (Occluded(app, c.pos, b.pos, B_SUN)) continue;
            float r = (float)(px * 1.8 + 14), g = (float)std::pow(1.0 - cover, 0.6);
            BeginBlendMode(BLEND_ADDITIVE);
            DrawCircleGradient((int)p.x, (int)p.y, r, {255, 240, 200, (unsigned char)(200 * g)}, {255, 200, 120, 0});
            DrawCircleGradient((int)p.x, (int)p.y, r * 3, {255, 200, 140, (unsigned char)(35 * g)}, {255, 160, 80, 0});
            EndBlendMode();
            if (px < 0.8) DrawCircleV(p, 2.5f, {255, 250, 230, 255});
        } else if (b.d.probe < 0 && b.outer / Len(rel) * c.focal < 0.8) {
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
    if (app.B[app.target].active) DrawMarker(app, app.target);  // ターゲットを優先して配置
    for (size_t i = 0; i < app.B.size(); i++) {
        const BodyDef& d = app.B[i].d;
        if ((int)i == app.target || !app.B[i].active) continue;
        if (d.probe >= 0) {
            // 探査機: 衛星マーカーと一緒に切り替え。近く（1,000 万 km 以内）か画面内に見える時だけ
            if (!app.showMoonMarkers) continue;
            Vector2 p;
            bool on = Project(c, app.B[i].pos - c.pos, &p) && p.x >= 0 && p.y >= 0 && p.x <= c.W && p.y <= c.H;
            if (!on && Len(app.B[i].pos - c.pos) > 1e7) continue;
        } else if (d.parent > 0) {
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
    bool over = !app.physics && std::fabs(s.speed) > vmax * 1.001;   // 重力モードでは最高速度の制限はない
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
        for (size_t i = 0; i < app.sky.stars.size(); i++) {
            const Star& st = app.sky.stars[i];
            if (st.mag > 6.5f) break;              // 暗い星は描かない（明るい順に並んでいる）
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

static void DrawToast(const App& app) {
    if (app.toastTime <= 0) return;
    float a = (float)std::min(1.0, app.toastTime);
    TxtC(app.toast.c_str(), app.cam.W * 0.5f, app.cam.H * 0.24f, 24, Fade({255, 240, 200, 255}, a));
}

static void DrawHUD(App& app) {
    const Cam& c = app.cam;
    if (app.photoMode > 0) {   // 写真モード: 一覧・入力欄と、切り替えの知らせだけ
        DrawToast(app);
        DrawModalPanels(app);
        return;
    }
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
        if (!app.B[i].active) continue;   // 打ち上げ前・運用終了後の探査機
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
    if (app.physics && Massive(app.B[s.ref])) {
        // 重力モード: 基準天体のまわりの軌道（パネルの下に）
        double peri, apo, period;
        const char* nm = TR(app.B[s.ref].d.jp, app.B[s.ref].d.en);
        if (OrbitInfo(app, &peri, &apo, &period))
            snprintf(b, sizeof b, T(UI_PHYS_ORBIT), FmtDist(peri).c_str(), FmtDist(apo).c_str(), FmtDuration(period).c_str(), nm);
        else
            snprintf(b, sizeof b, T(UI_PHYS_ESCAPE), nm);
        float tw = TxtW(b, 15) + 16;
        DrawRectangleRounded({6, lh * 6 + 24, tw, 26}, 0.3f, 6, {0, 10, 25, 150});
        Txt(b, 14, lh * 6 + 28, 15, {150, 230, 255, 255});
    }

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

    DrawToast(app);

    if (app.showHelp) {
        const int lines = UI_HELP15 - UI_HELP1 + 1;
        float w = std::min(c.W - 20, 980.0f), h = 74 + lines * 26.0f, hx = (c.W - w) / 2, hy = (c.H - h) / 2;
        DrawRectangleRounded({hx, hy, w, h}, 0.04f, 6, {0, 8, 20, 225});
        DrawRectangleRoundedLinesEx({hx, hy, w, h}, 0.04f, 6, 1.5f, {90, 130, 180, 255});
        TxtC(T(UI_HELP_TITLE), c.W * 0.5f, hy + 10, 22, {255, 220, 120, 255});
        for (int i = 0; i < lines; i++)
            Txt(T((UiId)(UI_HELP1 + i)), hx + 24, hy + 44 + i * 26.0f, i >= 12 ? 15 : 17, i >= 12 ? Color{170, 220, 190, 255} : Color{215, 228, 245, 255});
        TxtC(CREDIT, c.W * 0.5f, hy + h - 26, 13, {150, 170, 190, 255});
    }
    DrawTourPanel(app);
    DrawModalPanels(app);
}

// シェーダは GLSL 3.30 で書いてある。ブラウザ版 (WebGL2) では GLSL ES 3.00 に書き換える
static std::string GlslForPlatform(const char* src) {
    std::string s = src;
#if defined(__EMSCRIPTEN__)
    size_t p = s.find("#version 330");
    if (p != std::string::npos)   // #version は 1 行目に置く必要がある（ソースは改行から始まる）
        s = "#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler2D;\n" + s.substr(p + 12);
#endif
    return s;
}

// ---------------------------------------------------------------- main
int main(int argc, char** argv) {
    Options opt = ParseArgs(argc, argv);
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
    app.sky = LoadSky();
    app.belt = LoadAsteroids();
    app.showAsteroids = GetSetting("asteroids") != "0";
    app.showOrbits = GetSetting("orbits") != "0";
    app.showMoonMarkers = GetSetting("markers") != "0";
    app.invertY = GetSetting("invert") == "1";
    app.physics = GetSetting("physics") == "1";

    // フォント
    std::string allText = "°²·×〜";
    for (int i = 0; i < UI_COUNT; i++) allText += UI_JP[i];
    for (const Body& b : app.B) allText += b.d.jp;
    for (const Engine& e : ENGINES) allText += e.jp;
    for (const Constellation& c : app.sky.cons) allText += c.jp;
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
    {
        EmbeddedFile f = SaturnRingsPng();
        Image img = LoadImageFromMemory(".png", f.data, (int)f.size);
        if (img.data) {
            app.ringTex = LoadTextureFromImage(img);
            UnloadImage(img);
            SetTextureFilter(app.ringTex, TEXTURE_FILTER_BILINEAR);
            SetTextureWrap(app.ringTex, TEXTURE_WRAP_CLAMP);
        }
    }
    app.shader.locs[SHADER_LOC_MAP_ROUGHNESS] = GetShaderLocation(app.shader, "texture3");   // DrawMesh が texture3 に割り当てる
    LoadBodyMaps(app);
    app.locMapInfo = GetShaderLocation(app.shader, "mapInfo");
    app.locMapInfo2 = GetShaderLocation(app.shader, "mapInfo2");
    app.skyShader = LoadShaderFromMemory(GlslForPlatform(SKY_VS).c_str(), GlslForPlatform(SKY_FS).c_str());
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
    app.ring = GenRing(72000.0f / 60268.0f, 142500.0f / 60268.0f, 720);   // D 環の内側〜F 環の外側
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
    if (opt.engine >= 0) app.ship.engine = std::min(opt.engine, ENGINE_COUNT - 1);
    app.cockpit = opt.cockpit;
    app.camYaw = opt.camYaw; app.camPitch = opt.camPitch; app.camDist = opt.camDist;
    app.autopilot = opt.autopilot;
    app.apAutoEngine = opt.autoEngine;
    app.showHelp = opt.help;
    app.photoMode = std::max(0, std::min(2, opt.photo));
    app.settingsOpen = opt.settings;
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

        DrawSkyLabels(app);
        if (app.warpT < 0 && app.photoMode == 0) DrawMarkers(app);
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
    for (Mesh& m : app.shapeMesh) UnloadMesh(m);
    UnloadMesh(app.shipMesh);
    for (Mesh& m : app.probeMesh) UnloadMesh(m);
    UnloadShader(app.shipShader);
    UnloadShader(app.shader);
    UnloadShader(app.skyShader);
    if (app.earthTex) { UnloadTexture(app.earthColor); UnloadTexture(app.earthData); }
    if (app.earthMonths.id) UnloadTexture(app.earthMonths);
    if (app.earthClouds.id) UnloadTexture(app.earthClouds);
    if (app.moonTex) { UnloadTexture(app.moonColor); UnloadTexture(app.moonData); }
    if (app.milkyWay.id) UnloadTexture(app.milkyWay);
    if (app.ringTex.id) UnloadTexture(app.ringTex);
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
