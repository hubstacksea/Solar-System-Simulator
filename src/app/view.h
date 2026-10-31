// Solar System Voyager — Copyright (c) 2026 hubstacksea
// MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
// メッシュ・カメラと投影・数値の書式。
// main.cpp の一部で、main.cpp が決まった順に読み込む（単体ではコンパイルしない）

// ---------------------------------------------------------------- メッシュ
static Mesh GenUVSphere(int rings, int slices) {
    Mesh m = {0};
    int vc = (rings + 1) * (slices + 1);
    m.vertexCount = vc;
    m.triangleCount = rings * slices * 2;
    m.vertices = (float*)MemAlloc(vc * 3 * sizeof(float));
    m.normals = (float*)MemAlloc(vc * 3 * sizeof(float));
    m.texcoords = (float*)MemAlloc(vc * 2 * sizeof(float));
    m.indices = (unsigned short*)MemAlloc(m.triangleCount * 3 * sizeof(unsigned short));
    int k = 0;
    for (int r = 0; r <= rings; r++) {
        double phi = PI * r / rings;
        for (int s = 0; s <= slices; s++) {
            double th = 2 * PI * s / slices;
            float x = (float)(std::sin(phi) * std::cos(th)), y = (float)std::cos(phi), z = (float)(std::sin(phi) * std::sin(th));
            m.vertices[k * 3] = m.normals[k * 3] = x;
            m.vertices[k * 3 + 1] = m.normals[k * 3 + 1] = y;
            m.vertices[k * 3 + 2] = m.normals[k * 3 + 2] = z;
            m.texcoords[k * 2] = (float)s / slices;
            m.texcoords[k * 2 + 1] = (float)r / rings;
            k++;
        }
    }
    int t = 0;
    for (int r = 0; r < rings; r++) {
        for (int s = 0; s < slices; s++) {
            unsigned short a = r * (slices + 1) + s, b = (r + 1) * (slices + 1) + s;
            m.indices[t++] = a; m.indices[t++] = b + 1; m.indices[t++] = b;
            m.indices[t++] = a; m.indices[t++] = a + 1; m.indices[t++] = b + 1;
        }
    }
    UploadMesh(&m, false);
    return m;
}

static Mesh GenRing(float inner, float outer, int segs) {
    Mesh m = {0};
    m.vertexCount = (segs + 1) * 2;
    m.triangleCount = segs * 2;
    m.vertices = (float*)MemAlloc(m.vertexCount * 3 * sizeof(float));
    m.normals = (float*)MemAlloc(m.vertexCount * 3 * sizeof(float));
    m.texcoords = (float*)MemAlloc(m.vertexCount * 2 * sizeof(float));
    m.indices = (unsigned short*)MemAlloc(m.triangleCount * 3 * sizeof(unsigned short));
    for (int i = 0; i <= segs; i++) {
        float th = 2 * PI * i / segs, c = std::cos(th), s = std::sin(th);
        float rr[2] = {inner, outer};
        for (int j = 0; j < 2; j++) {
            int v = i * 2 + j;
            m.vertices[v * 3] = c * rr[j]; m.vertices[v * 3 + 1] = 0; m.vertices[v * 3 + 2] = s * rr[j];
            m.normals[v * 3] = 0; m.normals[v * 3 + 1] = 1; m.normals[v * 3 + 2] = 0;
            m.texcoords[v * 2] = (float)j; m.texcoords[v * 2 + 1] = (float)i / segs;
        }
    }
    int t = 0;
    for (int i = 0; i < segs; i++) {
        unsigned short a = i * 2, b = a + 1, c = a + 2, d = a + 3;
        m.indices[t++] = a; m.indices[t++] = b; m.indices[t++] = d;
        m.indices[t++] = a; m.indices[t++] = d; m.indices[t++] = c;
    }
    UploadMesh(&m, false);
    return m;
}

// ---------------------------------------------------------------- カメラ・投影
struct Cam {
    V3 pos, fwd, up, right;
    V3 shipRel;   // カメラから見た自機の位置。太陽から遠いと pos が km 単位で丸まり、自機の位置 − pos ではブレるので別に持つ
    V3 origin, local;   // pos = origin（基準天体の位置）+ local。太陽から何光年も離れると pos は数 m 単位で丸まるので、
                        // 天体の描画は基準天体からの差で求める（CamRel）
    double fovy = 60 * DEG;
    double fovBase = 60 * DEG;  // ズーム込みの視野角（fovy はこれにワープ演出を加えたもの）
    double focal = 1;  // ピクセル単位の焦点距離
    float W = 1280, H = 720;
};

