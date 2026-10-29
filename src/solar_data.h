// 太陽系天体データ（実寸）
//  - 半径: km（赤道半径）、扁平率
//  - 惑星軌道: JPL "Approximate Positions of the Planets" J2000 平均軌道要素
//  - 衛星軌道: 月は簡易月位置公式、ほかは JPL の平均軌道要素（J2000）による円軌道近似
//  - 自転軸: IAU 自転軸の赤経・赤緯
#pragma once
#include <cmath>
#include <cstring>
#include <vector>

enum BodyKind { KIND_STAR = 0, KIND_ROCKY = 1, KIND_GAS = 2, KIND_EARTH = 3, KIND_PROBE = 4 };

struct Col3 { float r, g, b; };

// 表面模様の種類（シェーダの detail.x。天体ごとの特徴的な地形・模様を描き分ける）
enum SurfaceStyle {
    S_GENERIC = 0, S_MOON, S_MERCURY, S_MARS, S_IO, S_EUROPA, S_GANYMEDE, S_CALLISTO, S_ICY, S_IAPETUS,
    S_PLUTO, S_ENCELADUS, S_TRITON, S_MOON_TEX, S_MAPPED,
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
    // 上の要素の 1 世紀あたりの変化（JPL "Approximate Positions of the Planets" 表 1）
    double da = 0, de = 0, dinc = 0, dL = 0, dvarpi = 0, dnode = 0;

    // 衛星: 円軌道
    double moonA = 0;       // 軌道長半径 km
    double moonPeriod = 0;  // 公転周期 日
    double moonIncl = 0;    // 傾斜角 deg（eclipticRef なら黄道面基準、それ以外は母天体赤道面基準）
    double moonNode = 0, moonPhase = 0;
    bool eclipticRef = false;
    // JPL の軌道面を使う衛星: 基準面の極（NaN なら母天体の赤道面）。moonNode は基準面が ICRF 赤道と交わる
    // 昇交点から測った昇交点経度、moonPhase は J2000 での面内の角度、libA / libB は秤動の振幅 deg
    bool jpl = false;
    double framePoleRA = 0, framePoleDec = 0;
    double libA = 0, libB = 0;

    bool inheritPole = false;  // 母天体の自転軸を使う
    double poleRA = 0, poleDec = 90;
    double rotPeriod = 1;      // 自転周期 日（負: 逆行、0: 公転と同期）
    double spin0 = 0;          // J2000 での自転角 deg（地球は実際の地図の向きに合わせる）
    double w0 = NAN;           // IAU の本初子午線の角度 W（J2000）deg。あれば地図の経度 0° を実際の向きに合わせる

    int kind = KIND_ROCKY;
    Col3 c1{1, 1, 1}, c2{0.5f, 0.5f, 0.5f}, atm{0, 0, 0};
    float bands = 0;
    float atmStrength = 0;
    bool rings = false;
    bool comet = false;        // 太陽に近づくとコマと尾が出る
    float cometScale = 1;      // 彗星: コマと尾の大きさ・活動を始める距離の倍率（ハレー彗星 = 1）
    int probe = -1;            // 探査機: assets/probes.bin の番号（位置は記録された軌跡から求める）
    bool probeCoast = false;   // 探査機: データの終わりより後も等速直線運動で外へ進む（太陽系を脱出する機体）
    int probeModel = 0;        // 探査機: 機体の形（src/ship_model.h の ProbeModel）
    bool probeEnd = false;     // 探査機: データの終わりが運用終了（天体への突入・着地）
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
                           132.99, -6.16, 6.3872,
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

