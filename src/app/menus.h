// Solar System Voyager — Copyright (c) 2026 hubstacksea
// MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
// 設定画面・タッチ操作・初回の案内・全体図・一覧などのパネル。
// main.cpp の一部で、main.cpp が決まった順に読み込む（単体ではコンパイルしない）

// ---------------------------------------------------------------- 設定（F10 キー。それぞれのキー操作も同じ関数を使う）
enum SettingItem { SET_LANG, SET_TEXTSIZE, SET_QUALITY, SET_SOUND, SET_MUSIC, SET_SKY, SET_MILKYWAY, SET_ORBITS, SET_ASTEROIDS,
                   SET_MARKERS, SET_INVERT, SET_PHYSICS, SET_RESUME, SET_FULLSCREEN, SET_COUNT };
static const float SET_ROW_H = 28;   // 設定画面の 1 行の高さ

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
        case SET_TEXTSIZE:
            g_uiScaleIdx = (g_uiScaleIdx + dir + 3) % 3;
            SetSetting("textsize", std::to_string(g_uiScaleIdx));
            break;
        case SET_MILKYWAY:
            app.showMilkyWay = !app.showMilkyWay;
            SetSetting("milkyway", onoff(app.showMilkyWay));
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
        case SET_RESUME:
            app.resumeMode = (app.resumeMode + dir + 3) % 3;
            SetSetting("resume_mode", std::to_string(app.resumeMode));
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
        case SET_TEXTSIZE: return T((UiId)(UI_SET_TS0 + g_uiScaleIdx));
        case SET_MILKYWAY: return onoff(app.showMilkyWay);
        case SET_QUALITY: return T((UiId)(UI_SET_Q0 + app.quality));
        case SET_SOUND: return onoff(!sfx::g.muted);
        case SET_MUSIC: return onoff(!sfx::gm.off);
        case SET_SKY: return T((UiId)(UI_SET_SKY0 + app.skyMode));
        case SET_ORBITS: return onoff(app.showOrbits);
        case SET_ASTEROIDS: return onoff(app.showAsteroids);
        case SET_MARKERS: return onoff(app.showMoonMarkers);
        case SET_INVERT: return onoff(app.invertY);
        case SET_PHYSICS: return T(app.physics ? UI_SET_GRAVITY : UI_SET_ARCADE);
        case SET_RESUME: return T((UiId)(UI_SET_RES0 + app.resumeMode));
        default: return onoff(IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE));
    }
}

// ---------------------------------------------------------------- タッチ操作（スマホ・タブレット）
enum TouchButton { TB_THRUST, TB_BRAKE, TB_STOP, TB_TARGET, TB_WARP, TB_AUTO, TB_TOUR, TB_EVENTS, TB_MAP, TB_INFO, TB_SEARCH, TB_SETTINGS,
                   TB_HELP, TB_SHARE, TB_COUNT };

// 画面のボタンの位置: 右に速度、左に行き先、下にメニュー（画面の高さに合わせた大きさ）
static Rectangle TouchButtonRect(const Cam& c, int i) {
    float s = std::max(56.0f, c.H * 0.1f), g = s * 0.15f;
    float ly = std::max(c.H * 0.30f, 160 + s * 0.55f + g * 2);   // 左の列は上のメニューの段より下に
    switch (i) {
        case TB_THRUST: return {c.W - s * 1.35f - 12, c.H * 0.30f, s * 1.35f, s * 1.1f};
        case TB_BRAKE: return {c.W - s * 1.35f - 12, c.H * 0.30f + s * 1.1f + g, s * 1.35f, s * 0.9f};
        case TB_STOP: return {c.W - s * 1.35f - 12, c.H * 0.30f + s * 2.0f + g * 2, s * 1.35f, s * 0.6f};
        case TB_TARGET: return {12, ly, s * 1.5f, s * 0.8f};
        case TB_WARP: return {12, ly + s * 0.8f + g, s * 1.5f, s * 0.8f};
        case TB_AUTO: return {12, ly + (s * 0.8f + g) * 2, s * 1.5f, s * 0.8f};
        default: {   // ツアー・イベント・全体図・情報・検索・設定・説明・共有: 左上のパネルの下に横に並べる
            int k = i - TB_TOUR;
            float w = s * 1.15f;
            return {12 + k * (w + g), 160, w, s * 0.55f};
        }
    }
}

static int TouchButtonAt(const Cam& c, Vector2 p) {
    for (int i = 0; i < TB_COUNT; i++)
        if (CheckCollisionPointRec(p, TouchButtonRect(c, i))) return i;
    return -1;
}

// エンジン欄の各エンジンの位置（DrawEngineBar と同じ）
static Rectangle EngineButtonRect(const Cam& c, int i) {
    float bw = std::min(118.0f, (c.W - 300) / ENGINE_COUNT), bh = 40;
    float total = bw * ENGINE_COUNT, x0 = std::max(10.0f, (c.W - total) * 0.5f - 60), y0 = c.H - bh - 14;
    return {x0 + i * bw + 2, y0, bw - 4, bh};
}

// 毎フレーム最初に: 指の状態から、押しているボタン・ドラッグ量・タップを求める
static void UpdateTouch(App& app) {
    const Cam c = UiCam(app.cam);
    float k = UiScale(app.cam);
    int n = GetTouchPointCount();
    if (n > 0 && !app.touchUI) {
        app.touchUI = true;
        app.mouseCaptured = false;
        EnableCursor();
    }
    app.touchDrag = {0, 0};
    app.tapped = false;
    if (!app.touchUI) return;
    bool down = n > 0;
    Vector2 p = down ? Vector2Scale(GetTouchPosition(0), 1 / k) : app.touchPrev;   // 2D の表示と同じ座標に
    if (down && !app.touchWasDown) {
        app.touchStart = app.touchPrev = p;
        app.touchMoved = false;
        app.touchHeld = TouchButtonAt(c, p);
    } else if (down) {
        Vector2 d = Vector2Subtract(p, app.touchPrev);
        if (Vector2Distance(p, app.touchStart) > 12) app.touchMoved = true;
        if (app.touchHeld < 0) app.touchDrag = Vector2Scale(d, k);   // ボタン以外の所のドラッグで機首を向ける（画面のピクセルで）
        app.touchPrev = p;
    } else if (app.touchWasDown) {
        if (!app.touchMoved) {
            app.tapped = true;
            app.tapPos = app.touchStart;
        }
        app.touchHeld = -1;
    }
    app.touchWasDown = down;
}

static void StartTour(App& app);
static void StopTour(App& app);
static void OpenEventsPage(App& app);

