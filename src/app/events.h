// Solar System Voyager — Copyright (c) 2026 hubstacksea
// MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
// 天文イベント・日時の指定・自動ツアー。
// main.cpp の一部で、main.cpp が決まった順に読み込む（単体ではコンパイルしない）

// ---------------------------------------------------------------- 天文イベント
// 太陽 → 影を落とす天体 の延長線（影の軸）に対する、相手の天体の中心の位置。
// along: 影を落とす天体から軸に沿った距離、perp: 軸からの距離（km）
static void ShadowAxis(V3 sun, V3 caster, V3 target, double* along, double* perp) {
    V3 u = Norm(caster - sun);
    V3 d = target - caster;
    *along = Dot(d, u);
    *perp = Len(d - u * *along);
}

// 地球の天体固定座標での緯度・経度 deg
static void LatLonOf(const Body& b, V3 dir, double* lat, double* lon) {
    V3 X, Z;
    BodyFixedAxes(b, &X, &Z);
    *lat = std::asin(std::max(-1.0, std::min(1.0, Dot(dir, b.pole)))) / DEG;
    *lon = std::atan2(-Dot(dir, Z), Dot(dir, X)) / DEG;
}

// 日食・月食の判定に使う量（地球と月だけ計算するので速い）
struct EclipseGeom { double solarPerp, solarAlong, lunarPerp, lunarAlong; bool newSide; };
static EclipseGeom EclipseAt(const std::vector<Body>& B, double t) {
    V3 E = EarthPos(B[B_EARTH].d, t), M = E + MoonGeoPos(t), S;
    EclipseGeom g;
    ShadowAxis(S, M, E, &g.solarAlong, &g.solarPerp);   // 月の影と地球
    ShadowAxis(S, E, M, &g.lunarAlong, &g.lunarPerp);   // 地球の影と月
    g.newSide = Dot(M - E, S - E) > 0;                   // 月が太陽側（新月の側）
    return g;
}

// f(t) の極小を [a, b] で黄金分割探索
template <class F> static double MinimizeT(F f, double a, double b) {
    const double r = 0.6180339887;
    double c = b - (b - a) * r, d = a + (b - a) * r, fc = f(c), fd = f(d);
    for (int i = 0; i < 40; i++) {
        if (fc < fd) { b = d; d = c; fd = fc; c = b - (b - a) * r; fc = f(c); }
        else { a = c; c = d; fc = fd; d = a + (b - a) * r; fd = f(d); }
    }
    return (a + b) * 0.5;
}

// 日食の種類と中心位置（食の最大の時刻 t）
static bool ClassifySolar(std::vector<Body>& B, double t, AstroEvent* ev) {
    UpdateBodies(B, t);
    const double Rs = B[B_SUN].d.radius, Rm = B[B_MOON].d.radius, Re = B[B_EARTH].d.radius;
    V3 S = B[B_SUN].pos, E = B[B_EARTH].pos, M = B[B_MOON].pos;
    double along, perp;
    ShadowAxis(S, M, E, &along, &perp);
    if (along <= 0) return false;
    double dSM = Len(M - S);
    double rPen = Rm + along * (Rs + Rm) / dSM;          // 地球の位置での半影の半径
    if (perp > Re + rPen) return false;
    V3 u = Norm(M - S), c = M + u * along;               // 軸上で地球中心に最も近い点
    V3 hit;
    if (perp < Re) {                                     // 影の軸が地球に当たる（中心食）
        double k = std::sqrt(Re * Re - perp * perp);
        hit = c - u * k;
        double sSurf = Dot(hit - M, u);
        double rUmb = Rm - sSurf * (Rs - Rm) / dSM;      // 地表での本影の半径（負なら金環）
        ev->kind = rUmb > 0 ? EV_SOLAR_TOTAL : EV_SOLAR_ANNULAR;
    } else {
        hit = E + Norm(c - E) * Re;
        ev->kind = EV_SOLAR_PARTIAL;
    }
    ev->t = t;
    LatLonOf(B[B_EARTH], Norm(hit - E), &ev->lat, &ev->lon);
    return true;
}

static bool ClassifyLunar(std::vector<Body>& B, double t, AstroEvent* ev) {
    UpdateBodies(B, t);
    const double Rs = B[B_SUN].d.radius, Rm = B[B_MOON].d.radius, Re = B[B_EARTH].d.radius;
    V3 S = B[B_SUN].pos, E = B[B_EARTH].pos, M = B[B_MOON].pos;
    double along, perp;
    ShadowAxis(S, E, M, &along, &perp);
    if (along <= 0) return false;
    double rUmb = Re * 1.02 - along * (Rs - Re) / Len(E - S);   // 月の位置での本影（大気で 2% 広がる）
    if (perp - Rm >= rUmb) return false;                        // 半影月食は目立たないので含めない
    ev->kind = perp + Rm < rUmb ? EV_LUNAR_TOTAL : EV_LUNAR_PARTIAL;
    ev->t = t;
    return true;
}

// 2 つの向きの間の角度 rad（小さい角度でも精度が落ちない式）
static double AngleBetween(V3 a, V3 b) { return std::atan2(Len(Cross(a, b)), Dot(a, b)); }

// [a, b] を step ごとに調べ、f の極小（f < limit）の時刻を細かく求めて返す
template <class F> static std::vector<double> FindMinima(F f, double a, double b, double step, double limit) {
    std::vector<double> out;
    double p0 = f(a - step), p1 = f(a);
    for (double t = a; t < b; t += step) {
        double p2 = f(t + step);
        if (p1 < p0 && p1 <= p2 && p1 < limit) out.push_back(MinimizeT(f, t - step, t + step));
        p0 = p1; p1 = p2;
    }
    return out;
}

// 惑星のイベント（地球の中心から見る）:
//  - 水星・金星の太陽面通過: 150 年先までの、それぞれ次の 2 回
//  - 惑星どうしの接近: 3 年分。角距離 1° 未満で、どちらも太陽から 15° 以上離れているもの
//  - 衝（地球をはさんで太陽の反対側）: 火星〜海王星の 3 年分
static void FindPlanetEvents(const std::vector<Body>& B, double t0, std::vector<AstroEvent>* out) {
    // 3 年分の位置は 0.25 日ごとに先に表にしておく（VSOP87 の級数は項が多いので、組ごとに計算し直さない）。
    // 表の時刻から外れた時刻（極小を細かく求める時）はその都度計算する
    const double span = 3 * 365.25, h = 0.25;
    std::vector<V3> tab[B_NEPTUNE + 1];
    for (int i = B_MERCURY; i <= B_NEPTUNE; i++)
        for (double k = -1; k <= span / h + 1; k++) tab[i].push_back(SimPlanetPos(B, i, t0 + k * h));
    auto pos = [&](int i, double t) {
        double f = (t - t0) / h + 1;
        long k = std::lround(f);
        if (k >= 0 && k < (long)tab[i].size() && std::fabs(f - k) < 1e-6) return tab[i][k];
        return SimPlanetPos(B, i, t);
    };
    // 太陽面通過: 10 年ずつ調べ、2 回見つかったら止める。内合の前後で太陽との角度は 1 日に水星 2.3°、金星 1.6° ほど
    // 変わるので、水星は 1 日、金星は 2 日ごとに調べる
    for (int p : {B_MERCURY, B_VENUS}) {
        auto f = [&](double t) {
            V3 e = SimPlanetPos(B, B_EARTH, t), q = SimPlanetPos(B, p, t) - e;
            return Len(q) < Len(e) && Dot(q, e) < 0 ? AngleBetween(q, e * -1.0) : 1e9;
        };
        const double st = p == B_MERCURY ? 1.0 : 2.0, chunk = 10 * 365.25;
        int found = 0;
        for (double a = t0; a < t0 + 150 * 365.25 && found < 2; a += chunk)
            for (double tm : FindMinima(f, a, a + chunk, st, 0.06)) {
                V3 e = SimPlanetPos(B, B_EARTH, tm), q = SimPlanetPos(B, p, tm) - e;
                double sunR = std::asin(B[B_SUN].d.radius / Len(e)), plR = std::asin(B[p].d.radius / Len(q));
                if (f(tm) >= sunR + plR) continue;
                AstroEvent ev;
                ev.kind = EV_TRANSIT;
                ev.t = tm;
                ev.body = p;
                ev.dist = f(tm) / sunR;
                out->push_back(ev);
                if (++found == 2) break;
            }
    }
    const int P[] = {B_MERCURY, B_VENUS, B_MARS, B_JUPITER, B_SATURN, B_URANUS, B_NEPTUNE};
    for (int i = 0; i < 7; i++)
        for (int j = i + 1; j < 7; j++) {
            auto sep = [&](double t) {
                V3 e = pos(B_EARTH, t);
                return AngleBetween(pos(P[i], t) - e, pos(P[j], t) - e);
            };
            for (double tm : FindMinima(sep, t0, t0 + span, 0.25, 1.0 * DEG)) {
                V3 e = pos(B_EARTH, tm);
                if (AngleBetween(pos(P[i], tm) - e, e * -1.0) < 15 * DEG || AngleBetween(pos(P[j], tm) - e, e * -1.0) < 15 * DEG)
                    continue;
                AstroEvent ev;
                ev.kind = EV_CONJ;
                ev.t = tm;
                ev.body = P[i];
                ev.body2 = P[j];
                ev.dist = sep(tm) / DEG;
                out->push_back(ev);
            }
        }
    for (int p : {B_MARS, B_JUPITER, B_SATURN, B_URANUS, B_NEPTUNE}) {
        auto f = [&](double t) { V3 e = pos(B_EARTH, t); return AngleBetween(pos(p, t) - e, e); };
        for (double tm : FindMinima(f, t0, t0 + span, 1.0, 0.1)) {
            AstroEvent ev;
            ev.kind = EV_OPPOSITION;
            ev.t = tm;
            ev.body = p;
            ev.dist = Len(pos(p, tm) - pos(B_EARTH, tm));
            out->push_back(ev);
        }
    }
}