// カメラから見た位置（ワールド座標の点 p）。基準天体の近くの点ほど丸めの誤差が小さい
static V3 CamRel(const Cam& c, V3 p) { return (p - c.origin) - c.local; }

// 画面座標へ投影。カメラ前方なら true
static bool Project(const Cam& c, V3 rel, Vector2* out) {
    double z = Dot(rel, c.fwd);
    if (z <= 1e-9) return false;
    out->x = (float)(c.W * 0.5 + Dot(rel, c.right) / z * c.focal);
    out->y = (float)(c.H * 0.5 - Dot(rel, c.up) / z * c.focal);
    return true;
}

// 文字の大きさ（設定）: 画面上の 2D の表示（計器・マーカー・パネル）をまとめてこの倍率で描く。
// 配置は画面を倍率で割った大きさ（UiCam）で計算する
static const float UI_SCALES[3] = {1.0f, 1.15f, 1.3f};
static int g_uiScaleIdx = 0;
// 画面の表示倍率（Windows の 150% など、高解像度の画面）。2D の表示はこれと文字の大きさを掛けた倍率で描く。
// macOS は raylib が Retina の倍率を受け持つので 1
static float g_dpiScale = 1;
static float g_uiK = 1;   // 2D の表示を描いている間の倍率（画面のピクセルを直接使う切り抜きで使う）
// 表示倍率の分は、上の左右のパネルとコンパスが横に並ぶ幅（1110）が残る範囲で。文字の大きさの分は縦にパネルが収まる範囲で
static float UiScale(const Cam& c) {
    float dpi = std::max(1.0f, std::min(g_dpiScale, c.W / 1110.0f));
    return std::max(1.0f, std::min(dpi * UI_SCALES[g_uiScaleIdx], c.H / 540.0f));
}
static Cam UiCam(const Cam& c) {
    float k = UiScale(c);
    Cam u = c;
    u.W /= k;
    u.H /= k;
    u.focal /= k;
    return u;
}

static Matrix g_view;
static void SetPassProjection(const Cam& c, double n, double f) {
    rlDrawRenderBatchActive();
    rlMatrixMode(RL_PROJECTION);
    rlLoadIdentity();
    double top = n * std::tan(c.fovy * 0.5), right = top * c.W / c.H;
    rlFrustum(-right, right, -top, top, n, f);
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    rlMultMatrixf(MatrixToFloat(g_view));
}
static void ClearDepth() {
    rlDrawRenderBatchActive();
    glClear(GL_DEPTH_BIT);
}

static Quaternion QuatFromBasis(V3 fwd, V3 upHint) {
    V3 f = Norm(fwd);
    V3 r = Cross(f, upHint);
    if (Len(r) < 1e-6) r = Cross(f, V3(1, 0, 0));
    if (Len(r) < 1e-6) r = Cross(f, V3(0, 0, 1));
    r = Norm(r);
    V3 u = Cross(r, f);
    Matrix m = MatrixIdentity();
    m.m0 = (float)r.x; m.m1 = (float)r.y; m.m2 = (float)r.z;
    m.m4 = (float)u.x; m.m5 = (float)u.y; m.m6 = (float)u.z;
    m.m8 = (float)-f.x; m.m9 = (float)-f.y; m.m10 = (float)-f.z;
    return QuaternionNormalize(QuaternionFromMatrix(m));
}

// 天体のモデル行列: 平行移動 * 赤道基底 * 自転(Y軸) * 拡大
static Matrix BodyMatrix(const Body& b, V3 center, double sEq, double sPol, double spin) {
    double cw = std::cos(spin), sw = std::sin(spin);
    V3 X = (b.ex * cw - b.ez * sw) * sEq, Y = b.pole * sPol, Z = (b.ex * sw + b.ez * cw) * sEq;
    Matrix m = {0};
    m.m0 = (float)X.x; m.m1 = (float)X.y; m.m2 = (float)X.z;
    m.m4 = (float)Y.x; m.m5 = (float)Y.y; m.m6 = (float)Y.z;
    m.m8 = (float)Z.x; m.m9 = (float)Z.y; m.m10 = (float)Z.z;
    m.m12 = (float)center.x; m.m13 = (float)center.y; m.m14 = (float)center.z;
    m.m15 = 1;
    return m;
}

