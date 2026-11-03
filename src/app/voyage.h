// Solar System Voyager — Copyright (c) 2026 hubstacksea
// MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
// 探査機の旅: 地球を出発してから目標に着くまでを探査機のそばから追う。何もない間は時間を速く進め、
// 出発・地球の重力圏の脱出・スイングバイ・目的外の天体への接近・到着では等倍まで落として知らせを出す。
// 行程は軌跡のデータ（assets/probes.bin）から作る。URL の ?voyage=名前&scene=場面 でその場面から始まる。
// main.cpp の一部で、main.cpp が決まった順に読み込む（単体ではコンパイルしない）

// 時間の進み方: できごとの前後 VOY_HOLD_SEC 秒は等倍、そこから離れるほど指数的に速くする（時定数 VOY_RAMP_SEC 秒）
static const double VOY_HOLD_SEC = 8.0, VOY_RAMP_SEC = 1.5;
static const double VOY_CARD_LEAD_SEC = 3000.0;      // できごとの知らせを出す範囲（前後のシミュレータの秒。実時間で 10 秒ほど）
// 巡航の速さの目安: 全行程をこの実時間 [秒] で進む速さ（ゆっくり・標準・速い。[ ] キーで切り替え）
static const double VOY_CRUISE_WALL_SEC[3] = {600.0, 240.0, 60.0};
static const double VOY_SCENE_LEAD_DAYS = 1.0;       // 場面を指定した時は、これだけ前から始める

// 終点を軌跡から決められない探査機（目標の天体がシミュレータにないか、到着の前にデータが終わる）
struct VoyageGoal {
    const char* probe;
    const char* date;       // "YYYYMMDDhhmm"（nullptr: データの終わり、"": 目標の天体への最接近）
    const char* body;       // 目標の天体の英語名（nullptr: なし）
    bool escape;            // 地球の重力圏の脱出を行程に入れるか
    const char* targetJp;
    const char* targetEn;
    const char* titleJp;
    const char* titleEn;
    const char* textJp;
    const char* textEn;
};
static const VoyageGoal VOYAGE_GOALS[] = {
    {"Parker Solar Probe", "202412241153", "Sun", true, "太陽", "the Sun", "太陽に最も近づいた", "Closest approach to the Sun",
     "7 回の金星スイングバイで軌道を縮め、太陽の表面から 約 610 万 km まで近づいた。\n人工物で最も太陽に近く、最も速い", 
     "Seven Venus flybys shrank its orbit until it came within about 6.1 million km of the Sun's surface,\nthe closest and fastest any human-made object has been"},
    {"James Webb Space Telescope", "202201241900", nullptr, false, "L2 点", "L2", "L2 点の周回軌道に到着",
     "Arrived in orbit around L2", "地球から 約 150 万 km 先、太陽と反対側の L2 点を回る軌道に入った。\nここで主鏡と観測装置を極低温に保って観測する",
     "It entered orbit around L2, about 1.5 million km from Earth on the side away from the Sun,\nwhere its mirror and instruments stay cryogenically cold"},
    {"Lucy", "", "Patroclus", true, "パトロクロス", "Patroclus", "トロヤ群の連星パトロクロスに到達",
     "Reached the Trojan binary Patroclus", "木星と同じ軌道を回るトロヤ群小惑星を巡る旅の最後の目標。パトロクロスはほぼ同じ大きさの\nメノイティオスと互いを回る連星（このシミュレータでは主星だけ。形は球で代えている）",
     "The last stop of its tour of Jupiter's Trojan asteroids: Patroclus, which orbits its near-twin Menoetius\n(only the primary is shown here, as a sphere)"},
    {"Psyche", nullptr, nullptr, true, "小惑星プシケ", "asteroid Psyche", "小惑星プシケへ向かう途中",
     "On the way to asteroid Psyche", "金属に富む小惑星プシケには 2029 年に着く予定。軌跡のデータはその手前までなので、ここで旅を終える",
     "It is due to reach the metal-rich asteroid Psyche in 2029. The trajectory data ends before that, so the voyage ends here"},
    {"JUICE", nullptr, "Jupiter", true, "木星", "Jupiter", "木星に到着する直前", "Just before reaching Jupiter",
     "2031 年 7 月に木星を回る軌道に入る予定。軌跡のデータはその手前までなので、ここで旅を終える",
     "It is due to enter orbit around Jupiter in July 2031. The trajectory data ends just before that, so the voyage ends here"},
};
static const VoyageGoal* FindVoyageGoal(const BodyDef& d) {
    for (const VoyageGoal& g : VOYAGE_GOALS)
        if (std::strcmp(g.probe, d.en) == 0) return &g;
    return nullptr;
}

// 延長ミッションの旅（2 番目の区間）: 地球スイングバイ（試料のカプセルの分離）から次の目標まで
struct VoyageLeg {
    const char* probe;
    const char* start;      // 出発にする地球への最接近の日付 "YYYYMMDD"（±3 日で探す）
    const char* body;       // 目標の天体の英語名
    const char* nameJp;
    const char* nameEn;
    const char* startTitleJp;
    const char* startTitleEn;
    const char* startTextJp;
    const char* startTextEn;
    const char* endTitleJp;
    const char* endTitleEn;
    const char* endTextJp;
    const char* endTextEn;
};
static const VoyageLeg VOYAGE_LEGS[] = {
    {"Hayabusa2", "20201205", "Torifune", "はやぶさ2（延長ミッション）", "Hayabusa2 (extended mission)",
     "カプセルを届けて延長ミッションへ", "Capsule delivered; on to the extended mission",
     "リュウグウの試料の入ったカプセルを分離してオーストラリアに届け、本体は地球をかすめて次の旅へ",
     "It released the capsule with the Ryugu samples, which landed in Australia, and swung past Earth on to its next voyage",
     "小惑星トリフネの近くを通過", "Flyby of asteroid Torifune",
     "延長ミッションで最初に近づく小惑星。このあと 2031 年に小惑星 1998 KY26 に着く予定",
     "The first asteroid of the extended mission; it is due to reach asteroid 1998 KY26 in 2031"},
    {"OSIRIS-REx", "20230924", "Apophis", "OSIRIS-APEX（延長ミッション）", "OSIRIS-APEX (extended mission)",
     "カプセルを届けて延長ミッションへ", "Capsule delivered; on to the extended mission",
     "ベンヌの試料の入ったカプセルを分離してユタ州に届け、本体は OSIRIS-APEX と名前を変えてアポフィスへ",
     "It released the capsule with the Bennu samples, which landed in Utah, and set off for Apophis as OSIRIS-APEX",
     "アポフィスに到着", "Arrived at Apophis",
     "2029 年 4 月 13 日に地球の中心から 3.8 万 km を通ったばかりのアポフィスに着き、\n地球の重力で表面や自転がどう変わったかを調べる",
     "It reaches Apophis just after its 13 April 2029 pass 38,000 km from Earth's center,\nto study how Earth's gravity changed its surface and spin"},
};
// 到着の場面の説明（イベント一覧の名場面に説明の無い探査機）
static const struct { const char* probe; const char* jp; const char* en; } VOYAGE_ARRIVAL_TEXT[] = {
    {"Hera", "DART が 2022 年に衝突したディモルフォスを調べに来た ESA の探査機。衝突でできたクレーターの大きさと深さ、\n衝突で形が変わったかを、2 機の小型衛星（ミラニとユベンタス）とともに調べる",
     "ESA's probe to study Dimorphos after DART's 2022 impact: the size and depth of the crater and whether the impact\nreshaped it, together with two CubeSats (Milani and Juventas)"},
};

// 探査機 d の区間 leg（1 が最初の旅、2 以降が延長ミッション）
static const VoyageLeg* FindVoyageLeg(const BodyDef& d, int leg) {
    int n = 1;
    for (const VoyageLeg& l : VOYAGE_LEGS)
        if (std::strcmp(l.probe, d.en) == 0 && ++n == leg) return &l;
    return nullptr;
}

