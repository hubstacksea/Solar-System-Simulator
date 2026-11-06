// Solar System Voyager — Copyright (c) 2026 hubstacksea
// MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
// エンジン・天体・小天体の形・探査機の軌跡。
// main.cpp の一部で、main.cpp が決まった順に読み込む（単体ではコンパイルしない）

// ---------------------------------------------------------------- エンジン
struct Engine {
    const char* jp;
    const char* en;
    double accel;  // m/s^2
    double vmax;   // m/s
    int dialUnit;  // 0: km/h, 1: km/s, 2: c, 3: 光年/秒
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
    // 遊び用の架空のエンジン: 1 秒に 1 光年（光の約 3,000 万倍）。約 2 秒で最高速度に
    {"超光速", "Hyperdrive", 9460730472580800.0 / 2, 9460730472580800.0, 3, {120, 220, 255, 255}},
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
    double orbitT = -1e9;   // 軌道線を計算した時刻（月と水星〜海王星）
    bool active = true;     // 探査機: その時刻に飛んでいるか（打ち上げ前・運用終了後は false で、表示しない）
    int shape = -1;         // 実際の形（g_shapes の番号。-1 は楕円体）
    int map = -1;           // 探査機の画像による地図（App::maps の番号。-1 はなし）
    double primeOffset = 0; // 自転角 0 の X 軸から IAU の基準（赤道と ICRF 赤道の昇交点）までの角度 rad
    V3 pole0, ex0, ez0;     // J2000 の自転軸と赤道の基底（地球は歳差でここから回す）
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

// ΔT（地球の自転の遅れ。地球時 − 世界時）日。惑星・地球・月の位置は地球時で、自転は世界時で求める。
// 紀元前 500 年〜西暦 2150 年は Espenak & Meeus の多項式（NASA の日食表と同じ。紀元前 500 年で約 4.8 時間、2026 年で約 69 秒）、
// その外は Morrison & Stephenson (2004) の放物線 −20 + 32u² 秒（u は 1820 年からの世紀。境目でつながる）
static double DeltaTSeconds(double y) {
    auto par = [](double y) { double u = (y - 1820) / 100; return -20 + 32 * u * u; };
    if (y < -500 || y >= 2150) return par(y);
    double u, t;
    if (y < 500) {
        u = y / 100;
        return 10583.6 - 1014.41 * u + 33.78311 * u * u - 5.952053 * u * u * u - 0.1798452 * std::pow(u, 4) + 0.022174192 * std::pow(u, 5) +
               0.0090316521 * std::pow(u, 6);
    }
    if (y < 1600) {
        u = (y - 1000) / 100;
        return 1574.2 - 556.01 * u + 71.23472 * u * u + 0.319781 * u * u * u - 0.8503463 * std::pow(u, 4) - 0.005050998 * std::pow(u, 5) +
               0.0083572073 * std::pow(u, 6);
    }
    if (y < 1700) { t = y - 1600; return 120 - 0.9808 * t - 0.01532 * t * t + t * t * t / 7129; }
    if (y < 1800) { t = y - 1700; return 8.83 + 0.1603 * t - 0.0059285 * t * t + 0.00013336 * t * t * t - std::pow(t, 4) / 1174000; }
    if (y < 1860) {
        t = y - 1800;
        return 13.72 - 0.332447 * t + 0.0068612 * t * t + 0.0041116 * t * t * t - 0.00037436 * std::pow(t, 4) + 0.0000121272 * std::pow(t, 5) -
               0.0000001699 * std::pow(t, 6) + 0.000000000875 * std::pow(t, 7);
    }
    if (y < 1900) { t = y - 1860; return 7.62 + 0.5737 * t - 0.251754 * t * t + 0.01680668 * t * t * t - 0.0004473624 * std::pow(t, 4) + std::pow(t, 5) / 233174; }
    if (y < 1920) { t = y - 1900; return -2.79 + 1.494119 * t - 0.0598939 * t * t + 0.0061966 * t * t * t - 0.000197 * std::pow(t, 4); }
    if (y < 1941) { t = y - 1920; return 21.20 + 0.84493 * t - 0.076100 * t * t + 0.0020936 * t * t * t; }
    if (y < 1961) { t = y - 1950; return 29.07 + 0.407 * t - t * t / 233 + t * t * t / 2547; }
    if (y < 1986) { t = y - 1975; return 45.45 + 1.067 * t - t * t / 260 - t * t * t / 718; }
    if (y < 2005) {
        t = y - 2000;
        return 63.86 + 0.3345 * t - 0.060374 * t * t + 0.0017275 * t * t * t + 0.000651814 * std::pow(t, 4) + 0.00002373599 * std::pow(t, 5);
    }
    if (y < 2050) { t = y - 2000; return 62.92 + 0.32217 * t + 0.005589 * t * t; }
    return par(y) - 0.5628 * (2150 - y);
}
static double DeltaTDays(double tDays) { return DeltaTSeconds(2000 + tDays / 365.25) / 86400.0; }

// 惑星の位置: VSOP87A（src/vsop87_data.h）。k は VSOP_NAMES（src/solar_data.h）の番号（地球は地球と月の重心）、
// tt は地球時の J2000 からの日数。太陽中心の位置 km
static V3 VsopSeries(int k, double tt) {
    double t = tt / 365250.0, xyz[3];
    for (int c = 0; c < 3; c++) {
        double s = 0, tp = 1;
        for (int p = 0; p < 6; p++, tp *= t) {
            double sp = 0;
            for (int i = VSOP_INDEX[k][c][p]; i < VSOP_INDEX[k][c][p + 1]; i++) sp += VSOP_TERMS[i].a * std::cos(VSOP_TERMS[i].b + VSOP_TERMS[i].c * t);
            s += sp * tp;
        }
        xyz[c] = s * AU_KM;
    }
    return EclToRender(xyz[0], xyz[1], xyz[2]);
}
// 級数は項が多く、イベントを探す時などに近い時刻を何千回も計算するので、惑星ごとに決まった間隔の時刻（格子点）の値を
// 覚えておき、まわりの 4 点の 3 次式で補間する。格子は時刻だけで決まるので、計算の順番で結果は変わらない（補間の誤差は 0.1 km 以下）
static V3 VsopPos(int k, double tt) {
    static const double STEP[8] = {0.125, 0.5, 0.5, 1, 4, 4, 8, 8};   // 日
    struct Slot { long n; V3 p; };
    static Slot cache[8][16];
    static bool init = false;
    if (!init) {
        for (auto& row : cache)
            for (Slot& s : row) s.n = LONG_MIN;
        init = true;
    }
    double h = STEP[k], f = tt / h;
    long n0 = (long)std::floor(f);
    double u = f - n0;
    V3 P[4];
    for (int j = 0; j < 4; j++) {
        long n = n0 - 1 + j;
        Slot& s = cache[k][n & 15];
        if (s.n != n) {
            s.n = n;
            s.p = VsopSeries(k, n * h);
        }
        P[j] = s.p;
    }
    return P[0] * (-u * (u - 1) * (u - 2) / 6) + P[1] * ((u + 1) * (u - 1) * (u - 2) / 2) + P[2] * (-(u + 1) * u * (u - 2) / 2) +
           P[3] * ((u + 1) * u * (u - 1) / 6);
}

// 歳差（IAU 1976、Lieske et al. 1977）: J2000 の平均赤道から時刻 t の平均赤道への回転の逆 Pᵀ を、描画座標のベクトル v に
// 掛ける。地球の向き（自転軸と赤道）を J2000 の向きから回すのに使う。何千年も離れると自転軸の向きが十数度変わる
static V3 PrecessFromJ2000TT(V3 v, double T) {   // T: 地球時の J2000 からの世紀
    double as = DEG / 3600.0;
    double zeta = (2306.2181 * T + 0.30188 * T * T + 0.017998 * T * T * T) * as;
    double z = (2306.2181 * T + 1.09468 * T * T + 0.018203 * T * T * T) * as;
    double th = (2004.3109 * T - 0.42665 * T * T - 0.041833 * T * T * T) * as;
    V3 ex = EquVecToRender(1, 0, 0), ey = EquVecToRender(0, 1, 0), ez = EquVecToRender(0, 0, 1);
    double x = Dot(v, ex), y = Dot(v, ey), w = Dot(v, ez);   // 赤道座標の成分
    auto r3 = [](double a, double& x, double& y) { double c = std::cos(a), s = std::sin(a), nx = c * x + s * y; y = -s * x + c * y; x = nx; };
    auto r2 = [](double a, double& x, double& z) { double c = std::cos(a), s = std::sin(a), nx = c * x - s * z; z = s * x + c * z; x = nx; };
    r3(z, x, y);     // Pᵀ = R3(ζ) R2(−θ) R3(z)（P = R3(−z) R2(θ) R3(−ζ)）
    r2(-th, x, w);
    r3(zeta, x, y);
    return ex * x + ey * y + ez * w;
}
static V3 PrecessFromJ2000(V3 v, double tDays) { return PrecessFromJ2000TT(v, (tDays + DeltaTDays(tDays)) / 36525.0); }

// ハレー彗星の回帰ごとの軌道（JPL Horizons の 1P/Halley の各回帰の解。紀元前 240 年〜1910 年の 30 回。J2000 黄道）。
// 近日点通過 [J2000 からの日数（地球時）], 近日点距離 [AU], 離心率, 傾斜角・昇交点・近点引数 [deg]。1948 年（1910 年と
// 1986 年の近日点の中間）より前は、時刻に一番近い回帰の軌道で動かす（それより後は今の軌道要素）
static const struct { double tp, q, e, i, node, peri; } HALLEY_APPS[] = {
    {-817637.3800, 0.585400, 0.967600, 163.4700, 30.8100, 88.1100},
    {-789706.9300, 0.584500, 0.967700, 163.7000, 32.0600, 89.1100},
    {-761681.0380, 0.585600, 0.967680, 163.3400, 34.0180, 90.7780},
    {-734221.6510, 0.587200, 0.967370, 163.5890, 35.9040, 92.5590},
    {-706355.5400, 0.585100, 0.967550, 163.5770, 36.1290, 92.6520},
    {-678906.0660, 0.583140, 0.967840, 163.4370, 37.2190, 93.6940},
    {-650725.7770, 0.581470, 0.967980, 163.5740, 37.9080, 94.1470},
    {-622629.1020, 0.575910, 0.968750, 163.3670, 39.1110, 95.2410},
    {-593837.1580, 0.577190, 0.968590, 163.5420, 40.5790, 96.5100},
    {-565581.2510, 0.573740, 0.968910, 163.4790, 41.2100, 97.0280},
    {-536635.3700, 0.575590, 0.968710, 163.3940, 41.9740, 97.5820},
    {-508707.0240, 0.580830, 0.968040, 163.4760, 43.2610, 98.7990},
    {-480380.7330, 0.579580, 0.968150, 163.4180, 43.8000, 99.1490},
    {-452756.8290, 0.581840, 0.967850, 163.4430, 44.6870, 99.9970},
    {-424714.2300, 0.582320, 0.967810, 163.4470, 44.9300, 100.1010},
    {-397179.8260, 0.580160, 0.968070, 163.3110, 45.6460, 100.7770},
    {-369006.8120, 0.581910, 0.967890, 163.3990, 46.5610, 101.4840},
    {-341051.5660, 0.574500, 0.968870, 163.1120, 47.6240, 102.4730},
    {-312167.9390, 0.574790, 0.968790, 163.2240, 49.0540, 103.7040},
    {-283880.6770, 0.574210, 0.968840, 163.1920, 49.3040, 103.8490},
    {-254998.9180, 0.572710, 0.968930, 163.0760, 50.1520, 104.5000},
    {-226858.8130, 0.576200, 0.968370, 163.1130, 51.0200, 105.2950},
    {-198522.8670, 0.579700, 0.968000, 162.8900, 51.8660, 105.8350},
    {-171052.2610, 0.581200, 0.967750, 162.9170, 53.0570, 106.9760},
    {-143240.9594, 0.583615, 0.967490, 162.9055, 53.7701, 107.5503},
    {-115889.2206, 0.582621, 0.967933, 162.2649, 55.5669, 109.2214},
    {-87952.4377, 0.584466, 0.967686, 162.3725, 57.2458, 110.7093},
    {-59946.0604, 0.586563, 0.967394, 162.2588, 57.5185, 110.7043},
    {-32763.3215, 0.587208, 0.967302, 162.2186, 58.5629, 111.7371},
};
static const double HALLEY_SWITCH = -18919.0;   // 1948 年

static V3 PlanetPos(const BodyDef& d0, double tDays) {
    if (d0.halley && tDays < HALLEY_SWITCH) {
        double tt = tDays + DeltaTDays(tDays);
        int best = 0;
        for (int k = 1; k < (int)(sizeof HALLEY_APPS / sizeof HALLEY_APPS[0]); k++)
            if (std::fabs(HALLEY_APPS[k].tp - tt) < std::fabs(HALLEY_APPS[best].tp - tt)) best = k;
        const auto& h = HALLEY_APPS[best];
        BodyDef d = d0;
        d.a_au = h.q / (1 - h.e); d.e = h.e; d.inc = h.i; d.node = h.node; d.varpi = h.node + h.peri;
        double n = 0.9856076686 / std::pow(d.a_au, 1.5);   // 平均運動 deg/日
        double M = std::remainder(n * (tt - h.tp), 360.0) * DEG;
        double E = M + 0.85 * d.e * (M >= 0 ? 1 : -1);
        for (int i = 0; i < 50; i++) {
            double dE = (E - d.e * std::sin(E) - M) / (1 - d.e * std::cos(E));
            E -= dE;
            if (std::fabs(dE) < 1e-12) break;
        }
        return PlanetOrbitPoint(d, E);
    }
    if (d0.parent == 0) tDays += DeltaTDays(tDays);
    if (d0.vsop >= 0) return VsopPos(d0.vsop, tDays);
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

// 月の地心位置 km（J2000 黄道系→描画座標）。Meeus『Astronomical Algorithms』第 2 版 47 章（ELP-2000/82 を縮めたもの。
// 経度・距離 60 項、緯度 60 項。精度は経度 約 10″、緯度 約 4″）。係数は soniakeys/meeus（MIT）の表から書き写した
struct MoonTermLR { signed char D, M, Mp, F; double l, r; };   // 経度 1e-6 deg、距離 1e-3 km
struct MoonTermB { signed char D, M, Mp, F; double b; };        // 緯度 1e-6 deg
static const MoonTermLR MOON_LR[] = {
    {0, 0, 1, 0, 6288774, -20905355}, {2, 0, -1, 0, 1274027, -3699111}, {2, 0, 0, 0, 658314, -2955968}, {0, 0, 2, 0, 213618, -569925},
    {0, 1, 0, 0, -185116, 48888}, {0, 0, 0, 2, -114332, -3149}, {2, 0, -2, 0, 58793, 246158}, {2, -1, -1, 0, 57066, -152138},
    {2, 0, 1, 0, 53322, -170733}, {2, -1, 0, 0, 45758, -204586}, {0, 1, -1, 0, -40923, -129620}, {1, 0, 0, 0, -34720, 108743},
    {0, 1, 1, 0, -30383, 104755}, {2, 0, 0, -2, 15327, 10321}, {0, 0, 1, 2, -12528, 0}, {0, 0, 1, -2, 10980, 79661},
    {4, 0, -1, 0, 10675, -34782}, {0, 0, 3, 0, 10034, -23210}, {4, 0, -2, 0, 8548, -21636}, {2, 1, -1, 0, -7888, 24208},
    {2, 1, 0, 0, -6766, 30824}, {1, 0, -1, 0, -5163, -8379}, {1, 1, 0, 0, 4987, -16675}, {2, -1, 1, 0, 4036, -12831},
    {2, 0, 2, 0, 3994, -10445}, {4, 0, 0, 0, 3861, -11650}, {2, 0, -3, 0, 3665, 14403}, {0, 1, -2, 0, -2689, -7003},
    {2, 0, -1, 2, -2602, 0}, {2, -1, -2, 0, 2390, 10056}, {1, 0, 1, 0, -2348, 6322}, {2, -2, 0, 0, 2236, -9884},
    {0, 1, 2, 0, -2120, 5751}, {0, 2, 0, 0, -2069, 0}, {2, -2, -1, 0, 2048, -4950}, {2, 0, 1, -2, -1773, 4130},
    {2, 0, 0, 2, -1595, 0}, {4, -1, -1, 0, 1215, -3958}, {0, 0, 2, 2, -1110, 0}, {3, 0, -1, 0, -892, 3258},
    {2, 1, 1, 0, -810, 2616}, {4, -1, -2, 0, 759, -1897}, {0, 2, -1, 0, -713, -2117}, {2, 2, -1, 0, -700, 2354},
    {2, 1, -2, 0, 691, 0}, {2, -1, 0, -2, 596, 0}, {4, 0, 1, 0, 549, -1423}, {0, 0, 4, 0, 537, -1117},
    {4, -1, 0, 0, 520, -1571}, {1, 0, -2, 0, -487, -1739}, {2, 1, 0, -2, -399, 0}, {0, 0, 2, -2, -381, -4421},
    {1, 1, 1, 0, 351, 0}, {3, 0, -2, 0, -340, 0}, {4, 0, -3, 0, 330, 0}, {2, -1, 2, 0, 327, 0},
    {0, 2, 1, 0, -323, 1165}, {1, 1, -1, 0, 299, 0}, {2, 0, 3, 0, 294, 0}, {2, 0, -1, -2, 0, 8752},
};
static const MoonTermB MOON_B[] = {
    {0, 0, 0, 1, 5128122}, {0, 0, 1, 1, 280602}, {0, 0, 1, -1, 277693}, {2, 0, 0, -1, 173237},
    {2, 0, -1, 1, 55413}, {2, 0, -1, -1, 46271}, {2, 0, 0, 1, 32573}, {0, 0, 2, 1, 17198},
    {2, 0, 1, -1, 9266}, {0, 0, 2, -1, 8822}, {2, -1, 0, -1, 8216}, {2, 0, -2, -1, 4324},
    {2, 0, 1, 1, 4200}, {2, 1, 0, -1, -3359}, {2, -1, -1, 1, 2463}, {2, -1, 0, 1, 2211},
    {2, -1, -1, -1, 2065}, {0, 1, -1, -1, -1870}, {4, 0, -1, -1, 1828}, {0, 1, 0, 1, -1794},
    {0, 0, 0, 3, -1749}, {0, 1, -1, 1, -1565}, {1, 0, 0, 1, -1491}, {0, 1, 1, 1, -1475},
    {0, 1, 1, -1, -1410}, {0, 1, 0, -1, -1344}, {1, 0, 0, -1, -1335}, {0, 0, 3, 1, 1107},
    {4, 0, 0, -1, 1021}, {4, 0, -1, 1, 833}, {0, 0, 1, -3, 777}, {4, 0, -2, 1, 671},
    {2, 0, 0, -3, 607}, {2, 0, 2, -1, 596}, {2, -1, 1, -1, 491}, {2, 0, -2, 1, -451},
    {0, 0, 3, -1, 439}, {2, 0, 2, 1, 422}, {2, 0, -3, -1, 421}, {2, 1, -1, 1, -366},
    {2, 1, 0, 1, -351}, {4, 0, 0, 1, 331}, {2, -1, 1, 1, 315}, {2, -2, 0, -1, 302},
    {0, 0, 1, 3, -283}, {2, 1, 1, -1, -229}, {1, 1, 0, -1, 223}, {1, 1, 0, 1, 223},
    {0, 1, -2, -1, -220}, {2, 1, -1, -1, -220}, {1, 0, 1, 1, -185}, {2, -1, -2, -1, 181},
    {0, 1, 2, 1, -177}, {4, 0, -2, -1, 176}, {4, -1, -1, -1, 166}, {1, 0, 1, -1, -164},
    {4, 0, 1, -1, 132}, {1, 0, -1, -1, -119}, {4, -1, 0, -1, 115}, {2, -2, 0, 1, 107},
};

static V3 MoonGeoPos(double tDays) {
    tDays += DeltaTDays(tDays);   // 地球時
    double T = tDays / 36525.0, T2 = T * T, T3 = T2 * T, T4 = T3 * T;
    double Lp = 218.3164477 + 481267.88123421 * T - 0.0015786 * T2 + T3 / 538841 - T4 / 65194000;
    double D = 297.8501921 + 445267.1114034 * T - 0.0018819 * T2 + T3 / 545868 - T4 / 113065000;
    double M = 357.5291092 + 35999.0502909 * T - 0.0001535 * T2 + T3 / 24490000;
    double Mp = 134.9633964 + 477198.8675055 * T + 0.0087414 * T2 + T3 / 69699 - T4 / 14712000;
    double F = 93.2720950 + 483202.0175233 * T - 0.0036539 * T2 - T3 / 3526000 + T4 / 863310000;
    double A1 = 119.75 + 131.849 * T, A2 = 53.09 + 479264.290 * T, A3 = 313.45 + 481266.484 * T;
    double E = 1 - 0.002516 * T - 0.0000074 * T2;
    auto sd = [](double deg) { return std::sin(deg * DEG); };
    auto cd = [](double deg) { return std::cos(deg * DEG); };
    auto ek = [&](int m) { return m == 0 ? 1.0 : (m == 1 || m == -1) ? E : E * E; };   // 太陽の近点角を含む項は離心率の減少で弱まる
    double sl = 3958 * sd(A1) + 1962 * sd(Lp - F) + 318 * sd(A2), sr = 0;
    double sb = -2235 * sd(Lp) + 382 * sd(A3) + 175 * sd(A1 - F) + 175 * sd(A1 + F) + 127 * sd(Lp - Mp) - 115 * sd(Lp + Mp);
    for (const MoonTermLR& t : MOON_LR) {
        double a = D * t.D + M * t.M + Mp * t.Mp + F * t.F, k = ek(t.M);
        sl += t.l * sd(a) * k;
        sr += t.r * cd(a) * k;
    }
    for (const MoonTermB& t : MOON_B) sb += t.b * sd(D * t.D + M * t.M + Mp * t.Mp + F * t.F) * ek(t.M);
    double lam = Lp + sl * 1e-6, bet = sb * 1e-6, r = 385000.56 + sr * 1e-3;
    // 何千年も離れると、この式の月は精密な天体暦（JPL Horizons の DE441）より遅れていく（紀元前 600 年で約 0.37°）。
    // 紀元前 2950 年〜西暦 2800 年の 250 年ごとの Horizons の位置との差に合わせた 3 次式で補正する（残りの差は 2 乗平均 85″）
    lam -= (45.335 * T - 0.33715 * T2 - 0.011130 * T3) / 3600.0;
    // λ・β はその日時の黄道と分点から測った値。その日時の黄道傾斜角で赤道座標にして、歳差で J2000 へ戻す
    // （黄道面も 100 年に約 47″ 傾いていくので、何千年も離れると緯度を J2000 の黄道のまま扱うと 0.3° 以上ずれる）
    double eps = 23.43929111 - 0.0130042 * T - 1.64e-7 * T2 + 5.04e-7 * T3;
    double x = r * cd(bet) * cd(lam), y = r * cd(bet) * sd(lam), z = r * sd(bet);
    return PrecessFromJ2000TT(EquVecToRender(x, y * cd(eps) - z * sd(eps), y * sd(eps) + z * cd(eps)), T);
}

// 地球の中心。軌道要素は地球と月の重心のものなので、月の位置の分だけずらす（月の質量比 0.0121505）
static V3 EarthPos(const BodyDef& d, double tDays) { return PlanetPos(d, tDays) - MoonGeoPos(tDays) * 0.0121505; }

// ---------------------------------------------------------------- 太陽系の外の恒星（src/nearby_stars.h）
// 系の重心は、元期の位置から等速直線運動（固有運動と視線速度）で動く。連星の星は、重心から相対軌道 × 係数の所にいる
static const double ARCSEC = PI / (180.0 * 3600.0);

// 重心の方向の基底（描画座標）: 太陽から見た方向 u・天の北 n・東 e。元期の値のまま使う（数百年で 0.1° も変わらない）
static void StarBasis(const NearbyStar& s, V3* u, V3* n, V3* e) {
    double ra = s.ra * DEG, dec = s.dec * DEG;
    *u = EquVecToRender(std::cos(dec) * std::cos(ra), std::cos(dec) * std::sin(ra), std::sin(dec));
    *n = EquVecToRender(-std::sin(dec) * std::cos(ra), -std::sin(dec) * std::sin(ra), std::cos(dec));
    *e = EquVecToRender(-std::sin(ra), std::cos(ra), 0);
}
static double StarDistance(const NearbyStar& s) { return PC_KM * 1000.0 / s.plx; }

// 星空の年代（設定）: 恒星と背景の星だけをこの年数だけ先（負なら前）の位置に動かす。惑星などの日時は変えない
static double g_skyEpochYears = 0;
static const double SKY_EPOCHS[] = {-100000, -50000, -20000, -10000, -5000, -1000, 0, 1000, 5000, 10000, 20000, 50000, 100000};
static const int SKY_EPOCH_COUNT = sizeof SKY_EPOCHS / sizeof SKY_EPOCHS[0];

static V3 StarBarycenter(const NearbyStar& s, double tDays) {
    tDays += g_skyEpochYears * 365.25;
    V3 u, n, e;
    StarBasis(s, &u, &n, &e);
    double dist = StarDistance(s);
    double k = 1e-3 * ARCSEC * dist / (365.25 * 86400);   // mas/年 → km/s
    V3 vel = u * s.rv + e * (s.pmra * k) + n * (s.pmdec * k);
    return u * dist + vel * ((tDays - (s.epoch - 2000.0) * 365.25) * 86400.0);
}

// 星のまわりの軌道の上の位置 km（天球の北・東・奥の基底の要素。周期・近点通過は日、傾斜角などは deg）。奥行きは太陽から遠ざかる向きが正
static V3 SkyOrbit(const NearbyStar& s, double P, double tp, double e, double a, double inc, double node, double peri, double tDays) {
    V3 u, n, ea;
    StarBasis(s, &u, &n, &ea);
    double M = std::remainder(2 * PI * (tDays - tp) / P, 2 * PI);
    double E = M + 0.85 * e * (M >= 0 ? 1 : -1);
    for (int it = 0; it < 50; it++) {
        double dE = (E - e * std::sin(E) - M) / (1 - e * std::cos(E));
        E -= dE;
        if (std::fabs(dE) < 1e-12) break;
    }
    double nu = 2 * std::atan2(std::sqrt(1 + e) * std::sin(E / 2), std::sqrt(1 - e) * std::cos(E / 2));
    double r = a * (1 - e * std::cos(E)), w = peri * DEG + nu;
    double O = node * DEG, ci = std::cos(inc * DEG), si = std::sin(inc * DEG);
    double north = r * (std::cos(w) * std::cos(O) - std::sin(w) * std::sin(O) * ci);
    double east = r * (std::cos(w) * std::sin(O) + std::sin(w) * std::cos(O) * ci);
    return n * north + ea * east + u * (r * std::sin(w) * si);
}

// 連星: 伴星の主星に対する位置 km
static V3 BinaryRelative(const NearbyStar& s, double tDays) {
    return SkyOrbit(s, s.P * 365.25, (s.T - 2000.0) * 365.25, s.e, s.a * ARCSEC * StarDistance(s), s.i, s.node, s.peri, tDays);
}

// 太陽系の外の惑星: 恒星に対する位置 km（host は恒星の BodyDef）
static V3 ExoRelative(const BodyDef& d, const BodyDef& host, double tDays) {
    const NearbyPlanet& p = NEARBY_PLANETS[d.exo];
    return SkyOrbit(NEARBY_STARS[host.star], p.P, p.tp, p.e, p.a * AU_KM, p.i, p.node, p.peri, tDays);
}

static V3 StarPos(const BodyDef& d, double tDays) {
    const NearbyStar& s = NEARBY_STARS[d.star];
    V3 p = StarBarycenter(s, tDays);
    return s.P > 0 ? p + BinaryRelative(s, tDays) * s.k : p;
}

// 見かけの等級（V）。距離 km から。太陽は絶対等級 4.83
static double StarAbsMag(const BodyDef& d) { return d.star >= 0 ? NEARBY_STARS[d.star].absMag : 4.83; }
static double StarApparentMag(const BodyDef& d, double distKm) {
    return StarAbsMag(d) + 5 * std::log10(std::max(distKm, 1.0) / PC_KM) - 5;
}

// 表面の描き方: 0 太陽型（粒状斑と低緯度の黒点）, 1 赤色矮星（大きな黒点が高緯度まで）, 2 高温の星（模様が淡い）,
// 3 白色矮星（なめらかで青白い）, 4 褐色矮星（雲の帯とまだらな雲）, 5 速く自転する高温の星（重力減光: 極が熱く明るい）
static int StarSurface(const BodyDef& d) {
    if (d.star < 0) return 0;
    const NearbyStar& ns = NEARBY_STARS[d.star];
    return ns.kind == 1 ? 3 : ns.kind == 2 ? 4 : ns.flattening > 0.05 ? 5 : ns.teff > 7500 ? 2 : ns.kind == 0 && ns.teff < 4000 ? 1 : 0;
}

// 太陽の黒点の活動（日付ごと）。周期の始まり（極小）の年は、観測で決まっている太陽活動周期の極小の年（第 1〜25 周期の
// 始まりの 1755〜2020 年。1712〜1745 年もそれ以前の極小）。黒点の量は平均的な周期の形の目安で、マウンダー極小期
// （1645〜1715 年）はほぼ 0、ダルトン極小期（1798〜1823 年）は弱く、表の前後は 11 年周期とする（各周期の強さの違いは入れない）
static const double SUNSPOT_MINIMA[] = {1712.0, 1723.5, 1734.0, 1745.0, 1755.2, 1766.5, 1775.5, 1784.7, 1798.3, 1810.6, 1823.3,
                                        1833.9, 1843.5, 1856.0, 1867.2, 1879.0, 1890.2, 1902.0, 1913.6, 1923.6, 1933.7, 1944.1,
                                        1954.3, 1964.8, 1976.2, 1986.7, 1996.5, 2009.0, 2020.0};
// 周期の中の形: 始まりから 4 割ほどで最大になる（x は周期の中の位置 0〜1）
static double CycleShape(double x) { double s = std::sin(PI * std::pow(std::max(0.0, std::min(1.0, x)), 0.75)); return s * s; }
// 年 y の黒点数の目安（年平均）と、周期の中の位置 x（0: 極小、1: 次の極小）
static void SunspotActivity(double y, double* ssn, double* x) {
    const int nM = sizeof SUNSPOT_MINIMA / sizeof SUNSPOT_MINIMA[0];
    const double mEnd = SUNSPOT_MINIMA[nM - 1];
    if (y >= SUNSPOT_MINIMA[0] && y < mEnd) {
        int k = 0;
        while (k + 1 < nM && SUNSPOT_MINIMA[k + 1] <= y) k++;
        *x = (y - SUNSPOT_MINIMA[k]) / (SUNSPOT_MINIMA[k + 1] - SUNSPOT_MINIMA[k]);
    } else {
        double a = y >= mEnd ? mEnd : SUNSPOT_MINIMA[0];
        *x = (y - a) / 11.0 - std::floor((y - a) / 11.0);
    }
    double amp = 120;                              // 平均的な周期の最大の黒点数（目安）
    if (y >= 1645 && y < 1715) amp = 3;            // マウンダー極小期
    else if (y >= 1798.3 && y < 1823.3) amp = 50;  // ダルトン極小期
    *ssn = amp * CycleShape(*x);
}

// 恒星の明るさ（太陽 = 1）: 半径² × (表面温度 / 5772 K)⁴
static double StarLuminosity(const BodyDef& d) {
    if (d.star < 0) return 1;
    const NearbyStar& ns = NEARBY_STARS[d.star];
    return ns.radius * ns.radius * std::pow(ns.teff / 5772.0, 4);
}
// ハビタブルゾーン（水が液体でいられる範囲）の内側・外側の半径 km。Kopparapu et al. (2013) の保守的な範囲の目安
static double HabitableInner(const BodyDef& d) { return std::sqrt(StarLuminosity(d) / 1.1) * AU_KM; }
static double HabitableOuter(const BodyDef& d) { return std::sqrt(StarLuminosity(d) / 0.36) * AU_KM; }

// フレア星（ときどき数分だけ明るくなる赤色矮星）。くじら座 UV 星は 1 分たらずで何倍にも明るくなる
static bool IsFlareStar(const BodyDef& d) {
    if (d.star < 0) return false;
    for (const char* n : {"Proxima Centauri", "UV Ceti", "BL Ceti", "Wolf 359", "Ross 154", "40 Eridani C"})
        if (std::strcmp(NEARBY_STARS[d.star].en, n) == 0) return true;
    return false;
}

// フレアの明るさ 0〜1（時刻 tDays のシミュレーションの時間で）。2 分ごとの区切りの 35% でフレアが始まり、
// 8 秒で明るくなって約 70 秒で減っていく（実際より頻繁に起こす）。seq はフレアの通し番号（場所を決める）
static double FlareLevel(const Body& b, int idx, double tDays, int* seq) {
    *seq = 0;
    if (!IsFlareStar(b.d)) return 0;
    const double slot = 2.0 / 1440;
    long long k0 = (long long)std::floor(tDays / slot);
    double best = 0;
    for (long long k = k0 - 4; k <= k0; k++) {
        uint64_t h = (uint64_t)k * 0x9E3779B97F4A7C15ull ^ (uint64_t)(idx + 1) * 0xBF58476D1CE4E5B9ull;
        h ^= h >> 31; h *= 0x94D049BB133111EBull; h ^= h >> 29;
        if ((h & 1023) > 358) continue;
        double start = (k + ((h >> 10) & 1023) / 1024.0) * slot, amp = 0.3 + 0.7 * ((h >> 20) & 1023) / 1023.0;
        double dt = (tDays - start) * 86400;
        if (dt < 0) continue;
        double lv = amp * (dt < 8 ? dt / 8 : std::exp(-(dt - 8) / 70));
        if (lv > best) {
            best = lv;
            *seq = (int)(k & 0xffff);
        }
    }
    return best;
}

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
struct ProbeTrack {
    std::string name;   // 英語名（探査機ではない天体の軌跡を名前で結び付ける）
    std::vector<ProbeSeg> segs;
};
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
        tr.name = std::string(name, strnlen(name, 32));
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
                sg.t.push_back(tp[0] - DeltaTDays(tp[0]));   // Horizons の時刻は力学時なので、シミュレータの世界時に直す
                sg.p.push_back(ecl(tp + 1));
                sg.v.push_back(EclToRender(v[0] * 86400.0, v[1] * 86400.0, v[2] * 86400.0));
            }
            if (sg.t.size() >= 2) tr.segs.push_back(sg);
        }
        g_probes.push_back(tr);
    }
}

