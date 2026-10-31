// Solar System Voyager — Copyright (c) 2026 hubstacksea
// MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
// ワープ演出・入力（キーボード・マウス・タッチ・ゲームパッド）・自動操縦・重力モード。
// main.cpp の一部で、main.cpp が決まった順に読み込む（単体ではコンパイルしない）

// ---------------------------------------------------------------- ワープ演出
// 加速（機首を目標へ向けながら光の筋が伸びる）→ 閃光の頂点で移動 → 到着（筋が縮んで消える）。
// 恒星間ワープ（STAR_WARP_KM より遠くへ）は長く大きく: 星空が前方へ集まって青く（後ろは赤く）なり、渦を巻く光のトンネルを抜ける
static const double WARP_CHARGE = 0.9, WARP_ARRIVE = 0.8;            // 実時間 秒
static const double STAR_WARP_CHARGE = 2.6, STAR_WARP_ARRIVE = 2.0;
static double WarpCharge(const App& app) { return app.warpBig ? STAR_WARP_CHARGE : WARP_CHARGE; }
static double WarpArrive(const App& app) { return app.warpBig ? STAR_WARP_ARRIVE : WARP_ARRIVE; }

static void StartWarp(App& app, int ti, bool land) {
    if (app.warpT >= 0) return;
    app.warpDist = Len(app.B[ti].pos - ShipWorld(app));
    app.warpBig = app.warpDist > STAR_WARP_KM;
    sfx::Warp(app.warpBig);
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
    double ch = WarpCharge(app);
    if (app.warpT < ch) { double k = app.warpT / ch; return k * k; }
    double k = 1 - std::min(1.0, (app.warpT - ch) / WarpArrive(app));
    return k * k * k;
}

// 移動の瞬間を頂点とする閃光 0..1（恒星間ワープは長く尾を引く）
static double WarpFlash(const App& app) {
    if (app.warpT < 0) return 0;
    double d = app.warpT - WarpCharge(app);
    return d < 0 ? std::exp(d * (app.warpBig ? 6.0 : 14.0)) : std::exp(-d * (app.warpBig ? 2.5 : 6.0));
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
        if (app.warpT >= WarpCharge(app)) {
            std::vector<Bookmark> bms = app.warpBookmark >= 0 ? LoadBookmarks(app) : std::vector<Bookmark>();
            if (app.warpEvent >= 0 && app.warpEvent < (int)app.events.size()) ApplyEvent(app, app.events[app.warpEvent]);
            else if (app.warpBookmark >= 0 && app.warpBookmark < (int)bms.size()) ApplyBookmark(app, bms[app.warpBookmark]);
            else WarpTo(app, app.warpTarget, app.warpLand);
            app.warpBookmark = -1;
            app.warpEvent = -1;
            app.warpJumped = true;
            if (app.physics) app.ship.vel = CircularVelocity(app);   // 重力モード: 着いた天体のまわりの円軌道に乗せる
        }
    } else if (app.warpT >= WarpCharge(app) + WarpArrive(app)) {
        app.warpT = -1;
        app.warpBig = false;
    }
}

// ---------------------------------------------------------------- ゲームパッド
struct PadState {
    double pitch = 0, yaw = 0, roll = 0;  // -1..1
    double throttle = 0, brake = 0;       // 0..1
    bool engineUp = false, engineDown = false, timeUp = false, timeDown = false;
    bool autopilot = false, stop = false, nextTarget = false, warp = false, land = false, view = false, help = false, zoom = false;
    bool lb = false, rb = false;   // LB / RB を押した瞬間（メニューの操作用。押している間のロールは roll に入る）
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
    p.lb = IsGamepadButtonPressed(g, GAMEPAD_BUTTON_LEFT_TRIGGER_1);
    p.rb = IsGamepadButtonPressed(g, GAMEPAD_BUTTON_RIGHT_TRIGGER_1);
    return p;
}

static bool InfoCardHit(const App& app, Vector2 p);