// 地球から出発する軌跡を持つ探査機か（地球を回る人工衛星は除く）
static bool VoyageCapable(const App& app, int i) {
    const BodyDef& d = app.B[i].d;
    if (d.kind != KIND_PROBE || d.tleN > 0 || d.probe < 0 || d.probe >= (int)g_probes.size()) return false;
    const std::vector<ProbeSeg>& S = g_probes[d.probe].segs;
    return !S.empty() && S.front().body == B_EARTH;
}

// 区間の出発の時刻（最初の旅は軌跡の始まり、延長ミッションは地球への最接近の 6 時間前）。見つからなければ NaN
static double VoyageStart(const App& app, int i, int leg) {
    const std::vector<ProbeSeg>& S = g_probes[app.B[i].d.probe].segs;
    if (leg <= 1) return S.front().t.front();
    const VoyageLeg* l = FindVoyageLeg(app.B[i].d, leg);
    if (!l) return NAN;
    double day = ParseDateDigits(l->start);
    for (const AstroEvent& ev : app.probeEvents)
        if (ev.probe == i && ev.kind == EV_FLYBY && ev.body == B_EARTH && std::fabs(ev.t - day) < 3) return ev.t - 0.25;
    return NAN;
}

// 旅の終点: 時刻・目標の天体・できごとの種類（EV_ARRIVE / EV_FLYBY / EV_IMPACT。到着しないものは EV_END）
static bool VoyageEnd(const App& app, int i, int leg, double* t, int* body, int* kind) {
    const std::vector<ProbeSeg>& S = g_probes[app.B[i].d.probe].segs;
    double dataEnd = S.back().t.back();
    *body = -1;
    *kind = EV_END;
    if (leg > 1) {   // 延長ミッション: 出発より後の、目標の天体への到着か最接近
        const VoyageLeg* l = FindVoyageLeg(app.B[i].d, leg);
        double t0 = VoyageStart(app, i, leg);
        if (!l || !std::isfinite(t0)) return false;
        for (const AstroEvent& ev : app.probeEvents)
            if (ev.probe == i && ev.t > t0 && (ev.kind == EV_ARRIVE || ev.kind == EV_FLYBY) &&
                std::strcmp(app.B[ev.body].d.en, l->body) == 0) {
                *t = ev.t;
                *body = ev.body;
                *kind = ev.kind;
                return true;
            }
        return false;
    }
    if (const VoyageGoal* g = FindVoyageGoal(app.B[i].d)) {
        *t = g->date && g->date[0] ? ParseDateDigits(g->date) : dataEnd - 1.0 / 86400;
        if (g->body)
            for (size_t k = 0; k < app.B.size(); k++)
                if (std::strcmp(app.B[k].d.en, g->body) == 0) *body = (int)k;
        if (g->date && !g->date[0])
            for (const AstroEvent& ev : app.probeEvents)
                if (ev.probe == i && ev.body == *body && ev.kind == EV_FLYBY) {
                    *t = ev.t;
                    *kind = EV_FLYBY;
                }
        return true;
    }
    const AstroEvent* last = nullptr;
    for (const AstroEvent& ev : app.probeEvents) {
        if (ev.probe != i) continue;
        if (ev.kind == EV_ARRIVE || ev.kind == EV_IMPACT) { last = &ev; break; }
        if (ev.kind == EV_FLYBY) last = &ev;
    }
    if (!last) return false;
    *t = last->t;
    *body = last->body;
    *kind = last->kind;
    if (last->kind == EV_IMPACT) *t = dataEnd - 0.5 / 86400;   // 衝突の直前（データの最後の点が衝突の瞬間）
    return true;
}

// 行程を作る。巡航中は 1 日ごと、惑星の近くでは 30 分ごとに調べる
static void BuildVoyage(const App& app, int i, int leg, std::vector<VoyageStep>* steps, double* length) {
    steps->clear();
    *length = 0;
    const BodyDef& d = app.B[i].d;
    const std::vector<ProbeSeg>& S = g_probes[d.probe].segs;
    double t0 = VoyageStart(app, i, leg), tEnd;
    int endBody, endKind;
    if (!std::isfinite(t0) || !VoyageEnd(app, i, leg, &tEnd, &endBody, &endKind)) return;
    const VoyageGoal* goal = leg <= 1 ? FindVoyageGoal(d) : nullptr;
    std::vector<Body> B = app.B;
    for (Body& b : B) b.orbit.clear();   // 軌道の線は引かないので計算を省く
    auto distTo = [&](int k, double t) {
        UpdateBodies(B, t);
        return Len(B[i].pos - B[k].pos);
    };
    auto helioSpeed = [&](double t) {   // km/s
        const double h = 0.02;
        UpdateBodies(B, t - h);
        V3 a = B[i].pos - B[B_SUN].pos;
        UpdateBodies(B, t + h);
        return Len(B[i].pos - B[B_SUN].pos - a) / (2 * h * 86400);
    };

    VoyageStep launch;
    launch.kind = VS_LAUNCH;
    launch.t = t0;
    launch.body = B_EARTH;
    steps->push_back(launch);

    // スイングバイ・到着（探査機のできごとの一覧から）。速さの変化は惑星の影響圏の出入りで比べる
    int swingbys = 0;
    for (const AstroEvent& ev : app.probeEvents) {
        if (ev.probe != i || ev.kind != EV_FLYBY || ev.t >= tEnd - 1e-6 || ev.t <= t0 + 0.5) continue;
        VoyageStep st;
        if (ev.body > B_PLUTO) {   // 小惑星: 実際の軌跡での距離（スイングバイではない）
            st.kind = VS_FLYBY;
            st.t = ev.t;
            st.body = ev.body;
            st.dist = ev.dist;
            steps->push_back(st);
            continue;
        }
        st.kind = VS_SWINGBY;
        st.t = ev.t;
        st.body = ev.body;
        st.dist = ev.dist;
        for (const ProbeSeg& sg : S)
            if (sg.body == ev.body && sg.t.front() <= ev.t && ev.t <= sg.t.back()) {
                st.vIn = helioSpeed(sg.t.front() + 0.05);
                st.vOut = helioSpeed(sg.t.back() - 0.05);
            }
        steps->push_back(st);
        swingbys++;
    }

    // 目標の小惑星そのものが途中で地球に最接近する（アポフィス）
    for (const AstroEvent& ev : app.cometEvents)
        if (ev.kind == EV_COMET_EARTH && ev.moon != 1 && ev.body == endBody && ev.t > t0 && ev.t < tEnd) {
            VoyageStep st;
            st.kind = VS_TARGET_EARTH;
            st.t = ev.t;
            st.body = ev.body;
            st.dist = ev.dist;
            st.vIn = distTo(ev.body, ev.t);   // 探査機からの距離
            steps->push_back(st);
        }

    // 地球の重力圏の脱出・目的外の天体への接近・飛行距離: 軌跡をたどって調べる
    bool wantEscape = !goal || goal->escape, escaped = false;
    struct Near { int body; double r, d0, d1, t0, t1; };
    std::vector<Near> cand;
    for (size_t k = 1; k < B.size(); k++) {
        const BodyDef& o = B[k].d;
        if ((int)k <= B_PLUTO && o.parent == 0) continue;   // 惑星は上のスイングバイで
        if ((int)k == endBody || o.kind == KIND_PROBE || o.kind == KIND_STAR) continue;
        bool listed = false;   // 軌跡で最接近を持つ天体は上で入れた
        for (const AstroEvent& ev : app.probeEvents)
            if (ev.probe == i && ev.body == (int)k) listed = true;
        if (listed) continue;
        if (HostStar(B, (int)k) != B_SUN) continue;
        cand.push_back({(int)k, std::max(o.radius * 100.0, o.parent > 0 ? 0.0 : 200000.0), 1e300, 1e300, t0, t0});
    }
    V3 prevPos;
    bool havePrev = false;
    double t = t0;
    while (t < tEnd) {
        UpdateBodies(B, t);
        int segBody = 0;
        for (const ProbeSeg& sg : S)
            if (sg.t.front() <= t && t <= sg.t.back()) segBody = sg.body;
        V3 pos = B[i].pos;
        if (havePrev) *length += Len(pos - B[B_SUN].pos - prevPos);
        prevPos = pos - B[B_SUN].pos;
        havePrev = true;
        double dEarth = Len(pos - B[B_EARTH].pos);
        if (wantEscape && !escaped && dEarth > B[B_EARTH].soi) {
            escaped = true;
            double a = std::max(t0, t - 1.0), b = t;
            for (int it = 0; it < 30; it++) {
                double m = (a + b) / 2;
                (distTo(B_EARTH, m) > B[B_EARTH].soi ? b : a) = m;
            }
            VoyageStep st;
            st.kind = VS_ESCAPE;
            st.t = b;
            st.body = B_EARTH;
            st.dist = B[B_EARTH].soi;
            st.vOut = helioSpeed(b);
            steps->push_back(st);
            UpdateBodies(B, t);
        }
        for (Near& n : cand) {
            int par = B[n.body].d.parent;
            if (par > 0 && par != segBody) {   // 母天体の近くにいない間の衛星は調べない
                n.d0 = n.d1 = 1e300;
                continue;
            }
            double dk = Len(pos - B[n.body].pos);
            if (n.d1 < n.d0 && n.d1 <= dk && n.d1 < std::max(n.r * 5, 5e6)) {   // 極小を挟んだので細かく探す
                double a = n.t0, b = t;
                double tm = MinimizeT([&](double x) { return distTo(n.body, x); }, a, b), dm = distTo(n.body, tm);
                if (dm < n.r && tm < tEnd - 0.1) {
                    VoyageStep st;
                    st.kind = VS_NEAR;
                    st.t = tm;
                    st.body = n.body;
                    st.dist = dm;
                    steps->push_back(st);
                }
                UpdateBodies(B, t);
            }
            n.t0 = n.t1;
            n.t1 = t;
            n.d0 = n.d1;
            n.d1 = dk;
        }
        t += segBody > 0 ? 1.0 / 48 : 1.0;
    }

    VoyageStep end;
    end.kind = VS_ARRIVE;
    end.t = tEnd;
    end.body = endBody;
    end.count = swingbys;
    end.evKind = endKind;
    if (endBody >= 0) end.dist = distTo(endBody, tEnd);
    steps->push_back(end);
    std::stable_sort(steps->begin(), steps->end(), [](const VoyageStep& a, const VoyageStep& b) { return a.t < b.t; });
}

