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
    auto pos = [&](int i, double t) { return SimPlanetPos(B, i, t); };
    for (int p : {B_MERCURY, B_VENUS}) {
        auto f = [&](double t) {
            V3 e = pos(B_EARTH, t), q = pos(p, t) - e;
            return Len(q) < Len(e) && Dot(q, e) < 0 ? AngleBetween(q, e * -1.0) : 1e9;
        };
        int found = 0;
        for (double tm : FindMinima(f, t0, t0 + 150 * 365.25, 1.0, 0.02)) {
            V3 e = pos(B_EARTH, tm), q = pos(p, tm) - e;
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
    const double span = 3 * 365.25;
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
    std::sort(out.begin(), out.end(), [](const AstroEvent& a, const AstroEvent& b) { return a.t < b.t; });
    return out;
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
            bool orbiter = sg.t.back() - sg.t.front() > 200;
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
            // フライバイは区間で一番近い点、周回機は最初の近点（軌道投入。2 周目以降の方が近いことがある）
            size_t best = 0;
            for (size_t j = 0; j < sg.t.size(); j++) {
                if (orbiter && best > 0 && Len(sg.p[j]) > Len(sg.p[best])) break;
                if (Len(sg.p[j]) < Len(sg.p[best])) best = j;
            }
            if (best == 0) continue;  // 打ち上げ直後の地球からの出発
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
        case EV_COMET_EARTH: snprintf(b, sizeof b, T(UI_EV_COMET_EARTH), name(ev.body)); break;
        case EV_FLYBY: snprintf(b, sizeof b, T(UI_EV_FLYBY), name(ev.probe), name(ev.body)); break;
        case EV_ARRIVE:
            snprintf(b, sizeof b, T(ev.body > B_PLUTO ? UI_EV_ARRIVE_SMALL : UI_EV_ARRIVE), name(ev.probe), name(ev.body));
            break;
        case EV_IMPACT: snprintf(b, sizeof b, T(UI_EV_IMPACT), name(ev.probe), name(ev.body)); break;
        default: snprintf(b, sizeof b, T(ev.body > B_PLUTO ? UI_EV_END_LAND : UI_EV_END), name(ev.probe), name(ev.body)); break;
    }
    return b;
}

