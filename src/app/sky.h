// Solar System Voyager — Copyright (c) 2026 hubstacksea
// MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
// 背景の星・小惑星帯などの粒。
// main.cpp の一部で、main.cpp が決まった順に読み込む（単体ではコンパイルしない）

// ---------------------------------------------------------------- 背景の星
// ヒッパルコス星表の 8 等級までの恒星と、88 星座の線・名前（tools/make_sky_assets.py で作った埋め込みデータ）。
// 太陽から 10 光年ほどまでの恒星（シリウスなど）は除いてあり、天体として実際の位置に描く。
// 星には年周視差からの距離と固有運動があり、太陽から離れた所や遠い昔・未来では見える向きと明るさが変わる（UpdateSkyParallax）
struct Star { V3 dir; Color col; float size; float mag; V3 pos; float mag0; V3 vel; };  // size 0 は 1 ピクセルの点。
// 太陽から見た明るさ（mag0）の順に並ぶ（動いても並べ直さない。超光速では毎フレーム計算し直すので）
// pos は J2000 の太陽からの位置 pc、mag0 は J2000 に太陽から見た等級、vel は固有運動による速度 pc/年（視線速度は入れない）
struct Constellation {
    V3 label; std::string jp, en; std::vector<std::vector<V3>> lines;   // 今の場所・年代で見た向き（自分のすぐ近くの頂点は 0）
    V3 labelPos, labelVel; std::vector<std::vector<V3>> linePos, lineVel;   // J2000 の太陽からの位置 pc と速度 pc/年
};
struct StarName { V3 dir; float mag; std::string jp, en; V3 pos, vel; };
struct Sky {
    std::vector<Star> stars;
    std::vector<Constellation> cons;
    std::vector<StarName> names;
    V3 obs = {0, 0, 0};   // 向きを計算した場所（太陽からの位置 pc）
    double years = 0;     // 向きを計算した年代（J2000 からの年）
};

// 固有運動 μα*・μδ [mas/年] と距離 pc → 速度 pc/年（天球の東・北の向き。描画座標）
static V3 ProperMotionVel(double ra, double dec, double pc, double pmra, double pmdec) {
    V3 east = EquToRender(ra + 90, 0), north = EquToRender(ra + 180, 90 - dec);
    return (east * pmra + north * pmdec) * (1e-3 / 206264.806 * pc);
}

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

// 等級 → 点の大きさ（0 は 1 ピクセルの点）と濃さ。4.5 等より明るい星は円で、暗い星は 1 ピクセルの点の濃さで明るさを表す
static float StarDotSize(float mag) { return mag < 4.5f ? std::min(3.2f, 1.0f + (4.5f - mag) * 0.38f) : 0.0f; }
static float StarDotAlpha(float mag) { return mag < 5.0f ? 255.0f : std::max(30.0f, 235.0f - (mag - 5.0f) * 65.0f); }
// 高解像度の画面では星も画面の倍率に合わせて大きく
static void DrawStarDot(Vector2 p, float size, Color col) {
    if (size > 0) DrawCircleV(p, size * g_dpiScale, col);
    else if (g_dpiScale > 1.4f) DrawCircleV(p, 0.5f * g_dpiScale, col);
    else DrawPixelV(p, col);
}