// タッチのボタンをパッドの入力に重ねる（押し続ける加速・減速と、タップで動くボタン）
static void TouchToPad(App& app, PadState& pad) {
    if (!app.touchUI) return;
    if (app.touchHeld == TB_THRUST) pad.throttle = 1;
    if (app.touchHeld == TB_BRAKE) pad.brake = 1;
    if (!app.tapped) return;
    const Cam c = UiCam(app.cam);   // タップの位置は 2D の表示の座標
    if (app.showInfo && InfoCardHit(app, app.tapPos)) {   // 天体の情報はタップで閉じる
        app.showInfo = false;
        return;
    }
    int b = TouchButtonAt(c, app.tapPos);
    switch (b) {
        case TB_STOP: pad.stop = true; break;
        case TB_TARGET: pad.nextTarget = true; break;
        case TB_WARP: pad.warp = true; break;
        case TB_AUTO: pad.autopilot = true; break;
        case TB_TOUR: if (app.tourOn) StopTour(app); else StartTour(app); break;
        case TB_EVENTS: OpenEventsPage(app); app.eventsOpen = true; break;
        case TB_MAP: OpenMap(app); break;
        case TB_INFO: app.showInfo = !app.showInfo; break;
        case TB_SEARCH: OpenSearch(app); break;
        case TB_SETTINGS: app.settingsOpen = true; app.settingsSel = 0; break;
        case TB_HELP: pad.help = true; break;
        case TB_SHARE: ShareScene(app); break;
        default:
            for (int i = 0; i < ENGINE_COUNT; i++)
                if (CheckCollisionPointRec(app.tapPos, EngineButtonRect(c, i))) app.ship.engine = i;
            break;
    }
}

// 自機の加速・移動に使う経過時間（時間倍率を反映、停止中は 0）
static double SimDt(const App& app, double dt) { return app.paused ? 0.0 : dt * TIME_SCALES[app.timeIdx]; }

// ゲームパッドで案内・設定・イベント一覧・日時指定・操作説明を操作する。ボタンを使ったら true
static bool PadMenus(App& app, const PadState& pad) {
    bool any = pad.autopilot || pad.stop || pad.nextTarget || pad.warp || pad.engineUp || pad.engineDown || pad.timeUp ||
               pad.timeDown || pad.lb || pad.rb;
    if (!any) return false;
    if (app.searchOpen) {
        std::vector<int> res = SearchResults(app);
        int n = (int)res.size();
        if (n > 0 && pad.timeDown) app.searchSel = (app.searchSel + 1) % n;
        if (n > 0 && pad.timeUp) app.searchSel = (app.searchSel + n - 1) % n;
        if (pad.engineUp || pad.engineDown) {
            app.searchTab = (app.searchTab + (pad.engineUp ? 1 : SEARCH_TABS - 1)) % SEARCH_TABS;
            app.searchSel = 0;
        }
        if (n > 0 && (pad.autopilot || pad.warp)) SearchPick(app, res[std::min(app.searchSel, n - 1)], pad.warp, false);
        if (pad.stop) app.searchOpen = false;
        return true;
    }
    if (app.introPage >= 0) {
        if (pad.stop) CloseIntro(app);
        else if (pad.engineDown) app.introPage = std::max(0, app.introPage - 1);
        else if ((pad.autopilot || pad.engineUp) && ++app.introPage >= INTRO_PAGES) CloseIntro(app);
        return true;
    }
    if (app.settingsOpen) {
        if (pad.timeDown) app.settingsSel = (app.settingsSel + 1) % SET_COUNT;
        if (pad.timeUp) app.settingsSel = (app.settingsSel + SET_COUNT - 1) % SET_COUNT;
        if (pad.engineUp || pad.autopilot) ChangeSetting(app, app.settingsSel, 1, false);
        if (pad.engineDown) ChangeSetting(app, app.settingsSel, -1, false);
        if (pad.stop) app.settingsOpen = false;
        return true;
    }
    if (app.eventsOpen) {
        int n = (int)app.events.size();
        if (n > 0 && pad.timeDown) app.eventSel = (app.eventSel + 1) % n;
        if (n > 0 && pad.timeUp) app.eventSel = (app.eventSel + n - 1) % n;
        if (pad.nextTarget || pad.rb) {
            app.eventsPage = (app.eventsPage + 1) % 3;
            OpenEventsPage(app);
        }
        if (pad.autopilot) GoSelectedEvent(app);
        if (pad.stop) app.eventsOpen = false;
        return true;
    }
    if (app.dateOpen) {
        double t = ParseDateDigits(app.dateDigits);
        if (std::isnan(t)) t = app.tDays;
        double d = (pad.timeUp ? 1 : 0) - (pad.timeDown ? 1 : 0) + (pad.engineUp ? 30 : 0) - (pad.engineDown ? 30 : 0) +
                   (pad.rb ? 365.25 : 0) - (pad.lb ? 365.25 : 0);
        if (d != 0) app.dateDigits = DigitsOfTime(t + d);
        if (pad.autopilot) ApplyDateEntry(app);
        if (pad.stop) app.dateOpen = false;
        return true;
    }
    if (app.showHelp) {   // 操作説明をメニューとして使う
        app.showHelp = false;
        if (pad.autopilot) OpenMap(app);
        else if (pad.nextTarget) { app.settingsOpen = true; app.settingsSel = 0; }
        else if (pad.warp) { OpenEventsPage(app); app.eventsOpen = true; }
        else if (pad.lb) { app.dateDigits = DigitsOfTime(app.tDays); app.dateOpen = true; }
        else if (pad.rb) app.showInfo = !app.showInfo;
        else if (pad.timeUp) OpenSearch(app);
        else if (!pad.stop) app.showHelp = true;   // ほかのボタンは説明を開いたまま
        return true;
    }
    return false;
}