// 彗星: 1980〜2070 年（軌道を合わせた期間）の近日点と、その前後 200 日の地球への最接近（2 AU 以内）
static std::vector<AstroEvent> FindCometEvents(const App& app) {
    std::vector<AstroEvent> out;
    const double t0 = -20 * 365.25, t1 = 70 * 365.25;
    for (size_t i = 0; i < app.B.size(); i++) {
        const BodyDef& d = app.B[i].d;
        if (!d.comet) continue;
        auto r = [&](double t) { return Len(PlanetPos(d, t)); };
        auto de = [&](double t) { return Len(PlanetPos(d, t) - SimPlanetPos(app.B, B_EARTH, t)); };
        for (double tp : FindMinima(r, t0, t1, 1.0, 1e300)) {
            AstroEvent ev;
            ev.kind = EV_PERIHELION;
            ev.t = tp;
            ev.body = (int)i;
            ev.dist = r(tp);
            out.push_back(ev);
            double best = 1e300, bt = tp;
            for (double tm : FindMinima(de, tp - 200, tp + 200, 0.25, 2 * AU_KM))
                if (de(tm) < best) { best = de(tm); bt = tm; }
            if (best < 2 * AU_KM) {
                ev.kind = EV_COMET_EARTH;
                ev.t = bt;
                ev.dist = best;
                out.push_back(ev);
            }
        }
    }
    // 軌跡を持つ小惑星（アポフィス）の地球への最接近: 地球中心の区間で一番近い時刻。宇宙からと地上からの 2 場面
    for (size_t i = 0; i < app.B.size(); i++) {
        const BodyDef& d = app.B[i].d;
        if (d.track < 0) continue;
        for (const ProbeSeg& sg : g_probes[d.track].segs) {
            if (sg.body != B_EARTH) continue;
            auto f = [&](double t) { return Len(SegPos(sg, t)); };
            double t0 = sg.t.front(), t1 = sg.t.back(), tb = t0;
            for (int k = 1; k <= 4000; k++) {
                double t = t0 + (t1 - t0) * k / 4000;
                if (f(t) < f(tb)) tb = t;
            }
            double h = (t1 - t0) / 4000;
            AstroEvent ev;
            ev.kind = EV_COMET_EARTH;
            ev.t = MinimizeT(f, std::max(t0, tb - h), std::min(t1, tb + h));
            ev.body = (int)i;
            ev.dist = f(ev.t);
            out.push_back(ev);
            ev.moon = 1;   // 地上から
            out.push_back(ev);
        }
    }
    // 連星の近星点通過（論文の軌道の近点通過の時刻 T から周期 P ごと。年は 365.25 日で J2000 からの日数に）
    for (size_t i = 0; i < app.B.size(); i++) {
        int si = app.B[i].d.star;
        if (si < 0 || NEARBY_STARS[si].P <= 0 || NEARBY_STARS[si].k >= 0) continue;   // 主星（係数が負）だけ
        const NearbyStar& ns = NEARBY_STARS[si];
        int comp = -1;
        for (size_t j = 0; j < app.B.size(); j++)
            if (j != i && app.B[j].d.star >= 0 && NEARBY_STARS[app.B[j].d.star].system == ns.system) comp = (int)j;
        if (comp < 0) continue;
        double y0 = 2000 + t0 / 365.25, y1 = 2000 + t1 / 365.25;
        for (double y = ns.T + std::ceil((y0 - ns.T) / ns.P) * ns.P; y < y1; y += ns.P) {
            AstroEvent ev;
            ev.kind = EV_PERIASTRON;
            ev.t = (y - 2000) * 365.25;
            ev.body = (int)i;
            ev.body2 = comp;
            ev.dist = ns.a * 1000.0 / ns.plx * (1 - ns.e) * AU_KM;
            out.push_back(ev);
        }
    }
    std::sort(out.begin(), out.end(), [](const AstroEvent& a, const AstroEvent& b) { return a.t < b.t; });
    return out;
}

// ---------------------------------------------------------------- 歴史上の日食・月食・ハレー彗星
// 史実の日付（1582 年 10 月 15 日より前はユリウス暦）の前後 2 日で食を探し、日食は史実の場所から見て太陽と月が最も近づく
// 時刻にする（その場所の地上から見る）。ハレー彗星は前後 80 日で近日点通過を探す
struct HistoricEclipse {
    const char* date;      // "YYYYMMDD"（紀元前は先頭に -）
    int kind;              // 0: 日食, 1: 月食, 2: ハレー彗星の近日点通過, 3: 超新星（SUPERNOVAE と同じ日付）, 4: 金星の太陽面通過,
                           // 5: シューメーカー・レビー第 9 彗星の木星衝突（破片 G の衝突痕が昼側に来た時、その上空から）,
                           // 6: 地上から惑星を望遠鏡のように見る（日付に時刻まで入れる。ガリレオの木星の衛星）
    double lat, lon;       // 史実の場所
    const char* jp; const char* en;          // 名前
    const char* placeJp; const char* placeEn;
};
static const HistoricEclipse HISTORY[] = {
    {"-07620615", 0, 36.36, 43.15, "アッシリアの日食", "Assyrian eclipse", "ニネヴェ（イラク北部）", "Nineveh (northern Iraq)"},
    {"-05840528", 0, 39.5, 34.0, "タレスの日食", "Eclipse of Thales", "ハリュス川（トルコ中部）", "Halys River (central Turkey)"},
    {"11831117", 0, 34.53, 133.73, "水島の戦いの金環日食", "Annular eclipse at the Battle of Mizushima", "水島（岡山県倉敷市）",
     "Mizushima (Okayama, Japan)"},
    {"15040229", 1, 18.44, -77.20, "コロンブスの月食", "Columbus's lunar eclipse", "ジャマイカ北岸", "north coast of Jamaica"},
    {"19190529", 0, 1.62, 7.40, "エディントンの日食観測", "Eddington's eclipse expedition", "プリンシペ島", "Principe"},
    {"20090722", 0, 29.47, 129.60, "21 世紀最長の皆既日食", "Longest total eclipse of the 21st century", "トカラ列島 悪石島", "Akusekijima, Tokara Islands"},
    // ハレー彗星（回帰ごとの軌道。場所の代わりに記録の名前）
    {"-02390525", 2, 0, 0, "最古の確かなハレー彗星の記録", "Earliest sure record of Halley's Comet", "『史記』秦始皇本紀",
     "Records of the Grand Historian"},
    {"10660320", 2, 0, 0, "ノルマン・コンクエストのハレー彗星", "Halley's Comet of the Norman Conquest", "バイユーのタペストリー",
     "Bayeux Tapestry"},
    {"13011025", 2, 0, 0, "ジョットが描いたハレー彗星", "Halley's Comet painted by Giotto", "「東方三博士の礼拝」", "Adoration of the Magi"},
    // 超新星（記録した人がいた場所の、その夜の空。日付は最初に見つかった日）
    {"10060501", 3, 30.04, 31.24, "超新星 1006（おおかみ座）", "SN 1006 (Lupus)", "カイロ（イブン・リドワーンの記録）", "Cairo (recorded by Ibn Ridwan)"},
    {"10540704", 3, 34.80, 114.31, "超新星 1054（かに星雲のもと）", "SN 1054 (the Crab Nebula)", "開封（宋の都。『宋史』）", "Kaifeng (Song capital)"},
    {"15721116", 3, 56.08, 13.05, "ティコの超新星（1572 年）", "Tycho's supernova (1572)", "ヘレヴァド修道院（デンマーク）", "Herrevad Abbey (Denmark)"},
    {"16041009", 3, 50.09, 14.42, "ケプラーの超新星（1604 年）", "Kepler's supernova (1604)", "プラハ", "Prague"},
    // 金星の太陽面通過（地球と金星を結ぶ線の上から、ズームして太陽を見る）
    {"16391204", 4, 53.70, -2.80, "ホロックスの金星の太陽面通過", "Horrocks's transit of Venus", "マッチ・フール（イングランド）", "Much Hoole (England)"},
    {"17690603", 4, -17.50, -149.50, "クックの金星の太陽面通過観測", "Cook's transit of Venus", "タヒチ（ポイント・ビーナス）", "Tahiti (Point Venus)"},
    {"18821206", 4, 0, 0, "19 世紀最後の金星の太陽面通過", "Last transit of Venus of the 19th century", "世界各地の観測隊", "expeditions worldwide"},
    {"161001071715", 6, 45.41, 11.88, "ガリレオが木星の衛星を見つけた夜", "Galileo discovers Jupiter's moons", "パドヴァ（東に 2 つ・西に 1 つ）",
     "Padua (two to the east, one to the west)"},
    {"19940718", 5, 0, 0, "シューメーカー・レビー第 9 彗星の木星衝突", "Comet Shoemaker-Levy 9 hits Jupiter", "破片 G の衝突痕",
     "the scar of fragment G"},
};

// 歴史上の超新星（肉眼で見えたもの）。位置は残骸の J2000 赤経・赤緯、最大の明るさ（等級）、最大のころの日（ユリウス暦）、
// 肉眼で見えた日数（記録の最後まで）。明るさは 15 日で最大になり、記録で見えなくなった日に 6 等になるよう直線で暗くなる
// とする（目安）。値は各超新星の記録をまとめた文献（Stephenson & Green 2002 など。Wikipedia の各記事で照合）による
// シューメーカー・レビー第 9 彗星の破片の木星への衝突（1994 年 7 月）。JPL Horizons の各破片の軌道から、木星の 1 気圧の面
// （扁平な楕円体）に入った時刻 [J2000 からの日数（世界時）] と、その時の木星中心からの位置 [km, J2000 黄道]。
// 衝突痕の大きさ（木星の中心から見た半径 rad）は、大きな痕を残した破片（G・K・L）・中くらい・小さいの 3 段階の目安
struct Sl9Impact { const char* name; double t, x, y, z, size; };
static const Sl9Impact SL9[] = {
    {"A", -1994.688377, 18749, -45132, -48940, 0.050},
    {"B", -1994.410992, 17618, -45692, -48863, 0.025},
    {"C", -1994.229951, 19387, -44782, -49012, 0.050},
    {"D", -1994.034323, 19410, -44705, -49047, 0.050},
    {"E", -1993.896175, 19616, -44607, -49052, 0.050},
    {"F", -1993.504325, 18719, -44799, -49218, 0.025},
    {"G", -1993.214215, 20177, -44215, -49161, 0.090},
    {"H", -1992.715342, 20475, -43958, -49276, 0.050},
    {"K", -1992.095795, 21177, -43563, -49324, 0.090},
    {"L", -1991.601005, 21574, -43246, -49390, 0.090},
    {"N", -1991.092377, 21326, -43005, -49697, 0.025},
    {"P2", -1990.889681, 20352, -43058, -50003, 0.025},
    {"Q2", -1990.706915, 21802, -42781, -49681, 0.025},
    {"Q1", -1990.686447, 22375, -42815, -49402, 0.050},
    {"R", -1990.296806, 22637, -42575, -49480, 0.050},
    {"S", -1989.893112, 22710, -42414, -49593, 0.050},
    {"T", -1989.772596, 20400, -42680, -50251, 0.025},
    {"U", -1989.612820, 21986, -42488, -49801, 0.025},
    {"V", -1989.346573, 22336, -42354, -49789, 0.025},
    {"W", -1989.191881, 23412, -42158, -49477, 0.025},
};
static const int SL9_COUNT = sizeof SL9 / sizeof SL9[0];

// 衝突痕の木星に固定した向き（本体座標の単位ベクトル。衝突の時刻の木星の自転で求める）
static std::vector<V3> Sl9FixedDirs(const std::vector<Body>& B0) {
    std::vector<Body> B = B0;
    std::vector<V3> out;
    for (const Sl9Impact& s : SL9) {
        UpdateBodies(B, s.t);
        const Body& j = B[B_JUPITER];
        V3 X, Z, v = Norm(EclToRender(s.x, s.y, s.z));
        BodyFixedAxes(j, &X, &Z);
        out.push_back(Norm(V3(Dot(v, X), Dot(v, j.pole), Dot(v, Z))));
    }
    return out;
}
// 本体座標の向き f → 今の木星の自転での向き（描画座標）
static V3 JupiterFixedToWorld(const Body& j, V3 f) {
    V3 X, Z;
    BodyFixedAxes(j, &X, &Z);
    return X * f.x + j.pole * f.y + Z * f.z;
}