// 右上の小さな全体図のために、全行程の太陽中心の位置を等間隔の時刻で求めておく
static void SampleVoyageTrack(App& app) {
    const int N = 1500;
    std::vector<Body> B = app.B;
    for (Body& b : B) b.orbit.clear();
    double t0 = app.voySteps.front().t, t1 = app.voySteps.back().t;
    app.voyTrack.clear();
    app.voyTrackT.clear();
    app.voyStepPos.clear();
    for (int k = 0; k <= N; k++) {
        double t = t0 + (t1 - t0) * k / N;
        UpdateBodies(B, t);
        app.voyTrack.push_back(B[app.voyProbe].pos - B[B_SUN].pos);
        app.voyTrackT.push_back(t);
    }
    for (const VoyageStep& st : app.voySteps) {
        UpdateBodies(B, st.t);
        app.voyStepPos.push_back(B[app.voyProbe].pos - B[B_SUN].pos);
    }
}

// イベント一覧の「探査機の旅」のページ。ev.t は出発、ev.body は目標、ev.dist は到着の時刻
// ev.moon は区間（1 が最初の旅、2 以降が延長ミッション）
static std::vector<AstroEvent> FindVoyages(const App& app) {
    std::vector<AstroEvent> out;
    for (size_t i = 0; i < app.B.size(); i++) {
        if (!VoyageCapable(app, (int)i)) continue;
        for (int leg = 1; leg == 1 || FindVoyageLeg(app.B[i].d, leg); leg++) {
            AstroEvent ev;
            int kind;
            ev.t = VoyageStart(app, (int)i, leg);
            if (!std::isfinite(ev.t) || !VoyageEnd(app, (int)i, leg, &ev.dist, &ev.body, &kind)) continue;
            ev.kind = EV_VOYAGE;
            ev.probe = (int)i;
            ev.moon = leg;
            out.push_back(ev);
        }
    }
    std::sort(out.begin(), out.end(), [](const AstroEvent& a, const AstroEvent& b) { return a.t < b.t; });
    return out;
}

static std::string VoyageTargetName(const App& app, int i, int leg, int body) {
    if (const VoyageGoal* g = leg <= 1 ? FindVoyageGoal(app.B[i].d) : nullptr) return TR(g->targetJp, g->targetEn);
    return body >= 0 ? TR(app.B[body].d.jp, app.B[body].d.en) : "";
}

// 探査機のそばのカメラの向きと位置: 探査機を画面の右上に入れて（左上と下は表示で埋まる）、見たい方向 dir を正面にする
static void PlaceVoyageCamera(App& app, V3 dir) {
    Ship& s = app.ship;
    s.ref = app.voyProbe;
    s.speed = 0;
    s.thrusting = false;
    if (app.voyLookYaw != 0 || app.voyLookPitch != 0) {   // 見回している分だけ向きを回す（探査機は画面の同じ所に残る）
        V3 right = Cross(dir, Y_UP);
        right = Len(right) > 1e-6 ? Norm(right) : V3(1, 0, 0);
        V3 up = Cross(right, dir);
        double y = app.voyLookYaw, p = app.voyLookPitch;
        dir = Norm((dir * std::cos(y) + right * std::sin(y)) * std::cos(p) + up * std::sin(p));
    }
    s.q = QuatFromBasis(dir, Y_UP);
    s.local = ShipFwd(s) * -0.032 - ShipRight(s) * 0.009 - ShipUp(s) * 0.005;
}

// 今見たい方向: 知らせを出しているできごとの天体、なければ次のできごとの天体、それもなければ進む向き
static V3 VoyageAimDir(const App& app, int* bodyOut = nullptr) {
    const Body& p = app.B[app.voyProbe];
    int body = -1;
    if (app.voyCard >= 0) body = app.voySteps[app.voyCard].body;
    if (body < 0)
        for (const VoyageStep& st : app.voySteps)
            if (st.t > app.tDays && st.body >= 0) { body = st.body; break; }
    if (bodyOut) *bodyOut = body;
    if (body >= 0) {
        const Body& o = app.B[body];
        V3 d = o.pos - p.pos;
        double r = Len(d);
        // 近くで夜側を通る時は、昼側の縁（三日月形に光る部分）の方を見る。夜側の深さと近さに応じて中心から縁へ
        // 連続的に寄せる（切り替えで画面が飛ばないように）。遠い時や恒星は中心を
        if (body != B_SUN && r > 1e-6 && r < o.d.radius * 8) {
            V3 out = Norm(d * -1.0), toSun = Norm(app.B[B_SUN].pos - o.pos);
            double facing = Dot(out, toSun);
            V3 side = toSun - out * facing;
            if (facing < 0 && Len(side) > 1e-9) {
                auto smooth = [](double x) { x = std::max(0.0, std::min(1.0, x)); return x * x * (3 - 2 * x); };
                double w = smooth(-facing / 0.3) * smooth((o.d.radius * 8 - r) / (o.d.radius * 2));
                d = o.pos + Norm(side) * (o.d.radius * w) - p.pos;
            }
        }
        // 衛星: 時間を速めている間は母天体のまわりを何周も回り、追うとカメラが左右に振れる。画面の上で
        // 1 周が短く（10 秒より短い）、振れの見かけの大きさ（衛星の軌道の見かけの半径）も見て分かる間は
        // 母天体の方を見て、時間が遅くなるにつれて衛星へ寄せる
        if (o.d.parent > 0 && o.d.moonA > 0 && o.d.moonPeriod > 0 && r > 1e-6) {
            auto ramp = [](double x, double lo, double hi) {
                x = std::max(0.0, std::min(1.0, (x - lo) / (hi - lo)));
                return x * x * (3 - 2 * x);
            };
            const Body& host = app.B[o.d.parent];
            double perSec = app.voyRate / (o.d.moonPeriod * 86400);              // 画面の 1 秒あたりの周回数
            double swing = std::atan(std::min(1.0, o.d.moonA / r)) / DEG;        // 振れの見かけの大きさ deg
            double w = 1 - ramp(perSec, 0.02, 0.1) * ramp(swing, 0.02, 0.08);
            d = host.pos + (p.pos + d - host.pos) * w - p.pos;
        }
        if (Len(d) > 1e-6) return Norm(d);
    }
    return Len(p.pos - app.B[B_SUN].pos) > 1e-6 ? Norm(app.voyAim) : V3(1, 0, 0);   // 目標のない巡航は今の向きのまま
}