static void HandleInput(App& app, double dt) {
    Ship& s = app.ship;
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    UpdateTouch(app);
    PadState pad = ReadPad(app);
    app.padZoom = pad.zoom;
    // ゲームパッド: LB+RB の同時押しで全体図。開いている間は十字キーなどで操作する
    if (app.pad >= 0) {
        int g = app.pad;
        bool lb = IsGamepadButtonDown(g, GAMEPAD_BUTTON_LEFT_TRIGGER_1), rb = IsGamepadButtonDown(g, GAMEPAD_BUTTON_RIGHT_TRIGGER_1);
        bool combo = (lb && IsGamepadButtonPressed(g, GAMEPAD_BUTTON_RIGHT_TRIGGER_1)) || (rb && IsGamepadButtonPressed(g, GAMEPAD_BUTTON_LEFT_TRIGGER_1));
        if (combo && !app.mapOpen && !app.settingsOpen && !app.eventsOpen && !app.dateOpen && app.introPage < 0) {
            OpenMap(app);
            return;
        }
        if (app.mapOpen) {
            if (combo || pad.stop) { CloseMap(app); return; }
            if (pad.engineUp || pad.engineDown) {
                int n = (int)app.B.size(), step = pad.engineUp ? 1 : -1;
                for (int i = 0; i < n; i++) {
                    app.target = (app.target + step + n) % n;
                    if (app.B[app.target].active) break;
                }
            }
            if (pad.timeUp) MapZoom(app, 1 / 1.6);
            if (pad.timeDown) MapZoom(app, 1.6);
            if (pad.nextTarget) MapToggleCenter(app);
            if ((pad.autopilot || pad.warp || pad.land) && app.warpT < 0) {
                StartWarp(app, app.target, pad.land);
                CloseMap(app);
                return;
            }
        } else if (PadMenus(app, pad)) {
            return;
        }
    }
    if (HandleModalInput(app)) return;
    TouchToPad(app, pad);
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
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !app.mouseCaptured && !app.touchUI) {
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
    if (app.touchUI) {   // タッチ: ドラッグした向きへ機首を向ける
        const double sens = 0.004 * zoomK;
        yaw -= app.touchDrag.x * sens;
        pitch -= app.touchDrag.y * sens * (app.invertY ? -1 : 1);
        if (Vector2Length(app.touchDrag) > 4) manual = true;
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
    if (IsKeyPressed(KEY_F) && !warping) {
        if (shift) ChangeSetting(app, SET_FRAME, 1, true);
        else FaceTarget(app);
    }
    if ((IsKeyPressed(KEY_T) || pad.autopilot) && !warping) {
        if (app.autopilot) {
            app.autopilot = false;
            Toast(app, T(UI_AP_OFF));
        } else {
            SetFrameLock(app, false, false);   // 自動操縦は動いている天体に向かうので、天体と一緒に動く基準系で
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
    if (IsKeyPressed(KEY_F2)) ShareScene(app);
    if (IsKeyPressed(KEY_F3)) OpenMap(app);
    if (IsKeyPressed(KEY_F4) || IsKeyPressed(KEY_SLASH)) OpenSearch(app);
    if (IsKeyPressed(KEY_F9)) ChangeSetting(app, SET_QUALITY, 1, true);
    if (IsKeyPressed(KEY_I)) app.showInfo = !app.showInfo;   // マウス上下反転は設定画面（F10）で
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

// 基準系を自機に固定する / 自動に戻す。固定中は太陽を基準天体にして乗り移らない（太陽は原点に止まっているので、
// アーケード式で止まっていれば太陽に対してその場にとどまり、惑星や衛星の方が動いていく）
static void SetFrameLock(App& app, bool on, bool toast) {
    if (on == app.frameLock) return;
    Ship& s = app.ship;
    int old = s.ref;
    V3 w = ShipWorld(app);
    app.frameLock = on;
    if (on) {
        app.autopilot = false;
        s.ref = B_SUN;
        SetShipWorld(app, w);
    } else {
        UpdateReferenceFrame(app);
    }
    if (app.physics && s.ref != old) s.vel += BodyVelocity(app, old) - BodyVelocity(app, s.ref);   // 重力モードでは実際の動きを変えない
    if (toast) Toast(app, T(on ? UI_FRAME_LOCK_ON : UI_FRAME_LOCK_OFF));
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

// 地表近く（高度 100 km 以下）で止まっている時は、天体の自転と一緒に回る（地上から空を見る・着陸した時に地面が流れない）。
// spin0 は時刻を進める前の自転角
static void CoRotateWithGround(App& app, int ref, double spin0) {
    Ship& s = app.ship;
    if (app.physics || app.autopilot || app.warpT >= 0 || s.speed != 0 || s.thrusting || s.ref != ref) return;
    const Body& b = app.B[ref];
    if (b.d.kind == KIND_PROBE || SurfaceDist(b, ShipWorld(app)) > 100) return;
    double a = b.spin - spin0;
    if (a == 0) return;
    double x = Dot(s.local, b.ex), y = Dot(s.local, b.pole), z = Dot(s.local, b.ez), c = std::cos(a), sn = std::sin(a);
    s.local = b.ex * (x * c + z * sn) + b.pole * y + b.ez * (-x * sn + z * c);   // 自転軸のまわりに a だけ回す
    s.q = QuaternionNormalize(QuaternionMultiply(QuaternionFromAxisAngle(ToF(b.pole), (float)a), s.q));
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
    c.fovy = std::min(150 * DEG, c.fovBase * (1.0 + (app.warpBig ? 1.1 : 0.75) * wi));  // ワープ中は視野が広がる
    c.focal = c.H * 0.5 / std::tan(c.fovy * 0.5);
    c.fwd = ShipFwd(s);
    c.up = ShipUp(s);
    c.right = ShipRight(s);
    V3 sw = ShipWorld(app);
    // ワープ中は自機が前へ飛び出し、カメラが置いていかれる。閃光の前後は小刻みに揺れる
    double lag = 0.055 + (app.warpJumped ? 0.12 : 0.2) * wi;
    V3 off = app.cockpit ? c.up * 0.0025 : c.up * 0.013 - c.fwd * lag;   // 自機からカメラへ
    double shake = WarpFlash(app) * (app.warpBig ? 0.0022 : 0.0012) + (app.warpBig && !app.warpJumped ? wi * 0.0006 : 0.0);
    if (shake > 0) {
        double t = GetTime() * 61.0;
        off = off + c.right * (std::sin(t) * shake) + c.up * (std::sin(t * 1.37 + 1.0) * shake);
    }
    if (!std::isnan(app.camYaw)) {
        // 確認用: 自機の周囲を回る視点（yaw 0 = 真後ろ）
        double y = app.camYaw * DEG, p = app.camPitch * DEG;
        V3 f0 = ShipFwd(s), u0 = ShipUp(s), r0 = ShipRight(s);
        V3 dir = (f0 * -std::cos(y) + r0 * std::sin(y)) * std::cos(p) + u0 * std::sin(p);
        off = dir * (app.camDist / 1000.0);
        c.fwd = Norm(-dir);
        c.right = Norm(Cross(c.fwd, u0));
        c.up = Cross(c.right, c.fwd);
    }
    c.origin = app.B[s.ref].pos;
    c.local = s.local + off;
    for (const Body& b : app.B) {
        V3 bl = b.pos - c.origin;   // 基準天体なら 0（丸めなし）
        V3 rel = c.local - bl;
        double d = Len(rel);
        if (d > b.d.radius * 1.2 + 1) continue;
        V3 dir = rel * (1.0 / d);
        double rs = SurfaceRadius(b, dir) + MinAltitude(b) * 0.5;
        if (d < rs) {   // 地面にめり込まないよう押し上げる
            c.local = bl + dir * rs;
            off = c.local - s.local;
        }
    }
    c.pos = c.origin + c.local;
    c.shipRel = off * -1.0;
    g_view = MatrixLookAt({0, 0, 0}, ToF(c.fwd), ToF(c.up));
}