static void DrawTouchButtons(const App& app) {
    if (!app.touchUI) return;
    const Cam& c = app.cam;
    static const UiId LABEL[TB_COUNT] = {UI_TB_THRUST, UI_TB_BRAKE, UI_TB_STOP, UI_TB_TARGET, UI_TB_WARP, UI_TB_AUTO,
                                         UI_TB_TOUR, UI_TB_EVENTS, UI_TB_MAP, UI_TB_INFO, UI_TB_SEARCH, UI_TB_SETTINGS,
                                         UI_TB_HELP, UI_TB_SHARE};
    for (int i = 0; i < TB_COUNT; i++) {
        Rectangle r = TouchButtonRect(c, i);
        bool held = app.touchHeld == i;
        bool on = (i == TB_AUTO && app.autopilot) || (i == TB_TOUR && app.tourOn) || (i == TB_INFO && app.showInfo);
        DrawRectangleRounded(r, 0.3f, 6, held ? Color{70, 110, 170, 220} : on ? Color{40, 90, 70, 200} : Color{0, 12, 30, 150});
        DrawRectangleRoundedLinesEx(r, 0.3f, 6, 1.5f, {110, 150, 200, 200});
        float fs = std::min(r.height * 0.36f, 20.0f);
        TxtC(T(LABEL[i]), r.x + r.width / 2, r.y + (r.height - fs) / 2, fs, {225, 235, 250, 255});
    }
}

// ---------------------------------------------------------------- 初回の案内
static const int INTRO_PAGES = 4;

static void CloseIntro(App& app) {
    app.introPage = -1;
    SetSetting("intro", "1");
    SaveSettings();
}

static void DrawIntro(const App& app) {
    if (app.introPage < 0) return;
    const Cam& c = app.cam;
    static const UiId TITLE[INTRO_PAGES] = {UI_INTRO_T1, UI_INTRO_T2, UI_INTRO_T3, UI_INTRO_T4};
    static const UiId BODY_A[INTRO_PAGES] = {UI_INTRO_A1, UI_INTRO_A2, UI_INTRO_A3, UI_INTRO_A4};
    static const UiId BODY_AT[INTRO_PAGES] = {UI_INTRO_A1, UI_INTRO_A2T, UI_INTRO_A3T, UI_INTRO_A4T};
    static const UiId BODY_B[INTRO_PAGES] = {UI_INTRO_B1, UI_INTRO_B2, UI_INTRO_B3, UI_INTRO_B4};
    int pg = app.introPage;
    float w = std::min(c.W - 20, 900.0f), h = 250, x = (c.W - w) / 2, y = (c.H - h) / 2;
    DrawRectangle(0, 0, (int)c.W, (int)c.H, {0, 0, 0, 110});
    DrawRectangleRounded({x, y, w, h}, 0.05f, 6, {0, 10, 28, 240});
    DrawRectangleRoundedLinesEx({x, y, w, h}, 0.05f, 6, 1.5f, {110, 160, 220, 255});
    char b[32];
    snprintf(b, sizeof b, "%d / %d", pg + 1, INTRO_PAGES);
    Txt(b, x + 18, y + 14, 15, {150, 170, 190, 255});
    TxtC(T(TITLE[pg]), c.W * 0.5f, y + 22, 26, {255, 220, 120, 255});
    // 本文は幅に合わせて折り返す（日本語は 1 文字ずつ、英語は単語ごと）
    auto wrap = [&](const char* text, float ty, float fs, Color col) {
        std::string s = text, line;
        float maxw = w - 60;
        size_t i = 0;
        while (i < s.size()) {
            size_t len = 1;
            unsigned char ch = (unsigned char)s[i];
            if (ch >= 0xF0) len = 4; else if (ch >= 0xE0) len = 3; else if (ch >= 0xC0) len = 2;
            if (ch < 0x80 && ch != ' ') {   // 英単語はまとめる
                len = 0;
                while (i + len < s.size() && (unsigned char)s[i + len] < 0x80 && s[i + len] != ' ') len++;
            }
            std::string piece = s.substr(i, len);
            if (!line.empty() && TxtW((line + piece).c_str(), fs) > maxw) {
                TxtC(line.c_str(), c.W * 0.5f, ty, fs, col);
                ty += fs * 1.5f;
                line.clear();
                if (piece == " ") { i += len; continue; }
            }
            line += piece;
            i += len;
        }
        if (!line.empty()) TxtC(line.c_str(), c.W * 0.5f, ty, fs, col);
        return ty + fs * 1.5f;
    };
    float ty = wrap(T(app.touchUI ? BODY_AT[pg] : BODY_A[pg]), y + 72, 19, {225, 235, 250, 255});
    wrap(T(BODY_B[pg]), ty + 8, 17, {170, 200, 230, 255});
    TxtC(T(app.touchUI ? UI_INTRO_TAP : UI_INTRO_KEYS), c.W * 0.5f, y + h - 30, 13, {150, 170, 190, 255});
    if (app.touchUI) TxtC("×", x + w - 24, y + 8, 26, {200, 210, 230, 255});
}

// 一覧・入力欄が開いている間のキー操作。開いていれば true（ほかの操作は止める）
// ---------------------------------------------------------------- 天体の検索（/ キー・F4 キー）
// 名前の一部（日本語はひらがなでも、英語は大文字小文字を区別せず）で絞り込み、Enter でターゲット、Shift+Enter でワープ。
// タッチ操作では文字を打てないので、分類のタブと一覧から選ぶ
enum BodyClassId { BC_STAR, BC_PLANET, BC_DWARF, BC_MOON, BC_ASTEROID, BC_COMET, BC_PROBE, BC_SAT };

static int BodyClass(const App& app, int i) {
    const BodyDef& d = app.B[i].d;
    std::string en = d.en;
    if (i == B_SUN) return BC_STAR;
    if (d.kind == KIND_PROBE) return d.tleN > 0 ? BC_SAT : BC_PROBE;
    if (d.parent > 0) return BC_MOON;
    if (i <= B_NEPTUNE) return BC_PLANET;
    if (i == B_PLUTO || en == "Ceres" || en == "Eris") return BC_DWARF;
    if (std::string(d.jp).find("彗星") != std::string::npos) return BC_COMET;
    return BC_ASTEROID;
}

// 分類のタブ: 0 すべて, 1 惑星・準惑星（と太陽）, 2 衛星, 3 小惑星・彗星, 4 探査機・人工衛星, 5 地上の都市
static const int SEARCH_TABS = 6;