static Sky LoadSky() {
    Sky sky;
    EmbeddedFile sb = StarsBin();
    uint32_t n = 0;
    if (sb.size >= 4) std::memcpy(&n, sb.data, 4);
    n = (uint32_t)std::min<size_t>(n, (sb.size - 4) / 24);
    sky.stars.reserve(n);
    for (uint32_t i = 0; i < n; i++) {
        const unsigned char* p = sb.data + 4 + 24 * (size_t)i;
        float ra, dec, pc, pmra, pmdec;
        int16_t mag100, bv1000;
        std::memcpy(&ra, p, 4);
        std::memcpy(&dec, p + 4, 4);
        std::memcpy(&mag100, p + 8, 2);
        std::memcpy(&bv1000, p + 10, 2);
        std::memcpy(&pc, p + 12, 4);
        std::memcpy(&pmra, p + 16, 4);
        std::memcpy(&pmdec, p + 20, 4);
        float mag = mag100 / 100.0f;
        V3 dir = EquToRender(ra, dec);
        sky.stars.push_back({dir, StarColor(bv1000 / 1000.0, (unsigned char)StarDotAlpha(mag)), StarDotSize(mag), mag, dir * (double)pc, mag,
                             ProperMotionVel(ra, dec, pc, pmra, pmdec)});
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
        auto num = [&](size_t k) { return atof(f[k].c_str()); };
        auto at = [&](size_t k) { return EquToRender(num(k), num(k + 1)); };
        auto vel = [&](size_t k) { return ProperMotionVel(num(k), num(k + 1), num(k + 2), num(k + 3), num(k + 4)); };
        if (f[0] == "C" && f.size() >= 6) {
            V3 d = at(1);
            sky.cons.push_back({d, f[4], f[5], {}, d * num(3), {0, 0, 0}, {}, {}});
        } else if (f[0] == "L" && !sky.cons.empty()) {
            std::vector<V3> line, pos, v;
            for (size_t k = 1; k + 4 < f.size(); k += 5) {
                line.push_back(at(k));
                pos.push_back(at(k) * num(k + 2));
                v.push_back(vel(k));
            }
            sky.cons.back().lines.push_back(line);
            sky.cons.back().linePos.push_back(pos);
            sky.cons.back().lineVel.push_back(v);
        } else if (f[0] == "S" && f.size() >= 9) {
            V3 d = at(1);
            sky.names.push_back({d, (float)num(6), f[7], f[8], d * num(3), vel(1)});
        }
    }
    // 星座名の位置は、星座線の星の見かけの動き（角速度）の平均で動かす
    for (Constellation& con : sky.cons) {
        V3 sum = {0, 0, 0};
        int cnt = 0;
        for (size_t i = 0; i < con.lineVel.size(); i++)
            for (size_t k = 0; k < con.lineVel[i].size(); k++) {
                sum = sum + con.lineVel[i][k] * (1 / std::max(1e-3, Len(con.linePos[i][k])));
                cnt++;
            }
        if (cnt > 0) con.labelVel = sum * (Len(con.labelPos) / cnt);
    }
    return sky;
}

// 場所 obs（太陽からの位置 pc）・年代 years（J2000 からの年）から見た星の向きと明るさに計算し直す。
// 0.002 pc（約 400 AU）動くか 20 年たつまでは前の結果のまま。
// 星座線は、自分のすぐ近く（0.3 pc 以内）の星につながる線を描かない（その星の中にいるので向きが決まらない）
static void UpdateSkyParallax(Sky& sky, V3 obs, double years) {
    if (Len(obs - sky.obs) < 0.002 && std::fabs(years - sky.years) < 20) return;
    sky.obs = obs;
    sky.years = years;
    for (Star& st : sky.stars) {
        V3 d = st.pos + st.vel * years - obs;
        double L = std::max(Len(d), 1e-3);
        st.dir = d * (1 / L);
        st.mag = st.mag0 + 5 * (float)std::log10(L / std::max(Len(st.pos), 1e-3));
        st.size = StarDotSize(st.mag);
        st.col.a = (unsigned char)StarDotAlpha(st.mag);
    }
    auto look = [&](V3 p, bool line) {   // p は年代 years の位置
        V3 d = p - obs;
        double L = Len(d);
        return line && L < 0.3 ? V3{0, 0, 0} : d * (1 / std::max(L, 1e-3));
    };
    for (Constellation& con : sky.cons) {
        con.label = look(con.labelPos + con.labelVel * years, false);
        for (size_t i = 0; i < con.lines.size(); i++)
            for (size_t k = 0; k < con.lines[i].size(); k++) con.lines[i][k] = look(con.linePos[i][k] + con.lineVel[i][k] * years, true);
    }
    for (StarName& n : sky.names) n.dir = look(n.pos + n.vel * years, false);
}

