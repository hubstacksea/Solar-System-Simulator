// Solar System Voyager — Copyright (c) 2026 hubstacksea
// MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
// アプリ全体の状態 (App)。
// main.cpp の一部で、main.cpp が決まった順に読み込む（単体ではコンパイルしない）

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
    bool showMilkyWay = true;   // 天の川の光
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
    bool mapOpen = false;                 // 全体図（F3 キー）
    int mapCenter = 0;                    // 全体図の中心の天体（0: 太陽、ほかは惑星など）
    double mapRange = 0;                  // 全体図の半径 km（0: 中心に合わせて自動）
    bool mapWasCaptured = false;          // 全体図を開く前にマウスを捕捉していたか
    bool timeReverse = false;             // 時間を逆に進める（全体図を開いている間だけ）
    bool showInfo = false;                // ターゲットの天体の情報（I キー）
    bool searchOpen = false;              // 天体の検索（/ キー）
    std::string searchText;               // 入力中の文字（UTF-8）
    int searchSel = 0, searchTab = 0;     // 選んでいる候補・分類のタブ
    double groundTz = 0;                  // 地上の都市の標準時（UTC からの時間）
    bool groundTzOk = false;              // 都市を選んでその近くにいる間だけ標準時で表示する
    int groundTzBody = -1;
    std::string groundTzName;             // "UTC+9" など
    V3 groundTzPlace;                     // その都市（天体固定座標）
    int resumeMode = 1;                   // 起動時に前回の続きから 0: しない, 1: 場所だけ, 2: 場所と日時
    int eventsPage = 0;                   // 0: 天文イベント, 1: 彗星, 2: 探査機のできごと（Tab で切り替え）
    std::vector<AstroEvent> probeEvents;  // 探査機のできごと（起動時に軌跡から求める）
    std::vector<AstroEvent> cometEvents;  // 彗星の近日点と地球への最接近（起動時に求める）
    std::vector<AstroEvent> events;
    int eventSel = 0;
    int warpEvent = -1;                   // イベントへのワープ中はその番号
    int photoMode = 0;                    // 写真モード（Z キー）0: 通常, 1: 表示を隠す, 2: 表示と自機を隠す
    bool touchUI = false;                 // 指で操作する（画面のボタンとドラッグ。タッチを検出すると有効になる）
    Vector2 touchDrag = {0, 0};           // このフレームのドラッグ量（機首の向き）
    int touchHeld = -1;                   // 押し続けているボタン
    bool touchWasDown = false, touchMoved = false;
    Vector2 touchStart = {0, 0}, touchPrev = {0, 0};
    bool tapped = false;                  // このフレームで指を離した（動かさずに）
    Vector2 tapPos = {0, 0};
    int introPage = -1;                   // 初回の案内のページ（-1: 表示しない）
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
    Mesh sphere{};
    std::vector<Mesh> ringMesh;           // 環（RingSets() と同じ並び）
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
    std::vector<Texture2D> ringTex;       // 環の半径方向の不透明度（RingSets() と同じ並び）
    int locRingInfo = -1;
    Shader beltShader{};                  // 小惑星などの粒を GPU で計算して描く
    unsigned int beltVao = 0, beltVbo = 0;
    int beltCount = 0;
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
    // 地表近く（高度 100 km 以下）にいる時は、上空を通る人工衛星や探査機に乗り移らない（地上で時間を進めた時など）
    const Body& cur = app.B[app.ship.ref];
    bool onGround = cur.d.kind != KIND_PROBE && SurfaceDist(cur, w) < 100;
    for (size_t i = 1; i < app.B.size(); i++) {
        const Body& b = app.B[i];
        if (!b.active || (onGround && b.d.kind == KIND_PROBE)) continue;
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
