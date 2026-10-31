// Solar System Voyager — Copyright (c) 2026 hubstacksea
// MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
// 2D の表示（マーカー・計器・パネル・天体の情報）。
// main.cpp の一部で、main.cpp が決まった順に読み込む（単体ではコンパイルしない）

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
        DrawStarDot(p, st.size, st.col);
    }
}

// 小惑星帯などの粒: 1 ピクセルの点。種類ごとに色を変える（地表近くでは軌道線と同じく描かない）
// GPU で描く（シェーダが使える時）。軌道要素は初めて描く時に 1 回だけ送る（ブラウザ版は届いた後）
static bool DrawAsteroidsGPU(App& app) {
    Shader& sh = app.beltShader;
    if (!sh.id || sh.id == rlGetShaderIdDefault()) return false;
    const AsteroidBelt& ab = app.belt;
    if (!app.beltVao) {
        int lo = GetShaderLocationAttrib(sh, "aOrbit"), lp = GetShaderLocationAttrib(sh, "aP"), lq = GetShaderLocationAttrib(sh, "aQ");
        if (lo < 0 || lp < 0 || lq < 0) return false;
        std::vector<float> v;
        v.reserve(ab.a.size() * 11);
        for (size_t k = 0; k < ab.a.size(); k++) {
            float add[11] = {(float)(ab.a[k] / AU_KM), ab.e[k], ab.n[k], ab.m0[k], (float)ab.P[k].x, (float)ab.P[k].y, (float)ab.P[k].z,
                             (float)ab.kind[k], (float)ab.Q[k].x, (float)ab.Q[k].y, (float)ab.Q[k].z};
            v.insert(v.end(), add, add + 11);
        }
        app.beltVao = rlLoadVertexArray();
        rlEnableVertexArray(app.beltVao);
        app.beltVbo = rlLoadVertexBuffer(v.data(), (int)(v.size() * sizeof(float)), false);
        const int stride = 11 * sizeof(float);
        rlSetVertexAttribute(lo, 4, RL_FLOAT, false, stride, 0);
        rlEnableVertexAttribute(lo);
        rlSetVertexAttribute(lp, 4, RL_FLOAT, false, stride, 4 * sizeof(float));
        rlEnableVertexAttribute(lp);
        rlSetVertexAttribute(lq, 3, RL_FLOAT, false, stride, 8 * sizeof(float));
        rlEnableVertexAttribute(lq);
        rlDisableVertexArray();
        app.beltCount = (int)ab.a.size();
    }
    const Cam& c = app.cam;
    rlDrawRenderBatchActive();
    float t = (float)app.tDays;
    Vector3 cam = ToF(c.pos * (1.0 / AU_KM)), r = ToF(c.right), u = ToF(c.up), f = ToF(c.fwd);
    Vector3 view = {c.W, c.H, (float)c.focal};
    SetShaderValue(sh, GetShaderLocation(sh, "t"), &t, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, GetShaderLocation(sh, "camPos"), &cam, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, GetShaderLocation(sh, "camRight"), &r, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, GetShaderLocation(sh, "camUp"), &u, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, GetShaderLocation(sh, "camFwd"), &f, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, GetShaderLocation(sh, "view"), &view, SHADER_UNIFORM_VEC3);
    rlEnableShader(sh.id);
    rlEnableVertexArray(app.beltVao);
    glDrawArrays(GL_POINTS_MODE, 0, app.beltCount);
    rlDisableVertexArray();
    rlDisableShader();
    return true;
}