    // ---- 準惑星・小惑星・彗星（衛星より後ろに置き、衛星の番号を変えない） ----
    // 軌道要素と変化率は tools/fit_small_bodies.py で JPL Horizons の 1980〜2070 年の位置に合わせた値
    // （行末は太陽から見た方向の最大誤差）。自転軸が分かっていないエリスと彗星は黄道北極に近い向きで代用。
    // 周期 3.3 年のエンケ彗星は木星の摂動と非重力効果で近日点の時刻が年ごとにずれ、この形では合わないので入れていない
    static const struct { const char* en; double a, e, i, L, varpi, node, da, de, di, dL, dvarpi, dnode; } SMALL[] = {
        {"Ceres", 2.76734676, 0.07783857, 10.593635, 160.766441, 153.247620, 80.544086,
         -0.00007500, -0.00055623, -0.012593, 7819.405235, 1.628721, -1.476294},  // ±0.59°
        {"Vesta", 2.36159616, 0.08954289, 7.137310, 234.427570, 254.275234, 103.951720,
         0.00022280, -0.00051393, 0.005442, 9918.850838, 1.353349, -0.860247},  // ±0.36°
        {"Eris", 67.81708613, 0.43784850, 44.091072, 20.381545, 187.667891, 35.915724,
         -0.14049099, -0.00135515, 0.005773, 64.644283, -0.093700, 0.081563},  // ±0.005°
        {"Halley", 17.87123378, 0.96704123, 162.236665, 237.311611, 171.040354, 59.046541,
         -0.03386758, -0.00036089, -0.293216, 478.708185, 1.661520, 0.992647},  // ±0.24°
        {"Hale-Bopp", 222.53884755, 0.99590531, 89.433731, 53.312066, 53.014050, 282.363864,
         -3.61013831, -0.00002048, -0.608259, 12.445837, 1.618030, 1.211445},  // ±0.22°
        {"Hyakutake", 1988.32581128, 0.99988433, 124.927752, 318.334357, 318.319476, 188.099666,
         3.06403800, -0.00000317, 0.112184, 0.405531, -0.000062, -0.055328},  // ±0.13°
        // 以下の 2 つは探査機が訪れた頃だけで合わせた値（チュリュモフ・ゲラシメンコ彗星は 2012〜2018 年、
        // リュウグウは 2012〜2024 年）。木星や地球への接近で軌道が変わるので、離れた年ほどずれる
        {"Churyumov-Gerasimenko", 3.47365237, 0.63517690, 6.999529, 271.639269, 62.952908, 50.481488,
         -0.05679580, 0.03909233, 0.271663, 5580.268350, -0.559222, -2.552549},  // ±0.07°
        {"Ryugu", 1.18742232, 0.18899094, 5.909952, 34.280403, 102.779918, 252.062251,
         0.01359665, 0.00718100, -0.171581, 27738.673544, 1.422512, -2.967596},  // ±0.93°
    };
    //                       名前                          半径    扁平率  質量
    //                       自転軸RA  Dec   自転周期[日]
    v.push_back(MakePlanet("ケレス", "Ceres", 482.1, 0.075, 9.384e20, 0, 0, 0, 0, 0, 0,
                           291.42, 66.76, 0.37809,
                           KIND_ROCKY, {0.46f, 0.45f, 0.43f}, {0.30f, 0.29f, 0.28f}));
    v.push_back(MakePlanet("ベスタ", "Vesta", 286.3, 0.22, 2.59e20, 0, 0, 0, 0, 0, 0,
                           309.03, 42.24, 0.22259,
                           KIND_ROCKY, {0.66f, 0.62f, 0.56f}, {0.42f, 0.39f, 0.35f}));
    v.push_back(MakePlanet("エリス", "Eris", 1163, 0.0, 1.647e22, 0, 0, 0, 0, 0, 0,
                           270.0, 66.56, 15.786,
                           KIND_ROCKY, {0.95f, 0.94f, 0.92f}, {0.78f, 0.77f, 0.76f}));
    BodyDef halley = MakePlanet("ハレー彗星", "Halley", 5.5, 0.0, 2.2e14, 0, 0, 0, 0, 0, 0,
                                270.0, 66.56, 2.2,
                                KIND_ROCKY, {0.24f, 0.22f, 0.20f}, {0.12f, 0.11f, 0.10f});
    halley.comet = true;
    v.push_back(halley);
    // ヘール・ボップ彗星（1997 年の大彗星、核の半径 約 30 km）と百武彗星（1996 年に地球から 0.1 AU を通過）
    BodyDef haleBopp = MakePlanet("ヘール・ボップ彗星", "Hale-Bopp", 30, 0.0, 1.3e16, 0, 0, 0, 0, 0, 0,
                                  270.0, 66.56, 0.473,
                                  KIND_ROCKY, {0.24f, 0.22f, 0.20f}, {0.12f, 0.11f, 0.10f});
    haleBopp.comet = true;
    haleBopp.cometScale = 2.5f;
    v.push_back(haleBopp);
    BodyDef hyakutake = MakePlanet("百武彗星", "Hyakutake", 2.1, 0.0, 1e13, 0, 0, 0, 0, 0, 0,
                                   270.0, 66.56, 0.261,
                                   KIND_ROCKY, {0.24f, 0.22f, 0.20f}, {0.12f, 0.11f, 0.10f});
    hyakutake.comet = true;
    hyakutake.cometScale = 0.8f;
    v.push_back(hyakutake);
    // チュリュモフ・ゲラシメンコ彗星（ロゼッタが 2014〜2016 年に周回）とリュウグウ（はやぶさ2 が 2018〜2019 年に滞在）
    BodyDef cg = MakePlanet("チュリュモフ・ゲラシメンコ彗星", "Churyumov-Gerasimenko", 1.65, 0.0, 1e13, 0, 0, 0, 0, 0, 0,
                            69.3, 64.1, 0.5178,
                            KIND_ROCKY, {0.22f, 0.21f, 0.20f}, {0.10f, 0.10f, 0.09f});
    cg.comet = true;
    cg.cometScale = 0.3f;
    v.push_back(cg);
    v.push_back(MakePlanet("リュウグウ", "Ryugu", 0.448, 0.0, 4.5e11, 0, 0, 0, 0, 0, 0,
                           96.4, -66.4, 0.3178,
                           KIND_ROCKY, {0.20f, 0.19f, 0.18f}, {0.10f, 0.09f, 0.09f}));
    // ---- 探査機（並びは tools/make_probe_assets.py と同じ） ----
    //   coast: データの後も直進する, end: データの終わりが運用終了（天体への突入・着地）
    //   機体の形: 0 ボイジャー, 1 パイオニア, 2 ガリレオ, 4 カッシーニ, 6 ニュー・ホライズンズ, 7 パーカー,
    //             8 ジュノー, 9 ロゼッタ, 10 はやぶさ2（ProbeModel）
    static const struct { const char* jp; const char* en; bool coast, end; int model; } PROBES[] = {
        {"ボイジャー1号", "Voyager 1", true, false, 0}, {"ボイジャー2号", "Voyager 2", true, false, 0},
        {"パイオニア10号", "Pioneer 10", true, false, 1}, {"パイオニア11号", "Pioneer 11", true, false, 1},
        {"ガリレオ", "Galileo", false, true, 2}, {"カッシーニ", "Cassini", false, true, 4},
        {"ニュー・ホライズンズ", "New Horizons", true, false, 6},
        {"パーカー・ソーラー・プローブ", "Parker Solar Probe", false, false, 7},
        {"ジュノー", "Juno", false, false, 8}, {"ロゼッタ", "Rosetta", false, true, 9},
        {"はやぶさ2", "Hayabusa2", false, false, 10},
    };
    for (int i = 0; i < (int)(sizeof PROBES / sizeof PROBES[0]); i++) {
        BodyDef d{};
        d.jp = PROBES[i].jp; d.en = PROBES[i].en; d.parent = 0;
        d.radius = 0.002; d.mass = 1000;
        d.rotPeriod = 1e9;
        d.kind = KIND_PROBE; d.c1 = {0.85f, 0.82f, 0.70f}; d.c2 = d.c1;
        d.probe = i; d.probeCoast = PROBES[i].coast; d.probeModel = PROBES[i].model; d.probeEnd = PROBES[i].end;
        v.push_back(d);
    }
    for (BodyDef& d : v)
        for (const auto& o : SMALL)
            if (std::strcmp(d.en, o.en) == 0) {
                d.a_au = o.a; d.e = o.e; d.inc = o.i; d.L0 = o.L; d.varpi = o.varpi; d.node = o.node;
                d.da = o.da; d.de = o.de; d.dinc = o.di; d.dL = o.dL; d.dvarpi = o.dvarpi; d.dnode = o.dnode;
            }