// ---------------------------------------------------------------- 書式
static void Txt(const char* s, float x, float y, float size, Color c) {
    float sp = g_jpFont ? 0.5f : size / 10;
    DrawTextEx(g_font, s, {x + 1, y + 1}, size, sp, {0, 0, 0, (unsigned char)(c.a * 0.8f)});
    DrawTextEx(g_font, s, {x, y}, size, sp, c);
}
static float TxtW(const char* s, float size) { return MeasureTextEx(g_font, s, size, g_jpFont ? 0.5f : size / 10).x; }
static void TxtC(const char* s, float cx, float y, float size, Color c) { Txt(s, cx - TxtW(s, size) / 2, y, size, c); }

static std::string Grouped(double v, int decimals) {
    char buf[64];
    snprintf(buf, sizeof buf, "%.*f", decimals, std::fabs(v));
    std::string s(buf), intPart = s.substr(0, s.find('.')), frac = s.find('.') == std::string::npos ? "" : s.substr(s.find('.'));
    std::string out;
    for (size_t i = 0; i < intPart.size(); i++) {
        if (i > 0 && (intPart.size() - i) % 3 == 0) out += ',';
        out += intPart[i];
    }
    return (v < 0 ? "-" : "") + out + frac;
}

static std::string FmtDist(double km) {
    char b[96];
    if (km < 1) snprintf(b, sizeof b, "%.0f m", km * 1000);
    else if (km < 1e7) snprintf(b, sizeof b, "%s km", Grouped(km, km < 100 ? 2 : 0).c_str());
    else if (km < 0.1 * LY_KM) snprintf(b, sizeof b, "%.3f AU", km / AU_KM);
    else snprintf(b, sizeof b, "%.3f %s", km / LY_KM, T(UI_LY));
    return b;
}

static std::string FmtSpeed(double kms) {
    char b[96];
    double a = std::fabs(kms);
    if (a < 1) snprintf(b, sizeof b, "%s km/h", Grouped(kms * 3600, 1).c_str());
    else snprintf(b, sizeof b, "%s km/s", Grouped(kms, a < 100 ? 3 : 0).c_str());
    std::string s = b;
    if (a > C_KMS * 0.001) {
        if (a > C_KMS * 1000) snprintf(b, sizeof b, "  (%s c)", Grouped(kms / C_KMS, 0).c_str());   // 超光速
        else snprintf(b, sizeof b, "  (%.4f c)", kms / C_KMS);
        s += b;
    }
    return s;
}

static std::string FmtDuration(double sec) {
    char b[96];
    if (!std::isfinite(sec) || sec < 0 || sec > 1e13) return "---";
    if (sec < 60) snprintf(b, sizeof b, "%.1f s", sec);
    else if (sec < 86400) snprintf(b, sizeof b, "%02d:%02d:%02d", (int)(sec / 3600), (int)std::fmod(sec / 60, 60), (int)std::fmod(sec, 60));
    else if (sec < 365.25 * 86400 * 10) snprintf(b, sizeof b, "%d%s %02d:%02d", (int)(sec / 86400), T(UI_DAY), (int)std::fmod(sec / 3600, 24), (int)std::fmod(sec / 60, 60));
    else snprintf(b, sizeof b, "%s%s", Grouped(sec / (365.25 * 86400), 1).c_str(), T(UI_YEAR));
    return b;
}

static double NowDaysSinceJ2000() { return ((double)std::time(nullptr) - J2000_UNIX) / 86400.0; }

static std::string FmtDate(double tDays) {
    double unixSec = J2000_UNIX + tDays * 86400.0;
    if (unixSec < -62135596800.0 || unixSec > 253402300799.0) return "----";
    time_t tt = (time_t)std::floor(unixSec);
    struct tm* g = std::gmtime(&tt);
    if (!g) return "----";
    char b[64];
    snprintf(b, sizeof b, "%04d-%02d-%02d %02d:%02d:%02d UTC", g->tm_year + 1900, g->tm_mon + 1, g->tm_mday, g->tm_hour, g->tm_min, g->tm_sec);
    return b;
}
