// Solar System Voyager — Copyright (c) 2026 hubstacksea
// MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
// 定数と double のベクトル。
// main.cpp の一部で、main.cpp が決まった順に読み込む（単体ではコンパイルしない）

// ---------------------------------------------------------------- 定数
static const double AU_KM = 149597870.7;
static const double C_KMS = 299792.458;
static const double LY_KM = 9460730472580.8;      // 1 光年
static const double PC_KM = 30856775814913.67;    // 1 パーセク
static const double J2000_UNIX = 946728000.0;  // 2000-01-01 12:00:00 UTC
static const double DEG = PI / 180.0;

// ---------------------------------------------------------------- double ベクトル
struct V3 {
    double x = 0, y = 0, z = 0;
    V3() {}
    V3(double a, double b, double c) : x(a), y(b), z(c) {}
    V3 operator+(const V3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    V3 operator-(const V3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    V3 operator*(double s) const { return {x * s, y * s, z * s}; }
    V3 operator-() const { return {-x, -y, -z}; }
    V3& operator+=(const V3& o) { x += o.x; y += o.y; z += o.z; return *this; }
};
static inline double Dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline V3 Cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
static inline double Len(V3 a) { return std::sqrt(Dot(a, a)); }
static inline V3 Norm(V3 a) { double l = Len(a); return l > 0 ? a * (1.0 / l) : V3(0, 0, 0); }
static inline Vector3 ToF(V3 a) { return {(float)a.x, (float)a.y, (float)a.z}; }
static inline V3 FromF(Vector3 v) { return {v.x, v.y, v.z}; }
static const V3 Y_UP(0, 1, 0);  // 描画座標の +Y = 黄道北

// 黄道座標 (x, y, z: z=黄道北) → 描画座標 (Y-up)
static V3 EclToRender(double x, double y, double z) { return {x, z, -y}; }
// 赤道座標 (x: 春分点, z: 天の北極) のベクトル → 描画座標
static V3 EquVecToRender(double x, double y, double z) {
    const double eps = 23.4392911 * DEG;
    return EclToRender(x, y * std::cos(eps) + z * std::sin(eps), -y * std::sin(eps) + z * std::cos(eps));
}
static V3 EquToRender(double raDeg, double decDeg) {
    double ra = raDeg * DEG, dec = decDeg * DEG;
    return EquVecToRender(std::cos(dec) * std::cos(ra), std::cos(dec) * std::sin(ra), std::sin(dec));
}