// 地上から空を見る場所（天体, 緯度・経度 deg, 標準時 UTC+時間（夏時間は考えない。NAN は UTC で表示））。
// 選ぶと地表に降り、赤道側（北半球なら南）の空を見上げる。月面では地球が出ていれば地球を見る
struct City { const char* jp; const char* en; double lat, lon; int body; double tz; };
static const City CITIES[] = {
    {"東京", "Tokyo", 35.68, 139.69, B_EARTH, 9},         {"札幌", "Sapporo", 43.06, 141.35, B_EARTH, 9},
    {"那覇", "Naha", 26.21, 127.68, B_EARTH, 9},          {"北京", "Beijing", 39.90, 116.41, B_EARTH, 8},
    {"シンガポール", "Singapore", 1.35, 103.82, B_EARTH, 8}, {"ムンバイ", "Mumbai", 19.08, 72.88, B_EARTH, 5.5},
    {"カイロ", "Cairo", 30.04, 31.24, B_EARTH, 2},        {"モスクワ", "Moscow", 55.76, 37.62, B_EARTH, 3},
    {"ロンドン", "London", 51.51, -0.13, B_EARTH, 0},      {"パリ", "Paris", 48.86, 2.35, B_EARTH, 1},
    {"レイキャビク", "Reykjavik", 64.15, -21.94, B_EARTH, 0}, {"ニューヨーク", "New York", 40.71, -74.01, B_EARTH, -5},
    {"ロサンゼルス", "Los Angeles", 34.05, -118.24, B_EARTH, -8},
    {"マウナケア山頂（ハワイ）", "Mauna Kea (Hawaii)", 19.82, -155.47, B_EARTH, -10},
    {"アタカマ砂漠（チリ）", "Atacama Desert (Chile)", -24.63, -70.40, B_EARTH, -4},
    {"リオデジャネイロ", "Rio de Janeiro", -22.91, -43.17, B_EARTH, -3}, {"ケープタウン", "Cape Town", -33.92, 18.42, B_EARTH, 2},
    {"シドニー", "Sydney", -33.87, 151.21, B_EARTH, 10},
    {"南極点", "South Pole", -89.99, 0.0, B_EARTH, NAN},  {"北極点", "North Pole", 89.99, 0.0, B_EARTH, NAN},
    // 月面（経度は東が正）
    {"静かの海（アポロ11号の着陸地点）", "Sea of Tranquility (Apollo 11)", 0.674, 23.473, B_MOON, NAN},
    {"タウルス・リットロウ谷（アポロ17号）", "Taurus-Littrow (Apollo 17)", 20.19, 30.77, B_MOON, NAN},
    {"月の南極", "Lunar south pole", -89.9, 0.0, B_MOON, NAN},
    {"フォン・カルマン・クレーター（嫦娥4号、月の裏側）", "Von Karman crater (Chang'e 4, far side)", -45.44, 177.6, B_MOON, NAN},
    // 火星（経度は東が正）
    {"ジェゼロ・クレーター（パーシビアランス）", "Jezero crater (Perseverance)", 18.44, 77.45, B_MARS, NAN},
    {"ゲール・クレーター（キュリオシティ）", "Gale crater (Curiosity)", -4.59, 137.44, B_MARS, NAN},
    {"クリュセ平原（バイキング1号）", "Chryse Planitia (Viking 1)", 22.27, -47.95, B_MARS, NAN},
};
static const int CITY_COUNT = sizeof CITIES / sizeof CITIES[0];
// 検索の候補の番号: 0 以上は天体、負は都市（-1 - 都市の番号）
static bool IsCityResult(int r) { return r < 0; }

static void GoToCity(App& app, int k, double elDeg = 35) {
    const City& c = CITIES[k];
    app.autopilot = false;
    app.warpT = -1;
    app.physics = false;
    int keep = app.target;
    PlaceAt(app, c.body, c.lat, c.lon, 0);
    app.target = keep == c.body ? B_SUN : keep;
    // 赤道側（北半球なら南、南半球なら北）の空を仰角 35° で見る
    const Body& e = app.B[c.body];
    V3 up = Norm(app.ship.local);
    V3 north = e.pole - up * Dot(e.pole, up);
    north = Len(north) < 1e-9 ? Norm(e.ex - up * Dot(e.ex, up)) : Norm(north);
    V3 h = c.lat >= 0 ? north * -1.0 : north;
    double el = elDeg * DEG;
    app.ship.q = QuatFromBasis(h * std::cos(el) + up * std::sin(el), up);
    if (c.body == B_MOON) {   // 月面: 地球が空にあれば地球を見る
        V3 toEarth = Norm(app.B[B_EARTH].pos - ShipWorld(app));
        if (Dot(toEarth, up) > std::sin(5 * DEG)) {
            app.target = B_EARTH;
            app.ship.q = QuatFromBasis(toEarth, up);
        }
    }
    app.cockpit = true;
    // 時刻はその都市の標準時で
    app.groundTzOk = !std::isnan(c.tz);
    app.groundTz = app.groundTzOk ? c.tz : 0;
    app.groundTzBody = c.body;
    {
        V3 X, Z;
        BodyFixedAxes(e, &X, &Z);
        V3 r = ShipWorld(app) - e.pos;
        app.groundTzPlace = V3(Dot(r, X), Dot(r, e.pole), Dot(r, Z));
        char tzs[24];
        if (c.tz == std::floor(c.tz)) snprintf(tzs, sizeof tzs, "UTC%+d", (int)c.tz);
        else snprintf(tzs, sizeof tzs, "UTC%+d:%02d", (int)c.tz, (int)std::lround(std::fabs(c.tz - (int)c.tz) * 60));
        app.groundTzName = tzs;
    }
    char b[160];
    snprintf(b, sizeof b, T(UI_CITY_AT), TR(c.jp, c.en), c.lat >= 0 ? "N" : "S", std::fabs(c.lat), c.lon >= 0 ? "E" : "W", std::fabs(c.lon));
    Toast(app, b);
}

// 今は飛んでいない探査機（運用終了・打ち上げ前）を選んだ時に移る日時: 到着 → 衝突・運用終了 → 最初のフライバイ → データの中ほど
static double ProbeVisitTime(const App& app, int i) {
    const BodyDef& d = app.B[i].d;
    for (int kind : {EV_ARRIVE, EV_IMPACT, EV_END, EV_FLYBY})
        for (const AstroEvent& ev : app.probeEvents)
            if (ev.probe == i && ev.kind == kind) return ev.t;
    if (d.tleN > 0) return d.launch + 30;
    if (d.probe >= 0 && d.probe < (int)g_probes.size() && !g_probes[d.probe].segs.empty())
        return (g_probes[d.probe].segs.front().t.front() + g_probes[d.probe].segs.back().t.back()) / 2;
    return app.tDays;
}
static bool InSearchTab(int cls, int tab) {
    switch (tab) {
        case 5: return false;
        case 1: return cls == BC_STAR || cls == BC_PLANET || cls == BC_DWARF;
        case 2: return cls == BC_MOON;
        case 3: return cls == BC_ASTEROID || cls == BC_COMET;
        case 4: return cls == BC_PROBE || cls == BC_SAT;
        default: return true;
    }
}

// ひらがなをカタカナに（天体の日本語名はカタカナが多い）
static std::string ToKatakana(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = s[i];
        if (c == 0xE3 && i + 2 < s.size()) {
            int cp = ((c & 0x0F) << 12) | ((s[i + 1] & 0x3F) << 6) | (s[i + 2] & 0x3F);
            if (cp >= 0x3041 && cp <= 0x3096) cp += 0x60;
            out += (char)(0xE0 | (cp >> 12));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
            i += 3;
        } else {
            out += (char)std::tolower(c);
            i++;
        }
    }
    return out;
}

