// Solar System Voyager — Copyright (c) 2026 hubstacksea
// MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
// 3D の描画（天体・自機・探査機・軌道・空）と音楽の選曲。
// main.cpp の一部で、main.cpp が決まった順に読み込む（単体ではコンパイルしない）

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
        if (d.parent < 0) continue;
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
static void SetRingInfo(const App& app, const RingSet& rs) {
    float arc = -1;
    if (rs.arcs) arc = (float)(std::fmod(820.1185 * app.tDays, 360.0) * DEG);   // 弧は環と同じ周期で回る（位置は目安）
    if (arc < -0.5f && rs.arcs) arc += (float)(2 * PI);
    float info[4] = {(float)rs.inner, (float)(rs.outer - rs.inner), arc, rs.thin ? 1.0f : 0.0f};
    SetShaderValue(app.shader, app.locRingInfo, info, SHADER_UNIFORM_VEC4);
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
            V3 cp = CamRel(app.cam, o.pos) * s;
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
        V3 rel = CamRel(c, b.pos);
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
        V3 rel = CamRel(c, b.pos);
        double dist = it.dist;
        double s = 100.0 / dist;
        double rPol = d.radius * (1 - d.flattening);
        // near: 最も近い表面（環があればその外縁）より手前
        double alt = dist - SurfaceRadius(b, rel * (-1.0 / dist));
        double nearKm = (dist > b.outer && b.outer > d.radius) ? std::min(alt, dist - b.outer) * 0.9 : alt * 0.5;
        nearKm = std::max(nearKm, dist * 1e-7);
        SetPassProjection(c, nearKm * s, (dist + b.outer) * s * 1.01);
        ClearDepth();

        V3 L = d.kind == KIND_STAR ? V3(0, 1, 0) : Norm(app.B[HostStar(app.B, it.i)].pos - b.pos);
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
        int ri = -1;   // 環（RingSets() の番号）
        for (size_t k = 0; k < RingSets().size(); k++)
            if (RingSets()[k].body == it.i) ri = (int)k;
        if (d.rings && ri >= 0) {   // 本体に落ちる環の影（土星）
            app.mat.maps[MATERIAL_MAP_ROUGHNESS].texture = app.ringTex[ri];
            SetRingInfo(app, RingSets()[ri]);
        }
        DrawMesh(b.shape >= 0 ? app.shapeMesh[b.shape] : app.sphere, app.mat, BodyMatrix(b, rel * s, d.radius * s, rPol * s, b.spin));

        if (d.style == S_TITAN) {   // 上空のもやの層（地表から 約 500 km 上）
            float hp[4] = {5.0f, 0, 0, 0.7f};
            Vector3 hc = {0.50f, 0.66f, 0.95f};
            SetShaderValue(app.shader, app.locParams, hp, SHADER_UNIFORM_VEC4);
            SetShaderValue(app.shader, app.locC1, &hc, SHADER_UNIFORM_VEC3);
            DrawMesh(app.sphere, app.mat, BodyMatrix(b, rel * s, d.radius * 1.19 * s, d.radius * 1.19 * s, 0));
        }
        if (ri >= 0 && app.ringTex[ri].id) {
            const RingSet& rs = RingSets()[ri];
            float rp[4] = {4.0f, (float)d.radius, 0, 0};
            Vector3 rc1 = {rs.c1.r, rs.c1.g, rs.c1.b}, rc2 = {rs.c2.r, rs.c2.g, rs.c2.b};
            app.mat.maps[MATERIAL_MAP_ROUGHNESS].texture = app.ringTex[ri];
            SetRingInfo(app, rs);
            SetShaderValue(app.shader, app.locC1, &rc1, SHADER_UNIFORM_VEC3);
            SetShaderValue(app.shader, app.locC2, &rc2, SHADER_UNIFORM_VEC3);
            SetShaderValue(app.shader, app.locParams, rp, SHADER_UNIFORM_VEC4);
            SetShaderValue(app.shader, app.locLightObj, &lo, SHADER_UNIFORM_VEC3);
            rlDisableBackfaceCulling();
            DrawMesh(app.ringMesh[ri], app.mat, BodyMatrix(b, rel * s, d.radius * s, d.radius * s, 0));
            rlEnableBackfaceCulling();
        }
    }
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
        double phase = 0.5 * (1 + Dot(Norm(app.B[HostStar(app.B, best)].pos - b.pos), -toBody));
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

// 探査機の機体（自機と同じシェーダ）。天体に隠れている時は描かない
static const double GALILEO_PROBE_RELEASE = -1633.27;  // 1995-07-13 05:30 UTC: ガリレオが大気圏突入機を切り離す
static const double HUYGENS_RELEASE = 1819.58;         // 2004-12-25 02:00 UTC: カッシーニがホイヘンスを切り離す
static void DrawProbes(App& app) {
    const Cam& c = app.cam;
    for (size_t i = 0; i < app.B.size(); i++) {
        const Body& b = app.B[i];
        if (b.d.probe < 0 || !b.active) continue;
        V3 rel = CamRel(c, b.pos);
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