struct Supernova { double ra, dec, peak; const char* date; double days; };
static const Supernova SUPERNOVAE[] = {
    {225.53, -41.95, -7.5, "10060501", 640},   // SN 1006（明るい 3 か月の後、さらに約 18 か月見えた）
    {83.633, 22.015, -6.0, "10540704", 642},   // SN 1054（昼に 23 日。最後の記録は 1056 年 4 月 6 日）
    {6.32, 64.14, -4.0, "15721116", 480},      // ティコ（11 月初めに現れ、16 日ごろ最大。1574 年初めまで）
    {262.675, -21.48, -2.5, "16041009", 365},  // ケプラー（10 月 9 日に最初の記録。ケプラーが 1 年追った）
};
static const int SUPERNOVA_COUNT = sizeof SUPERNOVAE / sizeof SUPERNOVAE[0];
// ---------------------------------------------------------------- 流星群
// 主な流星群（IMO の資料による極大の太陽黄経・放射点・ZHR。幅は活動の広がり（太陽黄経の標準偏差 deg）の目安）
struct MeteorShower {
    UiId name;
    double solLon, ra, dec, zhr, width;
};
static const MeteorShower SHOWERS[] = {
    {UI_MS_QUA, 283.15, 230.1, 48.5, 110, 0.45},   // しぶんぎ座
    {UI_MS_LYR, 32.32, 271.4, 33.6, 18, 0.6},      // 4 月こと座
    {UI_MS_ETA, 45.5, 338.0, -1.0, 50, 2.5},       // みずがめ座η
    {UI_MS_PER, 140.0, 48.0, 58.0, 100, 1.3},      // ペルセウス座
    {UI_MS_ORI, 208.0, 95.0, 16.0, 20, 2.5},       // オリオン座
    {UI_MS_LEO, 235.27, 152.0, 22.0, 15, 0.6},     // しし座
    {UI_MS_GEM, 262.2, 112.0, 33.0, 150, 0.9},     // ふたご座
};
static const int SHOWER_COUNT = sizeof SHOWERS / sizeof SHOWERS[0];

// 太陽黄経 deg（地球から見た太陽の黄経。春分で 0）
static double SolarLongitude(const std::vector<Body>& B, double t) {
    V3 s = PosAt(B, B_SUN, t) - PosAt(B, B_EARTH, t);
    return std::fmod(std::atan2(-s.z, s.x) / DEG + 360.0, 360.0);
}

// その時の流星群の ZHR（極大からの太陽黄経の差で減る）
static double ShowerZHR(const MeteorShower& m, double solLon) {
    double d = std::fabs(std::fmod(solLon - m.solLon + 540.0, 360.0) - 180.0);
    if (d > m.width * 4) return 0;
    return m.zhr * std::exp(-0.5 * (d / m.width) * (d / m.width));
}

// 地上の点（緯度・経度 deg）の真上の向き（時刻 t）
static V3 GroundUp(const std::vector<Body>& B, int g, double latDeg, double lonDeg, double t) {
    const Body& b = B[g];
    double s = SpinAt(B, g, t), cs = std::cos(s), sn = std::sin(s);
    V3 X = b.ex * cs - b.ez * sn, Z = b.ex * sn + b.ez * cs;
    double la = latDeg * DEG, lo = lonDeg * DEG;
    return X * (std::cos(la) * std::cos(lo)) + b.pole * std::sin(la) - Z * (std::cos(la) * std::sin(lo));
}

// 極大の前後 1 日で、東京から一番よく見える時刻（空が暗く、放射点が高い）
static double BestMeteorTime(const std::vector<Body>& B, const MeteorShower& m, double tPeak, double lat, double lon) {
    V3 rad = EquToRender(m.ra, m.dec);
    double best = tPeak, bestScore = -1;
    for (double t = tPeak - 1; t <= tPeak + 1; t += 1.0 / 144) {
        V3 up = GroundUp(B, B_EARTH, lat, lon, t);
        double sunAlt = Dot(Norm(PosAt(B, B_SUN, t) - PosAt(B, B_EARTH, t)), up);
        if (sunAlt > std::sin(-12 * DEG)) continue;
        double score = Dot(rad, up) * ShowerZHR(m, SolarLongitude(B, t)) / m.zhr;
        if (score > bestScore) { bestScore = score; best = t; }
    }
    return best;
}

static void FindMeteorEvents(const std::vector<Body>& B, double t0, double span, std::vector<AstroEvent>* out) {
    for (int k = 0; k < SHOWER_COUNT; k++) {
        const MeteorShower& m = SHOWERS[k];
        auto f = [&](double t) { return std::fmod(SolarLongitude(B, t) - m.solLon + 540.0, 360.0) - 180.0; };   // 極大で 0
        double prev = f(t0);
        for (double t = t0 + 1; t < t0 + span; t += 1) {
            double cur = f(t);
            if (prev < 0 && cur >= 0 && cur - prev < 10) {
                double a = t - 1, b = t;
                for (int i = 0; i < 30; i++) {
                    double mid = (a + b) / 2;
                    if (f(mid) < 0) a = mid; else b = mid;
                }
                AstroEvent ev;
                ev.kind = EV_METEOR;
                ev.t = (a + b) / 2;
                ev.moon = k;   // 流星群の番号
                ev.body = B_EARTH;
                ev.dist = m.zhr;
                out->push_back(ev);
            }
            prev = cur;
        }
    }
}

// t0 から先のイベントを探す: 日食・月食は 3 年分、木星面の影の通過は 4 衛星それぞれ次の 1 回、ほかは FindPlanetEvents
static std::vector<AstroEvent> FindEvents(const App& app, double t0) {
    std::vector<Body> B = app.B;
    std::vector<AstroEvent> out;
    const double step = 0.05, span = 3 * 365.25;
    auto solarF = [&](double t) { EclipseGeom g = EclipseAt(B, t); return g.newSide ? g.solarPerp : 1e12; };
    auto lunarF = [&](double t) { EclipseGeom g = EclipseAt(B, t); return g.newSide ? 1e12 : g.lunarPerp; };
    double ps[3] = {solarF(t0 - step), solarF(t0), 0}, pl[3] = {lunarF(t0 - step), lunarF(t0), 0};
    for (double t = t0; t < t0 + span; t += step) {
        ps[2] = solarF(t + step);
        pl[2] = lunarF(t + step);
        AstroEvent ev;
        if (ps[1] < ps[0] && ps[1] <= ps[2] && ps[1] < 2.0e4) {
            double tm = MinimizeT(solarF, t - step, t + step);
            if (ClassifySolar(B, tm, &ev)) {
                out.push_back(ev);
                if (ev.kind != EV_SOLAR_PARTIAL) {   // 皆既・金環は、食の中心の地上から見る場面も並べる
                    AstroEvent g = ev;
                    g.kind = EV_ECLIPSE_GROUND;
                    g.body2 = ev.kind;
                    out.push_back(g);
                }
            }
        }
        if (pl[1] < pl[0] && pl[1] <= pl[2] && pl[1] < 2.0e4) {
            double tm = MinimizeT(lunarF, t - step, t + step);
            if (ClassifyLunar(B, tm, &ev)) out.push_back(ev);
        }
        ps[0] = ps[1]; ps[1] = ps[2];
        pl[0] = pl[1]; pl[1] = pl[2];
    }
    // 木星面の影: 衛星が太陽側にあり、影の軸が木星に当たる時刻
    const int galilean[4] = {B_MOON + 3, B_MOON + 4, B_MOON + 5, B_MOON + 6};
    for (int m : galilean) {
        auto f = [&](double t) {
            UpdateBodies(B, t);
            double along, perp;
            ShadowAxis(B[B_SUN].pos, B[m].pos, B[B_JUPITER].pos, &along, &perp);
            return along > 0 ? perp : 1e12;
        };
        const double st = 0.01;
        double p0 = f(t0 - st), p1 = f(t0);
        for (double t = t0; t < t0 + 30; t += st) {
            double p2 = f(t + st);
            if (p1 < p0 && p1 <= p2 && p1 < B[B_JUPITER].d.radius) {
                AstroEvent ev;
                ev.kind = EV_SHADOW;
                ev.moon = m;
                ev.t = MinimizeT(f, t - st, t + st);
                out.push_back(ev);
                break;
            }
            p0 = p1; p1 = p2;
        }
    }
    FindPlanetEvents(B, t0, &out);
    FindMeteorEvents(B, t0, span, &out);
    std::sort(out.begin(), out.end(), [](const AstroEvent& a, const AstroEvent& b) { return a.t < b.t; });
    return out;
}