// 点列（時刻・位置・速度）の 3 次エルミート補間
static V3 HermiteAt(const std::vector<double>& T, const std::vector<V3>& P, const std::vector<V3>& Vel, double t) {
    size_t i = std::upper_bound(T.begin(), T.end(), t) - T.begin();
    i = std::max<size_t>(1, std::min(i, T.size() - 1));
    double t0 = T[i - 1], h = T[i] - t0, u = (t - t0) / h;
    double h00 = 2 * u * u * u - 3 * u * u + 1, h10 = u * u * u - 2 * u * u + u, h01 = -2 * u * u * u + 3 * u * u, h11 = u * u * u - u * u;
    return P[i - 1] * h00 + Vel[i - 1] * (h * h10) + P[i] * h01 + Vel[i] * (h * h11);
}
// 区間内の位置（基準天体から）
static V3 SegPos(const ProbeSeg& s, double t) { return HermiteAt(s.t, s.p, s.v, t); }

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

// ---------------------------------------------------------------- アポロ計画の宇宙船の軌跡
// assets/apollo.bin（tools/make_apollo_assets.py、NASA の飛行記録の位置と速度から計算）。区間ごとに地球中心か月中心の
// 慣性座標の点列、または天体固定座標（打ち上げ・月面への降下と上昇・突入と着水）の点列を持ち、点の間は 3 次エルミート補間。
// 月中心の区間は「シミュレータの月 + 相対位置」。前後の地球中心の区間には、境目でのずれ（シミュレータの月 − 実際の月）を
// APOLLO_BLEND_DAYS かけて足し込む。天体固定と慣性の区間の境目のずれ（地球の自転の式の違いなど 1 km 以下）も同じように埋める
struct ApolloSeg {
    int body = B_EARTH;       // 基準天体（地球か月）
    bool fixed = false;       // 天体固定座標（x: 経度 0°, y: 東経 90°, z: 北極）
    V3 moonStart, moonEnd;    // 区間の始まり・終わりでの実際の月の位置（地球中心）
    V3 deltaStart, deltaEnd;  // 同じ時刻のシミュレータの月 − 実際の月（地球中心）
    std::vector<double> t;    // J2000 からの日数（UTC）
    std::vector<V3> p, v;     // 慣性: 描画座標 km, km/日。天体固定: その座標 km, km/日
};
// 噴射（点火から停止まで）。dir が空なら天体固定の区間の動力飛行（打ち上げ・月面への降下と上昇）
enum { AP_ENG_MAIN = 0, AP_ENG_RCS, AP_ENG_LM_DPS, AP_ENG_LM_RCS };   // 主エンジン、小さな噴射口、つながった月着陸船のそれぞれ
struct ApolloBurn {
    double t0, t1;              // J2000 からの日数（UTC）
    int engine;
    std::vector<double> dt;     // 推力の向きの時刻
    std::vector<V3> dir;        // 推力の向き（描画座標の単位ベクトル）
};
struct ApolloTrack {
    std::string flight, name;   // 飛行の名前（"Apollo 11"）と機体の名前（"Columbia"）
    std::vector<ApolloSeg> segs;
    std::vector<ApolloBurn> burns;
};
static std::vector<ApolloTrack> g_apollo;
static const double APOLLO_BLEND_DAYS = 0.25, APOLLO_FRAME_BLEND_DAYS = 120.0 / 86400;