static void PrepareVoyageLocal(App& app, int k);
// 時刻 t に応じた時間の速さと、出す知らせを決める
static void UpdateVoyageRate(App& app) {
    const double day = 86400.0;
    double rate = app.voyCruise;
    int card = -1;
    for (size_t k = 0; k < app.voySteps.size(); k++) {
        double delta = (app.tDays - app.voySteps[k].t) * day;
        rate = std::min(rate, std::max(1.0, (std::fabs(delta) - VOY_HOLD_SEC) / VOY_RAMP_SEC));
        if (std::fabs(delta) < VOY_CARD_LEAD_SEC) card = (int)k;
    }
    if (app.voyDone) rate = 1;
    // 拡大図の軌跡は、場面に近づいた時に一度だけ求める（巡航の速さで 1 日以内）
    for (size_t k = 0; k < app.voySteps.size(); k++)
        if (!app.voyLocal[k].ready && std::fabs(app.tDays - app.voySteps[k].t) < 3.0) PrepareVoyageLocal(app, (int)k);
    if (card < 0 && app.voyDone) card = (int)app.voySteps.size() - 1;
    app.voyRate = rate;
    app.voyCard = card;
}

static void StopVoyage(App& app) {
    if (!app.voyageOn) return;
    app.voyageOn = false;
    app.cockpit = app.voySavedCockpit;
    app.timeIdx = 0;
    app.paused = false;
    app.eventZoom = false;
    if (app.voySavedPhysics) SetPhysics(app, true);
    UpdateReferenceFrame(app);
    Toast(app, T(UI_VOY_STOPPED));
}

// 時刻 t へ移す（場面の頭出し）
static void VoyageJump(App& app, double t) {
    app.tDays = t;
    app.voyDone = false;
    UpdateBodies(app.B, app.tDays);
    UpdateVoyageRate(app);
    app.voyAim = VoyageAimDir(app, &app.voyAimBody);
    app.voyAimPrevWant = app.voyAim;
    PlaceVoyageCamera(app, app.voyAim);
}

// 巡航の速さ（0: ゆっくり, 1: 標準, 2: 速い）
static void VoyageSetSpeed(App& app, int speed) {
    app.voySpeed = std::max(0, std::min(2, speed));
    if (app.voySteps.size() < 2) return;
    double days = app.voySteps.back().t - app.voySteps.front().t;
    app.voyCruise = std::max(0.2, std::min(80.0, days / VOY_CRUISE_WALL_SEC[app.voySpeed])) * 86400.0;
}

// 旅の名前（延長ミッションはその名前）
static std::string VoyageName(const App& app, int i, int leg) {
    if (const VoyageLeg* l = leg > 1 ? FindVoyageLeg(app.B[i].d, leg) : nullptr) return TR(l->nameJp, l->nameEn);
    return TR(app.B[i].d.jp, app.B[i].d.en);
}

// 探査機 i の区間 leg の旅を始める。scene: 場面の番号（0 が出発、負なら末尾から）。t が有効ならその時刻から
static bool StartVoyage(App& app, int i, int leg, int scene, double t) {
    if (!VoyageCapable(app, i)) return false;
    std::vector<VoyageStep> steps;
    double length;
    BuildVoyage(app, i, leg, &steps, &length);
    if (steps.size() < 2) return false;
    if (app.tourOn) app.tourOn = false;
    app.autopilot = false;
    app.warpT = -1;
    app.voySavedPhysics = app.physics;
    if (app.physics) SetPhysics(app, false);
    app.voySavedCockpit = app.voyageOn ? app.voySavedCockpit : app.cockpit;
    if (app.mouseCaptured) {   // 旅の間は下のタイムラインを押せるよう、マウスを捕まえない（見回すのは左ドラッグ）
        app.mouseCaptured = false;
        EnableCursor();
    }
    app.voyageOn = true;
    app.voyProbe = i;
    app.voyLeg = leg;
    app.voySteps = steps;
    app.voyLength = length;
    app.voyLocal.assign(steps.size(), App::VoyLocal());
    app.voyLookYaw = app.voyLookPitch = app.voyLookHold = 0;
    app.cockpit = true;       // 自機は描かない
    app.paused = false;
    app.timeIdx = 0;
    app.eventZoom = false;
    app.frameLock = false;
    app.target = i;
    SampleVoyageTrack(app);
    double t0 = steps.front().t, t1 = steps.back().t;
    VoyageSetSpeed(app, app.voySpeed);
    int n = (int)steps.size();
    if (scene < 0) scene = n + scene;
    scene = std::max(0, std::min(n - 1, scene));
    double start = std::isfinite(t) && t >= t0 && t <= t1 ? t : scene == 0 ? t0 : std::max(t0, steps[scene].t - VOY_SCENE_LEAD_DAYS);
    // 前のできごとを跨がないよう、直前のできごとの少しあとより前には戻らない
    if (!std::isfinite(t)) {
        if (scene > 0) start = std::max(start, steps[scene - 1].t + VOY_CARD_LEAD_SEC / 86400.0);
        start = std::min(start, steps[scene].t);
    }
    VoyageJump(app, start);
    return true;
}

// URL の voyage= と scene= から始める（起動時に一度）。scene: launch / arrive / 場面の番号（1 から）
static void StartPendingVoyage(App& app) {
    if (app.pendingVoyage.empty()) return;
    auto lower = [](std::string v) {
        for (char& ch : v) ch = (char)std::tolower((unsigned char)ch);
        return v;
    };
    int who = -1;
    for (size_t k = 0; k < app.B.size(); k++)
        if (VoyageCapable(app, (int)k) && lower(app.pendingVoyage) == lower(app.B[k].d.en)) who = (int)k;
    int scene = 0;
    const std::string& sc = app.pendingScene;
    if (sc == "arrive" || sc == "end") scene = -1;
    else if (!sc.empty() && sc != "launch") scene = std::max(1, atoi(sc.c_str())) - 1;
    if (who < 0 || !StartVoyage(app, who, std::max(1, app.pendingLeg), scene, app.pendingVoyageT)) Toast(app, T(UI_VOY_NOT_FOUND));
    app.pendingVoyage.clear();
}

// 場面 k へ移る（その場面の 1 日前から。前の場面の知らせが終わった後から）
static void VoyageGoScene(App& app, int k) {
    const std::vector<VoyageStep>& S = app.voySteps;
    if (k < 0 || k >= (int)S.size()) return;
    double t = k == 0 ? S[0].t : std::max(S[0].t, S[k].t - VOY_SCENE_LEAD_DAYS);
    if (k > 0) t = std::min(S[k].t, std::max(t, S[k - 1].t + VOY_CARD_LEAD_SEC / 86400.0));
    VoyageJump(app, t);
}