    // 惑星の軌道要素の変化率 /世紀（JPL "Approximate Positions of the Planets" 表 1、1800〜2050 年で有効。
    // 冥王星は表から外れたため以前の版の値）。地球は地球と月の重心の値
    static const struct { const char* en; double da, de, dinc, dL, dvarpi, dnode; } RATES[] = {
        {"Mercury", 0.00000037, 0.00001906, -0.00594749, 149472.67411175, 0.16047689, -0.12534081},
        {"Venus", 0.00000390, -0.00004107, -0.00078890, 58517.81538729, 0.00268329, -0.27769418},
        {"Earth", 0.00000562, -0.00004392, -0.01294668, 35999.37244981, 0.32327364, 0.0},
        {"Mars", 0.00001847, 0.00007882, -0.00813131, 19140.30268499, 0.44441088, -0.29257343},
        {"Jupiter", -0.00011607, -0.00013253, -0.00183714, 3034.74612775, 0.21252668, 0.20469106},
        {"Saturn", -0.00125060, -0.00050991, 0.00193609, 1222.49362201, -0.41897216, -0.28867794},
        {"Uranus", -0.00196176, -0.00004397, -0.00242939, 428.48202785, 0.40805281, 0.04240589},
        {"Neptune", 0.00026291, 0.00005105, 0.00035372, 218.45945325, -0.32241464, -0.00508664},
        {"Pluto", -0.00031596, 0.00005170, 0.00004818, 145.20780515, -0.04062942, -0.01183482},
    };
    for (BodyDef& d : v)
        for (const auto& r : RATES)
            if (std::strcmp(d.en, r.en) == 0) {
                d.da = r.da; d.de = r.de; d.dinc = r.dinc; d.dL = r.dL; d.dvarpi = r.dvarpi; d.dnode = r.dnode;
            }