static void LoadApolloTracks() {
    EmbeddedFile f = ApolloBin();
    const unsigned char* q = f.data;
    const unsigned char* end = f.data + f.size;
    auto rd = [&](void* dst, size_t n) {
        if (q + n > end) return false;
        std::memcpy(dst, q, n);
        q += n;
        return true;
    };
    auto ecl = [](const double* a) { return EclToRender(a[0], a[1], a[2]); };
    uint32_t nf = 0;
    if (!f.data || !rd(&nf, 4)) return;
    for (uint32_t i = 0; i < nf; i++) {
        char fname[16];
        uint32_t nt = 0;
        if (!rd(fname, 16) || !rd(&nt, 4)) return;
        for (uint32_t j = 0; j < nt; j++) {
            char tname[16];
            uint32_t ns = 0;
            if (!rd(tname, 16) || !rd(&ns, 4)) return;
            ApolloTrack tr;
            tr.flight = std::string(fname, strnlen(fname, 16));
            tr.name = std::string(tname, strnlen(tname, 16));
            for (uint32_t k = 0; k < ns; k++) {
                ApolloSeg sg;
                int32_t body, frame;
                double e[6];
                uint32_t n;
                if (!rd(&body, 4) || !rd(&frame, 4) || !rd(e, sizeof e) || !rd(&n, 4)) return;
                sg.body = body;
                sg.fixed = frame == 1;
                sg.moonStart = ecl(e);
                sg.moonEnd = ecl(e + 3);
                for (uint32_t m = 0; m < n; m++) {
                    double tp[4];
                    float v[3];
                    if (!rd(tp, sizeof tp) || !rd(v, sizeof v)) return;
                    sg.t.push_back(tp[0]);
                    if (sg.fixed) {   // 天体固定: そのまま（km/s → km/日）
                        sg.p.push_back(V3(tp[1], tp[2], tp[3]));
                        sg.v.push_back(V3(v[0], v[1], v[2]) * 86400.0);
                    } else {
                        sg.p.push_back(ecl(tp + 1));
                        sg.v.push_back(EclToRender(v[0] * 86400.0, v[1] * 86400.0, v[2] * 86400.0));
                    }
                }
                if (sg.t.size() >= 2) tr.segs.push_back(sg);
            }
            g_apollo.push_back(tr);
        }
    }
    uint32_t nb = 0;
    if (!rd(&nb, 4)) return;
    for (uint32_t i = 0; i < nb; i++) {
        char fname[16], tname[16];
        int32_t engine;
        double t[2];
        uint32_t n;
        if (!rd(fname, 16) || !rd(tname, 16) || !rd(&engine, 4) || !rd(t, sizeof t) || !rd(&n, 4)) return;
        ApolloBurn b;
        b.t0 = t[0];
        b.t1 = t[1];
        b.engine = engine;
        for (uint32_t k = 0; k < n; k++) {
            double tk;
            float d[3];
            if (!rd(&tk, 8) || !rd(d, sizeof d)) return;
            b.dt.push_back(tk);
            b.dir.push_back(EclToRender(d[0], d[1], d[2]));
        }
        for (ApolloTrack& tr : g_apollo)
            if (tr.flight == std::string(fname, strnlen(fname, 16)) && tr.name == std::string(tname, strnlen(tname, 16))) tr.burns.push_back(b);
    }
}