// 画面下のタイムライン（出発から到着までを時刻で等分した棒）の位置
static Rectangle VoyageTimelineRect(const Cam& c) {
    return {24, c.H - 46, c.W - 48, 10};
}
static float VoyageTimelineX(const App& app, Rectangle r, double t) {
    double t0 = app.voySteps.front().t, t1 = app.voySteps.back().t;
    return r.x + r.width * (float)std::max(0.0, std::min(1.0, (t - t0) / std::max(1e-9, t1 - t0)));
}
// p（2D の表示の座標）にある場面の番号。点から 9 ピクセル以内で一番近いもの。無ければ -1
static int VoyageTimelineScene(const App& app, Rectangle r, Vector2 p) {
    int best = -1;
    float bd = 9;
    for (size_t k = 0; k < app.voySteps.size(); k++) {
        float d = std::fabs(p.x - VoyageTimelineX(app, r, app.voySteps[k].t));
        if (d < bd && std::fabs(p.y - (r.y + r.height / 2)) < 14) { bd = d; best = (int)k; }
    }
    return best;
}
// タイムラインを押した: 場面の点ならその場面へ、ほかの所ならその時刻へ。押したなら true
static bool VoyageTimelineClick(App& app, Vector2 p) {
    Rectangle r = VoyageTimelineRect(UiCam(app.cam));
    if (!CheckCollisionPointRec(p, {r.x - 10, r.y - 12, r.width + 20, r.height + 24})) return false;
    int k = VoyageTimelineScene(app, r, p);
    if (k >= 0) VoyageGoScene(app, k);
    else {
        double t0 = app.voySteps.front().t, t1 = app.voySteps.back().t;
        VoyageJump(app, t0 + (t1 - t0) * std::max(0.0f, std::min(1.0f, (p.x - r.x) / r.width)));
    }
    return true;
}

static void VoyageCardText(const App& app, const VoyageStep& st, std::string* title, std::string* text);
static void DrawVoyageTimeline(const App& app) {
    const Cam& c = app.cam;
    Rectangle r = VoyageTimelineRect(c);
    float cy = r.y + r.height / 2, now = VoyageTimelineX(app, r, app.tDays);
    DrawRectangleRounded({r.x - 8, r.y - 6, r.width + 16, r.height + 12}, 0.5f, 6, {0, 8, 20, 190});
    DrawLineEx({r.x, cy}, {r.x + r.width, cy}, 2.0f, {90, 110, 140, 220});
    DrawLineEx({r.x, cy}, {now, cy}, 3.0f, {255, 196, 120, 230});
    for (size_t k = 0; k < app.voySteps.size(); k++) {
        float x = VoyageTimelineX(app, r, app.voySteps[k].t);
        bool done = app.voySteps[k].t <= app.tDays;
        Color col = (int)k == app.voyCard ? Color{255, 220, 80, 255} : done ? Color{230, 235, 245, 230} : Color{130, 140, 160, 230};
        DrawCircleV({x, cy}, (int)k == app.voyCard ? 5.0f : 3.5f, col);
    }
    DrawCircleV({now, cy}, 4.0f, {120, 255, 170, 255});
    DrawCircleLinesV({now, cy}, 7.0f, {120, 255, 170, 200});
    // マウスを乗せた場面の見出し
    Vector2 m = Vector2Scale(GetMousePosition(), 1 / UiScale(c));
    int k = app.mouseCaptured || app.touchUI ? -1 : VoyageTimelineScene(app, r, m);
    if (k >= 0) {
        std::string title, text;
        VoyageCardText(app, app.voySteps[k], &title, &text);
        std::string s = FmtDay(app.voySteps[k].t) + "  " + title;
        float w = TxtW(s.c_str(), 14) + 16, x = std::max(4.0f, std::min(c.W - w - 4, VoyageTimelineX(app, r, app.voySteps[k].t) - w / 2));
        DrawRectangleRounded({x, r.y - 34, w, 22}, 0.3f, 6, {0, 8, 20, 235});
        Txt(s.c_str(), x + 8, r.y - 31, 14, {255, 230, 170, 255});
    }
}

// 次・前の場面へ
static void VoyageSkip(App& app, int dir) {
    int n = (int)app.voySteps.size();
    if (dir > 0) {
        for (int k = 0; k < n; k++)
            if (app.voySteps[k].t > app.tDays + VOY_HOLD_SEC / 86400.0) {
                // ふつうは 1 日前へ。もう 1 日を切っていたら、等倍になる少し前（実時間で数秒）まで進める
                double t = app.voySteps[k].t - VOY_SCENE_LEAD_DAYS;
                if (t <= app.tDays) t = app.voySteps[k].t - 2 * VOY_HOLD_SEC / 86400.0;
                VoyageJump(app, std::max(app.tDays, t));
                return;
            }
        return;
    }
    for (int k = n - 1; k >= 0; k--)
        if (app.voySteps[k].t < app.tDays - 30.0 / 86400.0) {
            VoyageJump(app, k == 0 ? app.voySteps[0].t : std::max(app.voySteps[0].t, app.voySteps[k].t - VOY_SCENE_LEAD_DAYS));
            return;
        }
}

// 毎フレーム: 時間の速さを決め、カメラを探査機に付ける
static void UpdateVoyage(App& app, double dt) {
    if (!app.voyageOn) return;
    // 見回した向きは、操作をやめて 3 秒たつと滑らかに自動の向きへ戻る
    if (app.voyLookHold > 0) app.voyLookHold -= dt;
    else {
        double k = std::exp(-dt * 2.0);
        app.voyLookYaw *= k;
        app.voyLookPitch *= k;
        if (std::fabs(app.voyLookYaw) < 1e-4 && std::fabs(app.voyLookPitch) < 1e-4) app.voyLookYaw = app.voyLookPitch = 0;
    }
    double tEnd = app.voySteps.back().t;
    if (!app.voyDone && app.tDays >= tEnd) {
        // データの終わりで探査機が消える時は、その場で時間を止める。飛び続ける時は少し見せてから完了に
        std::vector<Body> B = app.B;
        for (Body& b : B) b.orbit.clear();
        UpdateBodies(B, tEnd + VOY_HOLD_SEC / 86400.0);
        bool ends = !B[app.voyProbe].active;
        if (ends || app.tDays >= tEnd + VOY_HOLD_SEC / 86400.0) {
            app.voyDone = true;
            if (ends) {
                app.paused = true;
                app.tDays = tEnd;
                UpdateBodies(app.B, app.tDays);
            }
        }
    }
    UpdateVoyageRate(app);
    int aimBody;
    V3 want = VoyageAimDir(app, &aimBody);
    // カメラの向き: 同じ天体を追っている間は、その天体の見かけの動きの分だけ回す（毎秒 20° まで。通過の直後に
    // 時間が速まって天体が後ろへ飛び去る時に、振り回されないように）。残りのずれは 0.4 秒ほどで滑らかに詰める
    // （毎秒 40° まで）。見る天体が変わった時も、この詰め方で振り向く
    auto rotateToward = [](V3 v, V3 to, double maxAngle) {
        double c = std::max(-1.0, std::min(1.0, Dot(v, to)));
        double ang = std::acos(c);
        if (ang < 1e-9) return v;
        V3 perp = to - v * c;
        if (Len(perp) < 1e-12) return v;
        double step = std::min(ang, maxAngle);
        return Norm(v * std::cos(step) + Norm(perp) * std::sin(step));
    };
    V3 aim = app.voyAim;
    if (aimBody == app.voyAimBody && aimBody >= 0) {
        double moved = std::acos(std::max(-1.0, std::min(1.0, Dot(app.voyAimPrevWant, want))));
        if (moved > 1e-9) {   // 前のフレームの見たい向きから今の見たい向きへの回転を、今の向きにも掛ける
            V3 axis = Cross(app.voyAimPrevWant, want);
            if (Len(axis) > 1e-12) {
                axis = Norm(axis);
                double a = std::min(moved, 20 * DEG * dt);
                aim = aim * std::cos(a) + Cross(axis, aim) * std::sin(a) + axis * (Dot(axis, aim) * (1 - std::cos(a)));
                aim = Norm(aim);
            }
        }
    }
    double gap = std::acos(std::max(-1.0, std::min(1.0, Dot(aim, want))));
    app.voyAim = rotateToward(aim, want, std::min(gap * (1 - std::exp(-dt / 0.4)), 40 * DEG * dt));
    app.voyAimBody = aimBody;
    app.voyAimPrevWant = want;
    app.target = app.voyCard >= 0 && app.voySteps[app.voyCard].body >= 0 ? app.voySteps[app.voyCard].body : app.voyProbe;
    PlaceVoyageCamera(app, app.voyAim);
    // 小惑星のそば: 小さくて普通の視野では点にしか見えないので、画面の 4 分の 1 ほどの大きさになるまでズームする
    app.eventZoom = false;
    if (app.voyCard >= 0) {
        const VoyageStep& st = app.voySteps[app.voyCard];
        if (st.body > B_PLUTO && (st.kind == VS_FLYBY || st.kind == VS_ARRIVE)) {
            const Body& o = app.B[st.body];
            double d = Len(o.pos - app.B[app.voyProbe].pos);
            double ang = 2 * std::atan(std::max(o.d.radius, o.outer) / std::max(d, 1e-6)) / DEG;
            app.eventZoom = true;
            app.eventZoomFov = std::max(0.5, std::min(60.0, ang * 4));
        }
    }
}

