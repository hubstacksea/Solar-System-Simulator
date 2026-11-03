// Solar System Voyager — Copyright (c) 2026 hubstacksea
// MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
// 探査機の旅: 地球を出発してから目標に着くまでを探査機のそばから追う。何もない間は時間を速く進め、
// 出発・地球の重力圏の脱出・スイングバイ・目的外の天体への接近・到着では等倍まで落として知らせを出す。
// 行程は軌跡のデータ（assets/probes.bin）から作る。URL の ?voyage=名前&scene=場面 でその場面から始まる。
// main.cpp の一部で、main.cpp が決まった順に読み込む（単体ではコンパイルしない）

// 時間の進み方: できごとの前後 VOY_HOLD_SEC 秒は等倍、そこから離れるほど指数的に速くする（時定数 VOY_RAMP_SEC 秒）
static const double VOY_HOLD_SEC = 8.0, VOY_RAMP_SEC = 1.5;
static const double VOY_CARD_LEAD_SEC = 3000.0;      // できごとの知らせを出す範囲（前後のシミュレータの秒。実時間で 10 秒ほど）
static const double VOY_CRUISE_WALL_SEC = 240.0;     // 巡航の速さの目安: 全行程をこの実時間で進む速さ
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

// 地球から出発する軌跡を持つ探査機か（地球を回る人工衛星は除く）
static bool VoyageCapable(const App& app, int i) {
    const BodyDef& d = app.B[i].d;
    if (d.kind != KIND_PROBE || d.tleN > 0 || d.probe < 0 || d.probe >= (int)g_probes.size()) return false;
    const std::vector<ProbeSeg>& S = g_probes[d.probe].segs;
    return !S.empty() && S.front().body == B_EARTH;
}