// 探査機のできごと: 惑星への最接近（フライバイ）、周回機の到着（周回軌道に入る最初の最接近）、運用終了（突入の 10 分前）
static std::vector<AstroEvent> FindProbeEvents(const App& app) {
    std::vector<AstroEvent> out;
    for (size_t i = 0; i < app.B.size(); i++) {
        const BodyDef& d = app.B[i].d;
        if (d.probe < 0 || d.probe >= (int)g_probes.size()) continue;
        const std::vector<ProbeSeg>& S = g_probes[d.probe].segs;
        for (const ProbeSeg& sg : S) {
            if (sg.body <= 0) continue;
            // 周回機: 長い区間か、データの最後の区間（周回に入るところや到着の直前でデータが終わる機体）
            bool orbiter = sg.t.back() - sg.t.front() > 200 || (&sg == &S.back() && sg.t.back() - sg.t.front() > 60);
            if (orbiter && app.B[sg.body].d.kind == KIND_ROCKY && app.B[sg.body].d.a_au > 0 && sg.body > B_MOON) {
                // 彗星・小惑星への到着は区間の始まり（周回や待機の開始）
                AstroEvent ev;
                ev.kind = EV_ARRIVE;
                ev.t = sg.t.front();
                ev.probe = (int)i;
                ev.body = sg.body;
                ev.dist = Len(sg.p.front());
                out.push_back(ev);
                continue;
            }
            if (sg.body > B_PLUTO && !orbiter) {
                // 小惑星のそばの短い区間: ほぼ直線なので点が両端しか残らないことがある。補間した軌跡を細かくたどる
                auto f = [&](double t) { return Len(SegPos(sg, t)); };
                double t0 = sg.t.front(), t1 = sg.t.back(), tb = t0;
                const int n = 2000;
                for (int k = 1; k <= n; k++) {
                    double t = t0 + (t1 - t0) * k / n;
                    if (f(t) < f(tb)) tb = t;
                }
                if (tb <= t0 || tb >= t1) continue;
                double h = (t1 - t0) / n;
                AstroEvent ev;
                ev.kind = EV_FLYBY;
                ev.t = MinimizeT(f, std::max(t0, tb - h), std::min(t1, tb + h));
                ev.probe = (int)i;
                ev.body = sg.body;
                ev.dist = f(ev.t);
                out.push_back(ev);
                continue;
            }
            // フライバイは区間で一番近い点、周回機は最初の近点（軌道投入。2 周目以降の方が近いことがある）
            size_t best = 0;
            for (size_t j = 0; j < sg.t.size(); j++) {
                if (orbiter && best > 0 && Len(sg.p[j]) > Len(sg.p[best])) break;
                if (Len(sg.p[j]) < Len(sg.p[best])) best = j;
            }
            if (best == 0) continue;  // 打ち上げ直後の地球からの出発
            if (&sg == &S.front() && sg.t[best] - sg.t.front() < 1.0) continue;   // 打ち上げ直後の待機軌道（まだ地球を離れていない）
            if (orbiter && best + 1 == sg.t.size()) continue;   // 到着の前にデータが終わる（JUICE）
            bool beforeArrival = false;   // 周回に入る少し前に影響圏をかすめただけの接近（ベピコロンボ）
            for (const ProbeSeg& o : S)
                if (&o != &sg && o.body == sg.body && o.t.front() > sg.t.back() && o.t.front() - sg.t.back() < 60 &&
                    o.t.back() - o.t.front() > 60)
                    beforeArrival = true;
            if (!orbiter && beforeArrival) continue;
            if (best + 1 == sg.t.size() && &sg == &S.back() && d.probeEnd) continue;  // 突入・衝突の直前（運用終了として扱う）
            auto f = [&](double t) { return Len(SegPos(sg, t)); };
            AstroEvent ev;
            ev.kind = orbiter ? EV_ARRIVE : EV_FLYBY;
            ev.t = MinimizeT(f, sg.t[best - 1], sg.t[std::min(best + 1, sg.t.size() - 1)]);
            ev.probe = (int)i;
            ev.body = sg.body;
            ev.dist = f(ev.t);
            out.push_back(ev);
        }
        if (d.probeEnd && !S.empty() && S.back().body > 0) {
            AstroEvent ev;
            ev.kind = EV_END;
            ev.t = S.back().t.back() - 10.0 / 1440;
            ev.probe = (int)i;
            ev.body = S.back().body;
            // 衝突で公転周期の変わった衛星があれば、その衛星への衝突（1 分前から見る）
            for (size_t k = 0; k < app.B.size(); k++)
                if (app.B[k].d.parent == ev.body && app.B[k].d.moonPeriod2 > 0) {
                    ev.kind = EV_IMPACT;
                    ev.body = (int)k;
                    ev.t = app.B[k].d.moonChangeT - 1.0 / 1440;
                }
            out.push_back(ev);
        }
    }
    std::sort(out.begin(), out.end(), [](const AstroEvent& a, const AstroEvent& b) { return a.t < b.t; });
    return out;
}

static std::string VoyageName(const App& app, int i, int leg);
static std::string EventTitle(const App& app, const AstroEvent& ev) {
    auto name = [&](int i) { return TR(app.B[i].d.jp, app.B[i].d.en); };
    char b[200];
    switch (ev.kind) {
        case EV_SOLAR_TOTAL: return T(UI_EV_SOLAR_TOTAL);
        case EV_ECLIPSE_GROUND:
            snprintf(b, sizeof b, T(UI_EV_GROUND), T(ev.body2 == EV_SOLAR_TOTAL ? UI_EV_SOLAR_TOTAL : UI_EV_SOLAR_ANNULAR));
            break;
        case EV_SOLAR_ANNULAR: return T(UI_EV_SOLAR_ANNULAR);
        case EV_SOLAR_PARTIAL: return T(UI_EV_SOLAR_PARTIAL);
        case EV_LUNAR_TOTAL: return T(UI_EV_LUNAR_TOTAL);
        case EV_LUNAR_PARTIAL: return T(UI_EV_LUNAR_PARTIAL);
        case EV_SHADOW: snprintf(b, sizeof b, T(UI_EV_SHADOW), name(ev.moon)); break;
        case EV_TRANSIT: snprintf(b, sizeof b, T(UI_EV_TRANSIT), name(ev.body)); break;
        case EV_CONJ: snprintf(b, sizeof b, T(UI_EV_CONJ), name(ev.body), name(ev.body2)); break;
        case EV_OPPOSITION: snprintf(b, sizeof b, T(UI_EV_OPPOSITION), name(ev.body)); break;
        case EV_PERIHELION: snprintf(b, sizeof b, T(UI_EV_PERIHELION), name(ev.body)); break;
        case EV_COMET_EARTH:
            if (!app.B[ev.body].d.comet) {   // 小惑星（アポフィス）: 宇宙から / 地上から
                snprintf(b, sizeof b, T(ev.moon == 1 ? UI_EV_ASTEROID_EARTH_GROUND : UI_EV_ASTEROID_EARTH), name(ev.body));
                break;
            }
            snprintf(b, sizeof b, T(UI_EV_COMET_EARTH), name(ev.body));
            break;
        case EV_FLYBY: snprintf(b, sizeof b, T(UI_EV_FLYBY), name(ev.probe), name(ev.body)); break;
        case EV_ARRIVE:
            snprintf(b, sizeof b, T(ev.body > B_PLUTO ? UI_EV_ARRIVE_SMALL : UI_EV_ARRIVE), name(ev.probe), name(ev.body));
            break;
        case EV_IMPACT: snprintf(b, sizeof b, T(UI_EV_IMPACT), name(ev.probe), name(ev.body)); break;
        case EV_METEOR: snprintf(b, sizeof b, T(UI_EV_METEOR), T(SHOWERS[ev.moon].name)); break;
        case EV_PERIASTRON: {   // 主星の名前の末尾の「 A」を除いて連星の名前に
            std::string n = name(ev.body);
            if (n.size() > 2 && n.compare(n.size() - 2, 2, " A") == 0) n.resize(n.size() - 2);
            snprintf(b, sizeof b, T(UI_EV_PERIASTRON), n.c_str());
            break;
        }
        case EV_VIEW: snprintf(b, sizeof b, T(UI_EV_VIEW), name(g_jp ? ev.body : ev.body2), name(g_jp ? ev.body2 : ev.body)); break;
        case EV_HISTORY: return TR(HISTORY[ev.moon].jp, HISTORY[ev.moon].en);
        case EV_VOYAGE: return VoyageName(app, ev.probe, ev.moon);
        default: snprintf(b, sizeof b, T(ev.body > B_PLUTO ? UI_EV_END_LAND : UI_EV_END), name(ev.probe), name(ev.body)); break;
    }
    return b;
}

static std::string VoyageTargetName(const App& app, int i, int leg, int body);
static std::string EventDetail(const App& app, const AstroEvent& ev) {
    char b[160];
    switch (ev.kind) {
        case EV_VOYAGE: {
            long long y;
            int m, d;
            CivilFromJdn((long long)std::floor(ev.dist + 2451545.5), &y, &m, &d);
            snprintf(b, sizeof b, T(UI_EV_VOYAGE_D), VoyageTargetName(app, ev.probe, ev.moon, ev.body).c_str(), (int)y);
            return b;
        }
        case EV_FLYBY:
        case EV_ARRIVE: snprintf(b, sizeof b, T(UI_EV_ALT), FmtDist(ev.dist - app.B[ev.body].d.radius).c_str()); return b;
        case EV_TRANSIT: snprintf(b, sizeof b, T(UI_EV_TRANSIT_D), ev.dist); return b;
        case EV_CONJ: snprintf(b, sizeof b, T(UI_EV_CONJ_D), ev.dist); return b;
        case EV_OPPOSITION:
        case EV_COMET_EARTH: snprintf(b, sizeof b, T(UI_EV_FROM_EARTH), FmtDist(ev.dist).c_str()); return b;
        case EV_PERIHELION: snprintf(b, sizeof b, T(UI_EV_FROM_SUN), FmtDist(ev.dist).c_str()); return b;
        case EV_IMPACT: return T(UI_EV_IMPACT_D);
        case EV_METEOR: snprintf(b, sizeof b, T(UI_EV_METEOR_D), (int)ev.dist); return b;
        case EV_PERIASTRON: snprintf(b, sizeof b, T(UI_EV_SEPARATION), FmtDist(ev.dist).c_str()); return b;
        case EV_HISTORY: {   // 食の種類と史実の場所（ハレー彗星は近日点の距離と記録の名前）
            if (ev.body2 == -4) return TR(HISTORY[ev.moon].placeJp, HISTORY[ev.moon].placeEn);
            if (ev.body2 == -3) {   // 衝突から何時間後か
                snprintf(b, sizeof b, "%.1f h / %s", (ev.t - SL9[ev.body].t) * 24, TR(HISTORY[ev.moon].placeJp, HISTORY[ev.moon].placeEn));
                return b;
            }
            if (ev.body2 == -2 || ev.body2 == EV_TRANSIT) {   // 超新星はその時の明るさ、太陽面通過は太陽の中心からの距離
                char d[64];
                if (ev.body2 == -2) snprintf(d, sizeof d, T(UI_INFO_MAG_UNIT), ev.dist);
                else snprintf(d, sizeof d, T(UI_EV_TRANSIT_D), ev.dist);
                snprintf(b, sizeof b, "%s / %s", d, TR(HISTORY[ev.moon].placeJp, HISTORY[ev.moon].placeEn));
                return b;
            }
            if (ev.body2 == EV_PERIHELION) {
                char d[64];
                snprintf(d, sizeof d, T(UI_EV_FROM_SUN), FmtDist(ev.dist).c_str());
                snprintf(b, sizeof b, "%s / %s", d, TR(HISTORY[ev.moon].placeJp, HISTORY[ev.moon].placeEn));
                return b;
            }
            UiId k = ev.body2 == EV_SOLAR_TOTAL ? UI_EV_SOLAR_TOTAL : ev.body2 == EV_SOLAR_ANNULAR ? UI_EV_SOLAR_ANNULAR
                   : ev.body2 == EV_SOLAR_PARTIAL ? UI_EV_SOLAR_PARTIAL : ev.body2 == EV_LUNAR_TOTAL ? UI_EV_LUNAR_TOTAL : UI_EV_LUNAR_PARTIAL;
            snprintf(b, sizeof b, "%s / %s", T(k), TR(HISTORY[ev.moon].placeJp, HISTORY[ev.moon].placeEn));
            return b;
        }
        default: break;
    }
    if (ev.kind > EV_SOLAR_PARTIAL && ev.kind != EV_ECLIPSE_GROUND) return "";
    char ll[96];
    if (g_jp)
        snprintf(ll, sizeof ll, "%s%.0f° %s%.0f°", ev.lat >= 0 ? "北緯" : "南緯", std::fabs(ev.lat), ev.lon >= 0 ? "東経" : "西経", std::fabs(ev.lon));
    else
        snprintf(ll, sizeof ll, "%.0f°%s %.0f°%s", std::fabs(ev.lat), ev.lat >= 0 ? "N" : "S", std::fabs(ev.lon), ev.lon >= 0 ? "E" : "W");
    snprintf(b, sizeof b, T(UI_EV_CENTER), ll);
    return b;
}