// できごとの見出しと説明
static void VoyageCardText(const App& app, const VoyageStep& st, std::string* title, std::string* text) {
    char b[320];
    auto name = [&](int i) { return std::string(TR(app.B[i].d.jp, app.B[i].d.en)); };
    const BodyDef& pr = app.B[app.voyProbe].d;
    *text = "";
    switch (st.kind) {
        case VS_LAUNCH:
            if (const VoyageLeg* l = FindVoyageLeg(pr, app.voyLeg)) {
                *title = TR(l->startTitleJp, l->startTitleEn);
                *text = TR(l->startTextJp, l->startTextEn);
                break;
            }
            *title = T(UI_VOY_LAUNCH);
            snprintf(b, sizeof b, T(UI_VOY_LAUNCH_D), TR(pr.jp, pr.en));
            *text = b;
            break;
        case VS_ESCAPE:
            *title = T(UI_VOY_ESCAPE);
            snprintf(b, sizeof b, T(UI_VOY_ESCAPE_D), FmtDist(st.dist).c_str(), st.vOut);
            *text = b;
            break;
        case VS_SWINGBY:
            snprintf(b, sizeof b, T(UI_VOY_SWINGBY), name(st.body).c_str());
            *title = b;
            snprintf(b, sizeof b, T(UI_VOY_SWINGBY_D), FmtDist(st.dist - app.B[st.body].d.radius).c_str(), st.vIn, st.vOut);
            *text = b;
            break;
        case VS_FLYBY:
            snprintf(b, sizeof b, T(UI_VOY_FLYBY), name(st.body).c_str());
            *title = b;
            snprintf(b, sizeof b, T(UI_VOY_FLYBY_D), FmtDist(st.dist).c_str(), app.B[st.body].d.radius * 2);
            *text = b;
            break;
        case VS_TARGET_EARTH:
            snprintf(b, sizeof b, T(UI_VOY_TARGET_EARTH), name(st.body).c_str());
            *title = b;
            snprintf(b, sizeof b, T(UI_VOY_TARGET_EARTH_D), FmtDist(st.dist).c_str(), FmtDist(st.vIn).c_str());
            *text = b;
            break;
        case VS_NEAR:
            snprintf(b, sizeof b, T(UI_VOY_NEAR), name(st.body).c_str());
            *title = b;
            snprintf(b, sizeof b, T(UI_VOY_NEAR_D), FmtDist(st.dist).c_str());
            *text = b;
            break;
        default: {
            const VoyageGoal* g = app.voyLeg <= 1 ? FindVoyageGoal(pr) : nullptr;
            if (const VoyageLeg* l = FindVoyageLeg(pr, app.voyLeg)) {
                *title = TR(l->endTitleJp, l->endTitleEn);
                *text = TR(l->endTextJp, l->endTextEn);
            } else if (g) {
                *title = TR(g->titleJp, g->titleEn);
                *text = TR(g->textJp, g->textEn);
            } else {
                UiId id = st.evKind == EV_IMPACT ? UI_VOY_IMPACT : st.evKind == EV_FLYBY ? UI_VOY_REACHED : UI_VOY_ARRIVED;
                snprintf(b, sizeof b, T(id), name(st.body).c_str());
                *title = b;
                for (const TourStop& ts : TOUR)
                    if (ts.page == 2 && std::strcmp(ts.who, pr.en) == 0 && ts.body && std::strcmp(ts.body, app.B[st.body].d.en) == 0 &&
                        ts.kind == st.evKind)
                        *text = TR(ts.jp, ts.en);
                for (const auto& a : VOYAGE_ARRIVAL_TEXT)
                    if (text->empty() && std::strcmp(a.probe, pr.en) == 0) *text = TR(a.jp, a.en);
            }
            double years = (st.t - app.voySteps.front().t) / 365.25;
            snprintf(b, sizeof b, T(UI_VOY_SUMMARY), years, st.count, app.voyLength / AU_KM);
            *text += (text->empty() ? "" : "\n") + std::string(b);
            break;
        }
    }
    if (st.kind != VS_LAUNCH && st.t > NowDaysSinceJ2000()) *title += T(UI_VOY_PLANNED);   // まだ起きていない（予定の軌道）
}

static std::vector<std::string> SplitLines(const std::string& s) {
    std::vector<std::string> lines;
    for (size_t a = 0; a <= s.size() && !s.empty();) {
        size_t e = std::min(s.find('\n', a), s.size());
        lines.push_back(s.substr(a, e - a));
        a = e + 1;
    }
    return lines;
}

// 時間の速さの表示（x1 / 1 秒あたり何日）
static std::string VoyageRateText(const App& app) {
    if (app.paused) return T(UI_PAUSED);
    if (app.voyRate < 1.5) return std::string("x1 (") + T(UI_REALTIME) + ")";
    char b[64];
    if (app.voyRate < 600) snprintf(b, sizeof b, "x%.0f", app.voyRate);
    else if (app.voyRate < 3600) snprintf(b, sizeof b, T(UI_VOY_RATE_MIN), app.voyRate / 60);
    else if (app.voyRate < 86400) snprintf(b, sizeof b, T(UI_VOY_RATE_H), app.voyRate / 3600);
    else snprintf(b, sizeof b, T(UI_VOY_RATE_D), app.voyRate / 86400);
    return b;
}

// 右上の小さな全体図: 太陽系を真上から見て（右が春分点の方向）、全行程が収まる縮尺で、惑星の軌道と位置、
// 探査機の通ってきた道（明るく）とこれから通る道（暗く）、場面の場所、今の位置を描く
static Rectangle VoyageMapRect(const Cam& c) {
    float s = std::max(150.0f, std::min(280.0f, std::min(c.W * 0.26f, c.H * 0.42f)));
    return {c.W - s - 10, 10, s, s};
}