// 旅の終点: 時刻・目標の天体・できごとの種類（EV_ARRIVE / EV_FLYBY / EV_IMPACT。到着しないものは EV_END）
static bool VoyageEnd(const App& app, int i, double* t, int* body, int* kind) {
    const std::vector<ProbeSeg>& S = g_probes[app.B[i].d.probe].segs;
    double dataEnd = S.back().t.back();
    *body = -1;
    *kind = EV_END;
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
static void BuildVoyage(const App& app, int i, std::vector<VoyageStep>* steps, double* length) {
    steps->clear();
    *length = 0;
    const BodyDef& d = app.B[i].d;
    const std::vector<ProbeSeg>& S = g_probes[d.probe].segs;
    double t0 = S.front().t.front(), tEnd;
    int endBody, endKind;
    if (!VoyageEnd(app, i, &tEnd, &endBody, &endKind)) return;
    const VoyageGoal* goal = FindVoyageGoal(d);
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
        if (ev.probe != i || ev.kind != EV_FLYBY || ev.t >= tEnd - 1e-6) continue;
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

// イベント一覧の「探査機の旅」のページ。ev.t は出発、ev.body は目標、ev.dist は到着の時刻
static std::vector<AstroEvent> FindVoyages(const App& app) {
    std::vector<AstroEvent> out;
    for (size_t i = 0; i < app.B.size(); i++) {
        if (!VoyageCapable(app, (int)i)) continue;
        AstroEvent ev;
        int kind;
        if (!VoyageEnd(app, (int)i, &ev.dist, &ev.body, &kind)) continue;
        ev.kind = EV_VOYAGE;
        ev.probe = (int)i;
        ev.t = g_probes[app.B[i].d.probe].segs.front().t.front();
        out.push_back(ev);
    }
    std::sort(out.begin(), out.end(), [](const AstroEvent& a, const AstroEvent& b) { return a.t < b.t; });
    return out;
}

static std::string VoyageTargetName(const App& app, int i, int body) {
    if (const VoyageGoal* g = FindVoyageGoal(app.B[i].d)) return TR(g->targetJp, g->targetEn);
    return body >= 0 ? TR(app.B[body].d.jp, app.B[body].d.en) : "";
}

// 探査機のそばのカメラの向きと位置: 探査機を画面の右上に入れて（左上と下は表示で埋まる）、見たい方向 dir を正面にする
static void PlaceVoyageCamera(App& app, V3 dir) {
    Ship& s = app.ship;
    s.ref = app.voyProbe;
    s.speed = 0;
    s.thrusting = false;
    s.q = QuatFromBasis(dir, Y_UP);
    s.local = ShipFwd(s) * -0.032 - ShipRight(s) * 0.009 - ShipUp(s) * 0.005;
}

// 今見たい方向: 知らせを出しているできごとの天体、なければ次のできごとの天体、それもなければ進む向き
static V3 VoyageAimDir(const App& app) {
    const Body& p = app.B[app.voyProbe];
    int body = -1;
    if (app.voyCard >= 0) body = app.voySteps[app.voyCard].body;
    if (body < 0)
        for (const VoyageStep& st : app.voySteps)
            if (st.t > app.tDays && st.body >= 0) { body = st.body; break; }
    if (body >= 0) {
        const Body& o = app.B[body];
        V3 d = o.pos - p.pos;
        // 近くで夜側を通る時は、昼側の縁（三日月形に光る部分）を正面にする。遠い時や恒星は中心を
        if (body != B_SUN && Len(d) < o.d.radius * 8 && Len(d) > 1e-6) {
            V3 out = Norm(d * -1.0), toSun = Norm(app.B[B_SUN].pos - o.pos);
            double facing = Dot(out, toSun);
            if (facing < 0) {
                V3 limb = o.pos + Norm(toSun - out * facing) * o.d.radius;
                V3 dl = limb - p.pos;
                if (Len(dl) > 1e-6) return Norm(dl);
            }
        }
        if (Len(d) > 1e-6) return Norm(d);
    }
    return Len(p.pos - app.B[B_SUN].pos) > 1e-6 ? Norm(app.voyAim) : V3(1, 0, 0);   // 目標のない巡航は今の向きのまま
}

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
    app.voyAim = VoyageAimDir(app);
    PlaceVoyageCamera(app, app.voyAim);
}

// 探査機 i の旅を始める。scene: 場面の番号（0 が出発、負なら末尾から）。t が有効ならその時刻から
static bool StartVoyage(App& app, int i, int scene, double t) {
    if (!VoyageCapable(app, i)) return false;
    std::vector<VoyageStep> steps;
    double length;
    BuildVoyage(app, i, &steps, &length);
    if (steps.size() < 2) return false;
    if (app.tourOn) app.tourOn = false;
    app.autopilot = false;
    app.warpT = -1;
    app.voySavedPhysics = app.physics;
    if (app.physics) SetPhysics(app, false);
    app.voySavedCockpit = app.voyageOn ? app.voySavedCockpit : app.cockpit;
    app.voyageOn = true;
    app.voyProbe = i;
    app.voySteps = steps;
    app.voyLength = length;
    app.cockpit = true;       // 自機は描かない
    app.paused = false;
    app.timeIdx = 0;
    app.eventZoom = false;
    app.frameLock = false;
    app.target = i;
    double t0 = steps.front().t, t1 = steps.back().t;
    app.voyCruise = std::max(0.5, std::min(20.0, (t1 - t0) / VOY_CRUISE_WALL_SEC)) * 86400.0;
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
    if (who < 0 || !StartVoyage(app, who, scene, app.pendingVoyageT)) Toast(app, T(UI_VOY_NOT_FOUND));
    app.pendingVoyage.clear();
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
    V3 want = VoyageAimDir(app);
    // 大きく向きを変える時は滑らかに振り向く。追いかけている天体の小さな動き（近くを速く通る小惑星など）にはそのまま付いていく
    double k = std::min(1.0, dt * 1.2);
    V3 mix = app.voyAim * (1 - k) + want * k;
    app.voyAim = Dot(app.voyAim, want) > std::cos(10 * DEG) ? want : Len(mix) > 1e-6 ? Norm(mix) : want;
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
        case VS_NEAR:
            snprintf(b, sizeof b, T(UI_VOY_NEAR), name(st.body).c_str());
            *title = b;
            snprintf(b, sizeof b, T(UI_VOY_NEAR_D), FmtDist(st.dist).c_str());
            *text = b;
            break;
        default: {
            const VoyageGoal* g = FindVoyageGoal(pr);
            if (g) {
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
    snprintf(b, sizeof b, T(UI_VOY_HEAD), TR(p.d.jp, p.d.en));
    Txt(b, x, y, fs + 3, {255, 220, 120, 255}); y += lh + 6;
    double elapsed = std::max(0.0, app.tDays - app.voySteps.front().t);
    snprintf(b, sizeof b, T(UI_VOY_DATE), FmtDate(app.tDays).c_str());
    Txt(b, x, y, fs, WHITE); y += lh;
    int ey = (int)(elapsed / 365.25);
    snprintf(b, sizeof b, T(UI_VOY_ELAPSED), ey, (int)(elapsed - ey * 365.25));
    Txt(b, x, y, fs, {200, 220, 240, 255}); y += lh;
    std::string rate = VoyageRateText(app);
    snprintf(b, sizeof b, "%s %s", T(UI_TSCALE), rate.c_str());
    Txt(b, x, y, fs, app.paused ? Color{255, 160, 80, 255} : app.voyRate < 1.5 ? Color{140, 230, 160, 255} : Color{255, 210, 120, 255}); y += lh;
    if (nextIdx >= 0) {
        std::string t, d;
        VoyageCardText(app, app.voySteps[nextIdx], &t, &d);
        double left = app.voySteps[nextIdx].t - app.tDays;
        std::string when = left > 1 ? std::to_string((int)left) + T(UI_VOY_DAYS) : FmtDuration(left * 86400);
        snprintf(b, sizeof b, T(UI_VOY_NEXT), t.c_str(), when.c_str());
        Txt(b, x, y, fs - 1, {170, 200, 230, 255});
    }

    // できごとの知らせ（右下の空いた所まで使えるよう、左下に）
    bool showCard = app.voyCard >= 0;
    float w = std::min(c.W - 24, 880.0f);
    float px = 12;
    if (showCard) {
        const VoyageStep& st = app.voySteps[app.voyCard];
        std::string title, text;
        VoyageCardText(app, st, &title, &text);
        std::vector<std::string> lines = SplitLines(text);
        float h = 72 + lines.size() * 22.0f, py = c.H - 34 - h;
        DrawRectangleRounded({px, py, w, h}, 0.08f, 6, {0, 8, 20, 215});
        DrawRectangleRoundedLinesEx({px, py, w, h}, 0.08f, 6, 1.5f, {90, 130, 180, 255});
        snprintf(b, sizeof b, T(UI_VOY_STEP), app.voyCard + 1, (int)app.voySteps.size());
        Txt((std::string(b) + "    " + FmtMinute(st.t) + " UTC").c_str(), px + 18, py + 10, 15, {170, 200, 230, 255});
        Txt(title.c_str(), px + 18, py + 32, 22, {255, 220, 120, 255});
        for (size_t i = 0; i < lines.size(); i++) Txt(lines[i].c_str(), px + 18, py + 64 + i * 22.0f, 16, {215, 228, 245, 255});
    }
    const char* keys = T(app.voyDone ? UI_VOY_KEYS_DONE : UI_VOY_KEYS);
    Txt(keys, 14, c.H - 26, 14, {150, 170, 200, 230});
#if defined(__EMSCRIPTEN__)
    if (int pend = DeferredAssetsPending(); pend > 0) {
        snprintf(b, sizeof b, T(UI_LOADING), pend);
        Txt(b, 14, c.H - 46, 14, {255, 210, 120, 230});
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
