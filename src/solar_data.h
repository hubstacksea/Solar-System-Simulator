// 太陽系天体データ（実寸）
//  - 半径: km（赤道半径）、扁平率
//  - 惑星軌道: JPL "Approximate Positions of the Planets" J2000 平均軌道要素
//  - 衛星軌道: 円軌道近似（軌道長半径・公転周期・母天体赤道面に対する傾斜角）
//  - 自転軸: IAU 自転軸の赤経・赤緯
#pragma once
#include <cmath>
#include <cstring>
#include <vector>

enum BodyKind { KIND_STAR = 0, KIND_ROCKY = 1, KIND_GAS = 2, KIND_EARTH = 3 };

struct Col3 { float r, g, b; };

// 表面模様の種類（シェーダの detail.x。天体ごとの特徴的な地形・模様を描き分ける）
enum SurfaceStyle {
    S_GENERIC = 0, S_MOON, S_MERCURY, S_MARS, S_IO, S_EUROPA, S_GANYMEDE, S_CALLISTO, S_ICY, S_IAPETUS,
    S_PLUTO, S_ENCELADUS, S_TRITON,
    S_JUPITER = 20, S_SATURN, S_URANUS, S_NEPTUNE, S_VENUS, S_TITAN,
    S_EARTH = 30, S_EARTH_TEX = 31, S_SUN = 40
};

struct BodyDef {
    const char* jp;
    const char* en;
    int parent;          // -1: 太陽（原点固定）
    double radius;       // 赤道半径 km
    double flattening;   // 扁平率
    double mass;         // kg

    // 惑星 (parent == 0): J2000 軌道要素
    double a_au = 0, e = 0, inc = 0, L0 = 0, varpi = 0, node = 0;

    // 衛星: 円軌道
    double moonA = 0;       // 軌道長半径 km
    double moonPeriod = 0;  // 公転周期 日
    double moonIncl = 0;    // 傾斜角 deg（eclipticRef なら黄道面基準、それ以外は母天体赤道面基準）
    double moonNode = 0, moonPhase = 0;
    bool eclipticRef = false;

    bool inheritPole = false;  // 母天体の自転軸を使う
    double poleRA = 0, poleDec = 90;
    double rotPeriod = 1;      // 自転周期 日（負: 逆行、0: 公転と同期）
    double spin0 = 0;          // J2000 での自転角 deg（地球は実際の地図の向きに合わせる）

    int kind = KIND_ROCKY;
    Col3 c1{1, 1, 1}, c2{0.5f, 0.5f, 0.5f}, atm{0, 0, 0};
    float bands = 0;
    float atmStrength = 0;
    bool rings = false;
    int style = S_GENERIC;
};

inline BodyDef MakePlanet(const char* jp, const char* en, double radius, double flat, double mass,
                          double a, double e, double inc, double L, double varpi, double node,
                          double poleRA, double poleDec, double rot,
                          int kind, Col3 c1, Col3 c2, float bands = 0, Col3 atm = {0, 0, 0}, float atmS = 0) {
    BodyDef d{};
    d.jp = jp; d.en = en; d.parent = 0;
    d.radius = radius; d.flattening = flat; d.mass = mass;
    d.a_au = a; d.e = e; d.inc = inc; d.L0 = L; d.varpi = varpi; d.node = node;
    d.poleRA = poleRA; d.poleDec = poleDec; d.rotPeriod = rot;
    d.kind = kind; d.c1 = c1; d.c2 = c2; d.bands = bands; d.atm = atm; d.atmStrength = atmS;
    return d;
}

inline BodyDef MakeMoon(const char* jp, const char* en, int parent, double radius, double mass,
                        double a, double period, double incl, int kind, Col3 c1, Col3 c2,
                        float bands = 0, Col3 atm = {0, 0, 0}, float atmS = 0) {
    BodyDef d{};
    d.jp = jp; d.en = en; d.parent = parent;
    d.radius = radius; d.flattening = 0; d.mass = mass;
    d.moonA = a; d.moonPeriod = period; d.moonIncl = incl;
    d.inheritPole = true; d.rotPeriod = 0;
    d.kind = kind; d.c1 = c1; d.c2 = c2; d.bands = bands; d.atm = atm; d.atmStrength = atmS;
    return d;
}

enum {
    B_SUN = 0, B_MERCURY, B_VENUS, B_EARTH, B_MARS, B_JUPITER, B_SATURN, B_URANUS, B_NEPTUNE, B_PLUTO,
    B_MOON
};