// 天体の近くの拡大図に切り替える場面（太陽と、天体のない場面は除く）。場面の前後 span 日の間
static int VoyageLocalStep(const App& app) {
    for (size_t k = 0; k < app.voySteps.size(); k++) {
        const VoyageStep& st = app.voySteps[k];
        if (st.body <= B_SUN || !app.voyLocal[k].ready) continue;
        if (std::fabs(app.tDays - st.t) < app.voyLocal[k].span) return (int)k;
    }
    return -1;
}

// 場面 k の拡大図の軌跡を求める: 最接近の距離の数倍まで離れる時間の範囲で、天体から見た探査機の位置
static void PrepareVoyageLocal(App& app, int k) {
    App::VoyLocal& L = app.voyLocal[k];
    if (L.ready) return;
    L.ready = true;
    const VoyageStep& st = app.voySteps[k];
    if (st.body <= B_SUN) return;
    std::vector<Body> B = app.B;
    for (Body& b : B) b.orbit.clear();
    auto relAt = [&](double t) {
        UpdateBodies(B, t);
        return B[app.voyProbe].pos - B[st.body].pos;
    };
    // 最接近の距離の 4 倍（地球の出発・脱出は重力圏の半径まで）離れるまでの時間を、前後それぞれ探す
    double d0 = std::max(Len(relAt(st.t)), app.B[st.body].d.radius * 2);
    double reach = st.kind == VS_LAUNCH || st.kind == VS_ESCAPE ? app.B[B_EARTH].soi * 1.3 : d0 * 4;
    double t0 = app.voySteps.front().t, t1 = app.voySteps.back().t;
    auto edge = [&](double dir) {
        double h = 1.0 / 1440;   // 1 分から倍々に
        while (h < 60 && Len(relAt(std::max(t0, std::min(t1, st.t + dir * h)))) < reach) h *= 2;
        return std::min(h, 60.0);
    };
    double before = std::min(edge(-1), st.t - t0), after = std::min(edge(1), t1 - st.t);
    if (st.kind == VS_LAUNCH) before = 0;
    L.span = std::max(before, after);
    const int N = 400;
    for (int j = 0; j <= N; j++) {
        double t = st.t - before + (before + after) * j / N;
        L.t.push_back(t);
        L.rel.push_back(relAt(t));
    }
}

// 色の透明度に掛ける（右上の図の切り替えのフェード）
static Color Alpha(Color c, float a) {
    c.a = (unsigned char)(c.a * std::max(0.0f, std::min(1.0f, a)));
    return c;
}

// 天体の近くの拡大図（場面 ls の天体を中心に）。fa は透明度
static void DrawVoyageMapLocal(const App& app, Rectangle r, int ls, float fa) {
    const App::VoyLocal& L = app.voyLocal[ls];
    const Body& o = app.B[app.voySteps[ls].body];
    double range = o.d.radius * 1.5;
    for (const V3& p : L.rel) range = std::max(range, std::sqrt(p.x * p.x + p.z * p.z));
    range *= 1.08;
    float cx = r.x + r.width * 0.5f, cy = r.y + r.height * 0.5f + 6, R = r.width * 0.5f - 10;
    double k = R / range;
    auto proj = [&](V3 p) { return Vector2{(float)(cx + p.x * k), (float)(cy + p.z * k)}; };
    Rectangle in = {r.x + 2, r.y + 2, r.width - 4, r.height - 4};
    DrawCircleV({cx, cy}, std::max(2.5f, (float)(o.d.radius * k)), Alpha(BodyColor(o, 0.9f), fa));
    for (size_t j = 1; j < L.rel.size(); j++) {
        Vector2 a = proj(L.rel[j - 1]), b = proj(L.rel[j]);
        if (!CheckCollisionPointRec(a, in) || !CheckCollisionPointRec(b, in)) continue;
        bool past = L.t[j] <= app.tDays;
        DrawLineEx(a, b, past ? 1.8f : 1.0f, past ? Alpha({255, 196, 120, 230}, fa) : Alpha({255, 196, 120, 70}, fa));
    }
    Vector2 me = proj(app.B[app.voyProbe].pos - o.pos);
    if (CheckCollisionPointRec(me, in)) {
        DrawCircleV(me, 3.0f, Alpha({120, 255, 170, 255}, fa));
        DrawCircleLinesV(me, 4.0f + 2.0f * (float)std::fabs(std::sin(GetTime() * 3)), Alpha({120, 255, 170, 220}, fa));
    }
    char b[96];
    snprintf(b, sizeof b, T(UI_VOY_MAP_NEAR), TR(o.d.jp, o.d.en));
    Txt(b, r.x + 8, r.y + 5, 13, Alpha({170, 200, 230, 255}, fa));
    double unit = std::pow(10.0, std::floor(std::log10(range * 0.6)));
    float bw = (float)(unit * k);
    DrawLineEx({r.x + 10, r.y + r.height - 10}, {r.x + 10 + bw, r.y + r.height - 10}, 1.5f, Alpha({170, 200, 230, 220}, fa));
    Txt(FmtDist(unit).c_str(), r.x + 14 + bw, r.y + r.height - 18, 12, Alpha({170, 200, 230, 220}, fa));
}

// 全行程の全体図。fa は透明度
static void DrawVoyageMapWhole(const App& app, Rectangle r, float fa) {
    double range = 0;
    for (const V3& p : app.voyTrack) range = std::max(range, std::sqrt(p.x * p.x + p.z * p.z));
    range = std::max(range * 1.08, 0.05 * AU_KM);
    float cx = r.x + r.width * 0.5f, cy = r.y + r.height * 0.5f + 6, R = r.width * 0.5f - 10;
    double k = R / range;
    auto proj = [&](V3 p) { return Vector2{(float)(cx + p.x * k), (float)(cy + p.z * k)}; };
    // 枠の外にはみ出す線や点は描かない（切り抜きは HUD の拡大と座標が合わないので使わない）
    Rectangle in = {r.x + 2, r.y + 2, r.width - 4, r.height - 4};
    auto line = [&](Vector2 a, Vector2 b, float w, Color col) {
        if (CheckCollisionPointRec(a, in) && CheckCollisionPointRec(b, in)) DrawLineEx(a, b, w, col);
    };
    auto dot = [&](Vector2 a, float rad, Color col) {
        if (CheckCollisionPointRec(a, in)) DrawCircleV(a, rad, col);
    };
    const V3 sun = app.B[B_SUN].pos;
    // 惑星の軌道と今の位置（全体図の外まで広がる軌道も、図の中の部分は描く）
    for (int i = 1; i <= B_PLUTO; i++) {
        const Body& b = app.B[i];
        if (b.orbit.size() < 2 || b.d.a_au * AU_KM * (1 - 0.25) > range * 1.5) continue;
        Color col = BodyColor(b, 0.8f);
        col.a = 90;
        Vector2 prev = proj(b.orbit[0]);
        for (size_t j = 1; j < b.orbit.size(); j++) {
            Vector2 q = proj(b.orbit[j]);
            line(prev, q, 1.0f, Alpha(col, fa));
            prev = q;
        }
        col.a = 230;
        dot(proj(b.pos - sun), 2.5f, Alpha(col, fa));
    }
    DrawCircleV({cx, cy}, 3.5f, Alpha({255, 220, 120, 255}, fa));
    // 探査機の道
    size_t now = 0;
    while (now + 1 < app.voyTrackT.size() && app.voyTrackT[now + 1] <= app.tDays) now++;
    for (size_t j = 1; j < app.voyTrack.size(); j++) {
        bool past = j <= now;
        line(proj(app.voyTrack[j - 1]), proj(app.voyTrack[j]), past ? 1.8f : 1.0f,
                   past ? Alpha({255, 196, 120, 230}, fa) : Alpha({255, 196, 120, 70}, fa));
    }
    // 場面の場所（済んだものは白、これからのものは灰色。知らせを出している場面は黄色）
    for (size_t j = 0; j < app.voyStepPos.size(); j++) {
        bool done = app.voySteps[j].t <= app.tDays;
        Color col = (int)j == app.voyCard ? Color{255, 220, 80, 255} : done ? Color{230, 235, 245, 220} : Color{130, 140, 160, 200};
        dot(proj(app.voyStepPos[j]), (int)j == app.voyCard ? 3.5f : 2.2f, Alpha(col, fa));
    }
    // 今の位置（点滅する輪）
    Vector2 me = proj(app.B[app.voyProbe].pos - sun);
    float pulse = 4.0f + 2.0f * (float)std::fabs(std::sin(GetTime() * 3));
    dot(me, 3.0f, Alpha({120, 255, 170, 255}, fa));
    if (CheckCollisionPointRec(me, in)) DrawCircleLinesV(me, pulse, Alpha({120, 255, 170, 220}, fa));
    // 見出しと縮尺（1 / 10 AU の棒）
    Txt(T(UI_VOY_MAP), r.x + 8, r.y + 5, 13, Alpha({170, 200, 230, 255}, fa));
    double unit = range > 8 * AU_KM ? 10 * AU_KM : range > 0.8 * AU_KM ? AU_KM : 0.1 * AU_KM;
    float bw = (float)(unit * k);
    char b[32];
    snprintf(b, sizeof b, "%g AU", unit / AU_KM);
    DrawLineEx({r.x + 10, r.y + r.height - 10}, {r.x + 10 + bw, r.y + r.height - 10}, 1.5f, Alpha({170, 200, 230, 220}, fa));
    Txt(b, r.x + 14 + bw, r.y + r.height - 18, 12, Alpha({170, 200, 230, 220}, fa));
}