    // 衛星の軌道（月以外）。軌道面は JPL Solar System Dynamics の平均軌道要素（https://ssd.jpl.nasa.gov/sats/elem/、
    // J2000）の基準面・傾斜角・昇交点。面内の角度 u = u0 + 360 t / P + A sin(2πt/T) + B cos(2πt/T) は
    // tools/fit_moon_orbits.py で JPL Horizons の 1980〜2070 年の位置に合わせた値（行末はその最大誤差）。
    // A, B はミマスとテティスの共鳴による秤動（T = 70.56 年）。円軌道の近似
    // 基準面の極: 赤経・赤緯（EQ: 母天体の赤道面）。天王星は逆行自転なので公転の向きに合わせて南側の極を使う
    static const double EQ = NAN;
    static const struct { const char* en; double a, i, node, ra, dec, u0, P, libA, libB; } ORBITS[] = {
        {"Phobos", 9375, 1.1, 169.2, 317.7, 52.9, 46.087, 0.3189101, 0.00, 0.00},  // ±2.9°
        {"Deimos", 23457, 1.8, 54.3, 316.6, 53.5, 205.081, 1.2624408, 0.00, 0.00},  // ±0.4°
        {"Io", 421800, 0.0, 0.0, 268.1, 64.5, 19.906, 1.7691378, 0.00, 0.00},  // ±0.5°
        {"Europa", 671100, 0.5, 184.0, 268.1, 64.5, 30.397, 3.5511808, 0.00, 0.00},  // ±1.1°
        {"Ganymede", 1070400, 0.2, 58.5, 268.2, 64.6, 163.144, 7.1545531, 0.00, 0.00},  // ±0.3°
        {"Callisto", 1882700, 0.3, 309.1, 268.7, 64.8, 131.402, 16.6890236, 0.00, 0.00},  // ±0.9°
        {"Mimas", 186000, 1.6, 66.2, 40.6, 83.5, 93.678, 0.9424217, -32.04, 28.42},  // ±2.6°
        {"Enceladus", 238400, 0.0, 0.0, 40.6, 83.5, 182.278, 1.3702181, 0.00, 0.00},  // ±0.9°
        {"Tethys", 295000, 1.1, 273.0, 40.6, 83.5, 275.377, 1.8878025, 1.61, -1.33},  // ±0.0°
        {"Dione", 377700, 0.0, 0.0, 40.6, 83.5, 176.932, 2.7369156, 0.00, 0.00},  // ±0.3°
        {"Rhea", 527200, 0.3, 133.7, 40.6, 83.5, 278.466, 4.5175027, 0.00, 0.00},  // ±0.1°
        {"Titan", 1221900, 0.3, 78.6, 36.4, 84.0, 293.055, 15.9454360, 0.00, 0.00},  // ±3.5°
        {"Iapetus", 3561700, 7.6, 86.5, 288.7, 78.9, 113.793, 79.3308625, 0.00, 0.00},  // ±2.5°
        {"Miranda", 129846, 4.4, 100.9, 77.31, 15.18, 227.820, 1.4134794, 0.00, 0.00},  // ±2.0°
        {"Ariel", 190929, 0.0, 0.0, 77.31, 15.18, 203.126, 2.5203792, 0.00, 0.00},  // ±0.2°
        {"Umbriel", 265986, 0.1, 174.8, 77.31, 15.18, 76.458, 4.1441771, 0.00, 0.00},  // ±0.5°
        {"Titania", 436298, 0.1, 29.5, 77.31, 15.18, 252.124, 8.7058699, 0.00, 0.00},  // ±0.3°
        {"Oberon", 583511, 0.1, 76.8, 77.31, 15.18, 275.762, 13.4632373, 0.00, 0.00},  // ±0.3°
        {"Triton", 354800, 157.3, 177.7, EQ, EQ, 58.444, 5.8768438, 0.00, 0.00},  // ±0.7°
        {"Charon", 19600, 0.0, 0.0, EQ, EQ, 304.119, 6.3872221, 0.00, 0.00},  // ±0.0°
    };
    for (BodyDef& d : v) {
        for (const auto& o : ORBITS) {
            if (std::strcmp(d.en, o.en) != 0) continue;
            d.jpl = true;
            d.moonA = o.a;
            d.moonIncl = o.i;
            d.moonNode = o.node;
            d.framePoleRA = o.ra;
            d.framePoleDec = o.dec;
            d.moonPhase = o.u0;
            d.moonPeriod = o.P;
            d.libA = o.libA;
            d.libB = o.libB;
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
    // IAU の本初子午線 W = W0 + 360° × t / 自転周期（WGCCRE。自転軸もこの表の値に合わせてある）。
    // 冥王星の自転軸は右手系の北（赤緯 -6.16°）で、自転は順行（カロンの公転と同じ向き）
    static const struct { const char* en; double w0; } PRIME[] = {
        {"Mercury", 329.5988}, {"Mars", 176.630}, {"Ceres", 170.650}, {"Vesta", 285.39}, {"Pluto", 302.695},
    };
    for (BodyDef& d : v)
        for (const auto& pm : PRIME)
            if (std::strcmp(d.en, pm.en) == 0) d.w0 = pm.w0;
    // 地球の自転角: グリニッジ子午線の赤経 = グリニッジ平均恒星時 (GMST)。
    // 本体の座標系では自転角 0 の X 軸が赤経 180° を向くので、180° を引く
    v[B_EARTH].spin0 = 280.46061837 - 180.0;
    for (BodyDef& d : v)
        for (const auto& st : STYLES)
            if (std::strcmp(d.en, st.en) == 0) d.style = st.style;
    return v;
}