inline std::vector<BodyDef> BuildSolarSystem() {
    std::vector<BodyDef> v;

    BodyDef sun{};
    sun.jp = "太陽"; sun.en = "Sun"; sun.parent = -1;
    sun.radius = 695700; sun.mass = 1.989e30;
    sun.poleRA = 286.13; sun.poleDec = 63.87; sun.rotPeriod = 25.38;
    sun.kind = KIND_STAR; sun.c1 = {1.0f, 0.93f, 0.70f}; sun.c2 = {1.0f, 0.62f, 0.22f};
    v.push_back(sun);

    //                 名前                         半径      扁平率     質量
    //                 a[AU]        e            i[deg]       L[deg]        ϖ[deg]        Ω[deg]
    //                 自転軸RA  Dec   自転周期[日]
    v.push_back(MakePlanet("水星", "Mercury", 2439.7, 0.0, 3.301e23,
                           0.38709927, 0.20563593, 7.00497902, 252.25032350, 77.45779628, 48.33076593,
                           281.01, 61.42, 58.646,
                           KIND_ROCKY, {0.62f, 0.60f, 0.57f}, {0.36f, 0.34f, 0.32f}));
    v.push_back(MakePlanet("金星", "Venus", 6051.8, 0.0, 4.867e24,
                           0.72333566, 0.00677672, 3.39467605, 181.97909950, 131.60246718, 76.67984255,
                           272.76, 67.16, -243.025,
                           KIND_GAS, {0.95f, 0.88f, 0.68f}, {0.86f, 0.76f, 0.55f}, 3, {1.0f, 0.9f, 0.7f}, 0.6f));
    v.push_back(MakePlanet("地球", "Earth", 6378.137, 0.0033528, 5.972e24,
                           1.00000261, 0.01671123, -0.00001531, 100.46457166, 102.93768193, 0.0,
                           0.0, 90.0, 360.0 / 360.98564736629,
                           KIND_EARTH, {0.06f, 0.18f, 0.45f}, {0.22f, 0.40f, 0.17f}, 0, {0.35f, 0.6f, 1.0f}, 1.2f));
    v.push_back(MakePlanet("火星", "Mars", 3396.2, 0.00589, 6.417e23,
                           1.52371034, 0.09339410, 1.84969142, -4.55343205, -23.94362959, 49.55953891,
                           317.68, 52.89, 1.02596,
                           KIND_ROCKY, {0.82f, 0.45f, 0.26f}, {0.52f, 0.25f, 0.14f}, 0, {0.9f, 0.6f, 0.45f}, 0.3f));
    v.push_back(MakePlanet("木星", "Jupiter", 71492, 0.06487, 1.898e27,
                           5.20288700, 0.04838624, 1.30439695, 34.39644051, 14.72847983, 100.47390909,
                           268.06, 64.50, 0.41354,
                           KIND_GAS, {0.90f, 0.82f, 0.68f}, {0.66f, 0.45f, 0.30f}, 14, {0.9f, 0.85f, 0.7f}, 0.3f));
    BodyDef saturn = MakePlanet("土星", "Saturn", 60268, 0.09796, 5.683e26,
                           9.53667594, 0.05386179, 2.48599187, 49.95424423, 92.59887831, 113.66242448,
                           40.59, 83.54, 0.44401,
                           KIND_GAS, {0.93f, 0.86f, 0.66f}, {0.78f, 0.67f, 0.47f}, 12, {0.95f, 0.9f, 0.7f}, 0.3f);
    saturn.rings = true;
    v.push_back(saturn);
    v.push_back(MakePlanet("天王星", "Uranus", 25559, 0.02293, 8.681e25,
                           19.18916464, 0.04725744, 0.77263783, 313.23810451, 170.95427630, 74.01692503,
                           257.31, -15.18, -0.71833,
                           KIND_GAS, {0.66f, 0.87f, 0.91f}, {0.58f, 0.80f, 0.86f}, 4, {0.7f, 0.9f, 1.0f}, 0.5f));
    v.push_back(MakePlanet("海王星", "Neptune", 24764, 0.01708, 1.024e26,
                           30.06992276, 0.00859048, 1.77004347, -55.12002969, 44.96476227, 131.78422574,
                           299.36, 43.46, 0.67125,
                           KIND_GAS, {0.32f, 0.48f, 0.92f}, {0.20f, 0.33f, 0.78f}, 6, {0.4f, 0.6f, 1.0f}, 0.5f));
    v.push_back(MakePlanet("冥王星", "Pluto", 1188.3, 0.0, 1.303e22,
                           39.48211675, 0.24882730, 17.14001206, 238.92903833, 224.06891629, 110.30393684,
                           132.99, -6.16, -6.3872,
                           KIND_ROCKY, {0.88f, 0.80f, 0.70f}, {0.58f, 0.43f, 0.33f}));

    // ---- 衛星 ----                 母天体     半径    質量       軌道長半径  周期[日]   傾斜
    BodyDef moon = MakeMoon("月", "Moon", B_EARTH, 1737.4, 7.342e22, 384400, 27.321661, 5.145,
                            KIND_ROCKY, {0.72f, 0.71f, 0.69f}, {0.40f, 0.40f, 0.40f});
    moon.eclipticRef = true;
    moon.inheritPole = false; moon.poleRA = 269.99; moon.poleDec = 66.54;
    v.push_back(moon);

    v.push_back(MakeMoon("フォボス", "Phobos", B_MARS, 11.27, 1.0659e16, 9376, 0.31891, 1.08,
                         KIND_ROCKY, {0.45f, 0.40f, 0.36f}, {0.28f, 0.25f, 0.22f}));
    v.push_back(MakeMoon("ダイモス", "Deimos", B_MARS, 6.2, 1.4762e15, 23463, 1.26244, 1.79,
                         KIND_ROCKY, {0.55f, 0.50f, 0.44f}, {0.35f, 0.32f, 0.28f}));

    v.push_back(MakeMoon("イオ", "Io", B_JUPITER, 1821.6, 8.932e22, 421700, 1.769138, 0.05,
                         KIND_ROCKY, {0.96f, 0.88f, 0.42f}, {0.78f, 0.48f, 0.18f}));
    v.push_back(MakeMoon("エウロパ", "Europa", B_JUPITER, 1560.8, 4.800e22, 671034, 3.551181, 0.47,
                         KIND_ROCKY, {0.92f, 0.88f, 0.80f}, {0.66f, 0.52f, 0.40f}));
    v.push_back(MakeMoon("ガニメデ", "Ganymede", B_JUPITER, 2634.1, 1.4819e23, 1070412, 7.154553, 0.20,
                         KIND_ROCKY, {0.64f, 0.60f, 0.55f}, {0.38f, 0.35f, 0.31f}));
    v.push_back(MakeMoon("カリスト", "Callisto", B_JUPITER, 2410.3, 1.0759e23, 1882709, 16.689018, 0.19,
                         KIND_ROCKY, {0.48f, 0.43f, 0.37f}, {0.26f, 0.23f, 0.20f}));

    v.push_back(MakeMoon("ミマス", "Mimas", B_SATURN, 198.2, 3.75e19, 185539, 0.942422, 1.57,
                         KIND_ROCKY, {0.80f, 0.80f, 0.78f}, {0.55f, 0.55f, 0.53f}));
    v.push_back(MakeMoon("エンケラドゥス", "Enceladus", B_SATURN, 252.1, 1.08e20, 237948, 1.370218, 0.0,
                         KIND_ROCKY, {0.98f, 0.98f, 1.0f}, {0.85f, 0.88f, 0.92f}));
    v.push_back(MakeMoon("テティス", "Tethys", B_SATURN, 531.1, 6.17e20, 294619, 1.887802, 1.12,
                         KIND_ROCKY, {0.88f, 0.88f, 0.86f}, {0.65f, 0.65f, 0.63f}));
    v.push_back(MakeMoon("ディオネ", "Dione", B_SATURN, 561.4, 1.095e21, 377396, 2.736915, 0.02,
                         KIND_ROCKY, {0.82f, 0.82f, 0.80f}, {0.55f, 0.55f, 0.55f}));
    v.push_back(MakeMoon("レア", "Rhea", B_SATURN, 763.8, 2.31e21, 527108, 4.518212, 0.35,
                         KIND_ROCKY, {0.80f, 0.79f, 0.77f}, {0.52f, 0.51f, 0.50f}));
    v.push_back(MakeMoon("タイタン", "Titan", B_SATURN, 2574.7, 1.345e23, 1221870, 15.945, 0.35,
                         KIND_GAS, {0.86f, 0.66f, 0.32f}, {0.78f, 0.58f, 0.28f}, 2, {0.9f, 0.7f, 0.4f}, 0.8f));
    v.push_back(MakeMoon("イアペトゥス", "Iapetus", B_SATURN, 734.5, 1.806e21, 3560820, 79.3215, 15.47,
                         KIND_ROCKY, {0.85f, 0.83f, 0.78f}, {0.20f, 0.16f, 0.12f}));

    v.push_back(MakeMoon("ミランダ", "Miranda", B_URANUS, 235.8, 6.4e19, 129390, 1.413479, 4.34,
                         KIND_ROCKY, {0.70f, 0.70f, 0.70f}, {0.45f, 0.45f, 0.45f}));
    v.push_back(MakeMoon("アリエル", "Ariel", B_URANUS, 578.9, 1.25e21, 191020, 2.520379, 0.04,
                         KIND_ROCKY, {0.75f, 0.74f, 0.72f}, {0.48f, 0.47f, 0.46f}));
    v.push_back(MakeMoon("ウンブリエル", "Umbriel", B_URANUS, 584.7, 1.27e21, 266000, 4.144177, 0.13,
                         KIND_ROCKY, {0.45f, 0.45f, 0.45f}, {0.28f, 0.28f, 0.28f}));
    v.push_back(MakeMoon("チタニア", "Titania", B_URANUS, 788.9, 3.4e21, 435910, 8.705872, 0.08,
                         KIND_ROCKY, {0.68f, 0.64f, 0.60f}, {0.44f, 0.41f, 0.38f}));
    v.push_back(MakeMoon("オベロン", "Oberon", B_URANUS, 761.4, 3.08e21, 583520, 13.463239, 0.07,
                         KIND_ROCKY, {0.60f, 0.55f, 0.52f}, {0.38f, 0.34f, 0.32f}));

    // トリトンは逆行軌道（傾斜角 > 90°）
    v.push_back(MakeMoon("トリトン", "Triton", B_NEPTUNE, 1353.4, 2.14e22, 354759, 5.876854, 156.885,
                         KIND_ROCKY, {0.88f, 0.82f, 0.80f}, {0.70f, 0.58f, 0.56f}));

    v.push_back(MakeMoon("カロン", "Charon", B_PLUTO, 606.0, 1.586e21, 19591, 6.387230, 0.0,
                         KIND_ROCKY, {0.62f, 0.60f, 0.58f}, {0.40f, 0.36f, 0.34f}));

    // 衛星の昇交点・初期位相を決定的に散らす
    for (size_t i = 0; i < v.size(); i++) {
        if (v[i].parent > 0) {
            v[i].moonNode = std::fmod(i * 73.0, 360.0);
            v[i].moonPhase = std::fmod(i * 137.5, 360.0);
        }
    }
    static const struct { const char* en; int style; } STYLES[] = {
        {"Sun", S_SUN}, {"Mercury", S_MERCURY}, {"Venus", S_VENUS}, {"Earth", S_EARTH}, {"Mars", S_MARS},
        {"Jupiter", S_JUPITER}, {"Saturn", S_SATURN}, {"Uranus", S_URANUS}, {"Neptune", S_NEPTUNE},
        {"Pluto", S_PLUTO}, {"Moon", S_MOON}, {"Io", S_IO}, {"Europa", S_EUROPA}, {"Ganymede", S_GANYMEDE},
        {"Callisto", S_CALLISTO}, {"Mimas", S_ICY}, {"Enceladus", S_ENCELADUS}, {"Tethys", S_ICY},
        {"Dione", S_ICY}, {"Rhea", S_ICY}, {"Titan", S_TITAN}, {"Iapetus", S_IAPETUS}, {"Miranda", S_ICY},
        {"Ariel", S_ICY}, {"Titania", S_ICY}, {"Oberon", S_ICY}, {"Triton", S_TRITON},
    };
    // 地球の自転角: グリニッジ子午線の赤経 = グリニッジ平均恒星時 (GMST)。
    // 本体の座標系では自転角 0 の X 軸が赤経 180° を向くので、180° を引く
    v[B_EARTH].spin0 = 280.46061837 - 180.0;
    for (BodyDef& d : v)
        for (const auto& st : STYLES)
            if (std::strcmp(d.en, st.en) == 0) d.style = st.style;
    return v;
}