// 彗星の活動の強さ（0〜1）と尾の長さ km。太陽から 4 AU（cometScale 倍の彗星は √倍）より内側で現れ、
// 近いほど長い（見た目の演出で、長さは実際の尾と同じ桁）
static double CometActivity(const Body& b) {
    double r = Len(b.pos) / AU_KM, r0 = 4.0 * std::sqrt(b.d.cometScale);
    double act = std::max(0.0, std::min(1.0, (r0 - r) / (r0 - 1.0)));
    return act * act * (3 - 2 * act);
}
static double CometTailLength(const Body& b) {
    return 2.0e7 * b.d.cometScale * CometActivity(b) * std::min(2.5, AU_KM / Len(b.pos));
}

static double ArrivalDistance(const Body& b);

// ワープの瞬間に呼ぶ: イベントの時刻へ移り、よく見える位置に自機を置く
static void ApplyEvent(App& app, const AstroEvent& ev) {
    app.tDays = ev.t;
    UpdateBodies(app.B, app.tDays);
    const Body& sun = app.B[B_SUN];
    const Body& earth = app.B[B_EARTH];
    app.ship.speed = 0;
    if (ev.kind == EV_TRANSIT) {
        // 地球と惑星を結ぶ線の上から、ズームして太陽を見る。この線の上ならどこでも、太陽面のどこを惑星が通るかは
        // 地球から見た時と同じ。惑星の見かけの半径が太陽の 1/10 になる距離に置く（地球からは 1/30〜1/160）
        const Body& pl = app.B[ev.body];
        double D = pl.d.radius * Len(pl.pos - sun.pos) / (0.1 * sun.d.radius);
        app.ship.ref = B_SUN;
        app.ship.local = pl.pos + Norm(earth.pos - pl.pos) * D;
        app.ship.q = QuatFromBasis(sun.pos - app.ship.local, Y_UP);
        app.target = ev.body;
        app.eventZoom = true;
    } else if (ev.kind == EV_CONJ) {
        // 地球のすぐ外（地球が視界に入らないよう前方へ 4 半径）から、2 つの惑星の中間の方向を見る
        V3 m = Norm(Norm(app.B[ev.body].pos - earth.pos) + Norm(app.B[ev.body2].pos - earth.pos));
        app.ship.ref = B_EARTH;
        app.ship.local = m * (earth.d.radius * 4);
        app.ship.q = QuatFromBasis(m, Y_UP);
        app.target = ev.body;
        app.eventZoom = true;
    } else if (ev.kind == EV_OPPOSITION) {
        // 惑星の地球側（ワープの到着距離）から見る。衝の惑星は満月のように全面が照らされる
        const Body& pl = app.B[ev.body];
        V3 d = Norm(earth.pos - pl.pos);
        app.ship.ref = ev.body;
        app.ship.local = d * ArrivalDistance(pl);
        app.ship.q = QuatFromBasis(d * -1.0, pl.pole);
        app.target = ev.body;
    } else if (ev.kind == EV_PERIHELION) {
        // 軌道面の上方から尾を横に見る（ダストの尾が曲がるのが分かる）。尾は画面の横向きに、核は中央の少し左に
        const Body& cm = app.B[ev.body];
        V3 anti = Norm(cm.pos), vel = PlanetPos(cm.d, app.tDays + 0.01) - PlanetPos(cm.d, app.tDays);
        V3 n = Norm(Cross(cm.pos, vel));
        double len = std::max(CometTailLength(cm), 2e6);
        V3 aim = cm.pos + anti * (len * 0.3);
        app.ship.ref = B_SUN;
        app.ship.local = aim + n * (len * 0.8);
        app.ship.q = QuatFromBasis(n * -1.0, Cross(anti, n * -1.0) * -1.0);
        app.target = ev.body;
    } else if (ev.kind == EV_COMET_EARTH && !app.B[ev.body].d.comet && ev.moon == 1) {
        // 小惑星の地上から: 最接近の時に小惑星の真下になる地点（高度 300 m）から、真上の小惑星を見上げる
        const Body& a = app.B[ev.body];
        double lat, lon;
        LatLonOf(earth, Norm(a.pos - earth.pos), &lat, &lon);
        PlaceAt(app, B_EARTH, lat, lon, 0.3);
        V3 sw = ShipWorld(app), up = Norm(sw - earth.pos);
        V3 north = Norm(earth.pole - up * Dot(earth.pole, up));
        app.ship.q = QuatFromBasis(Norm(a.pos - sw), north);
        app.target = ev.body;
    } else if (ev.kind == EV_COMET_EARTH && !app.B[ev.body].d.comet) {
        // 小惑星の宇宙から: 小惑星のすぐ外側（地球と反対側の斜め横 1.5 km）から、小惑星越しに地球を見る
        const Body& a = app.B[ev.body];
        V3 out = Norm(a.pos - earth.pos), side = Norm(Cross(out, earth.pole));
        app.ship.ref = ev.body;
        app.ship.speed = 0;
        app.ship.local = (out * 0.8 + side * 0.6) * 1.5;   // 小惑星の影響圏の中（外だと地球の基準系に移って置いていかれる）
        app.ship.q = QuatFromBasis(Norm(earth.pos - (a.pos + app.ship.local)), earth.pole);
        app.target = ev.body;
    } else if (ev.kind == EV_COMET_EARTH) {
        // 地球のすぐ外から彗星を見る（地球から見た尾の向きと長さ）
        V3 d = Norm(app.B[ev.body].pos - earth.pos);
        app.ship.ref = B_EARTH;
        app.ship.local = d * (earth.d.radius * 4);
        app.ship.q = QuatFromBasis(d, Y_UP);
        app.target = ev.body;
    } else if (ev.kind == EV_ECLIPSE_GROUND) {
        // 食の中心の地上（高度 300 m）から、ズームして太陽を見上げる
        PlaceAt(app, B_EARTH, ev.lat, ev.lon, 0.3);
        V3 sw = ShipWorld(app);
        app.ship.q = QuatFromBasis(sun.pos - sw, Norm(sw - earth.pos));
        app.target = B_MOON;
        app.eventZoom = true;
    } else if (ev.kind == EV_IMPACT) {
        // 衝突の 1 分前: 衛星のすぐ横（探査機が来る向きから 70° ずらした 1.5 km 先）から衛星と母天体を見る
        const Body& m = app.B[ev.body];
        const Body& host = app.B[m.d.parent];
        V3 u = Norm(app.B[ev.probe].pos - m.pos);
        V3 side = Norm(Cross(u, host.pole));
        V3 dir = Norm(u * 0.35 + side * 0.94);
        app.ship.ref = ev.body;
        app.ship.local = dir * 1.5;
        V3 aim = m.pos + (host.pos - m.pos) * 0.3;
        app.ship.q = QuatFromBasis(aim - (m.pos + app.ship.local), host.pole);
        app.target = ev.body;
    } else if (ev.kind == EV_HISTORY && ev.body2 == EV_TRANSIT) {
        AstroEvent p = ev;   // 金星の太陽面通過: 太陽面通過の場面と同じく、地球と金星を結ぶ線の上からズームして見る
        p.kind = EV_TRANSIT;
        ApplyEvent(app, p);
        return;
    } else if (ev.kind == EV_HISTORY && ev.body2 == -4) {
        // 地上から惑星を望遠鏡のように見る（視野 0.5°。ガリレオの望遠鏡は 15′ ほど）。ガリレオのスケッチと同じく、
        // 黄道の北を上にして衛星の並びを横に（東が左、西が右）
        PlaceAt(app, B_EARTH, ev.lat, ev.lon, 0.3);
        V3 sw = ShipWorld(app);
        app.ship.q = QuatFromBasis(app.B[ev.body].pos - sw, Y_UP);
        app.target = ev.body;
        app.eventZoom = true;
        app.eventZoomFov = 0.5;
    } else if (ev.kind == EV_HISTORY && ev.body2 == -3) {
        // シューメーカー・レビー第 9 彗星: 破片の衝突痕の上空（木星の半径の 1.2 倍の高さ）から、衝突痕を見下ろす
        const Body& j = app.B[B_JUPITER];
        V3 dir = JupiterFixedToWorld(j, app.sl9Dirs[ev.body]);
        app.ship.ref = B_JUPITER;
        app.ship.local = dir * (j.d.radius * 2.2);
        app.ship.q = QuatFromBasis(dir * -1.0, j.pole);
        app.target = B_JUPITER;
    } else if (ev.kind == EV_HISTORY && ev.body2 == -2) {
        // 超新星: 記録した人がいた場所の地上から、超新星の方角の夜空を見上げる
        PlaceAt(app, B_EARTH, ev.lat, ev.lon, 0.3);
        V3 sw = ShipWorld(app), up = Norm(sw - earth.pos);
        app.ship.q = QuatFromBasis(EquToRender(SUPERNOVAE[ev.body].ra, SUPERNOVAE[ev.body].dec), up);
        app.target = B_MOON;
    } else if (ev.kind == EV_HISTORY && ev.body2 == EV_PERIHELION) {
        AstroEvent p = ev;   // ハレー彗星: 近日点の場面と同じく、軌道面の上方から尾を見る
        p.kind = EV_PERIHELION;
        ApplyEvent(app, p);
        return;
    } else if (ev.kind == EV_HISTORY) {
        // 史実の場所の地上（高度 300 m）から、日食は太陽を（ズームして）、月食は月を見上げる
        bool solar = ev.body2 <= EV_SOLAR_PARTIAL;
        PlaceAt(app, B_EARTH, ev.lat, ev.lon, 0.3);
        V3 sw = ShipWorld(app);
        app.ship.q = QuatFromBasis((solar ? sun.pos : app.B[B_MOON].pos) - sw, Norm(sw - earth.pos));
        app.target = B_MOON;
        app.eventZoom = solar;
    } else if (ev.kind == EV_METEOR) {
        // 東京の地上から、極大の前後 1 日で一番よく見える夜に、放射点の方角の空を見上げる
        const MeteorShower& ms = SHOWERS[ev.moon];
        app.tDays = BestMeteorTime(app.B, ms, ev.t, CITIES[0].lat, CITIES[0].lon);
        UpdateBodies(app.B, app.tDays);
        GoToCity(app, 0);
        V3 up = Norm(app.ship.local), rad = EquToRender(ms.ra, ms.dec);
        V3 h = rad - up * Dot(rad, up);
        if (Len(h) > 1e-6) app.ship.q = QuatFromBasis(Norm(h) * std::cos(50 * DEG) + up * std::sin(50 * DEG), up);
    } else if (ev.kind == EV_PERIASTRON) {
        // 2 つの星を結ぶ線に垂直な、軌道の外から（間隔の 3 倍の距離）、2 つの星の真ん中を見る
        const Body& a = app.B[ev.body];
        const Body& b = app.B[ev.body2];
        V3 sep = b.pos - a.pos, n = Cross(sep, Y_UP);
        n = Len(n) > 1e-6 ? Norm(n) : V3(1, 0, 0);
        V3 mid = sep * 0.5;
        app.ship.ref = ev.body;
        app.ship.local = mid + n * (Len(sep) * 3) + Y_UP * (Len(sep) * 0.6);
        app.ship.q = QuatFromBasis(mid - app.ship.local, Y_UP);
        app.target = ev.body;
    } else if (ev.kind == EV_VIEW) {
        // body2 と反対側（惑星は照らす恒星の側へ寄せる）の body の半径の 6 倍の所から、body と body2 の間の方を見る
        // （両方が画面に入る）
        const Body& a = app.B[ev.body];
        V3 away = Norm(a.pos - app.B[ev.body2].pos), side = Y_UP - away * Dot(Y_UP, away);
        side = Len(side) > 1e-6 ? Norm(side) : V3(1, 0, 0);
        int host = HostStar(app.B, ev.body);
        V3 toHost = host != ev.body ? Norm(app.B[host].pos - a.pos) : V3(0, 0, 0);
        V3 u = Norm(away + side * 0.35 + toHost * 0.7);
        app.ship.ref = ev.body;
        app.ship.local = u * (a.d.radius * 6);
        app.ship.q = QuatFromBasis(Norm(u * -1.0 - away), Y_UP);
        app.target = ev.body;
    } else if (ev.kind >= EV_FLYBY) {
        // 探査機の斜め後ろ（惑星と反対側）から惑星の方を見る。探査機は画面の横に、惑星は正面に見える。
        // 惑星の夜側を通る時は、昼側の縁（三日月形に光る部分）を正面にする
        const Body& pr = app.B[ev.probe];
        const Body& pl = app.B[ev.body];
        V3 out = Norm(pr.pos - pl.pos);
        V3 side = Cross(out, Y_UP);
        side = Len(side) > 1e-6 ? Norm(side) : V3(1, 0, 0);
        V3 up = Cross(side, out);
        V3 toSun = Norm(sun.pos - pl.pos), aim = pl.pos;
        if (Dot(out, toSun) < 0) aim = pl.pos + Norm(toSun - out * Dot(toSun, out)) * pl.d.radius;
        app.ship.ref = ev.probe;
        app.ship.speed = 0;
        app.ship.local = out * 0.035 - side * 0.022 + up * 0.006;
        app.ship.q = QuatFromBasis(aim - (pr.pos + app.ship.local), up);
        app.target = ev.probe;
        app.eventZoom = ev.body > B_PLUTO;   // 彗星・小惑星は小さいのでズームして見る（探査機はマーカーで示す）
    } else if (ev.kind <= EV_SOLAR_PARTIAL) {
        // 食の中心の上空 12,000 km から見下ろす
        PlaceAt(app, B_EARTH, ev.lat, ev.lon, 12000);
        app.target = B_EARTH;
    } else if (ev.kind <= EV_LUNAR_PARTIAL) {
        // 月を横から見る（地球の影の軸に直交する方向へ 4 半径）
        const Body& m = app.B[B_MOON];
        V3 u = Norm(m.pos - sun.pos);
        V3 side = Norm(Cross(u, m.pole));
        app.ship.ref = B_MOON;
        app.ship.speed = 0;
        app.ship.local = (side * 4.0 - u * 1.0) * m.d.radius;
        app.ship.q = QuatFromBasis(app.ship.local * -1.0, m.pole);
        app.target = B_MOON;
    } else {   // EV_SHADOW
        // 木星面に落ちた影の真上（太陽側）から見下ろす
        const Body& j = app.B[B_JUPITER];
        const Body& mo = app.B[ev.moon];
        V3 u = Norm(mo.pos - sun.pos);
        double along, perp;
        ShadowAxis(sun.pos, mo.pos, j.pos, &along, &perp);
        double R = j.d.radius, k = std::sqrt(std::max(0.0, R * R - perp * perp));
        V3 spot = mo.pos + u * (along - k) - j.pos;   // 影の中心（木星中心から）
        app.ship.ref = B_JUPITER;
        app.ship.speed = 0;
        app.ship.local = spot - u * (R * 0.9);
        app.ship.q = QuatFromBasis(u, j.pole);
        app.target = ev.moon;
    }
    UpdateReferenceFrame(app);
}

