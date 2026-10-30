// Solar System Voyager — Copyright (c) 2026 hubstacksea
// MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
// 背景の星・小惑星帯などの粒。
// main.cpp の一部で、main.cpp が決まった順に読み込む（単体ではコンパイルしない）

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