// 天体固定座標 f（x: 経度 0°, y: 東経 90°, z: 北極）→ 描画座標の向き（自転角 spin のとき）
static V3 FixedToRender(const Body& b, double spin, V3 f) {
    double cs = std::cos(spin), sn = std::sin(spin);
    V3 X = b.ex * cs - b.ez * sn, Z = b.ex * sn + b.ez * cs;
    return X * f.x - Z * f.y + b.pole * f.z;
}

// アポロの宇宙船の位置。bodyAt(番号) はその時刻の地球・月の位置、spinAt(番号) は自転角。データの範囲外では *active = false
template <class F, class G>
static V3 ApolloPosAt(const std::vector<Body>& B, int track, double t, F bodyAt, G spinAt, bool* active) {
    *active = false;
    if (track < 0 || track >= (int)g_apollo.size() || g_apollo[track].segs.empty()) return V3(1e15, 0, 0);
    const std::vector<ApolloSeg>& S = g_apollo[track].segs;
    // 軌跡の始まりの少し前（2 分まで）は始まりの位置に置く（切り離しの時刻と、司令船の点から写した軌跡の始まりとの、
    // 刻みや時刻の丸めの差で、切り離しの瞬間に機体が消えないように）
    if (t < S.front().t.front() && t > S.front().t.front() - 120.0 / 86400) t = S.front().t.front();
    if (t < S.front().t.front() || t > S.back().t.back()) return V3(1e15, 0, 0);
    *active = true;
    size_t k = 0;
    while (k + 1 < S.size() && S[k].t.back() < t) k++;
    // 区間 j の時刻 tt での位置（境目のずれを埋める前）
    auto raw = [&](size_t j, double tt) {
        const ApolloSeg& s = S[j];
        V3 p = HermiteAt(s.t, s.p, s.v, tt);
        if (s.fixed) return bodyAt(s.body) + FixedToRender(B[s.body], spinAt(s.body), p);
        return bodyAt(s.body) + p;
    };
    const ApolloSeg& s = S[k];
    V3 pos = raw(k, t);
    // 月中心の区間に隣り合う地球中心の慣性の区間: シミュレータの月と実際の月のずれを足し込む
    if (!s.fixed && s.body == B_EARTH) {
        if (k > 0 && S[k - 1].body == B_MOON && !S[k - 1].fixed)
            pos = pos + S[k - 1].deltaEnd * BlendIn((t - s.t.front()) / APOLLO_BLEND_DAYS);
        if (k + 1 < S.size() && S[k + 1].body == B_MOON && !S[k + 1].fixed)
            pos = pos + S[k + 1].deltaStart * BlendIn((s.t.back() - t) / APOLLO_BLEND_DAYS);
    }
    // 天体固定と慣性の区間の境目: 慣性の側を天体固定の側の位置へ寄せる（ずれは境目から離れるにつれて消える）
    if (!s.fixed) {
        if (k > 0 && S[k - 1].fixed && S[k - 1].body == s.body)
            pos = pos + (raw(k - 1, s.t.front()) - raw(k, s.t.front())) * BlendIn((t - s.t.front()) / APOLLO_FRAME_BLEND_DAYS);
        if (k + 1 < S.size() && S[k + 1].fixed && S[k + 1].body == s.body)
            pos = pos + (raw(k + 1, s.t.back()) - raw(k, s.t.back())) * BlendIn((s.t.back() - t) / APOLLO_FRAME_BLEND_DAYS);
    }
    return pos;
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

// 月の向き（IAU WGCCRE 2009 の月の自転の式）: 自転軸の赤経・赤緯と本初子午線の角度 W [deg]。ほぼ一定の速さで自転し、
// 公転は楕円で傾いているので、地球から見て経度方向に ±8°、緯度方向に ±7° ほど首を振る（光学的秤動）
static void MoonOrientation(double tDays, double* ra, double* dec, double* W) {
    double d = tDays + DeltaTDays(tDays), T = d / 36525.0;
    auto E = [&](double a, double b) { return (a + b * d) * DEG; };
    double E1 = E(125.045, -0.0529921), E2 = E(250.089, -0.1059842), E3 = E(260.008, 13.0120009), E4 = E(176.625, 13.3407154),
           E5 = E(357.529, 0.9856003), E6 = E(311.589, 26.4057084), E7 = E(134.963, 13.0649930), E8 = E(276.617, 0.3287146),
           E9 = E(34.226, 1.7484877), E10 = E(15.134, -0.1589763), E11 = E(119.743, 0.0036096), E12 = E(239.961, 0.1643573),
           E13 = E(25.053, 12.9590088);
    *ra = 269.9949 + 0.0031 * T - 3.8787 * std::sin(E1) - 0.1204 * std::sin(E2) + 0.0700 * std::sin(E3) - 0.0172 * std::sin(E4) +
          0.0072 * std::sin(E6) - 0.0052 * std::sin(E10) + 0.0043 * std::sin(E13);
    *dec = 66.5392 + 0.0130 * T + 1.5419 * std::cos(E1) + 0.0239 * std::cos(E2) - 0.0278 * std::cos(E3) + 0.0068 * std::cos(E4) -
           0.0029 * std::cos(E6) + 0.0009 * std::cos(E7) + 0.0008 * std::cos(E10) - 0.0009 * std::cos(E13);
    *W = 38.3213 + 13.17635815 * d - 1.4e-12 * d * d + 3.5610 * std::sin(E1) + 0.1208 * std::sin(E2) - 0.0642 * std::sin(E3) +
         0.0158 * std::sin(E4) + 0.0252 * std::sin(E5) - 0.0066 * std::sin(E6) - 0.0047 * std::sin(E7) - 0.0046 * std::sin(E8) +
         0.0028 * std::sin(E9) + 0.0052 * std::sin(E10) + 0.0040 * std::sin(E11) + 0.0019 * std::sin(E12) - 0.0044 * std::sin(E13);
}
// 月の自転軸・赤道の基底・自転角を時刻 t の値にする（W は赤道と ICRF の赤道の昇交点から測る）
static void SetMoonOrientation(Body& b, double t) {
    double ra, dec, W;
    MoonOrientation(t, &ra, &dec, &W);
    b.pole = EquToRender(ra, dec);
    EquatorBasis(b.pole, &b.ex, &b.ez);
    V3 Q = Norm(Cross(EquToRender(0, 90), b.pole));
    b.primeOffset = std::atan2(-Dot(Q, b.ez), Dot(Q, b.ex));
    b.spin = std::fmod(b.primeOffset + W * DEG, 2 * PI);
}

// 惑星のシミュレータ上の位置（任意の時刻）
// 軌跡を持つ天体（アポフィス）: 軌跡のある期間はそちらの位置に（端の 30 日で軌道要素の位置 elem から滑らかに移る）
template <class F>
static V3 TrackBlend(const BodyDef& d, double t, V3 elem, F planetAt) {
    if (d.track < 0) return elem;
    BodyDef td;
    td.probe = d.track;
    bool act;
    V3 p = ProbePosAt(td, t, planetAt, &act);
    const std::vector<ProbeSeg>& S = g_probes[d.track].segs;
    double edge = std::min(t - S.front().t.front(), S.back().t.back() - t) / 30.0;
    return act && edge > 0 ? elem + (p - elem) * (1 - BlendIn(edge)) : elem;
}
static V3 SimPlanetPos(const std::vector<Body>& B, int i, double t) {
    if (i == B_EARTH) return EarthPos(B[i].d, t);
    return TrackBlend(B[i].d, t, PlanetPos(B[i].d, t), [&](int k) { return k == B_EARTH ? EarthPos(B[k].d, t) : PlanetPos(B[k].d, t); });
}

// ---------------------------------------------------------------- 惑星の環
// 土星は Voyager 2 の観測（assets/saturn_rings.png）。天王星・海王星・木星は NASA NSSDCA の Ring Fact Sheet の
// 半径・幅・垂直の光学的厚さ τ から、起動時に半径方向の不透明度の表を作る。天王星の環は細くて暗く、海王星と木星の環は
// τ が 10⁻²〜10⁻⁷ と非常に薄いので、そのままでは見えない。gain 倍に強調する
struct RingBand { double r, w, tau; };   // 中心の半径 km, 幅 km, τ
struct RingSet {
    int body;
    double inner, outer;   // 表の範囲 km
    double gain;           // τ に掛ける倍率（見やすくするための強調）
    Col3 c1, c2;           // 濃い所 / 薄い所の色
    bool arcs;             // 海王星のアダムズ環の弧
    bool thin;             // 細い環: 遠くで画素より細くなっても線として見えるよう濃くする
    std::vector<RingBand> bands;   // 空なら assets/saturn_rings.png
};
static const std::vector<RingSet>& RingSets() {
    static const std::vector<RingSet> R = {
        {B_SATURN, 72000, 142500, 1, {0.93f, 0.86f, 0.72f}, {0.55f, 0.50f, 0.44f}, false, false, {}},
        {B_URANUS, 37000, 52000, 4, {0.62f, 0.62f, 0.65f}, {0.42f, 0.42f, 0.45f}, false, true,
         {{38000, 2500, 0.001},   // ζ（塵）
          {41837, 2, 0.3}, {42234, 2.5, 0.5}, {42571, 2.5, 0.3},          // 6, 5, 4
          {44718, 9, 0.35}, {45661, 9, 0.2},                               // α, β
          {47176, 1.5, 0.3}, {47176, 55, 0.002},                           // η（細い芯と広がり）
          {47627, 2.5, 1.5}, {48300, 6, 0.45}, {50023, 1.5, 0.15},         // γ, δ, λ
          {51149, 58, 1.0}}},                                              // ε
        {B_NEPTUNE, 40000, 64000, 25, {0.62f, 0.54f, 0.47f}, {0.44f, 0.38f, 0.33f}, true, true,
         {{41900, 2000, 1e-4},                           // ガレ環
          {53200, 113, 2e-3}, {55200, 4000, 1e-4},       // ルヴェリエ環, ラッセル環
          {57200, 100, 1e-4},                            // アラゴ環
          {62932, 35, 2e-3}}},                           // アダムズ環（弧はシェーダで）
        {B_JUPITER, 92000, 226000, 100000, {0.62f, 0.48f, 0.36f}, {0.40f, 0.31f, 0.24f}, false, false,
         {{107250, 30500, 1e-6},                         // ハロー
          {125750, 6500, 5e-6},                          // 主環
          {155500, 53000, 1e-7},                         // アマルテアのゴサマー環
          {177500, 97000, 3e-8}}},                       // テーベのゴサマー環
    };
    return R;
}
static const RingSet* RingOf(int body) {
    for (const RingSet& r : RingSets())
        if (r.body == body) return &r;
    return nullptr;
}

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
        b.pole0 = b.pole; b.ex0 = b.ex; b.ez0 = b.ez;
        if (!std::isnan(d.w0)) {
            // IAU の W は、天体の赤道が ICRF の赤道と交わる昇交点 Q（赤経 = 自転軸の赤経 + 90°）から測る
            V3 Q = Norm(Cross(EquToRender(0, 90), b.pole));
            b.primeOffset = std::atan2(-Dot(Q, b.ez), Dot(Q, b.ex));
        }
        b.outer = RingOf((int)i) ? RingOf((int)i)->outer : d.radius;   // 環のある惑星は環の外側まで
        if (d.apolloTrack) {
            b.soi = 20;      // 近くにいる自機は宇宙船と一緒に移動する（月を回る間に月面の近くを飛ぶ自機を連れて行かないよう狭く）
            b.outer = 0.03;
        } else if (d.probe >= 0 || d.tleN > 0) {
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
        } else if (d.exo >= 0) {
            b.soi = d.moonA * std::pow(d.mass / B[d.parent].d.mass, 0.4);
            for (int k = 0; k <= 360; k++) b.orbit.push_back(ExoRelative(d, B[d.parent].d, d.moonPeriod * k / 360.0));
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
        if (d.star >= 0) {
            // 連星の軌道（重心から見た、この星の 1 周分）
            const NearbyStar& ns = NEARBY_STARS[d.star];
            if (ns.P > 0)
                for (int k = 0; k <= 360; k++) b.orbit.push_back(BinaryRelative(ns, ns.P * 365.25 * k / 360.0) * ns.k);
        }
        if (d.parent >= 0) b.soi = std::max(b.soi, d.radius * 5);
        for (size_t k = 0; k < g_shapes.size(); k++)
            if (g_shapes[k].name == d.en) {
                b.shape = (int)k;
                b.outer = g_shapes[k].rmax;
            }
    }
    // 恒星の影響圏: 連星の伴星は近点距離の 0.3 倍（そのほかの系の中は主星）、主星と単独の星は、太陽を含む隣の系までの 0.45 倍
    for (Body& b : B) {
        if (b.d.star < 0) continue;
        const NearbyStar& ns = NEARBY_STARS[b.d.star];
        if (ns.P > 0 && ns.k > 0) {
            b.soi = 0.3 * ns.a * ARCSEC * StarDistance(ns) * (1 - ns.e);
            continue;
        }
        V3 p = StarBarycenter(ns, 0);
        double nearest = Len(p);
        for (const NearbyStar& o : NEARBY_STARS)
            if (o.system != ns.system) nearest = std::min(nearest, Len(StarBarycenter(o, 0) - p));
        b.soi = 0.45 * nearest;
    }
    // 探査機: 惑星中心の区間の境目での、シミュレータの惑星と実際の惑星の差
    if (g_probes.empty()) LoadProbeTracks();
    // 探査機ではない天体の軌跡（アポフィス）
    for (Body& b : B)
        for (size_t k = 0; k < g_probes.size(); k++)
            if (b.d.kind != KIND_PROBE && b.d.probe < 0 && g_probes[k].name == b.d.en && !g_probes[k].segs.empty()) b.d.track = (int)k;
    // アポロ計画の宇宙船: 軌跡を名前で結び付け、月中心の区間の境目での、シミュレータの月と実際の月の差を求める
    if (g_apollo.empty()) LoadApolloTracks();
    for (Body& b : B)
        for (size_t k = 0; b.d.apolloTrack && k < g_apollo.size(); k++)
            if (g_apollo[k].flight + "/" + g_apollo[k].name == b.d.apolloTrack) b.d.apollo = (int)k;
    for (ApolloTrack& tr : g_apollo)
        for (ApolloSeg& sg : tr.segs) {
            sg.deltaStart = MoonGeoPos(sg.t.front()) - sg.moonStart;
            sg.deltaEnd = MoonGeoPos(sg.t.back()) - sg.moonEnd;
        }
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
            ProbeSeg& sg = g_probes[pr.d.probe].segs.back();
            if (sg.body != m.d.parent) continue;
            V3 r = sg.p.back() + sg.v.back() * (m.d.moonChangeT - sg.t.back());
            double th = std::atan2(Dot(r, m.oQ), Dot(r, m.oN));
            m.d.moonPhase = (th - 2 * PI * m.d.moonChangeT / m.d.moonPeriod) / DEG;
            // 軌跡のデータは衝突の少し前で終わるので、衝突までまっすぐ飛ぶ点を足す
            if (sg.t.back() < m.d.moonChangeT) {
                V3 v = sg.v.back();
                sg.t.push_back(m.d.moonChangeT);
                sg.p.push_back(r);
                sg.v.push_back(v);
            }
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

// 水星〜海王星の軌道線: 時刻 t の位置と速度から求めた接触楕円（惑星をちょうど通る。ほかの惑星に引かれて少しずつ変わる）
static void OsculatingOrbit(Body& b, double t) {
    const double MU = 1.32712440018e11 * 86400.0 * 86400.0;   // 太陽の重力定数 km³/日²
    V3 r = PlanetPos(b.d, t), v = PlanetPos(b.d, t + 0.5) - PlanetPos(b.d, t - 0.5);
    V3 h = Cross(r, v);
    double R = Len(r), a = 1 / (2 / R - Dot(v, v) / MU);
    V3 ev = Cross(v, h) * (1 / MU) - r * (1 / R);   // 近日点を向く離心率ベクトル
    double e = Len(ev);
    V3 P = e > 1e-9 ? ev * (1 / e) : r * (1 / R), Q = Norm(Cross(h, P));
    for (size_t k = 0; k < b.orbit.size(); k++) {
        double E = 2 * PI * k / (b.orbit.size() - 1);
        b.orbit[k] = P * (a * (std::cos(E) - e)) + Q * (a * std::sqrt(1 - e * e) * std::sin(E));
    }
}

static void UpdateBodies(std::vector<Body>& B, double t) {
    for (Body& b : B) {
        const BodyDef& d = b.d;
        if (d.star >= 0) {
            b.pos = StarPos(d, t);
        } else if (d.parent < 0) {
            b.pos = V3();
        } else if (d.tleN > 0) {
            b.pos = B[B_EARTH].pos + SatellitePos(d, t);
            b.active = t >= d.launch;
        } else if (d.apollo >= 0) {
            // 地球と月は先に更新済み（宇宙船は配列の後ろ）
            b.pos = ApolloPosAt(B, d.apollo, t, [&](int i) { return B[i].pos; }, [&](int i) { return B[i].spin; }, &b.active);
        } else if (d.probe >= 0) {
            // 惑星は先に更新済み（探査機は配列の最後）
            b.pos = ProbePosAt(d, t, [&](int i) { return B[i].pos; }, &b.active);
        } else if (d.exo >= 0) {
            b.pos = B[d.parent].pos + ExoRelative(d, B[d.parent].d, t);   // 恒星は先に更新済み（惑星は配列の最後）
        } else if (d.parent == 0) {
            b.pos = &b == &B[B_EARTH] ? EarthPos(d, t) : PlanetPos(d, t);
            b.pos = TrackBlend(d, t, b.pos, [&](int i) { return B[i].pos; });
            if (d.vsop >= 0 && std::fabs(t - b.orbitT) > 1.0) {
                b.orbitT = t;
                OsculatingOrbit(b, t);
            }
            if (&b == &B[B_EARTH]) {   // 地球の自転軸と赤道は歳差で回る（自転角 spin はこの赤道の上で測る）
                b.pole = PrecessFromJ2000(b.pole0, t);
                b.ex = PrecessFromJ2000(b.ex0, t);
                b.ez = PrecessFromJ2000(b.ez0, t);
            }
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
        if (&b == &B[B_MOON]) {
            SetMoonOrientation(b, t);   // 月は IAU の式（秤動で首を振る）
        } else if (d.parent > 0 && d.rotPeriod == 0) {
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
static double SpinAt(const std::vector<Body>& B, int i, double t);
// 時刻 t の天体の位置と自転角（UpdateBodies と同じ式を 1 天体分だけ。出入りの時刻を探すのに何百回も呼ぶ）
static V3 PosAt(const std::vector<Body>& B, int i, double t) {
    const BodyDef& d = B[i].d;
    if (d.star >= 0) return StarPos(d, t);
    if (d.parent < 0) return V3();
    if (d.tleN > 0) return PosAt(B, B_EARTH, t) + SatellitePos(d, t);
    if (d.apollo >= 0) {
        bool act;
        return ApolloPosAt(B, d.apollo, t, [&](int k) { return PosAt(B, k, t); }, [&](int k) { return SpinAt(B, k, t); }, &act);
    }
    if (d.probe >= 0) {
        bool act;
        return ProbePosAt(d, t, [&](int k) { return PosAt(B, k, t); }, &act);
    }
    if (d.exo >= 0) return PosAt(B, d.parent, t) + ExoRelative(d, B[d.parent].d, t);
    if (d.parent == 0)
        return i == B_EARTH ? EarthPos(d, t) : TrackBlend(d, t, PlanetPos(d, t), [&](int k) { return PosAt(B, k, t); });
    if (i == B_MOON) return PosAt(B, B_EARTH, t) + MoonGeoPos(t);
    double th = d.moonPhase * DEG + 2 * PI * t / d.moonPeriod;
    if (d.moonPeriod2 > 0 && t > d.moonChangeT)
        th = d.moonPhase * DEG + 2 * PI * (d.moonChangeT / d.moonPeriod + (t - d.moonChangeT) / d.moonPeriod2);
    if (d.libA != 0 || d.libB != 0) {
        double w = 2 * PI * t / (70.56 * 365.25);
        th += (d.libA * std::sin(w) + d.libB * std::cos(w)) * DEG;
    }
    return PosAt(B, d.parent, t) + (B[i].oN * std::cos(th) + B[i].oQ * std::sin(th)) * d.moonA;
}
static double SpinAt(const std::vector<Body>& B, int i, double t) {
    const Body& b = B[i];
    const BodyDef& d = b.d;
    if (i == B_MOON) {   // 月は IAU の式（自転軸の向きは今の値のまま）
        double ra, dec, W;
        MoonOrientation(t, &ra, &dec, &W);
        return std::fmod(b.primeOffset + W * DEG, 2 * PI);
    }
    if (d.parent > 0 && d.rotPeriod == 0) {
        V3 u = PosAt(B, d.parent, t) - PosAt(B, i, t);
        return std::atan2(-Dot(u, b.ez), Dot(u, b.ex));
    }
    double rp = d.rotPeriod == 0 ? d.moonPeriod : d.rotPeriod;
    if (!std::isnan(d.w0)) return std::fmod(b.primeOffset + d.w0 * DEG + 2 * PI * t / rp, 2 * PI);
    return std::fmod(d.spin0 * DEG + 2 * PI * t / rp, 2 * PI);
}

// 地上の点（天体固定座標 f）から見たターゲットの高度・方位 rad（方位は北から東回り）
static void AltAz(const std::vector<Body>& B, int g, V3 f, int target, double t, double* alt, double* az) {
    const Body& b = B[g];
    double s = SpinAt(B, g, t), cs = std::cos(s), sn = std::sin(s);
    V3 X = b.ex * cs - b.ez * sn, Z = b.ex * sn + b.ez * cs;
    V3 pg = PosAt(B, g, t), rel = X * f.x + b.pole * f.y + Z * f.z;
    V3 up = Norm(rel), v = Norm(PosAt(B, target, t) - (pg + rel));
    V3 north = b.pole - up * Dot(b.pole, up);
    north = Len(north) < 1e-9 ? Norm(X - up * Dot(X, up)) : Norm(north);
    V3 east = Cross(north, up);
    *alt = std::asin(std::max(-1.0, std::min(1.0, Dot(v, up))));
    *az = std::fmod(std::atan2(Dot(v, east), Dot(v, north)) + 2 * PI, 2 * PI);
}

// 天体を照らす恒星（太陽系の天体は太陽、恒星はその星自身）
static int HostStar(const std::vector<Body>& B, int i) {
    while (B[i].d.parent > 0) i = B[i].d.parent;
    return B[i].d.kind == KIND_STAR ? i : B_SUN;
}

static double MinAltitude(const Body& b) { return std::max(0.03, b.d.radius * 1e-5); }

static Color BodyColor(const Body& b, float bright = 1.0f) {
    const Col3& c = b.d.c1;
    auto f = [&](float v) { return (unsigned char)std::min(255.0f, (v * 0.7f + 0.3f) * 255 * bright); };
    return {f(c.r), f(c.g), f(c.b), 255};
}