// ---------------------------------------------------------------- 日時の指定
// "YYYYMMDD" または "YYYYMMDDhhmm"（UTC。先頭に - で紀元前、天文学的な年）→ J2000 からの日数。不正なら NaN。
// 1582 年 10 月 15 日より前はユリウス暦
static double ParseDateDigits(const std::string& s0) {
    bool neg = !s0.empty() && s0[0] == '-';
    std::string s = neg ? s0.substr(1) : s0;
    if (s.size() != 8 && s.size() != 12) return NAN;
    long long y = std::stoi(s.substr(0, 4)) * (neg ? -1 : 1);
    int mo = std::stoi(s.substr(4, 2)), d = std::stoi(s.substr(6, 2));
    int h = s.size() == 12 ? std::stoi(s.substr(8, 2)) : 0, mi = s.size() == 12 ? std::stoi(s.substr(10, 2)) : 0;
    if (y < DATE_YEAR_MIN || y > DATE_YEAR_MAX || mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59) return NAN;
    long long jdn = JdnFromDate(y, mo, d);
    long long ry;
    int rm, rd;
    CivilFromJdn(jdn, &ry, &rm, &rd);
    if (ry != y || rm != mo || rd != d) return NAN;   // 2 月 30 日や、暦の切り替えで飛んだ 1582 年 10 月 5〜14 日など
    return (double)jdn - 2451545.0 - 0.5 + (h * 60 + mi) / 1440.0;
}

// 入力中の数字を "2026-10-28 14:05" の形に（未入力は _。紀元前は先頭に −）
static std::string FormatDateDigits(const std::string& dg0) {
    bool neg = !dg0.empty() && dg0[0] == '-';
    std::string dg = neg ? dg0.substr(1) : dg0;
    std::string out = "____-__-__ __:__";
    size_t k = 0;
    for (size_t i = 0; i < out.size() && k < dg.size(); i++)
        if (out[i] == '_') out[i] = dg[k++];
    return neg ? "-" + out : out;
}

static std::string DigitsOfTime(double tDays) {
    CivilTime c = CivilOf(tDays);
    char b[32];
    snprintf(b, sizeof b, "%s%04lld%02d%02d%02d%02d", c.y < 0 ? "-" : "", c.y < 0 ? -c.y : c.y, c.mo, c.d, c.h, c.mi);
    return b;
}

static void StartWarp(App& app, int ti, bool land);

// イベントへワープする時に向かう天体
static int EventBody(const AstroEvent& ev) {
    if (ev.kind == EV_IMPACT || ev.kind == EV_PERIASTRON || ev.kind == EV_VIEW) return ev.body;
    if (ev.kind >= EV_FLYBY) return ev.probe;
    if (ev.kind == EV_HISTORY)
        return ev.body2 == EV_PERIHELION ? ev.body : ev.body2 == EV_TRANSIT ? B_VENUS : ev.body2 == -3 ? B_JUPITER : B_EARTH;   // -4 は地上へ
    if (ev.kind == EV_ECLIPSE_GROUND) return B_EARTH;
    if (ev.kind >= EV_TRANSIT) return ev.body;
    return ev.kind <= EV_SOLAR_PARTIAL ? B_EARTH : ev.kind <= EV_LUNAR_PARTIAL ? B_MOON : B_JUPITER;
}

// 時刻 t の超新星 k の等級（見えない時は NaN）
static double SupernovaMag(int k, double t) {
    static double tp[SUPERNOVA_COUNT];
    static bool init = false;
    if (!init) {
        for (int i = 0; i < SUPERNOVA_COUNT; i++) tp[i] = ParseDateDigits(SUPERNOVAE[i].date);
        init = true;
    }
    const Supernova& s = SUPERNOVAE[k];
    double dt = t - tp[k];
    if (dt < -15 || dt > s.days) return NAN;
    return dt < 0 ? s.peak + (6 - s.peak) * (-dt / 15) : s.peak + (6 - s.peak) * (dt / s.days);
}

// 地球の緯度・経度 deg の地上の位置（太陽中心の描画座標。B は時刻 t に更新済み）
static V3 EarthSite(const std::vector<Body>& B, double lat, double lon) {
    const Body& e = B[B_EARTH];
    double cs = std::cos(e.spin), sn = std::sin(e.spin), la = lat * DEG, lo = lon * DEG;
    V3 X = e.ex * cs - e.ez * sn, Z = e.ex * sn + e.ez * cs;
    return e.pos + (X * (std::cos(la) * std::cos(lo)) + e.pole * std::sin(la) - Z * (std::cos(la) * std::sin(lo))) * e.d.radius;
}

