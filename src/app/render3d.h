// Solar System Voyager — Copyright (c) 2026 hubstacksea
// MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
// 3D の描画（天体・自機・探査機・軌道・空）と音楽の選曲。
// main.cpp の一部で、main.cpp が決まった順に読み込む（単体ではコンパイルしない）

// シェーダは GLSL 3.30 で書いてある。ブラウザ版 (WebGL2) では GLSL ES 3.00 に書き換える。defs は #version の次の行に足す
static std::string GlslForPlatform(const char* src, const std::string& defs = "") {
    std::string s = src;
    size_t p = s.find("#version 330");
    if (p != std::string::npos && !defs.empty()) s.insert(s.find('\n', p) + 1, defs);
#if defined(__EMSCRIPTEN__)
    if (p != std::string::npos)   // #version は 1 行目に置く必要がある（ソースは改行から始まる）
        s = "#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler2D;\n" + s.substr(p + 12);
#endif
    return s;
}

// ---------------------------------------------------------------- 惑星のシェーダの版
// 惑星のシェーダ（PLANET_VS・PLANET_FS）は全部の天体の描き方を持っていて大きく、レジスタが足りずに値を退避するので描画が
// 重い（Intel UHD で、冥王星の地表の近くは種類を定数にすると 約 3 割速い）。画面で大きく見える天体（planetBig）は、種類
// （params.x）・表面模様（detail.x）・地図の表面模様（mapInfo2.x）を定数にした版（SPEC_KIND・SPEC_STYLE・SPEC_MAP）で描く。
// 版は初めて使う時に作り、できるまで待つ（1 つ 1 秒ほど。Intel UHD で、ドライバの保存したものが無い時 1.0〜1.3 秒、ある時
// 50 ms。裏でコンパイルさせて待たない作り方（parallel_shader_compile）も試したが、Mesa はコンパイルを頼む時に待ち、
// GPU を使う Chrome（ANGLE の OpenGL）ではかえって長く止まったので使わない）。値はすべて PlanetUniform で元のシェーダに
// 設定して写しを持っておき、版で描く前に、その版にまだ送っていない値だけを送る

// 元のシェーダの値の位置（名前を覚えておき、版でも同じ名前の位置を探す）
static int PlanetLoc(App& app, const char* name) {
    int loc = GetShaderLocation(app.shader, name);
    if (loc >= 0) {
        if ((int)app.planetUni.size() <= loc) app.planetUni.resize(loc + 1);
        app.planetUni[loc].name = name;
    }
    return loc;
}

static void PlanetUniform(App& app, int loc, const void* v, int type, int count = 1) {
    if (loc < 0) return;
    SetShaderValueV(app.shader, loc, v, type, count);
    if (loc >= (int)app.planetUni.size() || app.planetUni[loc].name.empty()) return;
    App::PlanetUni& u = app.planetUni[loc];
    int n = (type == SHADER_UNIFORM_VEC2 || type == SHADER_UNIFORM_IVEC2) ? 2 : (type == SHADER_UNIFORM_VEC3 || type == SHADER_UNIFORM_IVEC3) ? 3
          : (type == SHADER_UNIFORM_VEC4 || type == SHADER_UNIFORM_IVEC4) ? 4 : 1;
    u.type = type;
    u.count = count;
    u.data.assign((const unsigned char*)v, (const unsigned char*)v + 4 * n * count);
    u.ver = ++app.planetUniVer;
}

// 今の値で描く時のシェーダ（planetBig なら種類ごとの版、作れなければ元のシェーダ）
static Shader PlanetShaderFor(App& app) {
    if (!app.planetBig) return app.shader;
    auto first = [&](int loc) {   // その値の最初の成分
        if (loc < 0 || loc >= (int)app.planetUni.size() || app.planetUni[loc].data.size() < 4) return -1.0f;
        float f;
        std::memcpy(&f, app.planetUni[loc].data.data(), 4);
        return f;
    };
    int kind = (int)std::lround(first(app.locParams)), style = (int)std::lround(first(app.locDetail));
    int map = style == S_MAPPED ? (int)std::lround(first(app.locMapInfo2)) : -1;
    App::PlanetVar* v = nullptr;
    for (App::PlanetVar& pv : app.planetVars)
        if (pv.kind == kind && pv.style == style && pv.map == map) v = &pv;
    if (!v) {
        App::PlanetVar pv;
        pv.kind = kind, pv.style = style, pv.map = map;
        std::string defs = TextFormat("#define SPEC_KIND %d\n#define SPEC_STYLE %d\n#define SPEC_MAP %d\n", kind, style, map);
        double t0 = GetTime();
        pv.sh = LoadShaderFromMemory(GlslForPlatform(PLANET_VS, defs).c_str(), GlslForPlatform(PLANET_FS, defs).c_str());
        TraceLog(LOG_INFO, "SHADER: planet variant (kind %d, style %d, map %d) built in %.0f ms", kind, style, map, (GetTime() - t0) * 1000);
        pv.ok = IsShaderValid(pv.sh) && pv.sh.id != rlGetShaderIdDefault();
        if (pv.ok) {
            pv.sh.locs[SHADER_LOC_MAP_ROUGHNESS] = GetShaderLocation(pv.sh, "texture3");
            pv.sh.locs[SHADER_LOC_MAP_OCCLUSION] = GetShaderLocation(pv.sh, "texture4");
            pv.loc.assign(app.planetUni.size(), -1);
            for (size_t i = 0; i < app.planetUni.size(); i++)
                if (!app.planetUni[i].name.empty()) pv.loc[i] = GetShaderLocation(pv.sh, app.planetUni[i].name.c_str());
        }
        app.planetVars.push_back(pv);
        v = &app.planetVars.back();
    }
    if (!v->ok) return app.shader;
    for (size_t i = 0; i < app.planetUni.size(); i++) {
        const App::PlanetUni& u = app.planetUni[i];
        if (u.ver > v->synced && v->loc[i] >= 0) SetShaderValueV(v->sh, v->loc[i], u.data.data(), u.type, u.count);
    }
    v->synced = app.planetUniVer;
    return v->sh;
}

static void DrawPlanetMesh(App& app, const Mesh& mesh, Matrix m) {
    Material mat = app.mat;
    mat.shader = PlanetShaderFor(app);
    DrawMesh(mesh, mat, m);
}