// ---------------------------------------------------------------- 星雲・銀河・星団
// 明るい・よく知られた深宇宙天体。位置（J2000 の赤経・赤緯 deg）、見かけの大きさ（長径・短径 分角）、位置角（北から東へ deg）、
// 等級 V、距離（光年）は公表されている値を丸めて書き入れたもの。画像は使わず、種類ごとの作り方でテクスチャを作って描く
// （写真ではなく、形と色を似せた絵）。かに星雲は 1054 年の超新星の後にだけあり、年とともに広がる
enum DeepSkyKind { DSO_SPIRAL, DSO_EDGEON, DSO_ELLIPTICAL, DSO_IRREGULAR, DSO_EMISSION, DSO_RING, DSO_REFLECTION, DSO_PLANETARY,
                   DSO_REMNANT, DSO_SHELL, DSO_GLOBULAR };
struct DeepSky {
    const char* jp; const char* en; const char* id;
    int kind;
    double ra, dec, a, b, pa, mag, ly;
    double born;                    // できた年（それより前は描かない。NAN: ずっとある）
    unsigned char c1[3], c2[3];     // 中心と外側の色
};
static const DeepSky DEEP_SKY[] = {
    {"アンドロメダ銀河", "Andromeda Galaxy", "M31", DSO_SPIRAL, 10.685, 41.269, 190, 60, 35, 3.4, 2.54e6, NAN, {255, 228, 190}, {170, 188, 255}},
    {"M32", "M32", "M32", DSO_ELLIPTICAL, 10.674, 40.865, 8.7, 6.5, 170, 8.1, 2.49e6, NAN, {255, 230, 195}, {255, 225, 190}},
    {"M110", "M110", "M110", DSO_ELLIPTICAL, 10.092, 41.685, 21.9, 11.0, 170, 8.5, 2.69e6, NAN, {255, 230, 195}, {255, 225, 190}},
    {"さんかく座銀河", "Triangulum Galaxy", "M33", DSO_SPIRAL, 23.462, 30.660, 70.8, 41.7, 23, 5.7, 2.73e6, NAN, {255, 232, 205}, {165, 185, 255}},
    {"大マゼラン雲", "Large Magellanic Cloud", "LMC", DSO_IRREGULAR, 80.894, -69.756, 645, 550, 170, 0.9, 1.63e5, NAN, {240, 238, 255}, {195, 205, 255}},
    {"小マゼラン雲", "Small Magellanic Cloud", "SMC", DSO_IRREGULAR, 13.158, -72.800, 320, 205, 45, 2.7, 2.0e5, NAN, {240, 238, 255}, {195, 205, 255}},
    {"タランチュラ星雲", "Tarantula Nebula", "NGC 2070", DSO_EMISSION, 84.676, -69.101, 40, 25, 0, 8.0, 1.6e5, NAN, {255, 190, 200}, {255, 105, 135}},
    {"ボーデの銀河", "Bode's Galaxy", "M81", DSO_SPIRAL, 148.888, 69.065, 26.9, 14.1, 157, 6.9, 1.18e7, NAN, {255, 226, 185}, {180, 195, 255}},
    {"葉巻銀河", "Cigar Galaxy", "M82", DSO_EDGEON, 148.968, 69.680, 11.2, 4.3, 65, 8.4, 1.15e7, NAN, {245, 230, 215}, {255, 140, 150}},
    {"子持ち銀河", "Whirlpool Galaxy", "M51", DSO_SPIRAL, 202.470, 47.195, 11.2, 6.9, 163, 8.4, 2.3e7, NAN, {255, 228, 190}, {170, 188, 255}},
    {"回転花火銀河", "Pinwheel Galaxy", "M101", DSO_SPIRAL, 210.802, 54.349, 28.8, 26.9, 0, 7.9, 2.09e7, NAN, {255, 232, 200}, {165, 185, 255}},
    {"ソンブレロ銀河", "Sombrero Galaxy", "M104", DSO_EDGEON, 189.998, -11.623, 8.7, 3.5, 90, 8.0, 3.1e7, NAN, {255, 232, 200}, {225, 220, 230}},
    {"おとめ座 A", "Virgo A", "M87", DSO_ELLIPTICAL, 187.706, 12.391, 8.3, 6.6, 160, 8.6, 5.35e7, NAN, {255, 228, 190}, {255, 222, 185}},
    {"ケンタウルス座 A", "Centaurus A", "NGC 5128", DSO_EDGEON, 201.365, -43.019, 25.7, 20.0, 115, 6.8, 1.2e7, NAN, {255, 225, 190}, {240, 215, 195}},
    {"ちょうこくしつ座銀河", "Sculptor Galaxy", "NGC 253", DSO_SPIRAL, 11.888, -25.288, 27.5, 6.8, 52, 7.1, 1.14e7, NAN, {255, 225, 190}, {200, 200, 240}},
    {"南の回転花火銀河", "Southern Pinwheel Galaxy", "M83", DSO_SPIRAL, 204.254, -29.866, 12.9, 11.5, 45, 7.5, 1.5e7, NAN, {255, 230, 195}, {170, 188, 255}},
    {"オリオン大星雲", "Orion Nebula", "M42", DSO_EMISSION, 83.822, -5.391, 65, 60, 0, 4.0, 1344, NAN, {205, 240, 235}, {255, 110, 140}},
    {"干潟星雲", "Lagoon Nebula", "M8", DSO_EMISSION, 270.904, -24.387, 90, 40, 90, 6.0, 4100, NAN, {255, 175, 190}, {255, 100, 130}},
    {"三裂星雲", "Trifid Nebula", "M20", DSO_EMISSION, 270.596, -23.030, 28, 28, 0, 6.3, 5200, NAN, {255, 150, 175}, {140, 165, 255}},
    {"わし星雲", "Eagle Nebula", "M16", DSO_EMISSION, 274.700, -13.807, 35, 28, 0, 6.0, 5700, NAN, {255, 185, 175}, {255, 105, 120}},
    {"オメガ星雲", "Omega Nebula", "M17", DSO_EMISSION, 275.108, -16.177, 46, 37, 0, 6.0, 5500, NAN, {255, 185, 200}, {255, 100, 130}},
    {"イータカリーナ星雲", "Carina Nebula", "NGC 3372", DSO_EMISSION, 161.265, -59.867, 120, 120, 0, 1.0, 8500, NAN, {255, 200, 190}, {255, 95, 120}},
    {"北アメリカ星雲", "North America Nebula", "NGC 7000", DSO_EMISSION, 314.75, 44.33, 120, 100, 0, 4.0, 2590, NAN, {255, 140, 155}, {255, 90, 115}},
    {"ばら星雲", "Rosette Nebula", "NGC 2237", DSO_RING, 98.438, 4.998, 80, 60, 0, 6.0, 5200, NAN, {255, 150, 165}, {255, 95, 120}},
    {"カリフォルニア星雲", "California Nebula", "NGC 1499", DSO_EMISSION, 60.80, 36.62, 145, 40, 130, 6.0, 1000, NAN, {255, 125, 140}, {255, 85, 110}},
    {"網状星雲（はくちょう座ループ）", "Veil Nebula (Cygnus Loop)", "", DSO_SHELL, 312.75, 30.67, 180, 160, 0, 7.0, 2400, NAN, {130, 220, 255}, {255, 110, 120}},
    {"かに星雲", "Crab Nebula", "M1", DSO_REMNANT, 83.633, 22.015, 7, 5, 125, 8.4, 6500, 1054.5, {170, 200, 255}, {255, 150, 100}},
    {"すばる（プレアデス星団）", "Pleiades", "M45", DSO_REFLECTION, 56.75, 24.117, 110, 110, 0, 6.0, 444, NAN, {185, 205, 255}, {140, 170, 255}},
    {"環状星雲", "Ring Nebula", "M57", DSO_PLANETARY, 283.396, 33.029, 1.4, 1.0, 60, 8.8, 2570, NAN, {150, 235, 215}, {255, 110, 110}},
    {"あれい状星雲", "Dumbbell Nebula", "M27", DSO_PLANETARY, 299.901, 22.721, 8.0, 5.6, 30, 7.5, 1360, NAN, {150, 225, 220}, {255, 120, 125}},
    {"らせん星雲", "Helix Nebula", "NGC 7293", DSO_PLANETARY, 337.411, -20.837, 25, 16, 120, 7.6, 650, NAN, {150, 220, 230}, {255, 105, 105}},
    {"オメガ星団", "Omega Centauri", "NGC 5139", DSO_GLOBULAR, 201.697, -47.479, 36.3, 36.3, 0, 3.9, 17090, NAN, {255, 235, 205}, {255, 230, 200}},
    {"きょしちょう座 47", "47 Tucanae", "NGC 104", DSO_GLOBULAR, 6.024, -72.081, 30.9, 30.9, 0, 4.1, 14700, NAN, {255, 235, 205}, {255, 230, 200}},
    {"ヘルクレス座球状星団", "Hercules Globular Cluster", "M13", DSO_GLOBULAR, 250.422, 36.460, 20, 20, 0, 5.8, 22200, NAN, {255, 238, 215}, {255, 232, 205}},
    {"いて座球状星団", "Sagittarius Globular Cluster", "M22", DSO_GLOBULAR, 279.100, -23.905, 32, 32, 0, 5.1, 10600, NAN, {255, 236, 210}, {255, 230, 200}},
};
static const int DEEP_SKY_COUNT = sizeof(DEEP_SKY) / sizeof(DEEP_SKY[0]);