static std::string EventDetail(const App& app, const AstroEvent& ev) {
    char b[160];
    switch (ev.kind) {
        case EV_FLYBY:
        case EV_ARRIVE: snprintf(b, sizeof b, T(UI_EV_ALT), FmtDist(ev.dist - app.B[ev.body].d.radius).c_str()); return b;
        case EV_TRANSIT: snprintf(b, sizeof b, T(UI_EV_TRANSIT_D), ev.dist); return b;
        case EV_CONJ: snprintf(b, sizeof b, T(UI_EV_CONJ_D), ev.dist); return b;
        case EV_OPPOSITION:
        case EV_COMET_EARTH: snprintf(b, sizeof b, T(UI_EV_FROM_EARTH), FmtDist(ev.dist).c_str()); return b;
        case EV_PERIHELION: snprintf(b, sizeof b, T(UI_EV_FROM_SUN), FmtDist(ev.dist).c_str()); return b;
        case EV_IMPACT: return T(UI_EV_IMPACT_D);
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
// 年月日 → 1970-01-01 からの日数（グレゴリオ暦）
static long long DaysFromCivil(int y, int m, int d) {
    y -= m <= 2;
    long long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long long)doe - 719468;
}

// "YYYYMMDD" または "YYYYMMDDhhmm"（UTC）→ J2000 からの日数。不正なら NaN
static double ParseDateDigits(const std::string& s) {
    if (s.size() != 8 && s.size() != 12) return NAN;
    int y = std::stoi(s.substr(0, 4)), mo = std::stoi(s.substr(4, 2)), d = std::stoi(s.substr(6, 2));
    int h = s.size() == 12 ? std::stoi(s.substr(8, 2)) : 0, mi = s.size() == 12 ? std::stoi(s.substr(10, 2)) : 0;
    static const int MD[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (y < 1800 || y > 2200 || mo < 1 || mo > 12 || d < 1 || d > MD[mo - 1] || h > 23 || mi > 59) return NAN;
    bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    if (mo == 2 && d == 29 && !leap) return NAN;
    double unixSec = (double)DaysFromCivil(y, mo, d) * 86400.0 + h * 3600.0 + mi * 60.0;
    return (unixSec - J2000_UNIX) / 86400.0;
}

// 入力中の数字を "2026-10-28 14:05" の形に（未入力は _）
static std::string FormatDateDigits(const std::string& dg) {
    const char* pat = "____-__-__ __:__";
    std::string out = pat;
    size_t k = 0;
    for (size_t i = 0; i < out.size() && k < dg.size(); i++)
        if (out[i] == '_') out[i] = dg[k++];
    return out;
}

static std::string DigitsOfTime(double tDays) {
    std::string f = FmtDate(tDays);   // "YYYY-MM-DD hh:mm:ss UTC"
    std::string d;
    for (size_t i = 0; i < 16 && i < f.size(); i++)
        if (f[i] >= '0' && f[i] <= '9') d += f[i];
    return d;
}

static void StartWarp(App& app, int ti, bool land);

// イベントへワープする時に向かう天体
static int EventBody(const AstroEvent& ev) {
    if (ev.kind == EV_IMPACT) return ev.body;
    if (ev.kind >= EV_FLYBY) return ev.probe;
    if (ev.kind == EV_ECLIPSE_GROUND) return B_EARTH;
    if (ev.kind >= EV_TRANSIT) return ev.body;
    return ev.kind <= EV_SOLAR_PARTIAL ? B_EARTH : ev.kind <= EV_LUNAR_PARTIAL ? B_MOON : B_JUPITER;
}

// 一覧の中身をページに合わせて用意する。彗星・探査機のページでは、今の時刻に一番近いできごとを選ぶ
static void OpenEventsPage(App& app) {
    if (app.eventsPage == 0) {
        app.events = FindEvents(app, app.tDays);
        app.eventSel = 0;
        return;
    }
    app.events = app.eventsPage == 1 ? app.cometEvents : app.probeEvents;
    app.eventSel = 0;
    for (size_t i = 0; i < app.events.size(); i++)
        if (std::fabs(app.events[i].t - app.tDays) < std::fabs(app.events[app.eventSel].t - app.tDays)) app.eventSel = (int)i;
}

// ---------------------------------------------------------------- 自動ツアー（U キー）
// イベント一覧の名場面を年代順に巡る。各場面は一覧（0: 天文, 1: 彗星, 2: 探査機）の種類・天体名・日付で探す
struct TourStop {
    int page, kind;
    const char* who;    // 探査機か彗星の英語名（天文イベントは nullptr）
    const char* body;   // 探査機の相手の天体の英語名（nullptr: 問わない）
    const char* date;   // "YYYYMMDD"（UTC、±2 日で探す）
    const char* jp;
    const char* en;     // 説明（\n で改行）
};
static const TourStop TOUR[] = {
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
    {0, EV_ECLIPSE_GROUND, nullptr, nullptr, "20270802",
     "北アフリカから中東を通る皆既日食を、食の中心の地上から。皆既は最長 6 分 23 秒続き、\n今世紀でも有数の長さ。太陽の周りにコロナが広がり、空が暗くなって明るい星が見える",
     "The total eclipse crossing North Africa and the Middle East, seen from the ground at its center. Totality lasts\nup to 6 min 23 s, one of the longest this century: the corona appears and the sky darkens enough to show bright stars"},
    {0, EV_TRANSIT, nullptr, nullptr, "20321113",
     "水星が太陽の前を横切る太陽面通過。地球から見えるのは 100 年に 13 回ほどで、\n次は 2039 年",
     "Mercury crosses the face of the Sun. Seen from Earth this happens about 13 times a century;\nthe next one is in 2039"},
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
    Txt((std::string(head) + "    " + FmtDate(ev.t).substr(0, 16) + " UTC").c_str(), x + 18, y + 10, 15, {170, 200, 230, 255});
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