// 候補（名前の先頭が合うもの → 途中が合うもの の順）
static std::vector<int> SearchResults(const App& app) {
    std::string q = ToKatakana(app.searchText);
    std::vector<int> head, mid;
    auto match = [&](const char* jpName, const char* enName, int code) {
        if (q.empty()) { head.push_back(code); return; }
        std::string jp = ToKatakana(jpName), en = ToKatakana(enName);
        size_t a = jp.find(q), b = en.find(q);
        if (a == 0 || b == 0) head.push_back(code);
        else if (a != std::string::npos || b != std::string::npos) mid.push_back(code);
    };
    for (size_t i = 0; i < app.B.size(); i++) {
        int cls = BodyClass(app, (int)i);
        // 今は飛んでいない探査機も出す（選ぶとその名場面の日時へ移る）。衛星などで非表示のものは出さない
        if (!app.B[i].active && cls != BC_PROBE && cls != BC_SAT) continue;
        if (InSearchTab(cls, app.searchTab)) match(app.B[i].d.jp, app.B[i].d.en, (int)i);
    }
    if (app.searchTab == 5 || (app.searchTab == 0 && !q.empty()))   // 都市は「地上」のタブか、名前で探した時
        for (int k = 0; k < CITY_COUNT; k++) match(CITIES[k].jp, CITIES[k].en, -1 - k);
    head.insert(head.end(), mid.begin(), mid.end());
    return head;
}

static void OpenSearch(App& app) {
    app.searchOpen = true;
    app.searchText.clear();
    app.searchSel = 0;
    while (GetCharPressed() > 0) {}   // 開いたキー（/）が入力欄に入らないように捨てる
}

// 選んだものへ: 都市はその地上へ、今は飛んでいない探査機はその名場面の日時へ移ってワープ、
// ほかはターゲットに（warp ならワープも）して閉じる
static void SearchPick(App& app, int code, bool warp, bool land) {
    app.searchOpen = false;
    if (IsCityResult(code)) {
        GoToCity(app, -1 - code);
        return;
    }
    app.target = code;
    if (!app.B[code].active) {
        app.tDays = ProbeVisitTime(app, code);
        UpdateBodies(app.B, app.tDays);
        UpdateReferenceFrame(app);
        if (!app.B[code].active) return;
        StartWarp(app, code, false);
        char b[160];
        snprintf(b, sizeof b, T(UI_SEARCH_JUMPED), FmtDate(app.tDays).substr(0, 10).c_str(), TR(app.B[code].d.jp, app.B[code].d.en));
        Toast(app, b);
        return;
    }
    if (warp && app.warpT < 0) StartWarp(app, code, land);
}
// その候補は 1 回で決まる（ターゲットにするだけ、という段階がない）か
static bool PickDirectly(const App& app, int code) { return IsCityResult(code) || !app.B[code].active; }

static const int SEARCH_ROWS = 12;
struct SearchLayout {
    float x, y, w, h, rowY;
};
static SearchLayout GetSearchLayout(const Cam& c) {
    SearchLayout L;
    L.w = std::min(c.W - 20, 640.0f);
    L.h = 150 + SEARCH_ROWS * 26.0f;
    L.x = (c.W - L.w) / 2;
    L.y = (c.H - L.h) / 2;
    L.rowY = L.y + 112;
    return L;
}
static int SearchFirstRow(const App& app, int n) { return std::max(0, std::min(app.searchSel - SEARCH_ROWS / 2, n - SEARCH_ROWS)); }

static void DrawSearch(const App& app) {
    const Cam& c = app.cam;
    SearchLayout L = GetSearchLayout(c);
    static const UiId TAB[SEARCH_TABS] = {UI_SEARCH_ALL, UI_SEARCH_PLANETS, UI_SEARCH_MOONS, UI_SEARCH_SMALL, UI_SEARCH_PROBES,
                                          UI_SEARCH_CITIES};
    static const UiId CLS[] = {UI_INFO_STAR, UI_INFO_PLANET, UI_INFO_DWARF, UI_SEARCH_MOON, UI_INFO_ASTEROID, UI_INFO_COMET,
                               UI_INFO_PROBE, UI_INFO_SAT};
    DrawRectangleRounded({L.x, L.y, L.w, L.h}, 0.04f, 6, {0, 8, 20, 235});
    DrawRectangleRoundedLinesEx({L.x, L.y, L.w, L.h}, 0.04f, 6, 1.5f, {90, 130, 180, 255});
    Txt(T(UI_SEARCH_TITLE), L.x + 20, L.y + 12, 20, {255, 220, 120, 255});
    // 入力欄（点滅するカーソル）
    std::string field = app.searchText + (std::fmod(GetTime(), 1.0) < 0.5 ? "_" : " ");
    DrawRectangleRounded({L.x + 20, L.y + 42, L.w - 40, 30}, 0.3f, 6, {20, 34, 60, 255});
    Txt(app.searchText.empty() && !app.touchUI ? T(UI_SEARCH_HINT) : field.c_str(), L.x + 30, L.y + 47, 18,
        app.searchText.empty() ? Color{120, 140, 170, 255} : Color{240, 245, 255, 255});
    // 分類のタブ
    float tw = (L.w - 40) / SEARCH_TABS;
    for (int k = 0; k < SEARCH_TABS; k++) {
        Rectangle r = {L.x + 20 + k * tw, L.y + 80, tw - 4, 24};
        DrawRectangleRounded(r, 0.3f, 6, k == app.searchTab ? Color{60, 90, 140, 230} : Color{18, 30, 52, 230});
        TxtC(T(TAB[k]), r.x + r.width / 2, r.y + 4, 14, k == app.searchTab ? Color{255, 240, 200, 255} : Color{170, 195, 225, 255});
    }
    std::vector<int> res = SearchResults(app);
    int n = (int)res.size(), first = SearchFirstRow(app, n);
    if (n == 0) TxtC(T(UI_SEARCH_NONE), c.W * 0.5f, L.rowY + 20, 16, {170, 190, 215, 255});
    for (int r = 0; r < SEARCH_ROWS && first + r < n; r++) {
        int i = res[first + r];
        float ry = L.rowY + r * 26.0f;
        bool sel = first + r == app.searchSel;
        if (sel) DrawRectangle((int)L.x + 12, (int)ry - 3, (int)L.w - 24, 25, {60, 90, 140, 200});
        char right[200];
        if (IsCityResult(i)) {   // 都市
            const City& cc = CITIES[-1 - i];
            Txt(TR(cc.jp, cc.en), L.x + 28, ry, 17, sel ? Color{255, 240, 200, 255} : Color{200, 235, 210, 255});
            snprintf(right, sizeof right, "%s   %s %.1f°  %s %.1f°", TR(app.B[cc.body].d.jp, app.B[cc.body].d.en), cc.lat >= 0 ? "N" : "S",
                     std::fabs(cc.lat), cc.lon >= 0 ? "E" : "W", std::fabs(cc.lon));
        } else {
            const BodyDef& d = app.B[i].d;
            bool active = app.B[i].active;
            Color col = i == app.target ? Color{255, 220, 80, 255} : sel ? Color{255, 240, 200, 255}
                      : active ? Color{215, 228, 245, 255} : Color{140, 150, 170, 255};
            Txt(TR(d.jp, d.en), L.x + 28, ry, 17, col);
            std::string kind = T(CLS[BodyClass(app, i)]);
            if (BodyClass(app, i) == BC_MOON) kind = TR(app.B[d.parent].d.jp, app.B[d.parent].d.en) + std::string(" ") + kind;
            if (active) snprintf(right, sizeof right, "%s   %s", kind.c_str(), FmtDist(std::max(0.0, SurfaceDist(app.B[i], ShipWorld(app)))).c_str());
            else snprintf(right, sizeof right, T(UI_SEARCH_GOTO), FmtDate(ProbeVisitTime(app, i)).substr(0, 10).c_str());
        }
        Txt(right, L.x + L.w - 28 - TxtW(right, 14), ry + 2, 14, {150, 175, 210, 255});
    }
    if (n > SEARCH_ROWS) {
        char b[32];
        snprintf(b, sizeof b, "%d / %d", app.searchSel + 1, n);
        Txt(b, L.x + L.w - 28 - TxtW(b, 13), L.y + 16, 13, {150, 175, 210, 255});
    }
    TxtC(T(app.touchUI ? UI_SEARCH_KEYS_TOUCH : app.pad >= 0 ? UI_SEARCH_KEYS_PAD : UI_SEARCH_KEYS), c.W * 0.5f, L.y + L.h - 26, 13,
         {150, 175, 210, 255});
}