// 平均の面輝度（等級/平方秒角）→ 描く明るさ 0〜1（明るい惑星状星雲や星雲の中心は濃く、大きく淡い銀河や超新星残骸は薄く）
static double DeepSkyBrightness(const DeepSky& o) {
    double sb = o.mag + 2.5 * std::log10(PI / 4 * o.a * o.b * 3600);
    return std::max(0.3, std::min(1.0, 0.45 + (23.5 - sb) * 0.18));
}

// テクスチャを作る雑音（値の雑音を 5 段重ねる）
static float DsoHash(int x, int y, int s) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)s * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)((h ^ (h >> 16)) & 0xffffff) / 16777215.0f;
}
static float DsoNoise(float x, float y, int s) {
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    float fx = x - xi, fy = y - yi;
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    float a = DsoHash(xi, yi, s), b = DsoHash(xi + 1, yi, s), c = DsoHash(xi, yi + 1, s), d = DsoHash(xi + 1, yi + 1, s);
    return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}
static float DsoFbm(float x, float y, int s) {
    float sum = 0, amp = 0.5f;
    for (int i = 0; i < 5; i++, amp *= 0.5f, x *= 2.03f, y *= 2.03f) sum += DsoNoise(x, y, s + i * 17) * amp;
    return sum / 0.97f;
}
static float SmoothStep(float e0, float e1, float x) {
    float t = std::max(0.0f, std::min(1.0f, (x - e0) / (e1 - e0)));
    return t * t * (3 - 2 * t);
}