// 歴史上の日食・月食（HISTORY）を、史実の日付の前後 2 日で探す
static std::vector<AstroEvent> FindHistoryEvents(const App& app) {
    std::vector<AstroEvent> out;
    std::vector<Body> B = app.B;
    for (int k = 0; k < (int)(sizeof HISTORY / sizeof HISTORY[0]); k++) {
        const HistoricEclipse& h = HISTORY[k];
        double t0 = ParseDateDigits(h.date);
        if (std::isnan(t0)) continue;
        if (h.kind == 3) {   // 超新星: 記録の日から 4 日のうち、太陽が地平線の 12° より下で超新星が一番高い時刻
            int sn = -1;
            for (int i = 0; i < SUPERNOVA_COUNT; i++)
                if (std::strcmp(SUPERNOVAE[i].date, h.date) == 0) sn = i;
            if (sn < 0) continue;
            V3 dir = EquToRender(SUPERNOVAE[sn].ra, SUPERNOVAE[sn].dec);
            double bestAlt = -1, bt = t0;
            for (double t = t0 - 0.5; t <= t0 + 3.5; t += 1.0 / 96) {
                UpdateBodies(B, t);
                V3 o = EarthSite(B, h.lat, h.lon), up = Norm(o - B[B_EARTH].pos);
                if (Dot(Norm(B[B_SUN].pos - o), up) > std::sin(-12 * DEG)) continue;
                double a = Dot(dir, up);
                if (a > bestAlt) { bestAlt = a; bt = t; }
            }
            AstroEvent ev;
            ev.kind = EV_HISTORY;
            ev.body2 = -2;   // 超新星
            ev.body = sn;
            ev.moon = k;
            ev.lat = h.lat;
            ev.lon = h.lon;
            ev.t = bt;
            ev.dist = SupernovaMag(sn, bt);
            out.push_back(ev);
            continue;
        }
        if (h.kind == 6) {   // 地上から木星を望遠鏡のように見る（時刻は日付のとおり）
            AstroEvent ev;
            ev.kind = EV_HISTORY;
            ev.body2 = -4;
            ev.body = B_JUPITER;
            ev.moon = k;
            ev.lat = h.lat;
            ev.lon = h.lon;
            ev.t = t0;
            out.push_back(ev);
            continue;
        }
        if (h.kind == 5) {   // シューメーカー・レビー第 9 彗星: 破片 G の衝突から 12 時間のうち、衝突痕が太陽に一番向く時刻
            int g = 0;
            for (int i = 0; i < SL9_COUNT; i++)
                if (std::strcmp(SL9[i].name, "G") == 0) g = i;
            V3 f = Sl9FixedDirs(B)[g];
            double best = -2, bt = SL9[g].t;
            for (double t = SL9[g].t + 0.05; t <= SL9[g].t + 0.5; t += 1.0 / 288) {
                UpdateBodies(B, t);
                double v = Dot(JupiterFixedToWorld(B[B_JUPITER], f), Norm(B[B_SUN].pos - B[B_JUPITER].pos));
                if (v > best) { best = v; bt = t; }
            }
            AstroEvent ev;
            ev.kind = EV_HISTORY;
            ev.body2 = -3;   // シューメーカー・レビー第 9 彗星
            ev.body = g;
            ev.moon = k;
            ev.t = bt;
            out.push_back(ev);
            continue;
        }
        if (h.kind == 4) {   // 金星の太陽面通過: 地球の中心から見て金星が太陽の中心に一番近づく時刻
            auto sep = [&](double t) {
                UpdateBodies(B, t);
                V3 e = B[B_EARTH].pos;
                return std::acos(std::max(-1.0, std::min(1.0, Dot(Norm(B[B_SUN].pos - e), Norm(B[B_VENUS].pos - e)))));
            };
            double best = 1e9, bt = t0;
            for (double t = t0 - 2; t <= t0 + 3; t += 0.02)
                if (sep(t) < best) { best = sep(t); bt = t; }
            bt = MinimizeT(sep, bt - 0.02, bt + 0.02);
            UpdateBodies(B, bt);
            double rs = std::asin(B[B_SUN].d.radius / Len(B[B_SUN].pos - B[B_EARTH].pos));
            if (sep(bt) > rs) continue;   // 太陽の円盤にかからなければ入れない
            AstroEvent ev;
            ev.kind = EV_HISTORY;
            ev.body2 = EV_TRANSIT;
            ev.body = B_VENUS;
            ev.moon = k;
            ev.t = bt;
            ev.dist = sep(bt) / rs;   // 太陽の中心からの距離（太陽の半径が 1）
            out.push_back(ev);
            continue;
        }
        if (h.kind == 2) {   // ハレー彗星の近日点通過
            int hb = -1;
            for (size_t i = 0; i < B.size(); i++)
                if (B[i].d.halley) hb = (int)i;
            if (hb < 0) continue;
            auto r = [&](double t) { return Len(PlanetPos(B[hb].d, t)); };
            double best = 1e300, bt = t0;
            for (double t = t0 - 80; t <= t0 + 80; t += 0.5)
                if (r(t) < best) { best = r(t); bt = t; }
            AstroEvent ev;
            ev.kind = EV_HISTORY;
            ev.body2 = EV_PERIHELION;
            ev.body = hb;
            ev.moon = k;
            ev.t = MinimizeT(r, bt - 0.5, bt + 0.5);
            ev.dist = r(ev.t);
            out.push_back(ev);
            continue;
        }
        bool solar = h.kind == 0;
        auto f = [&](double t) {
            EclipseGeom g = EclipseAt(B, t);
            return solar ? (g.newSide ? g.solarPerp : 1e12) : (g.newSide ? 1e12 : g.lunarPerp);
        };
        double best = 1e300, bt = t0;
        for (double t = t0 - 2; t <= t0 + 3; t += 0.05) {
            double v = f(t);
            if (v < best) { best = v; bt = t; }
        }
        bt = MinimizeT(f, bt - 0.05, bt + 0.05);
        AstroEvent ev;
        if (!(solar ? ClassifySolar(B, bt, &ev) : ClassifyLunar(B, bt, &ev))) continue;
        ev.body2 = ev.kind;
        ev.kind = EV_HISTORY;
        ev.moon = k;
        ev.lat = h.lat;
        ev.lon = h.lon;
        if (solar) {   // 史実の場所から見て太陽と月の中心が最も近づく時刻（地上の場面はこの時刻に移る）
            auto sep = [&](double t) {
                UpdateBodies(B, t);
                V3 o = EarthSite(B, h.lat, h.lon);
                return std::acos(std::max(-1.0, std::min(1.0, Dot(Norm(B[B_SUN].pos - o), Norm(B[B_MOON].pos - o)))));
            };
            ev.t = MinimizeT(sep, bt - 0.15, bt + 0.15);
            ev.dist = sep(ev.t) / DEG;   // 太陽と月の中心の角距離
        } else {
            ev.t = bt;
        }
        out.push_back(ev);
    }
    std::sort(out.begin(), out.end(), [](const AstroEvent& a, const AstroEvent& b) { return a.t < b.t; });
    return out;
}

// 一覧の中身をページに合わせて用意する。彗星・探査機・歴史のページでは、今の時刻に一番近いできごとを選ぶ
static void OpenEventsPage(App& app) {
    if (app.eventsPage == 0) {
        app.events = FindEvents(app, app.tDays);
        app.eventSel = 0;
        return;
    }
    app.events = app.eventsPage == 1 ? app.cometEvents : app.eventsPage == 3 ? app.historyEvents
                 : app.eventsPage == 4 ? app.voyageEvents : app.probeEvents;
    app.eventSel = 0;
    if (app.eventsPage == 4) return;
    for (size_t i = 0; i < app.events.size(); i++)
        if (std::fabs(app.events[i].t - app.tDays) < std::fabs(app.events[app.eventSel].t - app.tDays)) app.eventSel = (int)i;
}