// キーボード・マウス・タッチ（パッドは PadMenus）。開いている間は true
static bool HandleSearchInput(App& app) {
    if (!app.searchOpen) return false;
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    for (int ch = GetCharPressed(); ch > 0; ch = GetCharPressed()) {
        if (ch < 32 || app.searchText.size() > 60) continue;
        int len = 0;
        const char* u = CodepointToUTF8(ch, &len);
        app.searchText.append(u, len);
        app.searchSel = 0;
    }
    if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !app.searchText.empty()) {
        size_t k = app.searchText.size() - 1;   // UTF-8 の 1 文字分を消す
        while (k > 0 && ((unsigned char)app.searchText[k] & 0xC0) == 0x80) k--;
        app.searchText.erase(k);
        app.searchSel = 0;
    }
    std::vector<int> res = SearchResults(app);
    int n = (int)res.size();
    if (n > 0 && (IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN))) app.searchSel = (app.searchSel + 1) % n;
    if (n > 0 && (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP))) app.searchSel = (app.searchSel + n - 1) % n;
    if (IsKeyPressed(KEY_TAB)) {
        app.searchTab = (app.searchTab + (shift ? SEARCH_TABS - 1 : 1)) % SEARCH_TABS;
        app.searchSel = 0;
    }
    app.searchSel = std::max(0, std::min(app.searchSel, n - 1));
    if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) && n > 0) SearchPick(app, res[app.searchSel], shift, false);
    else if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_F4)) app.searchOpen = false;
    // マウス / タップ: タブを選ぶ、行をクリックで選び、選んだ行をもう一度でワープ。枠の外で閉じる
    Vector2 p;
    bool click = false;
    if (app.tapped) { p = app.tapPos; click = true; }
    else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !app.touchUI) {
        p = Vector2Scale(GetMousePosition(), 1 / UiScale(app.cam));
        click = true;
    }
    if (click && app.searchOpen) {
        SearchLayout L = GetSearchLayout(UiCam(app.cam));
        if (!CheckCollisionPointRec(p, {L.x, L.y, L.w, L.h})) {
            app.searchOpen = false;
        } else if (p.y >= L.y + 80 && p.y < L.y + 104) {
            app.searchTab = std::max(0, std::min(SEARCH_TABS - 1, (int)((p.x - L.x - 20) / ((L.w - 40) / SEARCH_TABS))));
            app.searchSel = 0;
        } else if (p.y >= L.rowY - 3) {
            int row = SearchFirstRow(app, n) + (int)std::floor((p.y - (L.rowY - 3)) / 26.0f);
            if (row >= 0 && row < n) {
                if (row == app.searchSel && (PickDirectly(app, res[row]) || app.target == res[row])) SearchPick(app, res[row], true, false);
                else {
                    app.searchSel = row;
                    if (!PickDirectly(app, res[row])) app.target = res[row];
                }
            }
        }
    }
    return true;
}

// ---------------------------------------------------------------- 全体図（F3 キー）
// 太陽（または惑星）を中心に、黄道の北から見下ろした図。距離は対数目盛り（中心付近を広げる）で、
// 内惑星から外縁天体までを 1 枚に収める。天体をクリック / タップするとターゲットにし、もう一度でワープする
struct MapItem {
    int body;
    Vector2 p;
};

static bool IsProbeLike(const Body& b) { return b.d.kind == KIND_PROBE; }

// 中心の天体の既定の半径 km: 太陽は 40 AU、惑星は一番外の衛星の 1.4 倍
static double MapDefaultRange(const App& app, int center) {
    if (center == 0) return 40 * AU_KM;
    double r = app.B[center].d.radius * 40;
    for (const Body& b : app.B)
        if (b.d.parent == center && !IsProbeLike(b)) r = std::max(r, b.d.moonA * 1.4);
    return r;
}

struct MapView {
    float cx, cy, R;   // 画面上の中心と半径
    double range;      // 半径に当たる距離 km
    V3 origin;         // 中心の天体の位置
    // 太陽中心の位置 → 画面。黄道面に投影して、距離を対数目盛りに
    Vector2 Project(V3 w, bool* inside = nullptr) const {
        V3 r = w - origin;
        double d = std::sqrt(r.x * r.x + r.z * r.z);
        double r0 = range / 40;
        double f = std::log1p(d / r0) / std::log1p(range / r0);
        if (inside) *inside = f <= 1.0;
        f = std::min(f, 1.0);
        double k = d > 0 ? f * R / d : 0;
        return {(float)(cx + r.x * k), (float)(cy + r.z * k)};   // 右が春分点、上が黄経 90°
    }
};

static MapView GetMapView(const App& app, const Cam& c) {
    MapView v;
    v.cx = c.W * 0.5f;
    v.cy = c.H * 0.5f + 8;
    v.R = std::min(c.W * 0.5f - 40, c.H * 0.5f - 64);
    v.range = app.mapRange > 0 ? app.mapRange : MapDefaultRange(app, app.mapCenter);
    v.origin = app.B[app.mapCenter].pos;
    return v;
}

// 図に載せる天体（クリックで選べるもの）と画面の位置。中心が太陽なら惑星・準惑星・彗星・探査機、惑星なら衛星と近くの探査機
static std::vector<MapItem> MapItems(const App& app, const MapView& v) {
    std::vector<MapItem> items;
    for (size_t i = 0; i < app.B.size(); i++) {
        const Body& b = app.B[i];
        if (!b.active) continue;
        bool in = (int)i == app.mapCenter || b.d.parent == app.mapCenter ||
                  (IsProbeLike(b) && Len(b.pos - v.origin) < v.range && (app.mapCenter != 0 || b.d.tleN <= 0));
        if (!in) continue;
        bool inside;
        Vector2 p = v.Project(b.pos, &inside);
        if (inside || (int)i == app.target) items.push_back({(int)i, p});
    }
    return items;
}

