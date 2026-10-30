// Solar System Voyager — Copyright (c) 2026 hubstacksea
// MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
// 自機・設定ファイル・起動オプション。
// main.cpp の一部で、main.cpp が決まった順に読み込む（単体ではコンパイルしない）

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
// Linux は $XDG_CONFIG_HOME/solar_voyager/ (既定 ~/.config/solar_voyager/)。
// macOS はアプリの中に書かないよう、いつも ~/Library/Application Support/SolarSystemVoyager/
static std::string ConfigPath() {
    std::string local = std::string(GetApplicationDirectory()) + "solar_voyager.cfg";
#if defined(__APPLE__)
    const char* h = getenv("HOME");
    if (!h || !*h) return local;
    std::string dir = std::string(h) + "/Library/Application Support/SolarSystemVoyager";
    mkdir(dir.c_str(), 0755);
    return dir + "/solar_voyager.cfg";
#elif !defined(_WIN32)
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
#if defined(__EMSCRIPTEN__)
    char* text = WebLoadConfig();
    std::string t = text;
    free(text);
#else
    std::string path = ConfigPath();
    if (!FileExists(path.c_str())) return;
    char* text = LoadFileText(path.c_str());
    if (!text) return;
    std::string t = text;
    UnloadFileText(text);
#endif
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
// 設定ファイル（ブラウザ版は保存した設定）が読めたか
static bool SettingsLoaded() { return !g_cfg.empty(); }
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
#if defined(__EMSCRIPTEN__)
    WebSaveConfig(body.c_str());
#else
    SaveFileText(ConfigPath().c_str(), (char*)body.c_str());
#endif
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
    bool intro = false;         // 初回の案内を出して始める（確認用）
    bool touch = false;         // 画面のボタンを出して始める（確認用）
    bool showEvents = false, showDate = false;  // 一覧・日時入力を開いた状態で始める（確認用）
    int eventsPage = 0;                         // 一覧のページ 0: 天文, 1: 彗星, 2: 探査機（確認用）
    double timeScale = 1;
    const char* lang = nullptr;  // "ja" / "en"
    int quality = -1;            // 0〜2（確認用）
    int sky = -1;                // 星座の表示 0〜2（確認用）
    std::string share;           // 共有リンク（URL か ? 以降）の場面で始める。ブラウザ版はページの URL から
    float dpi = 0;               // 画面の表示倍率を指定（確認用）
    bool map = false;            // 全体図を開いた状態で始める（確認用）
    bool noMsaa = false;         // アンチエイリアス (MSAA) なしで始める（MSAA を使えない環境の確認用）
    bool checkAssets = false;    // データが読めるかを確かめて終了（ウィンドウを作れない CI の確認用）
    bool info = false;           // 天体の情報を開いて始める（確認用）
    bool resume = false;         // --shot でも前回の続きから始める（確認用）
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
        else if (a == "--intro") o.intro = true;
        else if (a == "--touch") o.touch = true;
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
        else if (a == "--share") o.share = next();
        else if (a == "--dpi") o.dpi = (float)atof(next());
        else if (a == "--map") o.map = true;
        else if (a == "--no-msaa") o.noMsaa = true;
        else if (a == "--check-assets") o.checkAssets = true;
        else if (a == "--info") o.info = true;
        else if (a == "--resume") o.resume = true;
        else if (a == "--hours") o.hours = atof(next());
        else if (a == "--at") { o.atLat = atof(next()); o.atLon = atof(next()); o.atAlt = atof(next()); }
    }
    return o;
}
