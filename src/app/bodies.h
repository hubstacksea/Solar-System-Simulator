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
    int dialUnit;  // 0: km/h, 1: km/s, 2: c
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
    double orbitT = -1e9;   // 軌道線を計算した時刻（月）
    bool active = true;     // 探査機: その時刻に飛んでいるか（打ち上げ前・運用終了後は false で、表示しない）
    int shape = -1;         // 実際の形（g_shapes の番号。-1 は楕円体）
    int map = -1;           // 探査機の画像による地図（App::maps の番号。-1 はなし）
    double primeOffset = 0; // 自転角 0 の X 軸から IAU の基準（赤道と ICRF 赤道の昇交点）までの角度 rad
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

static V3 PlanetPos(const BodyDef& d0, double tDays) {
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

// 月の地心位置 km（J2000 黄道系→描画座標）。Astronomical Almanac の簡易式（精度 約 0.3°）
static V3 MoonGeoPos(double tDays) {
    double T = tDays / 36525.0;
    auto sd = [](double deg) { return std::sin(deg * DEG); };
    auto cd = [](double deg) { return std::cos(deg * DEG); };
    double lam = 218.32 + 481267.881 * T + 6.29 * sd(135.0 + 477198.87 * T) - 1.27 * sd(259.3 - 413335.36 * T) +
                 0.66 * sd(235.7 + 890534.22 * T) + 0.21 * sd(269.9 + 954397.74 * T) -
                 0.19 * sd(357.5 + 35999.05 * T) - 0.11 * sd(186.5 + 966404.03 * T);
    double bet = 5.13 * sd(93.3 + 483202.02 * T) + 0.28 * sd(228.2 + 960400.89 * T) -
                 0.28 * sd(318.3 + 6003.15 * T) - 0.17 * sd(217.6 - 407332.21 * T);
    double par = 0.9508 + 0.0518 * cd(135.0 + 477198.87 * T) + 0.0095 * cd(259.3 - 413335.36 * T) +
                 0.0078 * cd(235.7 + 890534.22 * T) + 0.0028 * cd(269.9 + 954397.74 * T);
    lam -= 1.396971 * T;  // 日付の分点 → J2000 分点（歳差）
    double r = 6378.137 / sd(par);
    return EclToRender(r * cd(bet) * cd(lam), r * cd(bet) * sd(lam), r * sd(bet));
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

static V3 StarBarycenter(const NearbyStar& s, double tDays) {
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
struct ProbeTrack { std::vector<ProbeSeg> segs; };
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
                sg.t.push_back(tp[0]);
                sg.p.push_back(ecl(tp + 1));
                sg.v.push_back(EclToRender(v[0] * 86400.0, v[1] * 86400.0, v[2] * 86400.0));
            }
            if (sg.t.size() >= 2) tr.segs.push_back(sg);
        }
        g_probes.push_back(tr);
    }
}

// 区間内の位置（基準天体から）
static V3 SegPos(const ProbeSeg& s, double t) {
    size_t i = std::upper_bound(s.t.begin(), s.t.end(), t) - s.t.begin();
    i = std::max<size_t>(1, std::min(i, s.t.size() - 1));
    double t0 = s.t[i - 1], h = s.t[i] - t0, u = (t - t0) / h;
    double h00 = 2 * u * u * u - 3 * u * u + 1, h10 = u * u * u - 2 * u * u + u, h01 = -2 * u * u * u + 3 * u * u, h11 = u * u * u - u * u;
    return s.p[i - 1] * h00 + s.v[i - 1] * (h * h10) + s.p[i] * h01 + s.v[i] * (h * h11);
}

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

// 惑星のシミュレータ上の位置（任意の時刻）
static V3 SimPlanetPos(const std::vector<Body>& B, int i, double t) { return i == B_EARTH ? EarthPos(B[i].d, t) : PlanetPos(B[i].d, t); }

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
        if (!std::isnan(d.w0)) {
            // IAU の W は、天体の赤道が ICRF の赤道と交わる昇交点 Q（赤経 = 自転軸の赤経 + 90°）から測る
            V3 Q = Norm(Cross(EquToRender(0, 90), b.pole));
            b.primeOffset = std::atan2(-Dot(Q, b.ez), Dot(Q, b.ex));
        }
        b.outer = RingOf((int)i) ? RingOf((int)i)->outer : d.radius;   // 環のある惑星は環の外側まで
        if (d.probe >= 0 || d.tleN > 0) {
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
            const ProbeSeg& sg = g_probes[pr.d.probe].segs.back();
            if (sg.body != m.d.parent) continue;
            V3 r = sg.p.back() + sg.v.back() * (m.d.moonChangeT - sg.t.back());
            double th = std::atan2(Dot(r, m.oQ), Dot(r, m.oN));
            m.d.moonPhase = (th - 2 * PI * m.d.moonChangeT / m.d.moonPeriod) / DEG;
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
        } else if (d.probe >= 0) {
            // 惑星は先に更新済み（探査機は配列の最後）
            b.pos = ProbePosAt(d, t, [&](int i) { return B[i].pos; }, &b.active);
        } else if (d.exo >= 0) {
            b.pos = B[d.parent].pos + ExoRelative(d, B[d.parent].d, t);   // 恒星は先に更新済み（惑星は配列の最後）
        } else if (d.parent == 0) {
            b.pos = &b == &B[B_EARTH] ? EarthPos(d, t) : PlanetPos(d, t);
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
        if (d.parent > 0 && d.rotPeriod == 0) {
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
// 時刻 t の天体の位置と自転角（UpdateBodies と同じ式を 1 天体分だけ。出入りの時刻を探すのに何百回も呼ぶ）
static V3 PosAt(const std::vector<Body>& B, int i, double t) {
    const BodyDef& d = B[i].d;
    if (d.star >= 0) return StarPos(d, t);
    if (d.parent < 0) return V3();
    if (d.tleN > 0) return PosAt(B, B_EARTH, t) + SatellitePos(d, t);
    if (d.probe >= 0) {
        bool act;
        return ProbePosAt(d, t, [&](int k) { return PosAt(B, k, t); }, &act);
    }
    if (d.exo >= 0) return PosAt(B, d.parent, t) + ExoRelative(d, B[d.parent].d, t);
    if (d.parent == 0) return i == B_EARTH ? EarthPos(d, t) : PlanetPos(d, t);
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