// 天体 o の絵（加算で重ねるので、色に明るさを掛けた値。アルファは 255）。
// 横（u）が長径、縦（v）が短径の向きで、四角の端は長径・短径の 1.25 倍（はみ出す淡い光の分）。r = 1 が長径・短径の端
static Image DeepSkyImage(const DeepSky& o, int seed) {
    const int N = o.a >= 30 ? 256 : 128;
    Image img = GenImageColor(N, N, BLANK);
    Color* px = (Color*)img.data;
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            float u = ((x + 0.5f) / N * 2 - 1) * 1.25f, v = ((y + 0.5f) / N * 2 - 1) * 1.25f;
            float r = std::sqrt(u * u + v * v), th = std::atan2(v, u);
            float n = DsoFbm(u * 3 + 7, v * 3 + 3, seed), n2 = DsoFbm(u * 7 + 1, v * 7 + 5, seed + 101);
            float rgb[3] = {0, 0, 0};
            auto add = [&](const unsigned char* c, float k) {
                for (int i = 0; i < 3; i++) rgb[i] += c[i] * k;
            };
            switch (o.kind) {
            case DSO_SPIRAL: {   // 中心のふくらみ（黄色っぽい）と、2 本の腕（青白く、塵の暗い筋と赤い星形成領域）
                float w = th + seed * 1.7f - 2.6f * std::log(r + 0.03f) + 1.4f * (n - 0.5f);
                float arm = std::pow(0.5f + 0.5f * std::cos(2 * w), 1.6f) * SmoothStep(0.06f, 0.3f, r);
                float dust = std::pow(0.5f + 0.5f * std::cos(2 * w + 0.8f), 6.0f) * SmoothStep(0.1f, 0.25f, r);
                float disk = std::exp(-r / 0.33f) * (0.6f + 0.7f * arm * (0.5f + 0.9f * n)) * (1 - 0.45f * dust);
                float bulge = std::exp(-r / 0.085f);
                float mix = SmoothStep(0.05f, 0.5f, r);
                for (int i = 0; i < 3; i++) rgb[i] = (o.c1[i] * (1 - mix) + o.c2[i] * mix) * disk * 0.85f + o.c1[i] * bulge;
                if (n2 > 0.7f && arm > 0.45f && r > 0.15f) {
                    static const unsigned char pink[3] = {255, 120, 160};
                    add(pink, (n2 - 0.7f) * 2.5f * std::exp(-r / 0.5f));
                }
                break;
            }
            case DSO_EDGEON: {   // 横から見た円盤と中心のふくらみ、真ん中を横切る塵の帯
                float disk = std::exp(-r / 0.3f) * (0.75f + 0.5f * n), bulge = std::exp(-r / 0.12f);
                float lane = std::exp(-std::pow((v - 0.04f) / 0.12f, 2.0f)) * SmoothStep(0.8f, 0.3f, std::fabs(u));
                float k = 1 - 0.7f * lane * (0.8f + 0.4f * n2);
                add(o.c2, disk * k);
                add(o.c1, bulge * 1.2f * k);
                break;
            }
            case DSO_ELLIPTICAL:
                add(o.c1, std::exp(-r / 0.1f) * 0.9f + std::exp(-r / 0.3f) * 0.45f);
                break;
            case DSO_IRREGULAR: {   // 雑音でまだらにした光と、長径に沿った棒
                float base = std::exp(-r / 0.45f) * (0.25f + 1.4f * std::max(0.0f, n - 0.35f));
                float bar = std::exp(-std::pow(u / 0.45f, 2.0f) - std::pow((v + 0.08f) / 0.13f, 2.0f)) * (0.5f + n);
                add(o.c2, base);
                add(o.c1, bar * 0.65f);
                if (n2 > 0.74f) {
                    static const unsigned char pink[3] = {255, 120, 150};
                    add(pink, (n2 - 0.74f) * 2.0f * std::exp(-r / 0.6f));
                }
                break;
            }
            case DSO_EMISSION:
            case DSO_RING: {   // ゆがめた雑音の雲（水素の赤）と淡い暗い筋、中心は明るく別の色。ばら星雲は中心に穴
                float q = DsoFbm(u * 1.5f + 3, v * 1.5f + 8, seed + 33);
                float m = DsoFbm(u * 2.5f + 2.2f * q, v * 2.5f + 2.2f * q, seed + 55);
                float cloud = std::pow(std::max(0.0f, m - 0.2f) * 1.6f, 1.6f) * std::exp(-r * r / 0.38f) * 1.5f;
                cloud *= 1 - 0.45f * SmoothStep(0.55f, 0.75f, DsoFbm(u * 4 + q, v * 4, seed + 77));
                if (o.kind == DSO_RING) cloud *= SmoothStep(0.15f, 0.45f, r) * 1.3f;
                float core = o.kind == DSO_RING ? 0 : std::exp(-r / 0.1f) * 0.8f;
                float mix = std::exp(-r / 0.22f);
                for (int i = 0; i < 3; i++) rgb[i] = (o.c1[i] * mix + o.c2[i] * (1 - mix)) * cloud + o.c1[i] * core;
                break;
            }
            case DSO_REFLECTION: {   // 青い淡い光の筋
                float streak = DsoFbm(u * 1.6f + v * 0.9f + 11, v * 7 - u * 2 + 3, seed + 7);
                add(o.c1, std::pow(std::max(0.0f, streak - 0.3f) * 1.8f, 1.6f) * std::exp(-r * r / 0.45f));
                break;
            }
            case DSO_PLANETARY: {   // 環: 内側は青緑、外の縁は赤
                float ring = std::exp(-std::pow((r - 0.62f) / 0.2f, 2.0f)), inner = SmoothStep(0.65f, 0.0f, r) * 0.55f;
                float k = 0.75f + 0.5f * n;
                add(o.c1, inner * k + ring * 0.5f * k);
                add(o.c2, std::exp(-std::pow((r - 0.78f) / 0.14f, 2.0f)) * 0.8f * k);
                break;
            }
            case DSO_REMNANT: {   // かに星雲: 網目のような赤い筋と、内側の青白い光
                float fil = std::pow(1 - std::fabs(2 * DsoFbm(u * 3.5f + 2, v * 3.5f + 4, seed + 13) - 1), 4.0f) * std::exp(-r * r / 0.45f);
                add(o.c2, fil * 0.9f + std::exp(-r * r / 0.35f) * 0.35f);
                add(o.c1, std::exp(-r * r / 0.15f) * 0.5f);
                break;
            }
            case DSO_SHELL: {   // 網状星雲: 途切れ途切れの細い弧（何本か重なる）
                float m = SmoothStep(0.35f, 0.65f, n2);
                for (int j = 0; j < 3; j++) {
                    float d = r - (0.72f + 0.08f * j + 0.2f * (DsoFbm(u * 1.8f + j * 3, v * 1.8f, seed + j) - 0.5f));
                    float fil = std::exp(-std::pow(d / 0.025f, 2.0f)) * SmoothStep(0.35f, 0.55f, DsoFbm(u * 2.5f + 5 + j, v * 2.5f + 9, seed + 3 + j));
                    for (int i = 0; i < 3; i++) rgb[i] += (o.c1[i] * m + o.c2[i] * (1 - m)) * fil * 0.6f;
                }
                break;
            }
            case DSO_GLOBULAR: {   // 中心に集まった無数の星
                add(o.c1, std::exp(-std::pow(r / 0.28f, 2.0f)) * 0.7f + std::exp(-r / 0.06f) * 0.5f);
                if (DsoHash(x, y, seed + 9) < 0.5f * std::exp(-std::pow(r / 0.4f, 2.0f))) add(o.c2, 0.6f);
                break;
            }
            }
            float fade = SmoothStep(1.25f, 1.0f, r);
            px[y * N + x] = {(unsigned char)std::min(255.0f, rgb[0] * fade), (unsigned char)std::min(255.0f, rgb[1] * fade),
                             (unsigned char)std::min(255.0f, rgb[2] * fade), 255};
        }
    return img;
}

static std::vector<Texture2D> MakeDeepSkyTextures() {
    std::vector<Texture2D> tex;
    for (int i = 0; i < DEEP_SKY_COUNT; i++) {
        Image img = DeepSkyImage(DEEP_SKY[i], i * 31 + 5);
        Texture2D t = LoadTextureFromImage(img);
        SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
        UnloadImage(img);
        tex.push_back(t);
    }
    return tex;
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