static void DrawAsteroids(App& app) {
    if (!app.showOrbits || !app.showAsteroids || app.belt.a.empty()) return;
    const Cam& c = app.cam;
    for (const Body& b : app.B)
        if (Len(c.pos - b.pos) < b.d.radius * 1.3) return;
    if (DrawAsteroidsGPU(app)) return;
    // シェーダが使えない時は CPU で計算して描く
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
    double day = SkyDaylight(app);
    for (size_t i = 0; i < app.B.size(); i++) {
        const Body& b = app.B[i];
        V3 rel = CamRel(c, b.pos);
        Vector2 p;
        if (!Project(c, rel, &p)) continue;
        double dist = Len(rel), px = b.d.radius / dist * c.focal;
        if (b.d.kind == KIND_STAR) {
            double cover = 0;
            if (i == B_SUN) {
                // 日食: 欠けた分だけまぶしさを弱め、皆既の直前からコロナを描く（月の円盤は後で 3D に重なって中央を隠す）
                cover = SunCover(app, c.pos);
                double k = std::max(0.0, std::min(1.0, (cover - 0.985) / 0.015));
                if (k > 0) DrawCorona(p, (float)px, (float)k);
            }
            if (Occluded(app, c.pos, b.pos, (int)i)) continue;
            // まぶしさ: −3 等より明るい時（惑星から見た太陽、恒星のすぐそば）
            float mag = (float)StarApparentMag(b.d, dist);
            double glare = std::max(0.0, std::min(1.0, (-3.0 - mag) / 10.0));
            if (glare > 0) {
                // 光の色: 太陽の色との比で染める（太陽は今まで通り）
                const Col3 &tc = b.d.c1, &sc = app.B[B_SUN].d.c1;
                float r = (float)(px * 1.8 + 14), g = (float)(std::pow(1.0 - cover, 0.6) * glare);
                auto tint = [](float base, float k) { return (unsigned char)std::min(255.0f, base * k); };
                BeginBlendMode(BLEND_ADDITIVE);
                DrawCircleGradient((int)p.x, (int)p.y, r, {tint(255, tc.r / sc.r), tint(240, tc.g / sc.g), tint(200, tc.b / sc.b), (unsigned char)(200 * g)},
                                   {tint(255, tc.r / sc.r), tint(200, tc.g / sc.g), tint(120, tc.b / sc.b), 0});
                DrawCircleGradient((int)p.x, (int)p.y, r * 3, {tint(255, tc.r / sc.r), tint(200, tc.g / sc.g), tint(140, tc.b / sc.b), (unsigned char)(35 * g)},
                                   {tint(255, tc.r / sc.r), tint(160, tc.g / sc.g), tint(80, tc.b / sc.b), 0});
                EndBlendMode();
            }
            if (px >= 0.8) continue;
            if (glare > 0) {
                DrawCircleV(p, 2.5f, {255, 250, 230, 255});
            } else if (mag < 8.5f) {
                // 星空の星と同じ描き方（太陽から見た明るさなら、星表の星と同じ見た目）。昼の空では見えない
                double bv = b.d.star >= 0 ? NEARBY_STARS[b.d.star].bv : 0.65;
                float a = StarDotAlpha(mag) * (float)(i == B_SUN ? 1.0 : 1.0 - day);
                if (a >= 4) DrawStarDot(p, StarDotSize(mag), StarColor(bv, (unsigned char)a));
            }
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
    V3 rel = CamRel(c, b.pos);
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
        V3 rel = c.shipRel;
        if (Project(c, rel, &sp)) {
            float r = (float)(0.017 / Len(rel) * c.focal);
            g_placed.push_back({sp.x - r, sp.y - r * 0.7f, 2 * r, r * 1.5f});
        }
    }
    if (app.B[app.target].active) DrawMarker(app, app.target);  // ターゲットを優先して配置
    bool farAway = Len(c.pos) > 0.05 * LY_KM;   // 太陽系から遠く離れていると、太陽系の天体は太陽しか出さない（みな同じ所に重なる）
    auto solar = [&](int i) { return HostStar(app.B, i) == B_SUN; };
    for (size_t i = 0; i < app.B.size(); i++) {
        const BodyDef& d = app.B[i].d;
        if ((int)i == app.target || !app.B[i].active) continue;
        if (farAway && d.kind != KIND_STAR && solar((int)i)) continue;
        if (d.probe >= 0) {
            // 探査機: 衛星マーカーと一緒に切り替え。近く（1,000 万 km 以内）か画面内に見える時だけ
            if (!app.showMoonMarkers) continue;
            Vector2 p;
            bool on = Project(c, app.B[i].pos - c.pos, &p) && p.x >= 0 && p.y >= 0 && p.x <= c.W && p.y <= c.H;
            if (!on && Len(app.B[i].pos - c.pos) > 1e7) continue;
        } else if (d.star >= 0) {
            // 太陽系の外の恒星: 星の名前を出している時（C キー）か、その星から 0.2 光年以内、同じ系の中にいる時だけ
            const Body& rb = app.B[app.ship.ref];
            bool sameSys = rb.d.star >= 0 && NEARBY_STARS[rb.d.star].system == NEARBY_STARS[d.star].system;
            if (app.skyMode != 2 && !sameSys && Len(CamRel(c, app.B[i].pos)) > 0.2 * LY_KM) continue;
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
    BeginScissorMode((int)((cx - W / 2) * g_uiK), (int)(y0 * g_uiK), (int)(W * g_uiK), (int)((h + 30) * g_uiK));
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
    bool big = app.warpBig;
    Vector2 ctr = {c.W * 0.5f, c.H * 0.5f};
    float halfDiag = std::sqrt(c.W * c.W + c.H * c.H) * 0.5f;

    BeginBlendMode(BLEND_ADDITIVE);
    // 恒星間ワープ: 前方の中心が光の速さに近づいたように白く輝く
    if (big && wi > 0.01)
        DrawCircleGradient((int)ctr.x, (int)ctr.y, halfDiag * (0.06f + 0.22f * (float)wi), Fade({225, 235, 255, 255}, (float)(0.5 * wi)),
                           {120, 160, 255, 0});
    // 星の光の筋（画面中央から外へ伸びる）。恒星間ワープでは光速に近い速さで進む時のように、星が前方へ集まり（光行差）、
    // 前の星は青く、横から後ろの星は赤くなる（ドップラー効果）
    if (wi > 0.01) {
        float len = (float)((big ? 2.6 : 1.6) * wi);
        double beta = big ? 0.97 * wi : 0, gamma = 1 / std::sqrt(1 - beta * beta);
        for (size_t i = 0; i < app.sky.stars.size(); i++) {
            const Star& st = app.sky.stars[i];
            if (st.mag > 6.5f) break;              // 暗い星は描かない（明るい順に並んでいる）
            if (st.size <= 0 && i % 2) continue;  // 暗い星は半分に間引く（画面が白飛びしないように）
            V3 dir = st.dir;
            Color col = st.col;
            if (big) {
                double ct = Dot(dir, c.fwd);
                double ct2 = (ct + beta) / (1 + beta * ct);
                V3 perp = dir - c.fwd * ct;
                double pl = Len(perp);
                if (pl > 1e-9) dir = c.fwd * ct2 + perp * (std::sqrt(std::max(0.0, 1 - ct2 * ct2)) / pl);
                double D = 1 / (gamma * (1 - beta * ct2));   // 1 より大きいと青く、小さいと赤く
                double kb = std::min(0.85, std::max(0.0, (D - 1) * 0.5)), kr = std::min(1.0, std::max(0.0, (1 - D) * 1.5));
                auto mix = [](unsigned char a, double b, double k) { return (unsigned char)(a + (b - a) * k); };
                col = {mix(mix(col.r, 150, kb), 255, kr), mix(mix(col.g, 190, kb), 90, kr), mix(mix(col.b, 255, kb), 60, kr), col.a};
            }
            Vector2 p;
            if (!Project(c, dir, &p)) continue;
            Vector2 v = {p.x - ctr.x, p.y - ctr.y};
            float r = std::sqrt(v.x * v.x + v.y * v.y) / halfDiag;
            if (r > 1.2f) continue;
            Vector2 q = {p.x + v.x * len * r, p.y + v.y * len * r};
            col.a = (unsigned char)(std::min(1.0, 0.25 + wi) * (big ? 0.5 : 1.0) * col.a);   // 恒星間ワープは筋が多く重なるので淡く
            DrawLineEx(p, q, st.size > 0 ? 2.0f : 1.0f, col);
        }
    }
    // 奥から手前へ流れる光の粒（トンネル）。恒星間ワープでは数を増やし、渦を巻いて流れる
    float tunnel = (float)std::min(1.0, wi * 1.6);
    if (tunnel > 0.01) {
        int n = big ? 420 : 220;
        for (int i = 0; i < n; i++) {
            double h1 = std::fmod(std::sin(i * 12.9898) * 43758.5453, 1.0);
            double h2 = std::fmod(std::sin(i * 78.233) * 12543.123, 1.0);
            double ang = std::fabs(h1) * 2 * PI;
            double z = 1.0 - std::fmod(std::fabs(h2) + app.warpPhase * (0.6 + 0.4 * std::fabs(h1)), 1.0);  // 1(奥)→0(手前)
            double z2 = std::min(1.0, z + 0.05 + 0.25 * wi);
            double a1 = ang, a2 = ang;
            if (big) {   // 手前に来るほど回る
                a1 += app.warpPhase * 0.35 + (1 - z) * 1.2;
                a2 += app.warpPhase * 0.35 + (1 - z2) * 1.2;
            }
            float r1 = (float)(halfDiag * 0.06 / (z + 0.05)), r2 = (float)(halfDiag * 0.06 / (z2 + 0.05));
            Vector2 a = {ctr.x + (float)std::cos(a2) * r2, ctr.y + (float)std::sin(a2) * r2};
            Vector2 b = {ctr.x + (float)std::cos(a1) * r1, ctr.y + (float)std::sin(a1) * r1};
            float fade = (float)(tunnel * std::min(1.0, (1.0 - z) * 3.0));
            Color col = i % 3 == 0 ? Color{255, 220, 170, 255} : Color{140, 190, 255, 255};
            if (big && i % 5 == 1) col = {200, 150, 255, 255};
            DrawLineEx(a, b, 1.5f + 2.5f * (float)(1.0 - z), Fade(col, fade * (big ? 0.55f : 0.8f)));
        }
        // 恒星間ワープ: 奥から迫ってくる光の輪
        if (big)
            for (int k = 0; k < 6; k++) {
                double z = 1.0 - std::fmod(k / 6.0 + app.warpPhase * 0.5, 1.0);
                float r = (float)(halfDiag * 0.06 / (z + 0.05));
                float al = (float)(tunnel * std::min(1.0, (1.0 - z) * 2.0) * std::min(1.0, z * 4.0) * 0.35);
                DrawRing(ctr, r, r + 2.0f + 6.0f * (float)(1 - z), 0, 360, 72, Fade({150, 200, 255, 255}, al));
            }
    }
    // 恒星間ワープの到着: 閃光の後に広がる衝撃波の輪
    if (big && app.warpJumped) {
        double u = app.warpT - WarpCharge(app);
        float r = halfDiag * (float)(0.05 + 1.5 * u), th = std::max(2.0f, 40.0f * (float)(1 - u / 1.5));
        float al = (float)std::exp(-u * 1.8);
        if (al > 0.01) DrawRing(ctr, r, r + th, 0, 360, 96, Fade({190, 220, 255, 255}, al * 0.6f));
    }
    EndBlendMode();

    // 周辺の青い滲みと閃光
    DrawCircleGradient((int)ctr.x, (int)ctr.y, halfDiag, {40, 90, 200, 0}, Fade({60, 120, 255, 255}, (float)((big ? 0.75 : 0.55) * wi)));
    if (flash > 0.003) DrawRectangle(0, 0, (int)c.W, (int)c.H, Fade(big ? Color{235, 244, 255, 255} : Color{225, 238, 255, 255}, (float)std::min(1.0, flash)));
}

static void DrawToast(const App& app) {
    if (app.toastTime <= 0) return;
    float a = (float)std::min(1.0, app.toastTime);
    TxtC(app.toast.c_str(), app.cam.W * 0.5f, app.cam.H * 0.24f, 24, Fade({255, 240, 200, 255}, a));
}

// ---------------------------------------------------------------- 見え方（明るさ・満ち欠け・見かけの大きさ・月齢）
// 地上にいればその場所から、そうでなければ地球の中心から見た値。等級は惑星は Mallama & Hilton (2018) の式、
// 月・太陽は標準的な式、準惑星・大きな衛星は絶対等級 H から（位相の補正は 0.03 等/度の目安）
static int GroundBody(const App& app);

struct Appearance {
    bool ok = false, here = false;
    double mag = NAN, k = 0, alpha = 0, size = 0, age = NAN;
    int moonPhase = -1;
};

static Appearance SeenFrom(const App& app, int i) {
    Appearance a;
    int g = GroundBody(app);
    a.here = g >= 0;
    V3 O = a.here ? ShipWorld(app) : app.B[B_EARTH].pos;
    const Body& b = app.B[i];
    if (i == g || (!a.here && i == B_EARTH) || b.d.kind == KIND_PROBE) return a;
    V3 T = b.pos, S = app.B[HostStar(app.B, i)].pos;   // 太陽系の外の惑星はその恒星に照らされる
    double delta = Len(T - O), r = Len(T - S);
    if (delta <= b.d.radius) return a;
    a.ok = true;
    a.size = 2 * std::atan(b.d.radius / delta);
    if (i == B_SUN) {
        a.k = 1;
        a.mag = -26.74 + 5 * std::log10(delta / AU_KM);
        return a;
    }
    if (b.d.star >= 0) {   // 太陽系の外の恒星（褐色矮星は可視光ではほぼ見えないので等級を出さない）
        a.k = 1;
        if (StarAbsMag(b.d) < 90) a.mag = StarApparentMag(b.d, delta);
        return a;
    }
    double ca = Dot(Norm(S - T), Norm(O - T));
    a.alpha = std::acos(std::max(-1.0, std::min(1.0, ca))) / DEG;
    a.k = (1 + ca) / 2;
    double x = a.alpha, rd = 5 * std::log10((r / AU_KM) * (delta / AU_KM));
    std::string en = b.d.en;
    switch (i) {
        case B_MERCURY:
            a.mag = -0.613 + x * (6.3280e-2 + x * (-1.6336e-3 + x * (3.3644e-5 + x * (-3.4265e-7 + x * (1.6893e-9 - x * 3.0334e-12))))) + rd;
            break;
        case B_VENUS: a.mag = -4.384 + x * (-1.044e-3 + x * (3.687e-4 + x * (-2.814e-6 + x * 8.938e-9))) + rd; break;
        case B_MARS: a.mag = -1.601 + x * (2.267e-2 - x * 1.302e-4) + rd; break;
        case B_JUPITER: a.mag = -9.395 + x * (3.7e-4 + x * 6.16e-4) + rd; break;
        case B_SATURN: {   // 環の傾き（観測者から見た土星の環の開き）で明るさが変わる
            double sb = std::fabs(Dot(Norm(O - T), b.pole));
            a.mag = -8.914 - 1.825 * sb + 0.026 * x - 0.378 * sb * std::exp(-2.25 * x) + rd;
            break;
        }
        case B_URANUS: a.mag = -7.110 + x * (6.587e-3 + x * 1.045e-4) + rd; break;
        case B_NEPTUNE: a.mag = -7.00 + rd; break;
        case B_MOON: a.mag = 0.23 + rd + 0.026 * x + 4.0e-9 * x * x * x * x; break;
        default: {
            static const struct { const char* en; double H; } HS[] = {{"Pluto", -0.76}, {"Ceres", 3.34}, {"Vesta", 3.20},
                {"Eris", -1.11}, {"Io", -1.68}, {"Europa", -1.41}, {"Ganymede", -2.09}, {"Callisto", -1.05}, {"Titan", -1.28}};
            for (const auto& h : HS)
                if (en == h.en) a.mag = h.H + rd + 0.03 * x;
        }
    }
    // 月齢: 地球（の上）から見た月と太陽の黄経の差から
    if (i == B_MOON && (!a.here || g == B_EARTH)) {
        V3 e = app.B[B_EARTH].pos, m = T - e, s = S - e;
        double D = std::fmod((std::atan2(-m.z, m.x) - std::atan2(-s.z, s.x)) / DEG + 720.0, 360.0);
        a.age = D / 360.0 * 29.530589;
        a.moonPhase = (int)std::floor((D + 22.5) / 45.0) % 8;
    }
    return a;
}

static std::string FmtAngle(double rad) {
    char b[32];
    double d = rad / DEG;
    if (d >= 1) snprintf(b, sizeof b, "%.2f°", d);
    else if (d * 60 >= 1) snprintf(b, sizeof b, "%.1f′", d * 60);
    else if (d * 3600 >= 0.1) snprintf(b, sizeof b, "%.1f″", d * 3600);
    else if (d * 3600 >= 0.001) snprintf(b, sizeof b, "%.4f″", d * 3600);   // 恒星（ケンタウルス座アルファ星 A は 0.0085″）
    else snprintf(b, sizeof b, "%.1e″", d * 3600);                         // 太陽系の外の惑星
    return b;
}

// ---------------------------------------------------------------- 天体の情報（I キー）
// ターゲットの天体の大きさ・重力・自転と公転・今の距離などを、データから計算して右側に出す
struct InfoLine {
    std::string label, value;
    bool title = false;
};

static std::vector<InfoLine> InfoLines(const App& app) {
    const int i = app.target;
    const Body& b = app.B[i];
    const BodyDef& d = b.d;
    std::vector<InfoLine> L;
    char v[256];
    auto earthX = [&](double ratio) {
        char r[32], o[64];
        snprintf(r, sizeof r, "%.3g", ratio);
        snprintf(o, sizeof o, T(UI_INFO_EARTHX), r);
        return std::string(o);
    };
    // 種類
    std::string kind, en = d.en;
    const NearbyStar* ns = d.star >= 0 ? &NEARBY_STARS[d.star] : nullptr;
    if (i == B_SUN) kind = T(UI_INFO_STAR);
    else if (ns) kind = std::string(T(ns->kind == 1 ? UI_INFO_WHITE_DWARF : ns->kind == 2 ? UI_INFO_BROWN_DWARF : UI_INFO_STAR)) + "  (" + ns->spec + ")";
    else if (d.kind == KIND_PROBE) kind = T(d.tleN > 0 ? UI_INFO_SAT : UI_INFO_PROBE);
    else if (d.parent > 0) {
        snprintf(v, sizeof v, T(d.exo >= 0 ? UI_INFO_PLANET_OF : UI_INFO_MOON_OF), TR(app.B[d.parent].d.jp, app.B[d.parent].d.en));
        kind = v;
    } else if (i <= B_NEPTUNE) kind = T(UI_INFO_PLANET);
    else if (i == B_PLUTO || en == "Ceres" || en == "Eris") kind = T(UI_INFO_DWARF);
    else if (std::string(d.jp).find("彗星") != std::string::npos) kind = T(UI_INFO_COMET);
    else kind = T(UI_INFO_ASTEROID);
    L.push_back({TR(d.jp, d.en), kind, true});

    const double G = 6.674e-11;
    if (d.kind != KIND_PROBE) {
        snprintf(v, sizeof v, "%s km", Grouped(d.radius, d.radius < 10 ? 2 : d.radius < 100 ? 1 : 0).c_str());
        // 比べる相手: 恒星は太陽、褐色矮星は木星、ほかは地球（白色矮星の半径も地球と、質量は太陽と）
        auto times = [&](UiId fmt, double ratio) {
            char r[32], o[64];
            snprintf(r, sizeof r, "%.3g", ratio);
            snprintf(o, sizeof o, T(fmt), r);
            return std::string(o);
        };
        bool sunLike = d.kind == KIND_STAR && (!ns || ns->kind == 0), jupLike = ns && ns->kind == 2;
        std::string rx = sunLike ? times(UI_INFO_SUNX, d.radius / 695700.0) : jupLike ? times(UI_INFO_JUPX, d.radius / 69911.0) : earthX(d.radius / 6371.0);
        std::string mx = jupLike ? times(UI_INFO_JUPX, d.mass / 1.898e27) : d.kind == KIND_STAR ? times(UI_INFO_SUNX, d.mass / 1.989e30)
                                                                                                : earthX(d.mass / 5.9722e24);
        L.push_back({T(UI_INFO_RADIUS), std::string(v) + "  (" + rx + ")"});
        snprintf(v, sizeof v, "%.3e kg", d.mass);
        L.push_back({T(UI_INFO_MASS), std::string(v) + "  (" + mx + ")"});
        if (ns) {
            snprintf(v, sizeof v, "%s K", Grouped(ns->teff, 0).c_str());
            L.push_back({T(UI_INFO_TEMP), v});
        }
        double g = G * d.mass / std::pow(d.radius * 1000, 2), esc = std::sqrt(2 * G * d.mass / (d.radius * 1000));
        snprintf(v, sizeof v, g < 0.1 ? "%.4f m/s²  (%.4g g)" : "%.2f m/s²  (%.2f g)", g, g / 9.80665);
        L.push_back({T(UI_INFO_GRAVITY), v});
        if (esc < 1000) snprintf(v, sizeof v, "%.1f m/s", esc);
        else snprintf(v, sizeof v, "%.2f km/s", esc / 1000);
        L.push_back({T(UI_INFO_ESCAPE), v});
        // 自転（0 は公転と同期）
        if (d.rotPeriod == 0) L.push_back({T(UI_INFO_DAY), T(UI_INFO_SYNC)});
        else {
            std::string r = FmtDuration(std::fabs(d.rotPeriod) * 86400);
            if (d.rotPeriod < 0) r += std::string("  (") + T(UI_INFO_RETRO) + ")";
            L.push_back({T(UI_INFO_DAY), r});
        }
        // 公転
        if (d.parent == 0 && d.a_au > 0) {
            L.push_back({T(UI_INFO_YEAR), FmtDuration(std::pow(d.a_au, 1.5) * 365.25 * 86400)});
            snprintf(v, sizeof v, "%.3f AU", d.a_au);
            L.push_back({T(UI_INFO_SMA), v});
            snprintf(v, sizeof v, "%.3f / %.2f°", d.e, d.inc);
            L.push_back({T(UI_INFO_ECC), v});
        } else if (d.exo >= 0) {   // 太陽系の外の惑星: 公転周期・軌道長半径・離心率と傾斜角（天球に対して）・平衡温度
            const NearbyPlanet& np = NEARBY_PLANETS[d.exo];
            L.push_back({T(UI_INFO_YEAR), FmtDuration(np.P * 86400)});
            snprintf(v, sizeof v, "%.4g AU", np.a);
            L.push_back({T(UI_INFO_SMA), v});
            snprintf(v, sizeof v, "%.3f / %.0f°", np.e, np.i);
            L.push_back({T(UI_INFO_ECC), v});
            snprintf(v, sizeof v, "%.0f K", np.teq);
            L.push_back({T(UI_INFO_TEQ), v});
            L.push_back({T(UI_INFO_ESTIMATED), T(np.orbitKnown ? UI_INFO_EST_SURFACE : UI_INFO_EST_ORBIT)});
        } else if (d.parent > 0 && d.moonPeriod > 0) {
            L.push_back({T(UI_INFO_YEAR), FmtDuration(std::fabs(d.moonPeriod) * 86400)});
            snprintf(v, sizeof v, "%s km", Grouped(d.moonA, 0).c_str());
            L.push_back({T(UI_INFO_SMA), v});
        }
        if (ns && ns->P > 0)   // 連星の相手と、相手を回る周期・平均の距離
            for (const Body& o : app.B)
                if (&o != &b && o.d.star >= 0 && NEARBY_STARS[o.d.star].system == ns->system) {
                    snprintf(v, sizeof v, T(UI_INFO_PARTNER_V), TR(o.d.jp, o.d.en), ns->P, ns->a * 1000.0 / ns->plx);
                    L.push_back({T(UI_INFO_PARTNER), v});
                }
        if (ns && ns->estimated) L.push_back({T(UI_INFO_ESTIMATED), T(UI_INFO_ESTIMATED_V)});
        int moons = 0;
        for (const Body& o : app.B)
            if (o.d.parent == i && o.d.kind != KIND_PROBE) moons++;
        if (moons > 0 && d.kind != KIND_STAR) L.push_back({T(UI_INFO_MOONS), std::to_string(moons)});
    } else if (d.tleN > 0) {
        double alt = Len(b.pos - app.B[B_EARTH].pos) - app.B[B_EARTH].d.radius;
        L.push_back({T(UI_INFO_ALT), FmtDist(alt) + " / " + FmtDuration(86400 / d.tleN)});
    } else if (d.probe >= 0 && d.probe < (int)g_probes.size() && !g_probes[d.probe].segs.empty()) {
        const auto& segs = g_probes[d.probe].segs;
        L.push_back({T(UI_INFO_DATA), FmtDate(segs.front().t.front()).substr(0, 10) + " - " + FmtDate(segs.back().t.back()).substr(0, 10)});
    }
    // 今の距離
    if (i != B_SUN) {
        double ds = Len(b.pos - app.B[B_SUN].pos);
        L.push_back({T(UI_INFO_FROM_SUN), FmtDist(ds) + "  (" + T(UI_LIGHT) + " " + FmtDuration(ds / C_KMS) + ")"});
    }
    if (i != B_EARTH) L.push_back({T(UI_INFO_FROM_EARTH), FmtDist(Len(b.pos - app.B[B_EARTH].pos))});
    L.push_back({T(UI_INFO_FROM_SHIP), FmtDist(std::max(0.0, SurfaceDist(b, ShipWorld(app))))});
    // 見え方（地上にいればそこから、ほかは地球から）
    Appearance ap = SeenFrom(app, i);
    if (ap.ok) {
        if (!std::isnan(ap.mag)) {
            snprintf(v, sizeof v, T(UI_INFO_MAG_UNIT), ap.mag);
            L.push_back({T(ap.here ? UI_INFO_MAG_HERE : UI_INFO_MAG_EARTH), v});
        }
        if (d.kind != KIND_STAR) {
            int sh = ap.k < 0.03 ? 0 : ap.k < 0.35 ? 1 : ap.k < 0.65 ? 2 : ap.k < 0.97 ? 3 : 4;
            snprintf(v, sizeof v, "%.0f%%  (%s)", ap.k * 100, T((UiId)(UI_SHAPE0 + sh)));
            L.push_back({T(UI_INFO_PHASE), v});
        }
        L.push_back({T(UI_INFO_ASIZE), FmtAngle(ap.size)});
        if (!std::isnan(ap.age)) {
            snprintf(v, sizeof v, "%.1f  (%s)", ap.age, T((UiId)(UI_MOON0 + ap.moonPhase)));
            L.push_back({T(UI_INFO_AGE), v});
        }
    }
    // その天体のまわりのデータがある探査機（地球は打ち上げた全機が当たるので除く）
    if (d.kind != KIND_STAR && i != B_EARTH && d.kind != KIND_PROBE) {
        std::string list;
        for (const Body& o : app.B) {
            if (o.d.probe < 0 || o.d.probe >= (int)g_probes.size()) continue;
            for (const ProbeSeg& sg : g_probes[o.d.probe].segs)
                if (sg.body == i) {
                    if (!list.empty()) list += g_jp ? "、" : ", ";
                    list += TR(o.d.jp, o.d.en);
                    break;
                }
        }
        if (!list.empty()) L.push_back({T(UI_INFO_VISITS), list});
    }
    return L;
}

// 表示する行（値が長ければ次の行から折り返す）
struct InfoRow {
    std::string label, value;
    bool title, wrapped;
};
static const float INFO_W = 380, INFO_LH = 20;

static std::vector<InfoRow> InfoRows(const App& app) {
    std::vector<InfoRow> rows;
    for (const InfoLine& l : InfoLines(app)) {
        if (l.title || TxtW(l.label.c_str(), 13) + TxtW(l.value.c_str(), 14) + 30 < INFO_W) {
            rows.push_back({l.label, l.value, l.title, false});
            continue;
        }
        rows.push_back({l.label, "", false, false});
        std::string line, word;
        auto put = [&](const std::string& w) {
            if (!line.empty() && TxtW((line + w).c_str(), 14) > INFO_W - 40) {
                rows.push_back({"", line, false, true});
                line.clear();
            }
            line += w;
        };
        for (size_t k = 0; k < l.value.size(); k++) {   // 区切り（、 , 空白）の後で折り返す
            word += l.value[k];
            bool sep = l.value[k] == ' ' || l.value[k] == ',' || (k >= 2 && l.value.compare(k - 2, 3, "、") == 0);
            if (sep) { put(word); word.clear(); }
        }
        if (!word.empty()) put(word);
        if (!line.empty()) rows.push_back({"", line, false, true});
    }
    return rows;
}

// 右上のターゲット情報の下。タッチ操作の時は右の加速・減速ボタンの列にかからないよう左へずらす
static Rectangle InfoCardRect(const App& app, const Cam& c, size_t nrows) {
    float shift = app.touchUI ? TouchButtonRect(c, TB_THRUST).width + 18 : 0;
    return {c.W - INFO_W - 6 - shift, 118, INFO_W, 34 + nrows * INFO_LH + 22};
}

static bool InfoCardHit(const App& app, Vector2 p) {
    return CheckCollisionPointRec(p, InfoCardRect(app, UiCam(app.cam), InfoRows(app).size()));
}

static void DrawInfoCard(const App& app) {
    if (!app.showInfo || app.photoMode > 0) return;
    const Cam& c = app.cam;
    std::vector<InfoRow> rows = InfoRows(app);
    Rectangle r = InfoCardRect(app, c, rows.size());
    DrawRectangleRounded(r, 0.04f, 6, {0, 10, 25, 200});
    DrawRectangleRoundedLinesEx(r, 0.04f, 6, 1.0f, {90, 130, 180, 200});
    float y = r.y + 10;
    for (const InfoRow& row : rows) {
        if (row.title) {
            Txt(row.label.c_str(), r.x + 12, y, 19, {255, 220, 80, 255});
            Txt(row.value.c_str(), r.x + r.width - 12 - TxtW(row.value.c_str(), 14), y + 4, 14, {170, 200, 230, 255});
            y += 34;
            continue;
        }
        if (!row.label.empty()) Txt(row.label.c_str(), r.x + 12, y + 1, 13, {150, 175, 210, 255});
        if (!row.value.empty()) {
            float vx = row.wrapped ? r.x + 28 : r.x + r.width - 12 - TxtW(row.value.c_str(), 14);
            Txt(row.value.c_str(), vx, y, 14, {225, 235, 250, 255});
        }
        y += INFO_LH;
    }
    Txt(T(UI_INFO_CLOSE), r.x + 12, r.y + r.height - 20, 12, {120, 145, 180, 220});
}

// ---------------------------------------------------------------- 地上からの観測
// 地表（高度 2 km 以下）にいる時: 地平線と方位、ターゲットの高度・方位、出・南中・入りの時刻を出す

// 立っている天体（地表にいなければ -1）
static int GroundBody(const App& app) {
    const Body& b = app.B[app.ship.ref];
    // 小惑星など小さな天体は地平線がすぐ近くで丸いので対象にしない（半径 100 km 以上）
    if (app.warpT >= 0 || b.d.kind == KIND_PROBE || b.d.kind == KIND_GAS || b.d.kind == KIND_STAR || b.d.radius < 100) return -1;
    return SurfaceDist(b, ShipWorld(app)) < 2 ? app.ship.ref : -1;
}

static V3 GroundFixed(const App& app, int g) {
    V3 X, Z;
    BodyFixedAxes(app.B[g], &X, &Z);
    V3 r = ShipWorld(app) - app.B[g].pos;
    return V3(Dot(r, X), Dot(r, app.B[g].pole), Dot(r, Z));
}

// 出・南中・入り（今から 1 日分。月面は 1 か月分）。結果は時刻が大きく動くまで使い回す
struct RiseSet {
    int body = -1, target = -1;
    V3 f;
    double t0 = NAN, span = 0;
    double rise = NAN, set = NAN, transit = NAN, transitAlt = 0;
    bool alwaysUp = false, alwaysDown = false;
};
static RiseSet g_riseSet;

static const RiseSet& GetRiseSet(const App& app, int g) {
    RiseSet& r = g_riseSet;
    V3 f = GroundFixed(app, g);
    if (r.body == g && r.target == app.target && Len(r.f - f) < 1 && app.tDays >= r.t0 && app.tDays - r.t0 < r.span * 0.01) return r;
    r = RiseSet();
    r.body = g;
    r.target = app.target;
    r.f = f;
    r.t0 = app.tDays;
    const BodyDef& d = app.B[g].d;
    double P = d.rotPeriod == 0 ? d.moonPeriod : std::fabs(d.rotPeriod);
    r.span = P * 1.1;   // 太陽日（自転より少し長い）を含むように
    // 地平線: 地球は大気の屈折（太陽・月は半径も）を入れる
    double h0 = g == B_EARTH ? (app.target == B_SUN || app.target == B_MOON ? -0.833 : -0.567) * DEG : 0.0;
    const int N = 480;
    double prevAlt = 0, az, maxAlt = -10;
    auto altAt = [&](double t) { double a, z; AltAz(app.B, g, f, app.target, t, &a, &z); return a - h0; };
    auto refine = [&](double a, double b) {   // 符号が変わる所を二分法で
        double fa = altAt(a);
        for (int k = 0; k < 24; k++) {
            double m = (a + b) / 2, fm = altAt(m);
            if ((fm > 0) == (fa > 0)) { a = m; fa = fm; } else b = m;
        }
        return (a + b) / 2;
    };
    bool anyUp = false, anyDown = false;
    for (int k = 0; k <= N; k++) {
        double t = r.t0 + r.span * k / N, a;
        AltAz(app.B, g, f, app.target, t, &a, &az);
        a -= h0;
        if (a > 0) anyUp = true; else anyDown = true;
        if (a > maxAlt) { maxAlt = a; r.transit = t; }
        if (k > 0) {
            double tp = r.t0 + r.span * (k - 1) / N;
            if (prevAlt <= 0 && a > 0 && std::isnan(r.rise)) r.rise = refine(tp, t);
            if (prevAlt > 0 && a <= 0 && std::isnan(r.set)) r.set = refine(tp, t);
        }
        prevAlt = a;
    }
    // 南中は一番高い所を細かく（黄金分割）
    if (!std::isnan(r.transit)) {
        double a = std::max(r.t0, r.transit - r.span / N), b = std::min(r.t0 + r.span, r.transit + r.span / N);
        for (int k = 0; k < 40; k++) {
            double m1 = b - (b - a) * 0.618, m2 = a + (b - a) * 0.618;
            if (altAt(m1) < altAt(m2)) a = m1; else b = m2;
        }
        r.transit = (a + b) / 2;
        r.transitAlt = altAt(r.transit) + h0;
    }
    r.alwaysUp = !anyDown;
    r.alwaysDown = !anyUp;
    return r;
}

// 時刻の表示（tz: UTC からの時間）。1 日より長い範囲なら日付も
static std::string GroundTime(double t, double tz, bool withDate) {
    std::string s = FmtDate(t + tz / 24);   // "YYYY-MM-DD hh:mm:ss UTC"
    return withDate ? s.substr(5, 11) : s.substr(11, 5);
}

// 流れ星（地球の地上で、夜に）。流星群の放射点から飛び出す向きに流れる。散在流星（1 時間に 6 個）も
// 数は 1 時間あたり ZHR × 放射点の高さの sin。時間倍率をかけると増える（実時間 1 秒に 4 個まで）
static void UpdateMeteors(App& app, double dt, double simDt) {
    for (size_t k = 0; k < app.meteors.size();) {
        app.meteors[k].t += (float)dt;
        if (app.meteors[k].t > app.meteors[k].dur) app.meteors.erase(app.meteors.begin() + k);
        else k++;
    }
    int g = GroundBody(app);
    if (g != B_EARTH || simDt <= 0) return;
    V3 up = Norm(ShipWorld(app) - app.B[g].pos);
    double sunAlt = std::asin(Dot(Norm(app.B[B_SUN].pos - ShipWorld(app)), up)) / DEG;
    double dark = std::max(0.0, std::min(1.0, (-6 - sunAlt) / 6));   // 太陽が −6° から −12° で暗くなる
    if (dark <= 0) return;
    double solLon = SolarLongitude(app.B, app.tDays);
    auto spawn = [&](V3 rad, double rate) {   // rate: 1 時間あたり
        double expect = std::min(rate * simDt / 3600.0, 4.0 * dt) * dark;
        int n = (int)expect + (GetRandomValue(0, 9999) < (int)((expect - (int)expect) * 10000) ? 1 : 0);
        for (int i = 0; i < n && app.meteors.size() < 40; i++) {
            App::Meteor m;
            V3 any = std::fabs(rad.y) < 0.9 ? V3(0, 1, 0) : V3(1, 0, 0);
            V3 u = Norm(Cross(rad, any)), v = Cross(rad, u);
            double ph = GetRandomValue(0, 3599) / 10.0 * DEG;
            m.rad = rad;
            m.side = u * std::cos(ph) + v * std::sin(ph);
            m.d = (float)(GetRandomValue(8, 60) * DEG);
            m.len = (float)(GetRandomValue(4, 18) * DEG);
            m.dur = GetRandomValue(35, 110) / 100.0f;
            m.bright = GetRandomValue(40, 100) / 100.0f;
            V3 start = rad * std::cos(m.d) + m.side * std::sin(m.d);
            if (Dot(start, up) > std::sin(8 * DEG)) app.meteors.push_back(m);
        }
    };
    for (const MeteorShower& ms : SHOWERS) {
        double zhr = ShowerZHR(ms, solLon);
        if (zhr <= 0) continue;
        V3 rad = EquToRender(ms.ra, ms.dec);
        double h = Dot(rad, up);
        if (h > 0) spawn(rad, zhr * h);
    }
    // 散在流星: 放射点はでたらめ（地平線より上）
    V3 r = Norm(V3(GetRandomValue(-1000, 1000), GetRandomValue(-1000, 1000), GetRandomValue(-1000, 1000)));
    if (Dot(r, up) < 0) r = r - up * (2 * Dot(r, up));
    spawn(r, 6);
}

static void DrawMeteors(const App& app, V3 up) {
    const Cam& c = app.cam;
    for (const App::Meteor& m : app.meteors) {
        float s = m.t / m.dur;
        float fade = s < 0.15f ? s / 0.15f : s > 0.75f ? (1 - s) / 0.25f : 1.0f;
        auto at = [&](float u) { double a = m.d + m.len * u; return m.rad * std::cos(a) + m.side * std::sin(a); };
        const int K = 8;   // 尾ほど淡く
        for (int k = 0; k < K; k++) {
            float u0 = std::max(0.0f, s - 0.35f * (k + 1) / K), u1 = std::max(0.0f, s - 0.35f * k / K);
            Vector2 a, b;
            if (u1 <= u0 || Dot(at(u0), up) < 0 || Dot(at(u1), up) < 0) continue;   // 地平線より下（地面の向こう）は描かない
            if (!Project(c, at(u0), &a) || !Project(c, at(u1), &b)) continue;
            unsigned char al = (unsigned char)(255 * m.bright * fade * (1.0f - (float)k / K));
            DrawLineEx(a, b, 2.0f - 1.2f * k / K, {255, 250, 235, al});
        }
    }
}

static void DrawGroundView(const App& app) {
    int g = GroundBody(app);
    if (g < 0 || app.photoMode > 0) return;
    const Cam& c = app.cam;
    const Body& b = app.B[g];
    V3 up = Norm(ShipWorld(app) - b.pos);
    V3 north = b.pole - up * Dot(b.pole, up);
    if (Len(north) < 1e-9) {
        V3 X, Z;
        BodyFixedAxes(b, &X, &Z);
        north = X - up * Dot(X, up);
    }
    north = Norm(north);
    V3 east = Cross(north, up);
    DrawMeteors(app, up);
    // 地平線（5° ごと）と 8 方位の名前
    Vector2 prev{};
    bool have = false;
    for (int k = 0; k <= 72; k++) {
        double a = 2 * PI * k / 72;
        Vector2 p;
        bool ok = Project(c, north * std::cos(a) + east * std::sin(a), &p);
        if (ok && have) DrawLineEx(prev, p, 1.2f, {120, 170, 120, 110});
        prev = p;
        have = ok;
    }
    for (int k = 0; k < 8; k++) {
        double a = PI / 4 * k;
        Vector2 p;
        if (!Project(c, north * std::cos(a) + east * std::sin(a), &p)) continue;
        const char* name = T((UiId)(UI_DIR0 + k * 2));
        float fs = k % 2 == 0 ? 22.0f : 16.0f;
        TxtC(name, p.x, p.y - fs - 6, fs, k == 0 ? Color{255, 150, 130, 230} : Color{170, 220, 170, 220});
    }
    // パネル: 場所と時刻、ターゲットの高度・方位、出・南中・入り
    V3 f = GroundFixed(app, g);
    double lat = std::asin(std::max(-1.0, std::min(1.0, f.y / Len(f)))) / DEG, lon = std::atan2(-f.z, f.x) / DEG;
    // 検索の「地上」で選んだ都市の近く（50 km 以内）にいる間は、その都市の標準時で表示する
    bool tzOk = app.groundTzOk && app.groundTzBody == g && Len(f - app.groundTzPlace) < 50;
    double tz = tzOk ? app.groundTz : 0;
    char l1[200], l2[200], l3[240];
    std::string now = GroundTime(app.tDays, tz, false);
    snprintf(l1, sizeof l1, "%s  %s %.1f°  %s %.1f°    %s %s", TR(b.d.jp, b.d.en), lat >= 0 ? "N" : "S", std::fabs(lat),
             lon >= 0 ? "E" : "W", std::fabs(lon), now.c_str(), tzOk ? app.groundTzName.c_str() : "UTC");
    const Body& tb = app.B[app.target];
    l2[0] = l3[0] = 0;
    if (app.target != g) {
        double alt, az;
        AltAz(app.B, g, f, app.target, app.tDays, &alt, &az);
        int dir = (int)std::floor(az / (2 * PI) * 16 + 0.5) % 16;
        snprintf(l2, sizeof l2, "%s   %s %.1f°   %s %.1f° (%s)", TR(tb.d.jp, tb.d.en), T(UI_GND_ALT), alt / DEG, T(UI_GND_AZ), az / DEG,
                 T((UiId)(UI_DIR0 + dir)));
        Appearance ap = SeenFrom(app, app.target);   // 明るさと満ち欠け
        if (ap.ok && !std::isnan(ap.mag)) {
            char m[64];
            snprintf(m, sizeof m, T(UI_INFO_MAG_UNIT), ap.mag);
            size_t n = strlen(l2);
            if (app.target == B_SUN) snprintf(l2 + n, sizeof l2 - n, "   %s", m);
            else snprintf(l2 + n, sizeof l2 - n, "   %s  %.0f%%", m, ap.k * 100);
        }
        const RiseSet& rs = GetRiseSet(app, g);
        bool date = rs.span > 2;
        auto tm = [&](double t) { return std::isnan(t) ? std::string("--:--") : GroundTime(t, tz, date); };
        if (rs.alwaysUp) snprintf(l3, sizeof l3, "%s", T(UI_GND_ALWAYS_UP));
        else if (rs.alwaysDown) snprintf(l3, sizeof l3, "%s", T(UI_GND_ALWAYS_DOWN));
        else
            snprintf(l3, sizeof l3, "%s %s    %s %s (%.0f°)    %s %s", T(UI_GND_RISE), tm(rs.rise).c_str(),
                     T(lat >= 0 ? UI_GND_TRANSIT_S : UI_GND_TRANSIT_N), tm(rs.transit).c_str(), rs.transitAlt / DEG, T(UI_GND_SET),
                     tm(rs.set).c_str());
    }
    float w = std::max(TxtW(l1, 15), std::max(TxtW(l2, 15), TxtW(l3, 14))) + 24, h = l2[0] ? 76.0f : 30.0f;
    float x = 6, y = c.H - 64 - 44 - h;
    DrawRectangleRounded({x, y, w, h}, 0.1f, 6, {0, 10, 25, 170});
    Txt(l1, x + 12, y + 7, 15, {170, 220, 170, 255});
    if (l2[0]) {
        Txt(l2, x + 12, y + 29, 15, {255, 220, 80, 255});
        Txt(l3, x + 12, y + 51, 14, {210, 225, 245, 255});
    }
}

static void DrawHUD(App& app) {
    const Cam& c = app.cam;
    if (app.photoMode > 0) {   // 写真モード: 一覧・入力欄と、切り替えの知らせだけ
        DrawToast(app);
        DrawModalPanels(app);
        return;
    }
    DrawTouchButtons(app);
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
    snprintf(b, sizeof b, "%s %s    %s %s  %s %s", T(UI_FRAME), app.frameLock ? T(UI_FRAME_SHIP) : TR(app.B[s.ref].d.jp, app.B[s.ref].d.en), T(UI_NEAREST),
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
    DrawGroundView(app);
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

    // 左下の案内: エンジン欄と重なる時（狭い画面・大きい文字）はエンジン欄の上へ
    Rectangle eb = EngineButtonRect(c, 0);
    float hy = eb.x < TxtW(T(UI_HELP_HINT), 15) + 20 ? eb.y - 24 : c.H - 24;
    Txt(T(UI_HELP_HINT), 12, hy, 15, {150, 170, 200, 220});
#if defined(__EMSCRIPTEN__)
    if (int pend = DeferredAssetsPending(); pend > 0) {
        snprintf(b, sizeof b, T(UI_LOADING), pend);
        Txt(b, 12, hy - 40, 14, {255, 210, 120, 230});
    }
#endif
    snprintf(b, sizeof b, "%d FPS", GetFPS());
    Txt(b, 12, hy - 20, 13, {110, 130, 160, 200});

    DrawToast(app);

    if (app.showHelp) {
        const int lines = UI_HELP15 - UI_HELP1 + 1;
        float w = std::min(c.W - 20, 980.0f), h = 74 + lines * 26.0f + (app.pad >= 0 ? 26 : 0), hx = (c.W - w) / 2, hy = (c.H - h) / 2;
        DrawRectangleRounded({hx, hy, w, h}, 0.04f, 6, {0, 8, 20, 225});
        DrawRectangleRoundedLinesEx({hx, hy, w, h}, 0.04f, 6, 1.5f, {90, 130, 180, 255});
        TxtC(T(UI_HELP_TITLE), c.W * 0.5f, hy + 10, 22, {255, 220, 120, 255});
        for (int i = 0; i < lines; i++)
            Txt(T((UiId)(UI_HELP1 + i)), hx + 24, hy + 44 + i * 26.0f, i >= 12 ? 15 : 17, i >= 12 ? Color{170, 220, 190, 255} : Color{215, 228, 245, 255});
        if (app.pad >= 0) Txt(T(UI_HELP_PAD), hx + 24, hy + 44 + lines * 26.0f, 15, {255, 220, 140, 255});
        TxtC(CREDIT, c.W * 0.5f, hy + h - 26, 13, {150, 170, 190, 255});
    }
    DrawInfoCard(app);
    DrawTourPanel(app);
    DrawModalPanels(app);
}

// 後から届くデータ（ブラウザ版）: 読めるようになったものから組み込む。どれも、まだ組み込んでいない時だけ読む
static void RefreshDeferredAssets(App& app) {
    if (g_shapes.empty() && ShapesBin().data) {
        LoadShapes();
        for (Body& b : app.B)
            for (size_t k = 0; k < g_shapes.size(); k++)
                if (g_shapes[k].name == b.d.en) {
                    b.shape = (int)k;
                    b.outer = g_shapes[k].rmax;
                }
        for (const Shape& sh : g_shapes) {
            double r = 1;
            for (const Body& b : app.B)
                if (b.shape >= 0 && g_shapes[b.shape].name == sh.name) r = b.d.radius;
            app.shapeMesh.push_back(ShapeMesh(sh, r));
        }
    }
    if (app.belt.a.empty() && AsteroidsBin().data) app.belt = LoadAsteroids();
    if (app.maps.empty() && MapsBin().data) LoadBodyMaps(app);
    if (!app.earthMonths.id && EarthMonthsJpg().data) app.earthMonths = LoadEmbeddedTexture(EarthMonthsJpg(), ".jpg");
    if (!app.earthClouds.id && EarthCloudsJpg().data) app.earthClouds = LoadEmbeddedTexture(EarthCloudsJpg(), ".jpg");
    if (!app.moonTex && MoonColorJpg().data && MoonDataPng().data) LoadMoonTextures(app);
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