// 右上のボタン: 0 閉じる, 1 拡大, 2 縮小, 3 中心の切替, 4 時間を遅く, 5 速く, 6 停止, 7 逆再生（タッチ・マウス用）
static const int MAP_BUTTONS = 8;
static Rectangle MapButtonRect(const Cam& c, int i) {
    switch (i) {
        case 0: return {c.W - 110, 10, 96, 28};
        case 1: return {c.W - 110, 46, 44, 28};
        case 2: return {c.W - 58, 46, 44, 28};
        case 3: return {c.W - 110, 82, 96, 28};
        default: return {c.W - 110 + (i % 2) * 52, 130 + (i - 4) / 2 * 36.0f, 44, 28};
    }
}

static void MapTimeButton(App& app, int i) {
    if (i == 4) app.timeIdx = std::max(0, app.timeIdx - 1);
    if (i == 5) app.timeIdx = std::min(TIME_SCALE_COUNT - 1, app.timeIdx + 1);
    if (i == 6) app.paused = !app.paused;
    if (i == 7) app.timeReverse = !app.timeReverse;
}

static void MapZoom(App& app, double factor) {
    double range = app.mapRange > 0 ? app.mapRange : MapDefaultRange(app, app.mapCenter);
    app.mapRange = std::max(app.B[app.mapCenter].d.radius * 4, std::min(200 * AU_KM, range * factor));
}

// 中心: 太陽 ↔ ターゲット（衛星ならその惑星）のまわり
static void MapToggleCenter(App& app) {
    const Body& t = app.B[app.target];
    int planet = IsProbeLike(t) ? 0 : t.d.parent > 0 ? t.d.parent : app.target;
    app.mapCenter = app.mapCenter == 0 ? planet : 0;
    app.mapRange = 0;
}

static void OpenMap(App& app) {
    const Body& ref = app.B[app.ship.ref];
    // 中心: 惑星（衛星ならその惑星）の近くにいればその惑星、ほかは太陽
    app.mapCenter = IsProbeLike(ref) ? 0 : ref.d.parent > 0 ? ref.d.parent : app.ship.ref;
    if (app.mapCenter != 0 && Len(ShipWorld(app) - app.B[app.mapCenter].pos) > MapDefaultRange(app, app.mapCenter)) app.mapCenter = 0;
    app.mapRange = 0;
    app.mapWasCaptured = app.mouseCaptured;
    if (app.mouseCaptured) {
        app.mouseCaptured = false;
        EnableCursor();
    }
    app.mapOpen = true;
}

static void CloseMap(App& app) {
    app.mapOpen = false;
    app.timeReverse = false;
    if (app.mapWasCaptured && !app.touchUI) {
        app.mouseCaptured = true;
        DisableCursor();
    }
}

static void MapPick(App& app, Vector2 p, bool land) {
    const Cam c = UiCam(app.cam);
    for (int i = 0; i < MAP_BUTTONS; i++)
        if (CheckCollisionPointRec(p, MapButtonRect(c, i))) {
            if (i == 0) CloseMap(app);
            else if (i == 3) MapToggleCenter(app);
            else if (i >= 4) MapTimeButton(app, i);
            else MapZoom(app, i == 1 ? 1 / 1.6 : 1.6);
            return;
        }
    MapView v = GetMapView(app, c);
    int best = -1;
    float bd = 18;
    for (const MapItem& it : MapItems(app, v)) {
        float d = Vector2Distance(p, it.p);
        if (d < bd) { bd = d; best = it.body; }
    }
    if (best < 0) return;
    if (best == app.target && app.warpT < 0) {   // もう一度選ぶとワープ
        StartWarp(app, best, land);
        CloseMap(app);
    } else {
        app.target = best;
    }
}