// ---------------------------------------------------------------- 自動ツアー（U キー）
// イベント一覧の名場面を年代順に巡る。各場面は一覧（0: 天文, 1: 彗星・連星, 2: 探査機）の種類・天体名・日付で探す。
// 3 は一覧にない眺め（EV_VIEW: who の近くから body の方を見る）、4 は歴史上の日食・月食（who は HISTORY の英語名）
struct TourStop {
    int page, kind;
    const char* who;    // 探査機・彗星・連星の主星の英語名（天文イベントは nullptr）
    const char* body;   // 探査機の相手の天体の英語名（nullptr: 問わない）
    const char* date;   // "YYYYMMDD"（UTC、±2 日で探す）
    const char* jp;
    const char* en;     // 説明（\n で改行）
};
static const TourStop TOUR[] = {
    {4, EV_HISTORY, "Eclipse of Thales", nullptr, "-05840528",
     "ヘロドトスによれば、リュディアとメディアの戦いの最中に昼が夜になり、両軍は戦いをやめて和睦した。\nタレスが予言したと伝わる、日付が分かる最古の日食の一つ。夕暮れの空で太陽が欠ける",
     "According to Herodotus, day turned into night during a battle between the Lydians and the Medes, and both sides made peace.\nSaid to have been predicted by Thales, it is one of the oldest eclipses with a known date. The Sun is eclipsed low in the evening sky"},
    {4, EV_HISTORY, "Eddington's eclipse expedition", nullptr, "19190529",
     "エディントンらがアフリカ沖のプリンシペ島で皆既中の星の位置を測り、太陽の重力で光が曲がる量が\nアインシュタインの一般相対性理論の予言と合うことを確かめた",
     "On the island of Principe, Eddington's team measured the positions of stars during totality and found that the Sun's gravity\nbends starlight by the amount predicted by Einstein's general theory of relativity"},
    {2, EV_FLYBY, "Pioneer 10", "Jupiter", "19731204",
     "人類で初めて木星に接近した探査機。小惑星帯を越えて飛べることを確かめ、\n木星を取り巻く強い放射線帯を測った",
     "The first spacecraft to fly past Jupiter. It proved that a probe could cross the asteroid belt\nand measured Jupiter's intense radiation belts"},
    {2, EV_FLYBY, "Voyager 1", "Jupiter", "19790305",
     "イオの活火山と木星の細い環を発見した。このあと土星を経て、\n2012 年に人工物で初めて太陽圏の外へ出た",
     "It discovered Io's active volcanoes and Jupiter's faint ring. After Saturn it went on to become,\nin 2012, the first human-made object to leave the heliosphere"},
    {1, EV_PERIHELION, "Halley", nullptr, "19860209",
     "76 年ごとに戻ってくる周期彗星。1986 年の回帰では、\nジオットなど 5 機の探査機が核に迫った",
     "The periodic comet that returns every 76 years. During its 1986 return,\nfive spacecraft including Giotto flew past its nucleus"},
    {2, EV_FLYBY, "Voyager 2", "Neptune", "19890825",
     "海王星を訪れた唯一の探査機。北極の上空 約 5,000 km をかすめ、\n大暗斑やトリトンの間欠泉を撮影した",
     "The only spacecraft to visit Neptune. It skimmed about 5,000 km above the north pole\nand imaged the Great Dark Spot and the geysers of Triton"},
    {1, EV_COMET_EARTH, "Hyakutake", nullptr, "19960325",
     "地球から 0.1 AU（約 1,500 万 km）を通り、空に長い尾を引いた大彗星。\n日本のアマチュア天文家、百武裕司さんが発見した",
     "A great comet that passed 0.1 AU (15 million km) from Earth with a long tail across the sky.\nDiscovered by the Japanese amateur astronomer Yuji Hyakutake"},
    {1, EV_PERIHELION, "Hale-Bopp", nullptr, "19970401",
     "核の直径が約 60 km もある巨大な彗星。1 年半近く肉眼で見え、\n20 世紀で最も多くの人に見られた彗星と言われる",
     "A giant comet with a nucleus about 60 km across. Visible to the naked eye for a record 18 months,\nit was perhaps the most widely seen comet of the 20th century"},
    {2, EV_ARRIVE, "Cassini", "Saturn", "20040701",
     "環の隙間をくぐって土星を回る軌道に入った。13 年間の観測で、\nエンケラドゥスの地下の海から噴き出す水を見つけた",
     "It passed through a gap in the rings into orbit around Saturn. Over 13 years\nit found water jets erupting from the subsurface ocean of Enceladus"},
    {2, EV_ARRIVE, "Rosetta", "Churyumov-Gerasimenko", "20140806",
     "彗星を初めて周回した探査機。11 月には着陸機フィラエを\n核の表面に降ろした",
     "The first spacecraft to orbit a comet. In November it sent\nthe lander Philae down to the surface of the nucleus"},
    {2, EV_FLYBY, "New Horizons", "Pluto", "20150714",
     "冥王星を初めて間近に見た探査機。ハート形に広がる窒素の氷の平原\n「スプートニク平原」を撮影した",
     "The first close look at Pluto. It revealed the heart-shaped\nnitrogen-ice plain, Sputnik Planitia"},
    {2, EV_ARRIVE, "Juno", "Jupiter", "20160705",
     "北極と南極の上を通る軌道から木星の雲のすぐ上をかすめ、\n極に並ぶ巨大な渦の群れや、木星の内部を調べている",
     "From a polar orbit it skims just above Jupiter's clouds, studying the clusters\nof giant cyclones at the poles and the planet's interior"},
    {2, EV_END, "Cassini", "Saturn", "20170915",
     "燃料が尽きる前に、生命がいるかもしれない衛星を汚さないよう、\n土星の大気へ自ら突入して運用を終えた",
     "Before its fuel ran out, it plunged into Saturn's atmosphere\nso that it could never contaminate moons that might harbour life"},
    {2, EV_ARRIVE, "Hayabusa2", "Ryugu", "20180627",
     "直径 約 900 m のそろばん玉形の小惑星に到着。2 回の着地で岩石を採り、\n2020 年 12 月に地球へ届けた",
     "It reached this 900 m spinning-top-shaped asteroid, collected samples in two touchdowns\nand delivered them to Earth in December 2020"},
    {2, EV_ARRIVE, "OSIRIS-REx", "Bennu", "20181203",
     "炭素に富む小惑星ベンヌに到着。2020 年に表面の砂を採り、\n2023 年 9 月に地球へ届けた（いまは小惑星アポフィスへ向かう OSIRIS-APEX）",
     "It arrived at the carbon-rich asteroid Bennu, collected surface material in 2020\nand delivered it to Earth in September 2023 (now OSIRIS-APEX, bound for Apophis)"},
    {2, EV_IMPACT, "DART", "Dimorphos", "20220926",
     "小惑星の軌道を変えられるかを確かめる、人類初の惑星防衛の実験。秒速 6 km で衝突し、\nディモルフォスの公転周期を 33 分縮めた",
     "The first planetary-defense test: could a spacecraft change an asteroid's orbit? Hitting at 6 km/s,\nit shortened Dimorphos's orbital period by 33 minutes"},
    {3, EV_VIEW, "Sirius B", "Sirius A", "20261101",
     "夜空で一番明るい恒星シリウスの伴星。太陽ほどの質量が地球ほどの大きさに詰まった白色矮星で、\nスプーン 1 杯が数トンになる。20 AU 先に主星のシリウス A が輝く",
     "The companion of Sirius, the brightest star in the night sky: a white dwarf packing about the Sun's mass\ninto the size of the Earth, where a spoonful weighs tonnes. Sirius A shines 20 AU away"},
    {3, EV_VIEW, "Proxima Centauri b", "Alpha Centauri A", "20261102",
     "太陽に一番近い恒星を回る、地球ほどの大きさの惑星。赤い恒星のすぐ近くを 11 日で回り、\n水が液体でいられる範囲にある。空には 13,000 AU 先のアルファ星 A・B が明るく光る",
     "An Earth-sized planet around the nearest star to the Sun. It circles the red dwarf in 11 days,\nwithin the zone where water could be liquid. Alpha Centauri A and B shine brightly 13,000 AU away"},
    {2, EV_ARRIVE, "BepiColombo", "Mercury", "20261121",
     "日本と欧州の水星探査機。6 回の水星フライバイで減速し、水星を回る軌道に入る。\nこのあと日本の「みお」と欧州の MPO に分かれ、磁場と表面を調べる",
     "The joint Japanese-European Mercury mission. Slowed by six Mercury flybys, it enters orbit.\nIt then splits into Japan's Mio and Europe's MPO to study the magnetic field and the surface"},
    {0, EV_ECLIPSE_GROUND, nullptr, nullptr, "20270802",
     "北アフリカから中東を通る皆既日食を、食の中心の地上から。皆既は最長 6 分 23 秒続き、\n今世紀でも有数の長さ。太陽の周りにコロナが広がり、空が暗くなって明るい星が見える",
     "The total eclipse crossing North Africa and the Middle East, seen from the ground at its center. Totality lasts\nup to 6 min 23 s, one of the longest this century: the corona appears and the sky darkens enough to show bright stars"},
    {2, EV_ARRIVE, "Europa Clipper", "Jupiter", "20300411",
     "氷の下に海があるとみられるエウロパを調べる探査機。木星を回りながら\nエウロパに 49 回近づき、生命が住める環境かどうかを探る",
     "Bound for Europa, whose ice shell is thought to hide an ocean. From orbit around Jupiter\nit will fly past Europa 49 times to find out whether it could support life"},
    {0, EV_TRANSIT, nullptr, nullptr, "20321113",
     "水星が太陽の前を横切る太陽面通過。地球から見えるのは 100 年に 13 回ほどで、\n次は 2039 年",
     "Mercury crosses the face of the Sun. Seen from Earth this happens about 13 times a century;\nthe next one is in 2039"},
    {1, EV_PERIASTRON, "Alpha Centauri A", nullptr, "20350430",
     "ケンタウルス座アルファ星 A と B が、80 年に一度の近星点で 11 AU まで近づく。\n太陽系なら土星の軌道ほどの間隔で、2 つの太陽が互いを回る",
     "Alpha Centauri A and B come within 11 AU of each other at periastron, once every 80 years.\nThe two suns orbit each other about as far apart as Saturn is from our Sun"},
    {1, EV_PERIHELION, "Halley", nullptr, "20610728",
     "ハレー彗星の次の近日点。今度は地球から見やすい位置関係になり、\n1986 年より明るく見えると予想されている",
     "Halley's next perihelion. This time Earth is better placed,\nand the comet is expected to look brighter than in 1986"},
};
static const double TOUR_STOP_SEC = 15;

// ツアーの場面をイベント一覧から探す（見つからない場面は飛ばす）
static void BuildTour(App& app) {
    app.tourEvents.clear();
    app.tourStops.clear();
    for (int i = 0; i < (int)(sizeof TOUR / sizeof TOUR[0]); i++) {
        const TourStop& st = TOUR[i];
        double t = ParseDateDigits(st.date);
        if (st.page == 3) {
            AstroEvent ev;
            ev.kind = st.kind;
            ev.t = t;
            for (size_t k = 0; k < app.B.size(); k++) {
                if (std::strcmp(app.B[k].d.en, st.who) == 0) ev.body = (int)k;
                if (std::strcmp(app.B[k].d.en, st.body) == 0) ev.body2 = (int)k;
            }
            if (ev.body >= 0 && ev.body2 >= 0) {
                app.tourEvents.push_back(ev);
                app.tourStops.push_back(i);
            }
            continue;
        }
        if (st.page == 4) {
            for (const AstroEvent& ev : app.historyEvents)
                if (std::fabs(ev.t - t) <= 2 && std::strcmp(HISTORY[ev.moon].en, st.who) == 0) {
                    app.tourEvents.push_back(ev);
                    app.tourStops.push_back(i);
                    break;
                }
            continue;
        }
        std::vector<AstroEvent> list = st.page == 0 ? FindEvents(app, t - 3) : st.page == 1 ? app.cometEvents : app.probeEvents;
        for (const AstroEvent& ev : list) {
            if (ev.kind != st.kind || std::fabs(ev.t - t) > 2) continue;
            int who = ev.probe >= 0 ? ev.probe : ev.body;
            if (st.who && (who < 0 || std::strcmp(app.B[who].d.en, st.who) != 0)) continue;
            if (st.body && ev.probe >= 0 && std::strcmp(app.B[ev.body].d.en, st.body) != 0) continue;
            app.tourEvents.push_back(ev);
            app.tourStops.push_back(i);
            break;
        }
    }
}

static void GoTourStop(App& app, int i) {
    if (i < 0 || i >= (int)app.tourEvents.size() || app.warpT >= 0) return;
    app.tourIdx = i;
    app.tourTime = 0;
    app.paused = false;
    app.timeIdx = 0;
    const AstroEvent& ev = app.tourEvents[i];
    StartWarp(app, EventBody(ev), false);
    app.events.assign(1, ev);   // ワープの瞬間に ApplyEvent で使う
    app.warpEvent = 0;
}

static void StartTour(App& app) {
    BuildTour(app);
    if (app.tourEvents.empty()) {
        Toast(app, T(UI_TOUR_NONE));
        return;
    }
    app.tourOn = true;
    app.autopilot = false;
    GoTourStop(app, 0);
}

static void StopTour(App& app) {
    app.tourOn = false;
    Toast(app, T(UI_TOUR_END));
}

// 場面に着いてから TOUR_STOP_SEC 秒たったら次へ。最後の場面のあとは終了
static void UpdateTour(App& app, double dt) {
    if (!app.tourOn || app.warpT >= 0) return;
    app.tourTime += dt;
    if (app.tourTime < TOUR_STOP_SEC) return;
    if (app.tourIdx + 1 < (int)app.tourEvents.size()) GoTourStop(app, app.tourIdx + 1);
    else StopTour(app);
}

static void DrawTourPanel(const App& app) {
    if (!app.tourOn || app.tourIdx >= (int)app.tourEvents.size()) return;
    const Cam& c = app.cam;
    const AstroEvent& ev = app.tourEvents[app.tourIdx];
    const TourStop& st = TOUR[app.tourStops[app.tourIdx]];
    std::string desc = TR(st.jp, st.en);
    std::vector<std::string> lines;
    for (size_t a = 0; a <= desc.size();) {
        size_t b = std::min(desc.find('\n', a), desc.size());
        lines.push_back(desc.substr(a, b - a));
        a = b + 1;
    }
    // 右下の速度計に重ならないよう、左寄せで置く
    float w = std::min(c.W - 300, 900.0f), h = 84 + lines.size() * 22.0f, x = 12, y = c.H - 96 - h;
    DrawRectangleRounded({x, y, w, h}, 0.08f, 6, {0, 8, 20, 210});
    DrawRectangleRoundedLinesEx({x, y, w, h}, 0.08f, 6, 1.5f, {90, 130, 180, 255});
    char head[96];
    snprintf(head, sizeof head, T(UI_TOUR), app.tourIdx + 1, (int)app.tourEvents.size());
    Txt((std::string(head) + "    " + FmtMinute(ev.t) + " UTC").c_str(), x + 18, y + 10, 15, {170, 200, 230, 255});
    Txt(EventTitle(app, ev).c_str(), x + 18, y + 30, 21, {255, 220, 120, 255});
    for (size_t i = 0; i < lines.size(); i++) Txt(lines[i].c_str(), x + 18, y + 60 + i * 22.0f, 16, {215, 228, 245, 255});
    Txt(T(UI_TOUR_KEYS), x + w - 18 - TxtW(T(UI_TOUR_KEYS), 13), y + 12, 13, {150, 170, 190, 255});
    if (app.warpT < 0) {   // 次の場面までの残り
        float k = (float)std::min(1.0, app.tourTime / TOUR_STOP_SEC);
        DrawRectangle((int)(x + 18), (int)(y + h - 10), (int)((w - 36) * k), 3, {255, 220, 120, 200});
    }
}

static void SetPhysics(App& app, bool on);
static V3 CircularVelocity(const App& app);