// 右上の小さな図。全体図と拡大図が切り替わる時は 0.6 秒かけて重ねて入れ替える
static void DrawVoyageMap(const App& app) {
    if (!app.voyMap || app.voyTrack.empty()) return;
    Rectangle r = VoyageMapRect(app.cam);
    DrawRectangleRounded(r, 0.05f, 6, {0, 8, 20, 250});
    DrawRectangleRoundedLinesEx(r, 0.05f, 6, 1.2f, {90, 130, 180, 200});
    // 表示中の図（-1: 全体図, 0 以上: その場面の拡大図）と、フェードで消えていく図。旅が変わったら最初から
    static int shown = -2, fading = -2, key = -1;
    static double fadeStart = -1e9;
    int mode = VoyageLocalStep(app);
    int k = app.voyProbe * 16 + app.voyLeg;
    if (key != k || shown == -2) {
        key = k;
        shown = fading = mode;
        fadeStart = -1e9;
    }
    if (mode != shown) {
        fading = shown;
        shown = mode;
        fadeStart = GetTime();
    }
    float f = (float)std::min(1.0, (GetTime() - fadeStart) / 0.6);
    f = f * f * (3 - 2 * f);
    auto draw = [&](int m, float a) {
        if (m >= 0 && m < (int)app.voySteps.size()) DrawVoyageMapLocal(app, r, m, a);
        else DrawVoyageMapWhole(app, r, a);
    };
    if (f < 1) draw(fading, 1 - f);
    draw(shown, f < 1 ? f : 1.0f);
}

static void DrawVoyageHud(const App& app) {
    const Cam& c = app.cam;
    const Body& p = app.B[app.voyProbe];
    char b[320];
    float fs = 16, lh = 22;
    int nextIdx = -1;
    for (size_t k = 0; k < app.voySteps.size(); k++)
        if (app.voySteps[k].t > app.tDays) { nextIdx = (int)k; break; }
    float panelH = lh * (nextIdx >= 0 ? 5 : 4) + 34;
    DrawRectangleRounded({6, 6, 460, panelH}, 0.06f, 6, {0, 10, 25, 170});
    float x = 16, y = 12;
    snprintf(b, sizeof b, T(UI_VOY_HEAD), VoyageName(app, app.voyProbe, app.voyLeg).c_str());
    Txt(b, x, y, fs + 3, {255, 220, 120, 255}); y += lh + 6;
    double elapsed = std::max(0.0, app.tDays - app.voySteps.front().t);
    snprintf(b, sizeof b, T(UI_VOY_DATE), FmtDate(app.tDays).c_str());
    Txt(b, x, y, fs, WHITE); y += lh;
    int ey = (int)(elapsed / 365.25);
    snprintf(b, sizeof b, T(UI_VOY_ELAPSED), ey, (int)(elapsed - ey * 365.25));
    Txt(b, x, y, fs, {200, 220, 240, 255}); y += lh;
    std::string rate = VoyageRateText(app);
    static const UiId SPEED[3] = {UI_VOY_SPEED_SLOW, UI_VOY_SPEED_NORMAL, UI_VOY_SPEED_FAST};
    snprintf(b, sizeof b, "%s %s   [%s]", T(UI_TSCALE), rate.c_str(), T(SPEED[app.voySpeed]));
    Txt(b, x, y, fs, app.paused ? Color{255, 160, 80, 255} : app.voyRate < 1.5 ? Color{140, 230, 160, 255} : Color{255, 210, 120, 255}); y += lh;
    if (nextIdx >= 0) {
        std::string t, d;
        VoyageCardText(app, app.voySteps[nextIdx], &t, &d);
        double left = app.voySteps[nextIdx].t - app.tDays;
        std::string when = left > 1 ? std::to_string((int)left) + T(UI_VOY_DAYS) : FmtDuration(left * 86400);
        snprintf(b, sizeof b, T(UI_VOY_NEXT), t.c_str(), when.c_str());
        Txt(b, x, y, fs - 1, {170, 200, 230, 255});
    }

    DrawVoyageMap(app);

    // できごとの知らせ（右下の空いた所まで使えるよう、左下に）
    bool showCard = app.voyCard >= 0;
    float w = std::min(c.W - 24, 880.0f);
    float px = 12;
    if (showCard) {
        const VoyageStep& st = app.voySteps[app.voyCard];
        std::string title, text;
        VoyageCardText(app, st, &title, &text);
        std::vector<std::string> lines = SplitLines(text);
        float h = 72 + lines.size() * 22.0f, py = c.H - 60 - h;
        DrawRectangleRounded({px, py, w, h}, 0.08f, 6, {0, 8, 20, 215});
        DrawRectangleRoundedLinesEx({px, py, w, h}, 0.08f, 6, 1.5f, {90, 130, 180, 255});
        snprintf(b, sizeof b, T(UI_VOY_STEP), app.voyCard + 1, (int)app.voySteps.size());
        Txt((std::string(b) + "    " + FmtMinute(st.t) + " UTC").c_str(), px + 18, py + 10, 15, {170, 200, 230, 255});
        Txt(title.c_str(), px + 18, py + 32, 22, {255, 220, 120, 255});
        for (size_t i = 0; i < lines.size(); i++) Txt(lines[i].c_str(), px + 18, py + 64 + i * 22.0f, 16, {215, 228, 245, 255});
    }
    DrawVoyageTimeline(app);
    const char* keys = T(app.voyDone ? UI_VOY_KEYS_DONE : UI_VOY_KEYS);
    Txt(keys, 14, c.H - 22, 13, {150, 170, 200, 230});
#if defined(__EMSCRIPTEN__)
    if (int pend = DeferredAssetsPending(); pend > 0) {
        snprintf(b, sizeof b, T(UI_LOADING), pend);
        Txt(b, 14, c.H - 70, 14, {255, 210, 120, 230});
    }
#endif
}

// 旅の間のキー入力。自機の操縦は受け付けない
static void HandleVoyageInput(App& app, bool next, bool prev, bool quit) {
    if (quit) { StopVoyage(app); return; }
    if (next) {
        if (app.voyDone) StopVoyage(app);
        else VoyageSkip(app, 1);
    }
    if (prev) VoyageSkip(app, -1);
}