static void DrawMap(const App& app) {
    const Cam& c = app.cam;
    MapView v = GetMapView(app, c);
    DrawRectangle(0, 0, (int)c.W, (int)c.H, {0, 4, 12, 252});
    // 距離の目安の円（1 / 10 / 100 AU、惑星中心なら 10 万 km ごとの桁）
    double unit = app.mapCenter == 0 ? AU_KM : 1e4;
    for (double r = unit * 0.1; r <= v.range * 1.001; r *= 10) {
        float rp = (float)(std::log1p(r / (v.range / 40)) / std::log1p(40.0) * v.R);
        if (rp < 6) continue;
        DrawCircleLinesV({v.cx, v.cy}, rp, {40, 60, 90, 120});
        char rl[32];
        if (app.mapCenter == 0) snprintf(rl, sizeof rl, "%g AU", r / AU_KM);
        else snprintf(rl, sizeof rl, "%s", FmtDist(r).c_str());
        Txt(rl, v.cx + rp * 0.707f + 4, v.cy - rp * 0.707f - 14, 12, {90, 120, 160, 200});
    }
    DrawCircleLinesV({v.cx, v.cy}, v.R, {70, 100, 140, 160});
    // 軌道
    for (size_t i = 0; i < app.B.size(); i++) {
        const Body& b = app.B[i];
        if (b.d.parent != app.mapCenter || IsProbeLike(b) || b.orbit.size() < 2 || !b.active) continue;
        Color col = BodyColor(b, 0.8f);
        col.a = (int)i == app.target ? 200 : 90;
        Vector2 prev = v.Project(v.origin + b.orbit[0]);   // 軌道点は中心の天体からの位置
        for (size_t k = 1; k < b.orbit.size(); k++) {
            Vector2 p = v.Project(v.origin + b.orbit[k]);
            DrawLineEx(prev, p, 1.0f, col);
            prev = p;
        }
    }
    // 探査機の軌跡（通ってきた部分は明るく、これから通る部分は暗く）。太陽中心の図は太陽中心の区間を、
    // 惑星中心の図はその惑星のまわりの区間の前後 60 日分を描く
    for (size_t i = 0; i < app.B.size(); i++) {
        const Body& b = app.B[i];
        if (b.d.probe < 0 || !b.active || b.d.probe >= (int)g_probes.size()) continue;
        bool tgt = (int)i == app.target;
        for (const ProbeSeg& sg : g_probes[b.d.probe].segs) {
            if (sg.body != app.mapCenter || sg.t.size() < 2) continue;
            double ta = sg.t.front(), tb = sg.t.back();
            if (sg.body > 0) {
                ta = std::max(ta, app.tDays - 60);
                tb = std::min(tb, app.tDays + 60);
            }
            if (ta >= tb) continue;
            const int N = sg.body > 0 ? 4000 : 800;   // 時間で等分して補間（惑星のまわりは細かく）
            bool have = false;
            Vector2 prev = {0, 0};
            for (int j = 0; j <= N; j++) {
                double tt = ta + (tb - ta) * j / N;
                bool in;
                Vector2 q = v.Project(v.origin + SegPos(sg, tt), &in);
                if (!in) { have = false; continue; }
                if (have) {
                    bool past = tt <= app.tDays;
                    unsigned char a = (unsigned char)(tgt ? (past ? 190 : 80) : (past ? 80 : 30));
                    DrawLineEx(prev, q, 1.0f, {255, 196, 120, a});
                }
                prev = q;
                have = true;
            }
        }
    }
    // 天体（名前は重ならないものだけ。ターゲットは必ず）
    std::vector<MapItem> items = MapItems(app, v);
    Vector2 shipP = v.Project(ShipWorld(app));
    std::vector<Rectangle> labels = {{shipP.x + 10, shipP.y + 4, TxtW(T(UI_MAP_SHIP), 13), 15}};
    for (int pass = 0; pass < 3; pass++)   // 名前はターゲット → 大きな天体 → ほかの順に置く
        for (const MapItem& it : items) {
            const Body& b = app.B[it.body];
            bool tgt = it.body == app.target, probe = IsProbeLike(b);
            int order = tgt ? 0 : (!probe && (b.d.radius > 2000 || it.body == app.mapCenter)) ? 1 : 2;
            if (order != pass) continue;
            Color col = probe ? Color{255, 196, 120, 255} : BodyColor(b, 1.0f);
            float r = it.body == app.mapCenter ? 7.0f : probe ? 2.5f : b.d.parent == app.mapCenter && b.d.radius > 2000 ? 4.5f : 3.0f;
            if (probe) DrawPoly(it.p, 4, r + 1, 45, col);
            else DrawCircleV(it.p, r, col);
            if (tgt) {
                DrawCircleLinesV(it.p, r + 6, {255, 220, 80, 255});
                DrawLineEx(shipP, it.p, 1.0f, {255, 220, 80, 140});
            }
            const char* name = TR(b.d.jp, b.d.en);
            float fs = tgt ? 15.0f : probe ? 11.0f : 13.0f;
            Rectangle lr = {it.p.x + r + 4, it.p.y - fs * 0.6f, TxtW(name, fs), fs + 2};
            bool clash = false;
            for (const Rectangle& o : labels)
                if (CheckCollisionRecs(lr, o)) clash = true;
            if (clash && !tgt) continue;
            labels.push_back(lr);
            Color lc = tgt ? Color{255, 220, 80, 255} : probe ? Color{220, 180, 130, 200} : Color{210, 225, 245, 230};
            Txt(name, lr.x, lr.y, fs, lc);
        }
    // 自機
    DrawPoly(shipP, 3, 7, -90, {120, 255, 160, 255});
    DrawPolyLines(shipP, 3, 10, -90, {20, 60, 40, 255});
    Txt(T(UI_MAP_SHIP), shipP.x + 10, shipP.y + 4, 13, {120, 255, 160, 255});
    // 見出しと操作
    char b[200];
    std::string title = std::string(T(UI_MAP_TITLE)) + "  /  " +
                        (app.mapCenter == 0 ? std::string(T(UI_MAP_SUN)) : std::string(TR(app.B[app.mapCenter].d.jp, app.B[app.mapCenter].d.en)));
    Txt(title.c_str(), 20, 14, 22, {255, 220, 120, 255});
    char rs[32];
    if (app.mapCenter == 0) snprintf(rs, sizeof rs, "%.3g AU", v.range / AU_KM);
    else snprintf(rs, sizeof rs, "%s", FmtDist(v.range).c_str());
    snprintf(b, sizeof b, T(UI_MAP_NOTE), rs);
    Txt(b, 20, 44, 14, {160, 190, 225, 255});
    std::string ts = app.paused ? T(UI_PAUSED) : TIME_SCALE_NAMES[app.timeIdx];
    if (app.timeReverse && !app.paused) ts = std::string(T(UI_MAP_REV)) + " " + ts;
    snprintf(b, sizeof b, "%s %s    %s %s", T(UI_DATE), FmtDate(app.tDays).c_str(), T(UI_TSCALE), ts.c_str());
    Txt(b, 20, 66, 14, app.timeReverse ? Color{255, 190, 140, 255} : Color{160, 190, 225, 255});
    const char* btn[MAP_BUTTONS] = {T(UI_MAP_CLOSE), "+", "-", T(UI_MAP_CENTER), "<<", ">>", app.paused ? ">" : "||", "<-"};
    for (int i = 0; i < MAP_BUTTONS; i++) {
        Rectangle r = MapButtonRect(c, i);
        bool on = (i == 6 && app.paused) || (i == 7 && app.timeReverse);
        DrawRectangleRounded(r, 0.3f, 6, on ? Color{70, 110, 170, 230} : Color{20, 40, 70, 220});
        DrawRectangleRoundedLinesEx(r, 0.3f, 6, 1.0f, {90, 130, 180, 200});
        float fs = i == 1 || i == 2 ? 20.0f : i >= 4 ? 16.0f : 15.0f;
        TxtC(btn[i], r.x + r.width / 2, r.y + (r.height - fs) / 2 - 1, fs, {225, 235, 250, 255});
    }
    TxtC(T(UI_MAP_KEYS), c.W * 0.5f, c.H - 26, 13, {150, 175, 210, 255});
    if (app.pad >= 0) TxtC(T(UI_MAP_PAD), c.W * 0.5f, c.H - 46, 13, {150, 210, 180, 255});
}

// 日時の入力欄の内容の日時へ移る（Enter / パッドの A）
static void ApplyDateEntry(App& app) {
    double t = ParseDateDigits(app.dateDigits);
    if (std::isnan(t)) {
        Toast(app, T(UI_DATE_BAD));
        return;
    }
    app.tDays = t;
    UpdateBodies(app.B, app.tDays);
    UpdateReferenceFrame(app);
    app.dateOpen = false;
    char b[96];
    snprintf(b, sizeof b, T(UI_DATE_SET), FmtDate(t).c_str());
    Toast(app, b);
}

// イベント一覧で選んでいるイベントへワープする（Enter / パッドの A）
static void GoSelectedEvent(App& app) {
    if (app.events.empty() || app.warpT >= 0) return;
    const AstroEvent& ev = app.events[app.eventSel];
    StartWarp(app, EventBody(ev), false);
    app.warpEvent = app.eventSel;
    app.eventsOpen = false;
    std::string when = FmtDate(ev.t).substr(0, 16) + " UTC";
    char b[256];
    snprintf(b, sizeof b, T(UI_EV_GO), when.c_str(), EventTitle(app, ev).c_str());
    Toast(app, b);
}