// ---------------------------------------------------------------- 描画: 3D
static void DrawOrbits(App& app) {
    if (app.relU > 0.02) return;   // 光速に近い時は天体が光行差でずれて見え、軌道の線と合わなくなるので描かない
    const Cam& c = app.cam;
    // 地表近くでは軌道線が視界を横切って邪魔なので描かない
    for (const Body& b : app.B)
        if (Len(c.pos - b.pos) < b.d.radius * 1.3) return;
    SetPassProjection(c, 1.0, 1e11);
    rlDisableDepthTest();
    for (size_t i = 1; i < app.B.size(); i++) {
        const Body& b = app.B[i];
        const BodyDef& d = b.d;
        if (d.star >= 0) {
            // 連星: 重心のまわりの軌道（系の中にいる時だけ）
            const NearbyStar& ns = NEARBY_STARS[d.star];
            if (ns.P <= 0 || Len(CamRel(c, b.pos)) < d.radius * 40) continue;   // 自分の軌道線が星を貫いて見えるのを防ぐ
            V3 center = StarBarycenter(ns, app.tDays);
            if (Len(CamRel(c, center)) > ns.a * ARCSEC * StarDistance(ns) * 300) continue;
            Color col = BodyColor(b, 0.8f);
            col.a = (int)i == app.target ? 150 : 70;
            rlCheckRenderBatchLimit((int)b.orbit.size() * 2);
            rlBegin(RL_LINES);
            rlColor4ub(col.r, col.g, col.b, col.a);
            for (size_t k = 0; k + 1 < b.orbit.size(); k++) {
                Vector3 p0 = ToF(CamRel(c, center) + b.orbit[k]), p1 = ToF(CamRel(c, center) + b.orbit[k + 1]);
                rlVertex3f(p0.x, p0.y, p0.z);
                rlVertex3f(p1.x, p1.y, p1.z);
            }
            rlEnd();
            continue;
        }
        if (d.parent < 0 || !b.active) continue;   // データの期間の外（打ち上げ前の人工衛星など）は軌道線も描かない
        V3 center = d.parent == 0 ? V3() : app.B[d.parent].pos;
        if (d.parent > 0 && Len(c.pos - center) > d.moonA * 60) continue;  // 衛星の軌道は近くでだけ表示
        if (Len(c.pos - b.pos) < d.radius * 40) continue;  // 自分の軌道線が天体を貫いて見えるのを防ぐ
        Color col = BodyColor(b, 0.8f);
        col.a = (int)i == app.target ? 150 : (d.parent == 0 ? 70 : 55);
        rlCheckRenderBatchLimit((int)b.orbit.size() * 2);
        rlBegin(RL_LINES);
        rlColor4ub(col.r, col.g, col.b, col.a);
        for (size_t k = 0; k + 1 < b.orbit.size(); k++) {
            Vector3 p0 = ToF(CamRel(c, center) + b.orbit[k]), p1 = ToF(CamRel(c, center) + b.orbit[k + 1]);
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

// ターゲットのローバーが走った道（着地点からの走行の記録。地表の 3 m 上。走ってきた部分は明るく、これから走る部分は暗く）。
// 地表の上の線なので天体の後に描き、天体の向こう側（地平線の下）になる所は描かない
static void DrawRoverPath(App& app) {
    if (app.target < 0 || app.relU > 0.02) return;
    const Body& b = app.B[app.target];
    if (b.d.lander < 0 || !b.active) return;
    const LanderDef& L = LANDERS[b.d.lander];
    int k = LanderWorld(app.B, b.d.lander);
    if (k < 0 || !L.drive) return;
    const Body& w = app.B[k];
    const Cam& c = app.cam;
    double now = (app.tDays - L.t0) * 86400;
    SetPassProjection(c, 1e-4, 1e5);
    rlDisableDepthTest();
    rlCheckRenderBatchLimit(L.nDrive * 2 + 2);
    rlBegin(RL_LINES);
    V3 prev;
    bool prevVis = false;
    for (int i = 0; i < L.nDrive; i++) {
        V3 f = LanderOnTrack(w, L, L.drive[i][0], w.spin);
        V3 pnt = w.pos + f + Norm(f) * 0.003;
        bool vis = Dot(f, c.pos - pnt) > 0;   // 天体の手前の側
        if (i > 0 && vis && prevVis) {
            rlColor4ub(255, 196, 120, (unsigned char)(L.drive[i][0] <= now ? 200 : 70));
            Vector3 p0 = ToF(prev - c.pos), p1 = ToF(pnt - c.pos);
            rlVertex3f(p0.x, p0.y, p0.z);
            rlVertex3f(p1.x, p1.y, p1.z);
        }
        prev = pnt;
        prevVis = vis;
    }
    rlEnd();
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
// wide: 標高が 1 m 単位で -32768 m から（地図の標高の形式 2）
// 地形の手続きの起伏に使う画像を CPU 側に持つ（n = 3: RGB, n = 2: R と A）
static void SetAux(Terrain& T, EmbeddedFile f, const char* ext, int n) {
    Image img = LoadImageFromMemory(ext, f.data, (int)f.size);
    if (!img.data) return;
    Color* px = LoadImageColors(img);
    T.aw = img.width;
    T.ah = img.height;
    T.an = n;
    T.aux.resize((size_t)T.aw * T.ah * n);
    for (size_t i = 0; i < (size_t)T.aw * T.ah; i++) {
        T.aux[i * n] = px[i].r;
        if (n == 3) T.aux[i * n + 1] = px[i].g, T.aux[i * n + 2] = px[i].b;
        else T.aux[i * n + 1] = px[i].a;
    }
    UnloadImageColors(px);
    UnloadImage(img);
}

static Texture2D LoadEmbeddedTexture(EmbeddedFile f, const char* ext, int terrainBody = -1, App* app = nullptr, bool wide = false) {
    Image img = LoadImageFromMemory(ext, f.data, (int)f.size);
    if (!img.data) return Texture2D{};
    if (app && terrainBody >= 0 && app->B[terrainBody].terrain < 0) {   // 標高の画像: 同じ値を CPU 側にも持つ（地球は R、月・火星は上位バイト R と下位バイト A）
        Terrain T;
        T.w = img.width;
        T.h = img.height;
        T.earth = terrainBody == B_EARTH;
        T.body = terrainBody;
        T.wide = wide;
        Color* px = LoadImageColors(img);
        T.v.resize((size_t)T.w * T.h);
        for (size_t k = 0; k < T.v.size(); k++) T.v[k] = T.earth ? px[k].r : (unsigned short)(px[k].r * 256 + px[k].a);
        UnloadImageColors(px);
        T.lo = 1e9;
        T.hi = -1e9;
        for (size_t k = 0; k < T.v.size(); k++) {
            double h = T.Km((int)k);
            T.lo = std::min(T.lo, h);
            T.hi = std::max(T.hi, h);
        }
        Body& b = app->B[terrainBody];
        b.terrain = (int)g_terrain.size();
        b.outer = std::max(b.outer, b.d.radius + T.hi);
        g_terrain.push_back(std::move(T));
    }
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

// 環の半径方向の不透明度の表。土星は観測データの画像、ほかは帯の τ から作る（細い環は画素の幅に薄めて足し、
// 遠くでは縮小画像で平均されるようにする）
static Texture2D LoadRingTexture(const RingSet& rs) {
    Texture2D t{};
    if (rs.bands.empty()) {
        EmbeddedFile f = SaturnRingsPng();
        Image img = LoadImageFromMemory(".png", f.data, (int)f.size);
        if (!img.data) return t;
        t = LoadTextureFromImage(img);
        UnloadImage(img);
        SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(t, TEXTURE_WRAP_CLAMP);
        return t;
    }
    const int N = 4096;
    std::vector<double> tau(N, 0.0);
    double px = (rs.outer - rs.inner) / N;
    for (const RingBand& b : rs.bands) {
        double a0 = b.r - b.w / 2, a1 = b.r + b.w / 2;
        int k0 = std::max(0, (int)std::floor((a0 - rs.inner) / px)), k1 = std::min(N - 1, (int)std::floor((a1 - rs.inner) / px));
        for (int k = k0; k <= k1; k++) {
            double p0 = rs.inner + k * px, p1 = p0 + px;
            double overlap = std::max(0.0, std::min(a1, p1) - std::max(a0, p0));
            tau[k] += b.tau * overlap / px;
        }
    }
    Image img = GenImageColor(N, 1, BLACK);
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE);
    unsigned char* pix = (unsigned char*)img.data;
    for (int k = 0; k < N; k++) pix[k] = (unsigned char)std::lround(255 * (1 - std::exp(-tau[k] * rs.gain)));
    t = LoadTextureFromImage(img);
    UnloadImage(img);
    GenTextureMipmaps(&t);
    SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR);
    SetTextureWrap(t, TEXTURE_WRAP_CLAMP);
    return t;
}

// 環のシェーダの値: x 表の内側 km, y 表の幅 km, z 海王星のアダムズ環の弧の位置 rad（弧が無ければ -1）
static void SetRingInfo(App& app, const RingSet& rs) {
    float arc = -1;
    if (rs.arcs) arc = (float)(std::fmod(820.1185 * app.tDays, 360.0) * DEG);   // 弧は環と同じ周期で回る（位置は目安）
    if (arc < -0.5f && rs.arcs) arc += (float)(2 * PI);
    float info[4] = {(float)rs.inner, (float)(rs.outer - rs.inner), arc, rs.thin ? 1.0f : 0.0f};
    PlanetUniform(app, app.locRingInfo, info, SHADER_UNIFORM_VEC4);
}

// 細かい標高 g_detail[k]（0 地球・1 月・2 火星）: 区画の索引を読み、窓のテクスチャを作る（ブラウザ版はファイルが届いてから）。
// "EDT1" は地球の高さコードの PNG（tools/make_earth_detail.py）、"EDT2" は 16 bit の予測の残差を deflate したもの（make_body_detail.py）。
// f2: 2 つに分けたファイルの後ろ半分（火星。両方が揃ってから読む）
static void LoadDetail(App& app, int k, EmbeddedFile f, EmbeddedFile f2 = {nullptr, 0}, bool parts2 = false) {
    DetailMap& D = g_detail[k];
    int body = k == 0 ? B_EARTH : k == 1 ? B_MOON : B_MARS;
    if (D.file || !f.data || f.size < 16 || (parts2 && !f2.data) || app.B[body].terrain < 0) return;
    int fmt = std::memcmp(f.data, "EDT1", 4) == 0 ? 1 : std::memcmp(f.data, "EDT2", 4) == 0 ? 2 : 0;
    if (!fmt) return;
    std::memcpy(&D.cols, f.data + 4, 4);
    std::memcpy(&D.rows, f.data + 8, 4);
    std::memcpy(&D.tile, f.data + 12, 4);
    int n = D.cols * D.rows;
    if (n <= 0 || D.tile <= 0 || D.cols * D.tile < DetailMap::N || D.rows * D.tile < DetailMap::N || f.size < 16 + 8 * (size_t)n) return;
    D.pos.resize(n);
    D.len.resize(n);
    for (int i = 0; i < n; i++) {
        std::memcpy(&D.pos[i], f.data + 16 + 8 * i, 4);
        std::memcpy(&D.len[i], f.data + 20 + 8 * i, 4);
        size_t a = D.pos[i], b = a + D.len[i];   // 区画は 2 つのファイルにまたがらない
        if (b > f.size + f2.size || (a < f.size && b > f.size)) return;
    }
    D.fmt = fmt;
    D.W = D.cols * D.tile;
    D.H = D.rows * D.tile;
    D.file = f.data;
    D.file2 = f2.data;
    D.split = f.size;
    D.win.assign((size_t)DetailMap::N * DetailMap::N, fmt == 1 ? 127 : 20000);
    std::vector<unsigned char> blank((size_t)DetailMap::N * DetailMap::N * fmt, 0);
    Image img = {blank.data(), DetailMap::N, DetailMap::N, 1, fmt == 1 ? PIXELFORMAT_UNCOMPRESSED_GRAYSCALE : PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA};
    app.detailTex[k] = LoadTextureFromImage(img);
    GenTextureMipmaps(&app.detailTex[k]);
    SetTextureFilter(app.detailTex[k], TEXTURE_FILTER_TRILINEAR);
    SetTextureWrap(app.detailTex[k], TEXTURE_WRAP_CLAMP);
    Terrain& T = g_terrain[app.B[body].terrain];   // 細かいデータの山頂は全球の標高より高い（細かい起伏の分も見込む）
    T.hi = k == 0 ? std::max(T.hi, 9.4) : T.hi + 1.0;
    app.B[body].outer = std::max(app.B[body].outer, app.B[body].d.radius + T.hi);
}

// 区画 i を展開する（tile x tile の値。陸の無い区画（地球）は空）
static std::vector<unsigned short> DecodeDetailTile(const DetailMap& D, int i) {
    std::vector<unsigned short> v;
    size_t n = (size_t)D.tile * D.tile;
    if (!D.len[i]) return v;
    if (D.fmt == 1) {
        Image img = LoadImageFromMemory(".png", D.Tile(i), (int)D.len[i]);
        if (img.data && img.width == D.tile && img.height == D.tile && img.format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE)
            v.assign((unsigned char*)img.data, (unsigned char*)img.data + n);
        UnloadImage(img);
        return v;
    }
    if (D.len[i] < 2) return v;
    int size = 0;
    unsigned char* z = DecompressData(D.Tile(i) + 2, (int)D.len[i] - 2, &size);
    if (z && (size_t)size == 2 * n) {
        v.resize(n);
        unsigned short first;
        std::memcpy(&first, D.Tile(i), 2);
        int t = D.tile;
        for (int y = 0; y < t; y++)
            for (int x = 0; x < t; x++) {
                size_t k = (size_t)y * t + x;
                int zz = z[k] << 8 | z[n + k], r = zz & 1 ? -(zz + 1) / 2 : zz / 2, p;
                if (!x && !y) p = first;
                else if (!y) p = v[k - 1];
                else if (!x) p = v[k - t];
                else {
                    int a = v[k - 1], b = v[k - t], c = v[k - t - 1];
                    p = std::max(std::min(a, b), std::min(std::max(a, b), a + b - c));   // 3 つの中央値
                }
                v[k] = (unsigned short)(p + r);
            }
    }
    if (z) MemFree(z);
    return v;
}

// 窓をカメラの真下（緯度 lat・経度 lon rad）に合わせる。中心から窓の 1/4 以上ずれたら区画から組み直して GPU に載せる
static void UpdateDetail(App& app, int k, double lat, double lon) {
    DetailMap& D = g_detail[k];
    const int N = DetailMap::N;
    if (!D.file || !app.detailTex[k].id) return;
    int x0 = (int)std::lround((lon / (2 * PI) + 0.5) * D.W - 0.5 - N / 2);
    int y0 = std::max(0, std::min(D.H - N, (int)std::lround((0.5 - lat / PI) * D.H - 0.5 - N / 2)));
    x0 = ((x0 % D.W) + D.W) % D.W;
    int ddx = x0 - D.x0;
    ddx -= D.W * (int)std::floor((double)ddx / D.W + 0.5);
    if (D.active && std::abs(ddx) <= N / 4 && std::abs(y0 - D.y0) <= N / 4) return;
    // 展開した区画（最近使ったものだけ残す）
    static std::map<int, std::vector<unsigned short>> cache;
    static std::map<int, unsigned> used;
    static unsigned stamp = 0;
    static int cacheOf = -1;
    if (cacheOf != k) cache.clear(), used.clear(), cacheOf = k;
    stamp++;
    auto tileData = [&](int i) -> const unsigned short* {
        used[i] = stamp;
        auto it = cache.find(i);
        if (it == cache.end()) it = cache.emplace(i, DecodeDetailTile(D, i)).first;
        return it->second.empty() ? nullptr : it->second.data();
    };
    unsigned short sea = D.fmt == 1 ? 127 : 20000;   // 地球の陸の無い区画は海面
    for (int j = 0; j < N; j++) {
        int gy = y0 + j, tr = gy / D.tile, py = gy % D.tile;
        unsigned short* row = &D.win[(size_t)j * N];
        for (int i = 0; i < N;) {
            int gx = (x0 + i) % D.W, tc = gx / D.tile, px = gx % D.tile;
            int run = std::min(N - i, D.tile - px);
            const unsigned short* t = tileData(tr * D.cols + tc);
            if (t) std::memcpy(row + i, t + (size_t)py * D.tile + px, run * sizeof(unsigned short));
            else std::fill(row + i, row + i + run, sea);
            i += run;
        }
    }
    for (auto it = used.begin(); it != used.end();) {   // 今の窓で使わなかった区画は、多ければ捨てる
        if (cache.size() > 48 && it->second != stamp) {
            cache.erase(it->first);
            it = used.erase(it);
        } else ++it;
    }
    D.x0 = x0;
    D.y0 = y0;
    D.active = true;
    if (D.fmt == 1) {
        std::vector<unsigned char> px(D.win.begin(), D.win.end());
        UpdateTexture(app.detailTex[k], px.data());
        GenTextureMipmaps(&app.detailTex[k]);
        return;
    }
    // 16 bit の値は上位・下位バイトに分けて載せるので、縮小画像は GPU に作らせず（バイトごとに丸めると上位バイトの丸めで
    // 最大 64 m ずれる）、値のまま 2x2 を平均して作り、すべての段をまとめて載せ直す
    std::vector<unsigned char> px;
    std::vector<unsigned> lv(D.win.begin(), D.win.end());
    int levels = 0;
    for (int n = N;; n /= 2) {
        for (unsigned v : lv) px.push_back((unsigned char)(v >> 8)), px.push_back((unsigned char)(v & 255));
        levels++;
        if (n == 1) break;
        std::vector<unsigned> next((size_t)(n / 2) * (n / 2));
        for (int y = 0; y < n / 2; y++)
            for (int x = 0; x < n / 2; x++) {
                size_t a = (size_t)(2 * y) * n + 2 * x;
                next[(size_t)y * (n / 2) + x] = (lv[a] + lv[a + 1] + lv[a + n] + lv[a + n + 1] + 2) / 4;
            }
        lv.swap(next);
    }
    UnloadTexture(app.detailTex[k]);
    Image img = {px.data(), N, N, levels, PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA};
    app.detailTex[k] = LoadTextureFromImage(img);
    SetTextureFilter(app.detailTex[k], TEXTURE_FILTER_TRILINEAR);
    SetTextureWrap(app.detailTex[k], TEXTURE_WRAP_CLAMP);
}

// 自機の真下に、近くの天体（半径の 8 倍まで）の細かい標高の窓を合わせる。描く前に、高度・衝突の計算にも同じ窓を
// 使えるよう、毎フレーム天体を動かした後に呼ぶ
static void UpdateDetailWindows(App& app) {
    V3 ship = ShipWorld(app);
    for (int k = 0; k < 3; k++) {
        const Body& b = app.B[k == 0 ? B_EARTH : k == 1 ? B_MOON : B_MARS];
        V3 rel = ship - b.pos;
        double dist = Len(rel);
        if (!g_detail[k].file || dist > b.d.radius * 8 || dist <= 0) continue;
        double cw = std::cos(b.spin), sw = std::sin(b.spin);
        V3 X = b.ex * cw - b.ez * sw, Z = b.ex * sw + b.ez * cw, up = rel * (1.0 / dist);
        V3 o(Dot(up, X), Dot(up, b.pole), Dot(up, Z));   // 真下の向き（本体の座標系）
        UpdateDetail(app, k, MapLatDeg(b.d, o) * DEG, std::atan2(-o.z, o.x));
    }
}

// 地形の格子・地球の細かい標高を使わない状態に戻す（同じシェーダで描くほかの天体・環のため）
static void TerrainOff(App& app) {
    float a[4] = {0, 0, 0, 0};
    PlanetUniform(app, app.locTerrA, a, SHADER_UNIFORM_VEC4);
    PlanetUniform(app, app.locDetB, a, SHADER_UNIFORM_VEC4);
    if (app.mat.maps) app.mat.maps[MATERIAL_MAP_OCCLUSION].texture = Texture2D{};
}

// 標高データのある天体（地球・月・火星）の地表近く: カメラの真下を中心に、近くほど細かい格子を地平線の先まで敷き、
// 標高の分だけ持ち上げて描く（等倍）。範囲は、どこの地表も最低の高さより低くはないので、最高の地表がカメラから見て
// 最低の高さの球の陰に入るまで。その先の球はいつも地平線の向こうで見えないので、地形を描く間は球を描かない
// （球の見えない部分のシェーダを走らせないように）
static void DrawTerrain(App& app, const Body& b, V3 rel, double dist, double alt, double s, int texW, int texH) {
    const Terrain& T = g_terrain[b.terrain];
    const BodyDef& d = b.d;
    double e = 1 - d.flattening, a = d.radius;
    double cw = std::cos(b.spin), sw = std::sin(b.spin);
    V3 X = b.ex * cw - b.ez * sw, Z = b.ex * sw + b.ez * cw;
    V3 up = rel * (-1.0 / dist);
    V3 c = Norm(V3(Dot(up, X), Dot(up, b.pole) / e, Dot(up, Z)));   // 真下の向き（球面の点）
    double rlo = a * e + T.lo, rhi = a + T.hi;
    double thMax = std::acos(std::min(1.0, rlo / dist)) + std::acos(std::min(1.0, rlo / rhi));
    thMax = std::min(thMax * 1.02 + 1e-5, PI * 0.95);
    double thA = std::max(1e-9, std::min(0.5 * std::max(alt, 0.001) / a, thMax * 0.02));
    V3 ref = std::fabs(c.y) < 0.9999 ? V3(0, 1, 0) : V3(1, 0, 0);   // 格子の φ = 0 は東（極のすぐ近くだけ別の向き）
    V3 e1 = Norm(Cross(ref, c));
    double lnK = std::log(thMax / thA + 1);
    // 輪の θ: 基本は θa (exp(u lnK) − 1)（u に一様。近いほど細かい）。ただし地平線の近くでは輪が画面の縦に詰まり、細い
    // 三角形の縁の画素を何度も塗るのが重い（地球・火星の地表のシェーダは重い）ので、画面で TERRAIN_PX 画素に 1 本までに
    // 間引く（間隔は基本の 4 倍まで、かつ標高の半画素まで。それより粗くすると、カメラが動いて輪が地表の上を滑る時に
    // 間の山を拾ったり外したりして遠くの起伏が波打つ）。使わなかった輪は外の端に重ねる（面積 0 で塗らない）。画面の縦の
    // 間隔はカメラから見た角度 α（半径 a の球の上の点）の変化で見る。真下の近くで 1 周の頂点が画面で詰まる輪も、同じ
    // 間隔まで間引く
    // 地球・月・火星: 細かい標高の窓（UpdateDetailWindows で自機の真下に合わせてある）。輪の間隔もその 1 画素に合わせる
    int dk = DetailIndex(T.body);
    bool det = dk >= 0 && g_detail[dk].active;
    const double TERRAIN_PX = 32, texel = PI / (det ? g_detail[dk].H : texH) * 0.5;
    static float ring[4 * 61], step[4 * 61];
    {
        const int NS = 2048;
        static double cum[NS + 1];
        auto dens = [&](int k) {   // u あたりの輪の本数（基本は輪の数）
            double u = (double)k / NS, th = thA * (std::exp(u * lnK) - 1);
            double x = a * std::sin(th), y = dist - a * std::cos(th);
            double dAlpha = std::fabs(a * (a - dist * std::cos(th))) / std::max(x * x + y * y, 1e-12);   // dα/dθ
            double dth = thA * lnK * std::exp(u * lnK), screen = app.cam.focal / TERRAIN_PX * dAlpha * dth;
            return std::min(1.0, std::max({screen / TERRAIN_RINGS, 0.25, dth / texel / TERRAIN_RINGS})) * TERRAIN_RINGS;
        };
        cum[0] = 0;
        double prev = dens(0);
        for (int k = 1; k <= NS; k++) {
            double cur = dens(k);
            cum[k] = cum[k - 1] + 0.5 * (prev + cur) / NS;
            prev = cur;
        }
        int k = 0;
        for (int i = 0; i <= TERRAIN_RINGS; i++) {
            if (i >= cum[NS]) { ring[i] = (float)thMax; continue; }
            while (k < NS && cum[k + 1] < i) k++;
            double u = (k + (i - cum[k]) / std::max(cum[k + 1] - cum[k], 1e-30)) / NS;
            ring[i] = (float)(thA * (std::exp(u * lnK) - 1));
        }
        for (int i = 0; i <= TERRAIN_RINGS; i++) {
            double th = ring[i], x = a * std::sin(th), y = dist - a * std::cos(th);
            double px = 2 * PI * x / TERRAIN_SLICES / std::max(std::sqrt(x * x + y * y), 1e-12) * app.cam.focal;   // 頂点 1 つ分の画素
            int st = 1;
            while (st < TERRAIN_SLICES / 8 && px * st * 2 <= TERRAIN_PX && 2 * PI * std::sin(th) / TERRAIN_SLICES * st * 2 <= texel) st *= 2;
            step[i] = (float)st;
        }
    }
    float ta[4] = {1, (float)thA, (float)lnK, T.earth ? 1.0f : T.wide ? 3.0f : 2.0f};
    float tb[4] = {(float)c.x, (float)c.y, (float)c.z, (float)e};
    // 高さは半径の 6〜8 倍の間で 0 から等倍へなめらかに増やす（その先は球で描くので、切り替わる時に形が跳ばない。
    // 凹凸の大きいベスタでは 8 倍の所でも山の高さが数画素ある）
    double grow = std::max(0.0, std::min(1.0, (8.0 - dist / a) / 2.0));
    float tc[4] = {(float)e1.x, (float)e1.y, (float)e1.z, (float)(grow * grow * (3 - 2 * grow) / (a * 1000))};
    float td[4] = {(float)texW, (float)texH, (float)TERRAIN_RINGS, (float)TERRAIN_SLICES};
    PlanetUniform(app, app.locTerrA, ta, SHADER_UNIFORM_VEC4);
    PlanetUniform(app, app.locTerrB, tb, SHADER_UNIFORM_VEC4);
    PlanetUniform(app, app.locTerrC, tc, SHADER_UNIFORM_VEC4);
    PlanetUniform(app, app.locTerrD, td, SHADER_UNIFORM_VEC4);
    PlanetUniform(app, app.locTerrRing, ring, SHADER_UNIFORM_VEC4, 61);
    PlanetUniform(app, app.locTerrStep, step, SHADER_UNIFORM_VEC4, 61);
    if (det) {
        const DetailMap& D = g_detail[dk];
        float da[4] = {(float)D.x0, (float)D.y0, (float)DetailMap::N, (float)DetailMap::MARGIN};
        float db[4] = {(float)D.W, (float)D.H, 1, (float)D.fmt};
        PlanetUniform(app, app.locDetA, da, SHADER_UNIFORM_VEC4);
        PlanetUniform(app, app.locDetB, db, SHADER_UNIFORM_VEC4);
        app.mat.maps[MATERIAL_MAP_OCCLUSION].texture = app.detailTex[dk];
    }
    // データより細かい手続きの起伏: 半径 m・全球の標高の周波数・種類（1 月, 2 地図のある天体）・細かい標高の周波数
    float te[4] = {(float)(d.radius * 1000), (float)(texH / (2 * PI)), (float)T.bumps, (float)(det ? g_detail[dk].H / (2 * PI) : 0)};
    PlanetUniform(app, app.locTerrE, te, SHADER_UNIFORM_VEC4);
    // 原点は真下の楕円体の上の点（カメラからの位置を倍精度で求める）、軸は本体の座標系、長さは赤道半径を 1 とする
    V3 o = rel + (X * c.x + b.pole * (c.y * e) + Z * c.z) * a;
    Matrix m = BodyMatrix(b, o * s, a * s, a * s, b.spin);
    DrawPlanetMesh(app, app.terrainMesh, m);
    TerrainOff(app);
}

static void LoadEarthTextures(App& app) {
    app.earthColor = LoadEmbeddedTexture(EarthColorJpg(), ".jpg");
    app.earthData = LoadEmbeddedTexture(EarthDataPng(), ".png", B_EARTH, &app);
    app.earthTex = app.earthColor.id && app.earthData.id;
    app.earthMonths = LoadEmbeddedTexture(EarthMonthsJpg(), ".jpg");
    app.earthClouds = LoadEmbeddedTexture(EarthCloudsJpg(), ".jpg");
    if (!app.earthTex) {
        TraceLog(LOG_WARNING, "Earth textures unavailable, using procedural surface");
        return;
    }
    LoadDetail(app, 0, EarthDetailBin());
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
        int tb = -1;
        for (size_t i = 0; i < app.B.size(); i++)
            if (m.name == app.B[i].d.en && app.B[i].d.kind == KIND_ROCKY) tb = (int)i;
        if (height) m.data = LoadEmbeddedTexture(data, ".png", tb, &app, height == 2);
        m.height = m.data.id != 0;
        m.wide = height == 2;
        m.liquid = pad == 1;   // 撮影範囲の画像のアルファが液体の湖・海（タイタン）
        if (m.height && tb >= 0 && app.B[tb].terrain >= 0) {   // 地形の格子の手続きの起伏を CPU 側でも同じに求めるため
            Terrain& T = g_terrain[app.B[tb].terrain];
            T.bumps = 2;
            T.style = app.B[tb].d.style;
            T.pattern = kind == 2;
            T.liquid = m.liquid;
            SetAux(T, mask, ".png", 2);
        }
        if (!m.color.id || !m.mask.id) continue;
        for (Body& b : app.B)
            if (m.name == b.d.en && (b.d.kind == KIND_ROCKY || b.d.kind == KIND_GAS)) b.map = (int)app.maps.size();
        app.maps.push_back(m);
    }
    LoadDetail(app, 2, MarsDetailBin(), MarsDetail2Bin(), true);
    if (MarsTiPng().data) app.marsTi = LoadEmbeddedTexture(MarsTiPng(), ".png");
}

static void LoadMoonTextures(App& app) {
    app.moonColor = LoadEmbeddedTexture(MoonColorJpg(), ".jpg");
    app.moonData = LoadEmbeddedTexture(MoonDataPng(), ".png", B_MOON, &app);
    app.moonTex = app.moonColor.id && app.moonData.id;
    if (!app.moonTex) {
        TraceLog(LOG_WARNING, "Moon textures unavailable, using procedural surface");
        return;
    }
    if (MoonRockPng().data) app.moonRock = LoadEmbeddedTexture(MoonRockPng(), ".png");
    // 地形の格子のクレーター（海は少ない）を CPU 側でも同じに求めるため、地表の色を持つ
    if (app.B[B_MOON].terrain >= 0) {
        Terrain& T = g_terrain[app.B[B_MOON].terrain];
        T.bumps = 1;
        SetAux(T, MoonColorJpg(), ".jpg", 3);
    }
    LoadDetail(app, 1, MoonDetailBin());
}

// 天の川の光: 赤道座標の全天画像を、カメラを中心とする球の内側に薄く貼る（星より先に描く）。
// 画像は中央が赤経 0h で、左へ行くほど赤経が増える
// 光速に近い速さで動く自機から見た星（特殊相対論）。v は進む向き、beta は見え方に使う速さ / 光速。
// 光行差で星は前方へ集まり（cos θ′ = (cos θ + β) / (1 + β cos θ)）、ドップラー係数 D = 1 / (γ(1 − β cos θ′)) が 1 より大きい
// 前方は青く明るく、小さい後方は赤く暗くなる（明るさは D³ に比例。色は青・赤へ寄せる見た目の近似）
struct Relativity { bool on = false; V3 v; double beta = 0, gamma = 1; };

// 見え方の強さ relU を、今の速さ / 光速へ時間をかけて近づける。急停止（X）や下のエンジンへの切り替えで速さが一気に 0 に
// なっても、集まった星がゆっくりほどけ始め、約 1 秒で元に戻る（臨界減衰のばね。動き出しも収まりも滑らか）。
// 加速は滑らかなので、強まる側は少し遅れて追うだけ。超光速エンジン（架空）では 0 へ向かい、ワープ中は消す
static void UpdateRelativityView(App& app, double dt) {
    const Ship& s = app.ship;
    double sp = app.physics ? Len(s.vel) : std::fabs(s.speed);
    double target = app.warpT >= 0 || sp > C_KMS * 1.0001 ? 0.0 : std::min(1.0, sp / C_KMS);
    if (target > 0.005 && app.warpT < 0)
        app.relDir = app.physics ? s.vel * (1 / sp) : ShipFwd(s) * (s.speed >= 0 ? 1.0 : -1.0);
    const double w = 6.0;   // 固有角振動数 /秒（臨界減衰で 1 秒ほどで収まる）
    for (double left = dt; left > 0; left -= 1.0 / 240) {   // 長いフレームでも安定するよう細かく刻む
        double h = std::min(left, 1.0 / 240);
        app.relUV += (w * w * (target - app.relU) - 2 * w * app.relUV) * h;
        app.relU = std::max(0.0, std::min(1.0, app.relU + app.relUV * h));
    }
    if (app.warpT >= 0) app.relU = app.relUV = 0;   // ワープは自分の演出があるので消す
}

static Relativity GetRelativity(const App& app) {
    Relativity r;
    if (app.relU < 0.005) return r;
    r.on = true;
    r.v = app.relDir;
    // 光行差は光速のすぐ近くで急に強くなり、光速からブレーキをかけると最初の 1 秒ほどで星の集まりが一気に解けてしまう。
    // 見え方が速さに沿って少しずつ変わるよう、真横（90°）の星が見える向きが速さに比例して 90° から約 6°（光速の 99.5% の値）
    // まで動くように β を決める（cos θ′ = β）
    r.beta = std::cos((90.0 - (90.0 - 5.73) * app.relU) * DEG);
    r.gamma = 1 / std::sqrt(1 - r.beta * r.beta);
    return r;
}
static V3 Aberrate(const Relativity& r, V3 dir, double* D) {
    double ct = Dot(dir, r.v), ct2 = (ct + r.beta) / (1 + r.beta * ct);
    V3 perp = dir - r.v * ct;
    double pl = Len(perp);
    *D = 1 / (r.gamma * (1 - r.beta * ct2));
    return pl > 1e-12 ? r.v * ct2 + perp * (std::sqrt(std::max(0.0, 1 - ct2 * ct2)) / pl) : dir;
}
// 自機から見た天体の位置（カメラからの差）。光速に近い時は光行差で見える向きを前方へ寄せる（距離はそのまま）。
// 点・名前の印・探査機の模型に使う。惑星などの本体はシェーダで頂点ごとに寄せるので、本体の中心もこの位置に描かれる
static V3 ViewRel(const App& app, V3 world) {
    V3 r = CamRel(app.cam, world);
    Relativity rv = GetRelativity(app);
    double L = Len(r), D;
    return rv.on && L > 1e-9 ? Aberrate(rv, r * (1 / L), &D) * L : r;
}
static Color DopplerColor(Color col, double D) {
    double kb = std::min(0.85, std::max(0.0, (D - 1) * 0.5)), kr = std::min(1.0, std::max(0.0, (1 - D) * 1.5));
    auto mix = [](unsigned char a, double b, double k) { return (unsigned char)(a + (b - a) * k); };
    double bright = std::min(1.0, std::pow(D, 3.0));
    return {mix(mix(col.r, 150, kb), 255, kr), mix(mix(col.g, 190, kb), 90, kr), mix(mix(col.b, 255, kb), 60, kr),
            (unsigned char)(col.a * bright)};
}

static void DrawMilkyWay(const App& app) {
    if (!app.milkyWay.id || !app.showMilkyWay) return;
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
    Relativity rel = GetRelativity(app);   // 光速に近いと、天の川も星と同じく前方へ集まって青く（後方は赤く暗く）
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
        if (rel.on) {
            double D;
            p = Aberrate(rel, p * (1 / R), &D) * R;
            Color cc = DopplerColor({br, br, br, 255}, D);
            unsigned char a = cc.a;   // 明るさは色に掛ける（加算で重ねるので）
            rlColor4ub((unsigned char)(cc.r * a / 255), (unsigned char)(cc.g * a / 255), (unsigned char)(cc.b * a / 255), 255);
        }
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
        if (b.d.probe >= 0 || b.d.lander >= 0 || !b.active) continue;
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

// beta: レイリー散乱（火星は塵）, mie: 太陽のまわりの光の色, ozone: オゾンの吸収（地球だけ）。
// 地面の照らし方: ext は日光を遮る天頂方向の光学的厚さ（色ごと）、alb は散らす割合（単一散乱アルベド）、down は散らされた光の
// うち地面へ向かう割合。地球は晴れた日の目安で、レイリー散乱（光学的厚さ 650・550・450 nm で 0.05・0.10・0.22。空の色の beta は
// 見た目に合わせた値で、これより大きい）にエアロゾル 0.1（色によらない。散らす割合 0.9、前へ強く散らす）を足したもの。火星の塵は光の波長より大きな粒で、日光を色に
// よらずほぼ同じだけ遮り（光学的厚さ 0.5 は目安。季節と砂嵐で 0.3〜1 以上に変わる）、青い光を吸収する（アルベドは Tomasko
// ほか 1999 などの目安）。前へ強く散らすので地面へ向かう割合は 0.75 とする
struct SkyAtmosphere { int body; double top, H; Vector3 beta, mie, ozone, ext, alb; float down; };
static const SkyAtmosphere SKY_ATM[] = {
    {B_EARTH, 120, 8.5, {0.10f, 0.23f, 0.56f}, {1.0f, 0.95f, 0.9f}, {0.05f, 0.08f, 0.003f}, {0.15f, 0.20f, 0.32f}, {0.93f, 0.95f, 0.97f}, 0.6f},
    {B_MARS, 80, 11.1, {0.16f, 0.11f, 0.07f}, {0.45f, 0.65f, 1.0f}, {0, 0, 0}, {0.5f, 0.5f, 0.5f}, {0.97f, 0.92f, 0.80f}, 0.75f},   // 塵で昼は黄褐色、夕方は太陽のまわりが青白い
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
    for (const Body& v : app.B)   // 金星の雲・タイタンのもやの層の下: 空は覆われ、昼も夜も星は見えない
        if (const HazeWorld* hw = HazeWorldOf(v.d))
            if (Len(app.cam.pos - v.pos) < v.d.radius + hw->top) return 1;
    if (!CameraAtmosphere(app, &density, &up, &sunEl)) return 0;
    double light = std::max(0.0, std::min(1.0, (sunEl + 0.17) / 0.2));
    return std::min(1.0, density * 2.0) * light;
}

// 明るさの自動調整（露出）: 地表の近くでは、真下の地面に当たる光（直射 + 空の光。シェーダの sunTint・skyL と同じ式、太陽が
// 真上の時を 1）が弱いほど画面を明るくし、白い物が白く写るようにする（光の -0.7 乗、最大 2.2 倍）。場面の平均で決めると雪原が
// 灰色に写るので、光の強さで決める。目は明るい夕焼けの空にも合わせるので、太陽高度 2〜12° で弱めて日の入りで 1。宇宙からと夜は 1
static double ExposureTarget(const App& app) {
    const Cam& c = app.cam;
    int best = -1;
    double bestK = 1e300;
    for (size_t i = 0; i < app.B.size(); i++) {
        const BodyDef& d = app.B[i].d;
        if (d.kind != KIND_ROCKY && d.kind != KIND_EARTH) continue;
        double k = SurfaceDist(app.B[i], c.pos) / d.radius;
        if (k < bestK) { bestK = k; best = (int)i; }
    }
    if (best < 0 || bestK > 0.01 || HazeWorldOf(app.B[best].d)) return 1;
    const Body& b = app.B[best];
    V3 up = Norm(c.pos - b.pos);
    double el = Dot(up, Norm(app.B[HostStar(app.B, best)].pos - c.pos));
    double e = std::max(el, 0.0), illum = e;
    for (const SkyAtmosphere& a : SKY_ATM)
        if (a.body == best) {
            double m = std::min(1.0 / (e + 0.06), 7.0), ext[3] = {a.ext.x, a.ext.y, a.ext.z}, alb[3] = {a.alb.x, a.alb.y, a.alb.z};
            double w[3] = {0.3, 0.59, 0.11};
            illum = 0;
            for (int k = 0; k < 3; k++) {
                double tz = std::exp(-ext[k]), dz = a.down * alb[k] * (1 - tz) / tz;
                double sky = a.down * alb[k] * (1 - std::exp(-ext[k] * m)) / tz * e;
                illum += w[k] * (e * std::exp(-ext[k] * std::max(m - 1, 0.0)) + sky) / (1 + dz);
            }
        }
    double g = std::min(2.2, std::pow(std::max(illum, 0.1), -0.7));
    double day = std::max(0.0, std::min(1.0, (el - 0.035) / 0.17));
    double near = 1 - std::max(0.0, std::min(1.0, (bestK - 0.002) / 0.008));
    return 1 + (g - 1) * day * near;
}

// 露出の倍率を求めて、天体と自機のシェーダに渡す（3D を描く前に毎フレーム。シェーダの expo で掛ける）。空の明るさは見た目に
// 合わせた値で、明るい空にも目が合うので掛けない
static void UpdateExposure(App& app, float dt) {
    double t = ExposureTarget(app);
    static V3 last(1e300, 0, 0);   // ワープ・共有リンクなどで 1 フレームに 100 km 以上動いた時はすぐ合わせる
    if (Len(app.cam.pos - last) > 100) app.exposure = (float)t;
    else app.exposure += (float)((t - app.exposure) * std::min(1.0, dt * 1.5));   // 目の順応のようにゆっくり
    last = app.cam.pos;
    PlanetUniform(app, app.locExposure, &app.exposure, SHADER_UNIFORM_FLOAT);
    SetShaderValue(app.shipShader, app.locShipExposure, &app.exposure, SHADER_UNIFORM_FLOAT);
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

// ブラウザ版: 描くのに時間がかかる時は 3D の解像度を自動で下げる。ブラウザが画面を描き替える間隔 present（秒。GPU が
// 追いつかない時はこれが延びる）が 28 ms（約 36 fps）を超えたら 1 割ずつ下げる（0.5 倍、3D の縦 360 画素まで）。
// 下げて 1.2 秒たっても速くならなければ（GPU ではなく CPU で時間がかかっている）元に戻し、その重さのうちは下げない。
// 余裕のある状態（20 ms 未満）が続いたら 1 割ずつ戻す。戻した直後にまた下げた時は、次に戻すまでの待ちを倍にする
// （行ったり来たりしないように）。地図の読み込みなどの一時的な引っかかり（0.25 秒を超える間隔）は除く
static void UpdateAutoRes(App& app, double dt, double present) {
    static double since = 0, wait = 3, raised = -1, before = 0, cpuAvg = 0;
    static float trialFrom = -1;
    if (dt <= 0 || dt > 0.25 || present <= 0 || present > 0.25) return;
    since += dt;
    if (raised >= 0) raised += dt;
    if (cpuAvg > 0 && present < cpuAvg * 0.8) cpuAvg = 0;   // 軽くなった（場面が変わった）
    float lo = std::max(0.5f, std::min(1.0f, 360.0f / (std::max(1, GetScreenHeight()) * QUALITY_SCALE[app.quality])));
    if (trialFrom > 0) {   // 下げた効き目を見る
        if (since < 1.2) return;
        if (present > before * 0.97) {
            app.autoRes = trialFrom;
            cpuAvg = before;
        }
        trialFrom = -1;
        since = 0;
    } else if (present > 0.028 && since > 0.6 && app.autoRes > lo && !(cpuAvg > 0 && present < cpuAvg * 1.2)) {
        before = present;
        trialFrom = app.autoRes;
        app.autoRes = std::max(lo, app.autoRes * 0.9f);
        if (raised >= 0 && raised < 5) wait = std::min(wait * 2, 60.0);
        raised = -1;
        since = 0;
    } else if (present < 0.020 && since > wait && app.autoRes < 1) {
        app.autoRes = std::min(1.0f, app.autoRes / 0.9f);
        if (app.autoRes > 0.99f) app.autoRes = 1;
        raised = 0;
        since = 0;
    }
}

// 天体 i に影を落としうる天体（母天体・兄弟の衛星・自分の衛星）を最大 4 つシェーダへ渡す
static void SetOccluders(App& app, int i, double s) {
    const Body& b = app.B[i];
    float data[16] = {0};
    int n = 0;
    if (b.d.kind != KIND_STAR) {
        const Body& star = app.B[HostStar(app.B, i)];
        V3 L = Norm(star.pos - b.pos);
        double sunAng = star.d.radius / Len(star.pos - b.pos);
        struct Cand { int j; double k; };
        std::vector<Cand> cands;
        for (size_t j = 1; j < app.B.size(); j++) {
            const Body& o = app.B[j];
            if ((int)j == i || o.d.kind == KIND_STAR) continue;   // 太陽系の外の惑星の母天体は恒星（影を落とさない）
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
            V3 cp = CamRel(app.cam, o.pos) * s;   // 陰影は元の位置で計算する（光行差はシェーダで頂点にだけ掛ける）
            bool atm = o.d.kind == KIND_EARTH;   // 大気で赤い光が回り込む（月食）
            data[n * 4 + 0] = (float)cp.x; data[n * 4 + 1] = (float)cp.y; data[n * 4 + 2] = (float)cp.z;
            data[n * 4 + 3] = (float)(o.d.radius * s) * (atm ? -1.0f : 1.0f);
            n++;
        }
        float sa = (float)sunAng;
        PlanetUniform(app, app.locSunAng, &sa, SHADER_UNIFORM_FLOAT);
    }
    if (n > 0) PlanetUniform(app, app.locOcc, data, SHADER_UNIFORM_VEC4, n);
    PlanetUniform(app, app.locOccN, &n, SHADER_UNIFORM_INT);
}

// 雲（もや）の層の下の空の明るさ（色ごと）。高さ h km、太陽の高さ sinEl（sin）。地表と層の下端の間・層の中は対数で
// 補間する。表示は目が慣れた明るさとして比を平方根で縮め、2 倍にする。太陽の高さで昼と夜が変わる（光は空全体から
// 来るので、太陽が低くても明るさの落ち方はゆるやか）
static Vector3 HazeSkyColor(const HazeWorld& hw, double h, double sinEl) {
    double lnE = h < hw.base ? std::log(hw.eSurf) + std::log(hw.eBase / hw.eSurf) * h / hw.base
                             : std::log(hw.eBase) * (1 - std::min(1.0, (h - hw.base) / (hw.top - hw.base)));
    double t = std::max(0.0, std::min(1.0, (sinEl + 0.05) / 0.25));
    double D = std::min(1.0, 2.0 * std::sqrt(std::exp(lnE))) * t * t * (3 - 2 * t) * (0.35 + 0.65 * std::max(0.0, sinEl));
    double k = std::min(1.0, h / hw.top);
    return {(float)(D * (hw.tint[0] + (1 - hw.tint[0]) * k)), (float)(D * (hw.tint[1] + (1 - hw.tint[1]) * k)),
            (float)(D * (hw.tint[2] + (1 - hw.tint[2]) * k))};
}

static void DrawBodies(App& app) {
    const Cam& c = app.cam;
    // 光速に近い時は、天体の頂点ごとに光行差を掛ける（シェーダ）。天体全体を動かさないので、近くの大きな天体も正しく歪む
    Relativity rv = GetRelativity(app);
    float rb = rv.on ? (float)rv.beta : 0.0f;
    Vector3 rd = ToF(rv.on ? rv.v : V3(0, 0, -1));
    PlanetUniform(app, app.locRelBeta, &rb, SHADER_UNIFORM_FLOAT);
    PlanetUniform(app, app.locRelDir, &rd, SHADER_UNIFORM_VEC3);
    struct Item { int i; double dist; };
    std::vector<Item> items;
    for (size_t i = 0; i < app.B.size(); i++) {
        const Body& b = app.B[i];
        V3 rel = CamRel(c, b.pos);
        double dist = Len(rel);
        if (b.d.probe >= 0 || b.d.lander >= 0) continue;  // 探査機と着陸機は DrawProbes で描く
        if (!rv.on && Dot(rel, c.fwd) < -b.outer) continue;   // 光速に近いと後ろの天体も前方に見えるので省かない
        if (b.outer / dist * c.focal < 0.8) continue;  // サブピクセルは 2D の点で描く
        items.push_back({(int)i, dist});
    }
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.dist > b.dist; });

    for (const Item& it : items) {
        const Body& b = app.B[it.i];
        const BodyDef& d = b.d;
        V3 rel = CamRel(c, b.pos);
        double dist = it.dist;
        double s = 100.0 / dist;
        double rPol = d.radius * (1 - d.flattening);
        // near: 最も近い表面（環があればその外縁）より手前。near は視線方向の奥行きなので、画面の隅では距離の cos 倍になる分も見込む
        double alt = dist - SurfaceRadius(b, rel * (-1.0 / dist));
        double nearK = 0.9 * c.focal / std::sqrt(c.focal * c.focal + 0.25 * (c.W * c.W + c.H * c.H));
        double nearKm = (dist > b.outer && b.outer > d.radius) ? std::min(alt, dist - b.outer) * nearK : alt * std::min(0.5, nearK);
        const HazeWorld* hw = HazeWorldOf(d);
        bool under = hw && dist - d.radius < hw->top;   // 雲（もや）の層の上端より下
        if (under)   // 層の下からは、すぐ上の層の殻も切り取らない
            nearKm = std::min(nearKm, std::max(0.05, (std::max(d.radius + hw->top, dist + 1) * 0.9998 - dist) * nearK));
        nearKm = std::max(nearKm, dist * 1e-7);
        SetPassProjection(c, nearKm * s, (dist + b.outer) * s * 1.01);
        ClearDepth();

        V3 L = d.kind == KIND_STAR ? V3(0, 1, 0) : Norm(app.B[HostStar(app.B, it.i)].pos - b.pos);
        app.planetBig = d.radius / std::max(dist, d.radius) * c.focal > 30;   // 半径が 30 画素より大きい
        V3 Lobj(Dot(L, b.ex), Dot(L, b.pole), Dot(L, b.ez));
        Vector3 lf = ToF(L), lo = ToF(Lobj);
        Vector3 c1 = {d.c1.r, d.c1.g, d.c1.b}, c2 = {d.c2.r, d.c2.g, d.c2.b}, atm = {d.atm.r, d.atm.g, d.atm.b};
        // 大気の縁の光は外から見た時だけ（地表付近では地面全体が光ってしまう）
        float atmK = (float)std::min(1.0, std::max(0.0, alt / (d.radius * 0.05)));
        float params[4] = {(float)d.kind, d.bands, (float)(it.i * 7.31), d.atmStrength * atmK};
        if (d.kind == KIND_STAR) {   // 恒星: y は表面の描き方、w はフレア（整数部が通し番号、小数部が明るさ）
            int seq;
            double fl = FlareLevel(b, it.i, app.tDays, &seq);
            params[1] = (float)StarSurface(d);
            params[3] = fl > 0.01 ? (float)(seq % 4096) + (float)std::min(0.99, fl) : 0.0f;
        }
        PlanetUniform(app, app.locLight, &lf, SHADER_UNIFORM_VEC3);
        // 本体の描画では自転込みの座標系での太陽方向（環の影に使う）
        {
            double cw = std::cos(b.spin), sw = std::sin(b.spin);
            V3 X = b.ex * cw - b.ez * sw, Z = b.ex * sw + b.ez * cw;
            Vector3 ls = ToF(V3(Dot(L, X), Dot(L, b.pole), Dot(L, Z)));
            PlanetUniform(app, app.locLightObj, &ls, SHADER_UNIFORM_VEC3);
        }
        SetOccluders(app, it.i, s);
        {   // 太陽の黒点の活動（日付ごとの黒点数と周期の中の位置。ほかの恒星は既定の模様）
            float si[4] = {-1, 0, 0, 0};
            if (it.i == B_SUN) {
                double ssn, x;
                SunspotActivity(2000 + app.tDays / 365.25, &ssn, &x);
                si[0] = (float)(ssn / 200.0);
                si[1] = (float)(28.0 - 20.0 * x);   // シュペーラーの法則: 周期の初めは 30° 近く、終わりは 8° 近く
                si[2] = 7.0f;
            }
            PlanetUniform(app, app.locSunInfo, si, SHADER_UNIFORM_VEC4);
        }
        {   // 木星の衝突痕（シューメーカー・レビー第 9 彗星。1994 年 7 月）: 衝突から 1 時間で広がり、2 か月ほどで横に広がりながら
            // 数か月かけて薄れる（見た目の目安）
            float sc[80] = {0}, info[4] = {0, 0, 0, 0};
            int n = 0;
            if (it.i == B_JUPITER && (int)app.sl9Dirs.size() == SL9_COUNT)
                for (int k = 0; k < SL9_COUNT && n < 20; k++) {
                    double age = app.tDays - SL9[k].t;
                    if (age < 0 || age > 400) continue;
                    double r = SL9[k].size * std::min(1.0, age * 24 + 0.05) * (1 + age / 60);
                    sc[n * 4 + 0] = (float)app.sl9Dirs[k].x; sc[n * 4 + 1] = (float)app.sl9Dirs[k].y;
                    sc[n * 4 + 2] = (float)app.sl9Dirs[k].z; sc[n * 4 + 3] = (float)r;
                    n++;
                }
            if (n > 0) {
                double age0 = app.tDays - SL9[0].t;
                info[0] = (float)n;
                info[1] = (float)(0.85 * std::exp(-std::max(0.0, age0) / 120));
                PlanetUniform(app, app.locScars, sc, SHADER_UNIFORM_VEC4, 20);
            }
            PlanetUniform(app, app.locScarCount, info, SHADER_UNIFORM_VEC4);
        }
        PlanetUniform(app, app.locC1, &c1, SHADER_UNIFORM_VEC3);
        PlanetUniform(app, app.locC2, &c2, SHADER_UNIFORM_VEC3);
        PlanetUniform(app, app.locAtm, &atm, SHADER_UNIFORM_VEC3);
        PlanetUniform(app, app.locParams, params, SHADER_UNIFORM_VEC4);
        float mapLatK = (float)((1 - d.flattening) * MapLatScale(d) - 1);   // シェーダの mapDir
        PlanetUniform(app, app.locMapLatK, &mapLatK, SHADER_UNIFORM_FLOAT);
        float oppB0 = d.atmStrength > 0 ? 0.0f : 1.0f;   // 衝効果は大気の無い天体だけ（シェーダの oppB0）
        PlanetUniform(app, app.locOppB0, &oppB0, SHADER_UNIFORM_FLOAT);
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
            PlanetUniform(app, app.locMapInfo2, info2, SHADER_UNIFORM_VEC4);
        } else if (d.style == S_MOON && app.moonTex) {
            style = S_MOON_TEX;
            app.mat.maps[MATERIAL_MAP_DIFFUSE].texture = app.moonColor;
            app.mat.maps[MATERIAL_MAP_SPECULAR].texture = app.moonData;
            // 岩の多さ（texture2。mapInfo2.w が 1 の時だけ使う）
            app.mat.maps[MATERIAL_MAP_NORMAL].texture = app.moonRock;
            float info2[4] = {0, 0, 0, app.moonRock.id ? 1.0f : 0.0f};
            PlanetUniform(app, app.locMapInfo2, info2, SHADER_UNIFORM_VEC4);
        } else if (b.map >= 0) {
            const App::BodyMap& m = app.maps[b.map];
            style = S_MAPPED;
            app.mat.maps[MATERIAL_MAP_DIFFUSE].texture = m.color;
            app.mat.maps[MATERIAL_MAP_SPECULAR].texture = m.height ? m.data : m.color;
            app.mat.maps[MATERIAL_MAP_NORMAL].texture = m.mask;
            float info[4] = {1.0f / std::max(0.05f, m.mean), (float)m.kind, m.height ? (m.wide ? 2.0f : 1.0f) : 0.0f, (float)d.radius};
            // 木星の地図は大赤斑が経度 0° にあるので、大赤斑の今の西経（体系 II）だけ回す
            float shift = it.i == B_JUPITER ? (float)(GreatRedSpotLon(app.tDays) * DEG) : 0.0f;
            // 火星: 熱慣性（texture3。mapInfo2.w が 1 の時だけ使う）
            bool ti = it.i == B_MARS && app.marsTi.id;
            if (ti) app.mat.maps[MATERIAL_MAP_ROUGHNESS].texture = app.marsTi;
            float info2[4] = {(float)d.style, shift, m.liquid ? 1.0f : 0.0f, ti ? 1.0f : 0.0f};
            PlanetUniform(app, app.locMapInfo, info, SHADER_UNIFORM_VEC4);
            PlanetUniform(app, app.locMapInfo2, info2, SHADER_UNIFORM_VEC4);
        }
        float detail[4] = {(float)style, (float)(QUALITY_DETAIL[app.quality] / (c.focal * d.radius * s)), (float)app.tDays, cloudK};
        PlanetUniform(app, app.locDetail, detail, SHADER_UNIFORM_VEC4);
        // 大気のある天体: 日光の色と、カメラが同じ大気の中にいる時のかすみ
        {
            Vector3 beta = {0, 0, 0}, mie = {0, 0, 0}, ozone = {0, 0, 0}, up = {0, 1, 0}, alb = {0, 0, 0};
            float ext[4] = {0, 0, 0, 0};
            float haze[4] = {0, (float)(1.0 / s), 1, 1};
            for (const SkyAtmosphere& a : SKY_ATM)
                if (a.body == it.i) {
                    beta = a.beta;
                    mie = a.mie;
                    ozone = a.ozone;
                    ext[0] = a.ext.x, ext[1] = a.ext.y, ext[2] = a.ext.z, ext[3] = a.down;
                    alb = a.alb;
                    double density, sunEl;
                    V3 u;
                    if (CameraAtmosphere(app, &density, &u, &sunEl) == &a) {
                        haze[0] = (float)density;
                        haze[2] = (float)a.H;
                        haze[3] = (float)(1.0 - SunCover(app, app.cam.pos));
                        up = ToF(u);
                    }
                }
            PlanetUniform(app, app.locAtmBeta, &beta, SHADER_UNIFORM_VEC3);
            PlanetUniform(app, app.locAtmMie, &mie, SHADER_UNIFORM_VEC3);
            PlanetUniform(app, app.locAtmOzone, &ozone, SHADER_UNIFORM_VEC3);
            PlanetUniform(app, app.locAtmExt, ext, SHADER_UNIFORM_VEC4);
            PlanetUniform(app, app.locAtmAlb, &alb, SHADER_UNIFORM_VEC3);
            PlanetUniform(app, app.locAtmHaze, haze, SHADER_UNIFORM_VEC4);
            PlanetUniform(app, app.locCamUp, &up, SHADER_UNIFORM_VEC3);
        }
        int ri = -1;   // 環（RingSets() の番号）
        for (size_t k = 0; k < RingSets().size(); k++)
            if (RingSets()[k].body == it.i) ri = (int)k;
        if (d.rings && ri >= 0) {   // 本体に落ちる環の影（土星）
            app.mat.maps[MATERIAL_MAP_ROUGHNESS].texture = app.ringTex[ri];
            SetRingInfo(app, RingSets()[ri]);
        }
        // 地表近くは標高の分だけ持ち上げた地形の格子で描く（地図の画像を割り当てた時だけ。遠くでは山の高さは 1 画素に満たない）
        int texW = 0, texH = 0;
        if (b.terrain >= 0 && b.shape < 0 && dist < d.radius * 8) {
            if (style == S_EARTH_TEX) texW = app.earthData.width, texH = app.earthData.height;
            else if (style == S_MOON_TEX) texW = app.moonData.width, texH = app.moonData.height;
            else if (style == S_MAPPED && app.maps[b.map].height) texW = app.maps[b.map].data.width, texH = app.maps[b.map].data.height;
        }
        // 金星・タイタン: 地表は雲（もや）の層の上端より下にいる時だけ描き、層を重ねる（HazeWorld）
        if (under) {
            double h = std::max(0.0, dist - d.radius);
            Vector3 sky = HazeSkyColor(*hw, h, Dot(rel * (-1.0 / dist), L));
            float ha[4] = {1, (float)(1.0 / s), (float)h, h > hw->base ? hw->layerBeta : hw->lowBeta};
            float hb[4] = {hw->beta[0], hw->beta[1], hw->beta[2], (float)hw->H};
            Vector3 g1 = {hw->g1.r, hw->g1.g, hw->g1.b}, g2 = {hw->g2.r, hw->g2.g, hw->g2.b};
            float pv[4] = {params[0], params[1], params[2], 0};
            PlanetUniform(app, app.locHazeSky, &sky, SHADER_UNIFORM_VEC3);
            PlanetUniform(app, app.locHazeAir, ha, SHADER_UNIFORM_VEC4);
            PlanetUniform(app, app.locHazeBeta, hb, SHADER_UNIFORM_VEC4);
            PlanetUniform(app, app.locC1, &g1, SHADER_UNIFORM_VEC3);
            PlanetUniform(app, app.locC2, &g2, SHADER_UNIFORM_VEC3);
            PlanetUniform(app, app.locParams, pv, SHADER_UNIFORM_VEC4);
        }
        if (hw && !under) {
        } else if (texW > 0) DrawTerrain(app, b, rel, dist, alt, s, texW, texH);
        else DrawPlanetMesh(app, b.shape >= 0 ? app.shapeMesh[b.shape] : app.sphere, BodyMatrix(b, rel * s, d.radius * s, rPol * s, b.spin));
        if (hw) {   // 雲（もや）の層: 外からは今までの模様（ガス惑星と同じ描き方）、下からは一様な曇り空
            float ha0[4] = {0, 0, 0, 0};
            PlanetUniform(app, app.locHazeAir, ha0, SHADER_UNIFORM_VEC4);
            PlanetUniform(app, app.locC1, &c1, SHADER_UNIFORM_VEC3);
            PlanetUniform(app, app.locC2, &c2, SHADER_UNIFORM_VEC3);
            double above = dist - d.radius - hw->top;
            float pc[4] = {under ? 6.0f : (float)KIND_GAS, d.bands, params[2],
                           d.atmStrength * (float)std::min(1.0, std::max(0.0, above / (d.radius * 0.05)))};
            float dc[4] = {(float)d.style, detail[1], detail[2], detail[3]};
            PlanetUniform(app, app.locParams, pc, SHADER_UNIFORM_VEC4);
            PlanetUniform(app, app.locDetail, dc, SHADER_UNIFORM_VEC4);
            // 下から見る時は、球の面の中ほどが半径より内側に入る分（約 0.015%）を見込んで、カメラより確実に上に置く
            double rc = (under ? std::max(d.radius + hw->top, dist + 1) / (1 - 2e-4) : d.radius + hw->top) * s;
            if (under) rlDisableBackfaceCulling();
            DrawPlanetMesh(app, app.sphere, BodyMatrix(b, rel * s, rc, rc, b.spin));
            if (under) rlEnableBackfaceCulling();
        }

        if (d.style == S_TITAN && !under) {   // 上空のもやの層（地表から 約 500 km 上。縁でだけ見える）
            float hp[4] = {5.0f, 0, 0, 0.7f};
            Vector3 hc = {0.50f, 0.66f, 0.95f};
            PlanetUniform(app, app.locParams, hp, SHADER_UNIFORM_VEC4);
            PlanetUniform(app, app.locC1, &hc, SHADER_UNIFORM_VEC3);
            DrawPlanetMesh(app, app.sphere, BodyMatrix(b, rel * s, d.radius * 1.19 * s, d.radius * 1.19 * s, 0));
        }
        if (ri >= 0 && app.ringTex[ri].id) {
            const RingSet& rs = RingSets()[ri];
            float rp[4] = {4.0f, (float)d.radius, 0, 0};
            Vector3 rc1 = {rs.c1.r, rs.c1.g, rs.c1.b}, rc2 = {rs.c2.r, rs.c2.g, rs.c2.b};
            app.mat.maps[MATERIAL_MAP_ROUGHNESS].texture = app.ringTex[ri];
            SetRingInfo(app, rs);
            PlanetUniform(app, app.locC1, &rc1, SHADER_UNIFORM_VEC3);
            PlanetUniform(app, app.locC2, &rc2, SHADER_UNIFORM_VEC3);
            PlanetUniform(app, app.locParams, rp, SHADER_UNIFORM_VEC4);
            PlanetUniform(app, app.locLightObj, &lo, SHADER_UNIFORM_VEC3);
            rlDisableBackfaceCulling();
            DrawPlanetMesh(app, app.ringMesh[ri], BodyMatrix(b, rel * s, d.radius * s, d.radius * s, 0));
            rlEnableBackfaceCulling();
        }
    }
    app.planetBig = false;
}

static bool Occluded(const App& app, V3 from, V3 to, int self);

// 一番明るく見える恒星（太陽系の中では太陽）
static int BrightestStar(const App& app, V3 from, double* mag) {
    int best = B_SUN;
    *mag = 1e9;
    for (size_t i = 0; i < app.B.size(); i++) {
        if (app.B[i].d.kind != KIND_STAR) continue;
        double m = StarApparentMag(app.B[i].d, Len(app.B[i].pos - from));
        if (m < *mag) { *mag = m; best = (int)i; }
    }
    return best;
}

// 金星の雲・タイタンのもやの層の下か
static bool UnderHaze(const App& app, V3 p) {
    for (const Body& b : app.B)
        if (const HazeWorld* hw = HazeWorldOf(b.d))
            if (Len(p - b.pos) < b.d.radius + hw->top) return true;
    return false;
}

// 自機の照明: 一番明るい恒星（天体の影に入ると消える。恒星の間では暗く）と、近くの天体からの照り返し
static void SetShipLighting(App& app, V3 shipWorld) {
    double mag;
    int si = BrightestStar(app, shipWorld, &mag);
    const Body& star = app.B[si];
    V3 sun = Norm(star.pos - shipWorld);
    // 冥王星から見た太陽（−19 等）までは今まで通りの明るさ。恒星の間（0 等ほど）では最低限の明るさ
    float sunK = Occluded(app, shipWorld, star.pos, si) ? 0.0f : 1.05f * (float)std::max(0.12, std::min(1.0, (-mag - 4.0) / 12.0));
    const Col3 &tc = star.d.c1, &sc = app.B[B_SUN].d.c1;   // 恒星の色（太陽の色との比）
    Vector3 sunDir = ToF(sun), sunColor = {sunK * std::min(1.2f, tc.r / sc.r), sunK * 0.98f * std::min(1.2f, tc.g / sc.g),
                                           sunK * 0.94f * std::min(1.2f, tc.b / sc.b)};
    // 見かけの大きさが最大の天体（太陽以外）
    int best = -1;
    double bestK = 0;
    for (size_t i = 1; i < app.B.size(); i++) {
        if (app.B[i].d.probe >= 0 || app.B[i].d.lander >= 0) continue;
        double d = Len(app.B[i].pos - shipWorld);
        double k = app.B[i].d.radius / d;
        if (k > bestK) { bestK = k; best = (int)i; }
    }
    Vector3 fillDir = {0, 1, 0}, fillColor = {0, 0, 0};
    if (best > 0) {
        const Body& b = app.B[best];
        V3 toBody = Norm(b.pos - shipWorld);
        // 天体の昼側がこちらを向いているほど明るい
        double phase = 0.5 * (1 + Dot(Norm(app.B[HostStar(app.B, best)].pos - b.pos), -toBody));
        double k = std::min(0.45, bestK * bestK * 1.5) * phase;
        fillDir = ToF(toBody);
        fillColor = {(float)(b.d.c1.r * k), (float)(b.d.c1.g * k), (float)(b.d.c1.b * k)};
    }
    // 金星の雲・タイタンのもやの層の下: 日光は直接届かず、地表と同じ空の光が上から当たる
    for (const Body& b : app.B)
        if (const HazeWorld* hw = HazeWorldOf(b.d)) {
            V3 r = shipWorld - b.pos;
            double h = Len(r) - b.d.radius;
            if (h >= hw->top) continue;
            V3 up = Norm(r);
            Vector3 sky = HazeSkyColor(*hw, std::max(0.0, h), Dot(up, Norm(app.B[HostStar(app.B, (int)(&b - &app.B[0]))].pos - shipWorld)));
            sunColor = {0, 0, 0};
            fillDir = ToF(up);
            fillColor = {sky.x * 1.2f, sky.y * 1.2f, sky.z * 1.2f};
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
        V3 rel = c.shipRel;
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

// アポロの噴射の炎と噴流の 1 層（モデル座標 m）: o から向き ax へ長さ len、根元の半径 r0 から先の半径 r1 へ広がり、
// 先へ向かって消える（加算合成）
static void PlumeCone(V3 o, V3 ax, double r0, double r1, double len, Color c) {
    V3 a = Norm(ax);
    V3 p = Norm(Len(Cross(a, V3(1, 0, 0))) > 0.1 ? Cross(a, V3(1, 0, 0)) : Cross(a, V3(0, 1, 0))), q = Cross(a, p);
    const int segs = 20;
    auto vtx = [](V3 v) { rlVertex3f((float)v.x, (float)v.y, (float)v.z); };
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < segs; i++) {
        double a0 = 2 * PI * i / segs, a1 = 2 * PI * (i + 1) / segs;
        V3 e0 = p * std::cos(a0) + q * std::sin(a0), e1 = p * std::cos(a1) + q * std::sin(a1);
        V3 b0 = o + e0 * r0, b1 = o + e1 * r0, t0 = o + a * len + e0 * r1, t1 = o + a * len + e1 * r1;
        rlColor4ub(c.r, c.g, c.b, c.a);
        vtx(b0);
        vtx(b1);
        rlColor4ub(c.r, c.g, c.b, 0);
        vtx(t1);
        rlColor4ub(c.r, c.g, c.b, c.a);
        vtx(b0);
        rlColor4ub(c.r, c.g, c.b, 0);
        vtx(t1);
        vtx(t0);
    }
    rlEnd();
}

// アポロの噴射: エンジンの出口（モデル座標 m）と噴流の種類。返り値はエンジンの数
// 種類 0: F-1（ケロシンと液体酸素。明るい炎）, 1: J-2（液体水素と液体酸素。ほとんど透明）, 2: 自己着火性の推進剤（機械船・月着陸船。
// 真空ではかすか）, 3: 姿勢制御の小さな噴射口（向きは推力の反対）。*axis は噴流の向き（+Z か -Z）
struct ApolloNozzle { float x, y, z, r; };
static int ApolloNozzles(int model, int engine, ApolloNozzle* out, int* kind, float* axis) {
    *axis = 1;
    int n = 0;
    auto quads = [&](float rad, float z) {
        for (int k = 0; k < 4; k++) out[n++] = {cosf(PI / 4 + PI / 2 * k) * rad, sinf(PI / 4 + PI / 2 * k) * rad, z, 0.08f};
    };
    bool csm = model == PM_APOLLO_CSM || model == PM_APOLLO_CSM_LM || model == PM_APOLLO_CSM_LMG || model == PM_APOLLO_CSM_LMA;
    // ドッキングした形の月着陸船の接地点の高さ（ship_model.h の組み立てと同じ）
    float lmBase = model == PM_APOLLO_CSM_LM || model == PM_APOLLO_CSM_LMG ? -12.75f : model == PM_APOLLO_CM_LM ? -7.5f : 0;
    if (engine == AP_ENG_RCS || engine == AP_ENG_LM_RCS) {
        *kind = 3;
        if (engine == AP_ENG_LM_RCS && lmBase != 0) quads(2.0f, lmBase + 4.2f);
        else if (csm) quads(2.05f, -1.85f);
        else if (model == PM_APOLLO_LM || model == PM_LM_STOWED || model == PM_LM_ASCENT) quads(2.0f, -4.2f);
        return n;
    }
    if (engine == AP_ENG_LM_DPS) {
        *kind = 2;
        *axis = -1;
        if (lmBase != 0) out[n++] = {0, 0, lmBase + 0.4f, 0.75f};
        return n;
    }
    auto five = [&](float rad, float z, float r) {
        for (int k = 0; k < 4; k++) out[n++] = {cosf(PI / 4 + PI / 2 * k) * rad, sinf(PI / 4 + PI / 2 * k) * rad, z, r};
        out[n++] = {0, 0, z, r};
    };
    if (model == PM_SATURN_V) { *kind = 0; five(3.0f, -0.1f, 1.85f); }
    else if (model == PM_SATURN_SII_RING || model == PM_SATURN_SII_LES || model == PM_SATURN_SII) { *kind = 1; five(2.6f, 3.4f - SATURN_RING_LEN, 1.0f); }
    else if (model == PM_SATURN_SIVB) { *kind = 1; out[n++] = {0, 0, 3.4f, 1.0f}; }
    else if (csm) { *kind = 2; out[n++] = {0, 0, 6.65f, 1.25f}; }
    else if (model == PM_APOLLO_LM || model == PM_LM_STOWED) { *kind = 2; out[n++] = {0, 0, -0.4f, 0.75f}; }
    else if (model == PM_LM_ASCENT) { *kind = 2; out[n++] = {0, 0, -2.6f, 0.5f}; }
    return n;
}

// 噴流の長さ m（F-1 は空気が薄くなるにつれて長く広がる）
static double ApolloPlumeLen(int kind, double altKm) {
    double thin = 1 - std::exp(-std::max(0.0, altKm) / 7.2);
    return kind == 0 ? 45 + 150 * thin : kind == 1 ? 30 : kind == 2 ? 14 : 2.5;
}

// アポロの機体の噴射を描く（機体のモデル行列の中で）。噴射していなければ何もしない
static void DrawApolloPlume(const App& app, int i, int model, Quaternion q, V3 rel) {
    const Body& b = app.B[i];
    const ApolloBurn* bn = ApolloBurnAt(b.d, app.tDays);
    if (!bn) return;
    ApolloNozzle nz[8];
    int kind;
    float axis;
    int n = ApolloNozzles(model, bn->engine, nz, &kind, &axis);
    if (n == 0) return;
    // 点火と停止: 0.4 秒で立ち上がり、消える
    double e = std::min(1.0, std::min(app.tDays - bn->t0, bn->t1 - app.tDays) * 86400 / 0.4);
    e = std::max(0.25, e);
    double alt = SurfaceDist(app.B[B_EARTH], b.pos);
    double thin = 1 - std::exp(-std::max(0.0, alt) / 7.2);
    double len = ApolloPlumeLen(kind, alt);
    float tm = (float)GetTime();
    V3 jet = V3(0, 0, axis);
    if (kind == 3 && !bn->dir.empty()) {   // 小さな噴射口: 推力の反対へ（モデル座標で）
        Vector3 d = Vector3RotateByQuaternion(ToF(ApolloThrustDir(*bn, app.tDays)), QuaternionInvert(q));
        jet = V3(-d.x, -d.y, -d.z);
    }
    rlPushMatrix();
    rlTranslatef((float)rel.x, (float)rel.y, (float)rel.z);
    rlMultMatrixf(MatrixToFloat(QuaternionToMatrix(q)));
    rlScalef(0.001f, 0.001f, 0.001f);
    BeginBlendMode(BLEND_ADDITIVE);
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    auto A = [&](double k) { return (unsigned char)std::min(255.0, k * e); };
    for (int k = 0; k < n; k++) {
        V3 o(nz[k].x, nz[k].y, nz[k].z);
        double r = nz[k].r, fl = 1 + 0.05 * std::sin(tm * 41.0f + k * 1.7f) + 0.03 * std::sin(tm * 67.0f + k);
        if (kind == 3) {
            V3 out = Norm(V3(nz[k].x, nz[k].y, 0));
            if (Dot(out, jet) < -0.3) continue;   // 機体の内側へ向く噴射口は使わない
            PlumeCone(o, jet, r, 0.6, len * fl, {235, 235, 255, A(70)});
        } else if (kind == 0) {
            PlumeCone(o, jet, r, r * (1.3 + 5 * thin), len * fl, {255, 150, 50, A(150)});
            PlumeCone(o, jet, r * 0.8, r * 0.9, len * 0.35 * fl, {255, 235, 190, A(220)});
        } else if (kind == 1) {
            PlumeCone(o, jet, r, r * 4, len * fl, {150, 175, 255, A(45)});
            PlumeCone(o, jet, r * 0.8, r * 0.6, 4 * fl, {220, 230, 255, A(90)});
        } else {
            PlumeCone(o, jet, r, r * 3.5, len * r / 1.25 * fl, {255, 190, 140, A(50)});
            PlumeCone(o, jet, r * 0.8, r * 0.6, r * 2.5 * fl, {255, 230, 200, A(100)});
        }
    }
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
    EndBlendMode();
    rlPopMatrix();
}

// 探査機の機体（自機と同じシェーダ）。天体に隠れている時は描かない
static const double GALILEO_PROBE_RELEASE = -1633.27;  // 1995-07-13 05:30 UTC: ガリレオが大気圏突入機を切り離す
// 着陸機の形（基準からの秒 s）。スカイクレーンの着陸機は背面カバーを切り離した後、降下段をローバーの上に別に描く（*dsUp: 吊り下げで
// 伸ばした長さ m。降下段を描かない時は負）
static int LanderModel(int li, double s, double* dsUp) {
    const LanderDef& L = LANDERS[li];
    const double* e = L.ev;
    *dsUp = -1;
    switch (L.kind) {
    case LK_HUYGENS:   // 主パラシュートを開く 09:10:23 まで大気に入る時の形（前面シールドを切り離す 09:10:53 までの 30 秒は描き分けない）、
                       // 安定用パラシュートに替える 09:25:21、着地 11:38:11（Lebreton ほか 2005）
        return s < 623 ? PM_HUYGENS_ENTRY : s < 1521 ? PM_HUYGENS_MAIN : s < L.land ? PM_HUYGENS_STAB : PM_HUYGENS;
    case LK_SKYCRANE:
        // パーシビアランスは Karlgaard ほか（MEDLI2 の再構成）の表、吊り下げは NAIF の SPK の説明。キュリオシティのパラシュートと
        // 背面カバーは SPK の軌道の加速度の変わり目から求め、前面シールドはパラシュートの 24 秒後（打ち上げ前の NASA の資料の目安）
        if (s < e[0]) return PM_EDL_ENTRY;
        if (s < e[1]) return PM_EDL_CHUTE;
        if (s < e[2]) return PM_EDL_BACKSHELL;
        if (s < 0) {   // 背面カバーを切り離した後、着地まで（着地の後に飛び去る降下段は描かない）
            double u = std::max(0.0, std::min(1.0, (s - e[3]) / (e[4] - e[3])));
            *dsUp = L.lower * u * u * (3 - 2 * u);
        }
        return PM_MARS_ROVER;
    case LK_AIRBAG:
        // パラシュートは軌道の加速度の変わり目から。前面シールドはその 20 秒後、着陸機を吊るのはさらに 10 秒後、エアバッグは
        // 最初に地面に当たる 8 秒前、跳ねて止まるのは 10 分後（目安）。止まった後はローバーだけを描く（エアバッグをしぼませて
        // 花びらを開くまでと、残した着陸台は描かない）
        if (s < e[0]) return PM_MER_ENTRY;
        if (s < e[1]) return PM_MER_CHUTE;
        if (s < e[2]) return PM_MER_BACKSHELL;
        if (s < e[3]) return PM_MER_BRIDLE;
        if (s < 0) return PM_MER_AIRBAG_DESC;
        return s < e[4] ? PM_AIRBAG : PM_MER_ROVER;
    default:
        // パラシュートと背面カバーから離れる時は軌道の加速度の変わり目から。前面シールドはパラシュートの 15 秒後（目安）
        if (s < e[0]) return PM_LEG_ENTRY;
        if (s < e[1]) return PM_LEG_CHUTE;
        if (s < e[2]) return PM_LEG_BACKSHELL;
        if (s < 0) return PM_LEG_POWERED;
        return li == 5 ? PM_PHX_LANDED : PM_INSIGHT_LANDED;
    }
}
static void DrawProbes(App& app) {
    const Cam& c = app.cam;
    // アポロ計画: 月面に残った下降段（離陸した後）
    for (int m = 0; m < APOLLO_COUNT; m++) {
        V3 pos, up;
        if (!ApolloDescentStage(app, m, &pos, &up)) continue;
        V3 rel = ViewRel(app, pos);
        double dist = Len(rel);
        if (0.01 / dist * c.focal < 0.5 || Dot(rel, c.fwd) < -0.02) continue;
        SetPassProjection(c, std::max(1e-5, (dist - 0.01) * 0.5), dist + 0.01);
        ClearDepth();
        SetShipLighting(app, pos);
        Vector3 fillDir = ToF(Norm(c.pos - pos)), fillColor = {0.3f, 0.3f, 0.32f}, zero = {0, 0, 0};
        float off = 0;
        SetShaderValue(app.shipShader, app.locFillDir, &fillDir, SHADER_UNIFORM_VEC3);
        SetShaderValue(app.shipShader, app.locFillColor, &fillColor, SHADER_UNIFORM_VEC3);
        SetShaderValue(app.shipShader, app.locEngColor, &zero, SHADER_UNIFORM_VEC3);
        SetShaderValue(app.shipShader, app.locEngGlow, &off, SHADER_UNIFORM_FLOAT);
        SetShaderValue(app.shipShader, app.locNavBlink, &off, SHADER_UNIFORM_FLOAT);
        V3 east = Cross(app.B[B_MOON].pole, up);
        Quaternion q = QuatFromBasis(up, Len(east) > 1e-9 ? Norm(east) : V3(1, 0, 0));
        Matrix model = MatrixMultiply(MatrixMultiply(MatrixScale(0.001f, 0.001f, 0.001f), QuaternionToMatrix(q)),
                                      MatrixTranslate((float)rel.x, (float)rel.y, (float)rel.z));
        rlDisableBackfaceCulling();
        DrawMesh(app.probeMesh[PM_LM_DESCENT], app.shipMat, model);
        rlEnableBackfaceCulling();
    }
    // アポロ計画: 機体から離れていく部品（月着陸船を引き出す手順の第3段とアダプターの板、切り離したサターンの段）。機体の
    // そば（60 m 以内）の物は機体と同じ奥行きの描画の中で描く（別々に描くと、後に描いた司令船につながった月着陸船が、手前の
    // 第3段のアダプターより前に出る）
    std::vector<ApolloPiece> tdeNear[APOLLO_COUNT];
    std::vector<ApolloPiece> tdeFar;
    for (int m = 0; m < APOLLO_COUNT; m++) {
        ApolloPieces ps;
        if (!ApolloPiecesAt(app, m, &ps)) continue;
        int csm = ApolloBody(app, m, 0);
        for (const ApolloPiece& p : ps.parts) (Len(p.pos - app.B[csm].pos) < 0.06 ? tdeNear[m] : tdeFar).push_back(p);
    }
    auto meshAt = [&](int pm, V3 pos, Quaternion q) {
        V3 rel = ViewRel(app, pos);
        Matrix model = MatrixMultiply(MatrixMultiply(MatrixScale(0.001f, 0.001f, 0.001f), QuaternionToMatrix(q)),
                                      MatrixTranslate((float)rel.x, (float)rel.y, (float)rel.z));
        rlDisableBackfaceCulling();
        DrawMesh(app.probeMesh[pm], app.shipMat, model);
        rlEnableBackfaceCulling();
    };
    for (const ApolloPiece& p : tdeFar) {
        V3 rel = ViewRel(app, p.pos);
        double dist = Len(rel), size = std::max(0.01, (double)app.probeOuter[p.pm] * 1.1);
        if (app.probeOuter[p.pm] / dist * c.focal < 0.5 || Dot(rel, c.fwd) < -size) continue;
        if (Occluded(app, c.pos, p.pos, -1)) continue;
        SetPassProjection(c, std::max(1e-5, (dist - size) * 0.5), dist + size);
        ClearDepth();
        SetShipLighting(app, p.pos);
        Vector3 fillDir = ToF(Norm(c.pos - p.pos)), fillColor = {0.3f, 0.3f, 0.32f}, zero = {0, 0, 0};
        float off = 0;
        SetShaderValue(app.shipShader, app.locFillDir, &fillDir, SHADER_UNIFORM_VEC3);
        SetShaderValue(app.shipShader, app.locFillColor, &fillColor, SHADER_UNIFORM_VEC3);
        SetShaderValue(app.shipShader, app.locEngColor, &zero, SHADER_UNIFORM_VEC3);
        SetShaderValue(app.shipShader, app.locEngGlow, &off, SHADER_UNIFORM_FLOAT);
        SetShaderValue(app.shipShader, app.locNavBlink, &off, SHADER_UNIFORM_FLOAT);
        meshAt(p.pm, p.pos, p.q);
    }
    for (size_t i = 0; i < app.B.size(); i++) {
        const Body& b = app.B[i];
        if ((b.d.probe < 0 && b.d.apollo < 0 && b.d.lander < 0) || !b.active) continue;
        // アポロ: 時刻に応じた機体の形（つながっている間の月着陸船は司令船の側で描く）
        int apModel = -1, mission = -1;
        if (b.d.apollo >= 0) {
            int vehicle;
            if (!ApolloVehicleOf(app, (int)i, &mission, &vehicle)) continue;
            apModel = ApolloModel(APOLLO_MISSIONS[mission], vehicle, app.tDays);
            if (apModel < 0) continue;
        }
        double dsUp = -1;
        int ldModel = b.d.lander >= 0 ? LanderModel(b.d.lander, (app.tDays - LANDERS[b.d.lander].t0) * 86400, &dsUp) : -1;
        double size = apModel >= 0 ? std::max(0.02, (double)app.probeOuter[apModel] * 1.1) : 0.02;
        if (ldModel >= 0) size = std::max(0.02, (double)app.probeOuter[ldModel] * 1.1);
        if (apModel >= 0 && ApolloBurnAt(b.d, app.tDays)) size += 0.25;   // 噴流の分（F-1 は 200 m まで）
        const std::vector<ApolloPiece>* near = nullptr;   // 同じ描画で描く部品
        for (int m = 0; m < APOLLO_COUNT && apModel >= 0; m++)
            if (!tdeNear[m].empty() && ApolloBody(app, m, 0) == (int)i) near = &tdeNear[m];
        if (near)
            for (const ApolloPiece& p : *near) size = std::max(size, Len(p.pos - b.pos) + (double)app.probeOuter[p.pm] * 1.1);
        V3 rel = ViewRel(app, b.pos);
        double dist = Len(rel);
        if (0.015 / dist * c.focal < 0.5 || Dot(rel, c.fwd) < -size) continue;
        if (Occluded(app, c.pos, b.pos, (int)i)) continue;
        SetPassProjection(c, std::max(1e-5, (dist - size) * 0.5), dist + size);
        ClearDepth();
        SetShipLighting(app, b.pos);
        // 太陽の反対側から見ると真っ黒な影絵になるので、視点側から弱い補助光を当てる（雲・もやの下は空の光だけ）
        if (!UnderHaze(app, b.pos)) {
            Vector3 fillDir = ToF(Norm(c.pos - b.pos)), fillColor = {0.3f, 0.3f, 0.32f};
            SetShaderValue(app.shipShader, app.locFillDir, &fillDir, SHADER_UNIFORM_VEC3);
            SetShaderValue(app.shipShader, app.locFillColor, &fillColor, SHADER_UNIFORM_VEC3);
        }
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
        if (pm == PM_CASSINI && app.tDays < HuygensRelease()) pm = PM_CASSINI_HUYGENS;
        V3 ldUp;
        if (ldModel >= 0) {
            // 着陸機: 前面シールドを進む向きへ（パラシュートは後ろへなびく）。地表に対して秒速 200 m より遅くなると上を天頂へ
            // 向けていく（秒速 100 m で真上。パラシュートは風と一緒に流れるので、遅い時は地表に対する速さの向きを使わない）
            pm = ldModel;
            int k = LanderWorld(app.B, b.d.lander);
            ldUp = Norm(b.pos - app.B[k].pos);
            V3 v = LanderVelocity(app.B, b.d.lander, app.tDays);
            double sp = Len(v) * 1000, f = std::max(0.0, std::min(1.0, (sp - 100) / 100));
            aim = ldUp * (1 - f * f * (3 - 2 * f)) - (sp > 0 ? v * (1.0 / Len(v)) : V3()) * (f * f * (3 - 2 * f));
            upHint = app.B[k].pole;
            V3 dd = pm == PM_MARS_ROVER || pm == PM_MER_ROVER ? LanderDriveDir(app.B, b.d.lander, app.tDays) : V3();
            if (Len(dd) > 0) upHint = Cross(dd, ldUp);   // ローバーの前（+X）を走ってきた向きへ（+Y = 前 × 天頂）
        }
        Quaternion q = QuatFromBasis(Len(aim) > 1e-6 ? aim : V3(1, 0, 0), upHint);
        if (apModel >= 0) {
            pm = apModel;
            q = ApolloPose(app, (int)i, pm);
        }
        Matrix model = MatrixMultiply(MatrixMultiply(MatrixScale(0.001f, 0.001f, 0.001f), QuaternionToMatrix(q)),
                                      MatrixTranslate((float)rel.x, (float)rel.y, (float)rel.z));
        rlDisableBackfaceCulling();
        DrawMesh(app.probeMesh[pm], app.shipMat, model);
        // 15〜17 号: 科学機器室の扉を捨てた後は、機械船に開いた所を重ねる
        bool withSm = pm == PM_APOLLO_CSM || pm == PM_APOLLO_CSM_LM || pm == PM_APOLLO_CSM_LMG || pm == PM_APOLLO_CSM_LMA;
        if (withSm && mission >= 0 && app.tDays >= ApolloSimDoorT(APOLLO_MISSIONS[mission])) DrawMesh(app.probeMesh[PM_SIM_BAY], app.shipMat, model);
        if (dsUp >= 0) {   // 降下段（ローバーの上面 1.3 m の上）と、吊り下げている間の吊りひも
            V3 at = rel + ldUp * ((1.3 + dsUp) * 0.001);
            DrawMesh(app.probeMesh[PM_DESCENT_STAGE], app.shipMat,
                     MatrixMultiply(MatrixMultiply(MatrixScale(0.001f, 0.001f, 0.001f), QuaternionToMatrix(q)), MatrixTranslate((float)at.x, (float)at.y, (float)at.z)));
            V3 top = rel + ldUp * 0.0013;
            if (dsUp > 0.01)
                DrawMesh(app.probeMesh[PM_BRIDLES], app.shipMat,
                         MatrixMultiply(MatrixMultiply(MatrixScale(0.001f, 0.001f, 0.001f * (float)dsUp), QuaternionToMatrix(q)), MatrixTranslate((float)top.x, (float)top.y, (float)top.z)));
        }
        rlEnableBackfaceCulling();
        if (near)
            for (const ApolloPiece& p : *near) meshAt(p.pm, p.pos, p.q);
        if (apModel >= 0) DrawApolloPlume(app, (int)i, pm, q, rel);
    }
    rlDrawRenderBatchActive();
}