static bool HandleModalInput(App& app) {
    const Cam c = UiCam(app.cam);   // タップの位置は 2D の表示の座標
    if (app.introPage >= 0) {
        bool next = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_SPACE);
        if (app.tapped) {
            float w = std::min(c.W - 20, 900.0f), h = 250, x = (c.W - w) / 2, y = (c.H - h) / 2;
            if (CheckCollisionPointRec(app.tapPos, {x + w - 50, y, 50, 50})) { CloseIntro(app); return true; }
            next = true;
        }
        if (IsKeyPressed(KEY_LEFT) && app.introPage > 0) app.introPage--;
        if (IsKeyPressed(KEY_ESCAPE)) { CloseIntro(app); return true; }
        if (next) {
            if (++app.introPage >= INTRO_PAGES) CloseIntro(app);
        }
        return true;
    }
    if (HandleSearchInput(app)) return true;
    if (app.mapOpen) {
        bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        float k = UiScale(app.cam);
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_F3)) { CloseMap(app); return true; }
        float wheel = GetMouseWheelMove();
        if (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP) || wheel > 0) MapZoom(app, 1 / 1.6);
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN) || wheel < 0) MapZoom(app, 1.6);
        if (IsKeyPressed(KEY_SPACE)) MapToggleCenter(app);
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_LEFT_BRACKET)) MapTimeButton(app, 4);
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_RIGHT_BRACKET)) MapTimeButton(app, 5);
        if (IsKeyPressed(KEY_P)) MapTimeButton(app, 6);
        if (IsKeyPressed(KEY_R)) MapTimeButton(app, 7);
        if (IsKeyPressed(KEY_TAB)) {
            int n = (int)app.B.size(), step = shift ? -1 : 1;
            for (int i = 0; i < n; i++) {
                app.target = (app.target + step + n) % n;
                if (app.B[app.target].active) break;
            }
        }
        if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_G)) && app.warpT < 0) {
            StartWarp(app, app.target, shift);
            CloseMap(app);
            return true;
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !app.touchUI) MapPick(app, Vector2Scale(GetMousePosition(), 1 / k), shift);
        if (app.tapped) MapPick(app, app.tapPos, false);
        return true;
    }
    if (app.settingsOpen && app.tapped) {
        // タッチ: 項目をタップすると変わる。パネルの外をタップすると閉じる
        float w = std::min(c.W - 20, 760.0f), h = 120 + SET_COUNT * SET_ROW_H, x = (c.W - w) / 2, y = (c.H - h) / 2;
        if (!CheckCollisionPointRec(app.tapPos, {x, y, w, h})) {
            app.settingsOpen = false;
        } else {
            int row = (int)std::floor((app.tapPos.y - (y + 68)) / SET_ROW_H);
            if (row >= 0 && row < SET_COUNT) {
                app.settingsSel = row;
                ChangeSetting(app, row, 1, false);
            }
        }
        return true;
    }
    if (app.eventsOpen && app.tapped) {
        // タッチ: 行をタップで選び、選んでいる行をもう一度タップでワープ。見出しをタップでページ切替、外で閉じる
        int n = (int)app.events.size();
        int rows = std::max(1, std::min(n, 14));
        float w = std::min(c.W - 20, 900.0f), h = 96 + rows * 28.0f, x = (c.W - w) / 2, y = (c.H - h) / 2;
        if (!CheckCollisionPointRec(app.tapPos, {x, y, w, h})) {
            app.eventsOpen = false;
            return true;
        }
        if (app.tapPos.y < y + 64) {
            app.eventsPage = (app.eventsPage + 1) % 3;
            OpenEventsPage(app);
            return true;
        }
        int first = std::max(0, std::min(app.eventSel - rows / 2, n - rows));
        int row = first + (int)std::floor((app.tapPos.y - (y + 65)) / 28.0f);
        if (row >= 0 && row < n) {
            if (row == app.eventSel && app.warpT < 0) {
                const AstroEvent& ev = app.events[row];
                StartWarp(app, EventBody(ev), false);
                app.warpEvent = row;
                app.eventsOpen = false;
            } else {
                app.eventSel = row;
            }
        }
        return true;
    }
    if ((app.showHelp || app.dateOpen) && app.tapped) {
        app.showHelp = false;
        app.dateOpen = false;
        return true;
    }
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
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) ApplyDateEntry(app);
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
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) GoSelectedEvent(app);
        return true;
    }
    return false;
}

static void DrawModalPanels(const App& app) {
    const Cam& c = app.cam;
    if (app.mapOpen) DrawMap(app);
    if (app.searchOpen) DrawSearch(app);
    if (app.settingsOpen) {
        static const UiId NAME[SET_COUNT] = {UI_SET_LANG, UI_SET_TEXTSIZE, UI_SET_QUALITY, UI_SET_SOUND, UI_SET_MUSIC, UI_SET_SKY,
                                             UI_SET_MILKYWAY, UI_SET_ORBITS, UI_SET_ASTEROIDS, UI_SET_MARKERS, UI_SET_INVERT,
                                             UI_SET_PHYSICS, UI_SET_RESUME, UI_SET_FULLSCREEN};
        float w = std::min(c.W - 20, 760.0f), h = 120 + SET_COUNT * SET_ROW_H, x = (c.W - w) / 2, y = (c.H - h) / 2;
        DrawRectangleRounded({x, y, w, h}, 0.04f, 6, {0, 8, 20, 230});
        DrawRectangleRoundedLinesEx({x, y, w, h}, 0.04f, 6, 1.5f, {90, 130, 180, 255});
        TxtC(T(UI_SET_TITLE), c.W * 0.5f, y + 10, 22, {255, 220, 120, 255});
        TxtC(T(UI_SET_KEYS), c.W * 0.5f, y + 40, 14, {170, 200, 230, 255});
        for (int i = 0; i < SET_COUNT; i++) {
            float ry = y + 72 + i * SET_ROW_H;
            bool sel = i == app.settingsSel;
            if (sel) DrawRectangle((int)x + 12, (int)ry - 4, (int)w - 24, (int)SET_ROW_H - 1, {60, 90, 140, 200});
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
        float w = std::min(c.W - 20, 620.0f), h = app.pad >= 0 ? 176 : 150, x = (c.W - w) / 2, y = (c.H - h) / 2;
        DrawRectangleRounded({x, y, w, h}, 0.06f, 6, {0, 8, 20, 235});
        DrawRectangleRoundedLinesEx({x, y, w, h}, 0.06f, 6, 1.5f, {90, 130, 180, 255});
        TxtC(T(UI_DATE_TITLE), c.W * 0.5f, y + 12, 22, {255, 220, 120, 255});
        TxtC((FormatDateDigits(app.dateDigits) + " UTC").c_str(), c.W * 0.5f, y + 52, 32, {255, 255, 255, 255});
        TxtC(T(UI_DATE_KEYS), c.W * 0.5f, y + 108, 15, {170, 200, 230, 255});
        if (app.pad >= 0) TxtC(T(UI_PAD_DATE), c.W * 0.5f, y + 136, 14, {150, 210, 180, 255});
    }
    DrawIntro(app);
}

static void FaceTarget(App& app) {
    V3 d = app.B[app.target].pos - ShipWorld(app);
    app.ship.q = QuatFromBasis(d, ShipUp(app.ship));
}
