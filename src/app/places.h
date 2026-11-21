// Solar System Voyager — Copyright (c) 2026 hubstacksea
// MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
// ブックマーク・共有リンク・前回の続き。
// main.cpp の一部で、main.cpp が決まった順に読み込む（単体ではコンパイルしない）

// ---------------------------------------------------------------- ブックマーク
// 天体に固定した座標（自転と一緒に回る）で位置と向きを保存する。設定ファイルの bm1〜bm9
// 形式: 天体の英語名,位置 xyz km,機首 xyz,上 xyz
static const int MAX_BOOKMARKS = 9;

static void BodyFixedAxes(const Body& b, V3* X, V3* Z) {
    double cs = std::cos(b.spin), sn = std::sin(b.spin);
    *X = b.ex * cs - b.ez * sn;
    *Z = b.ex * sn + b.ez * cs;
}

struct Bookmark {
    int body = -1;
    V3 pos, fwd, up;   // 天体固定座標
};

static bool ParseBookmark(const App& app, const std::string& v, Bookmark* bm) {
    size_t c = v.find(',');
    if (c == std::string::npos) return false;
    std::string name = v.substr(0, c);
    for (size_t i = 0; i < app.B.size(); i++)
        if (name == app.B[i].d.en) bm->body = (int)i;
    double a[9];
    if (bm->body < 0 || sscanf(v.c_str() + c + 1, "%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf", &a[0], &a[1], &a[2], &a[3], &a[4], &a[5],
                               &a[6], &a[7], &a[8]) != 9)
        return false;
    for (double x : a)
        if (!std::isfinite(x)) return false;
    bm->pos = V3(a[0], a[1], a[2]);
    bm->fwd = V3(a[3], a[4], a[5]);
    bm->up = V3(a[6], a[7], a[8]);
    // 向きは直交する単位ベクトルに直す（手で書き換えたリンクでも姿勢が壊れないように）。位置が中心・向きが平行なら読まない
    double r = Len(bm->pos), f = Len(bm->fwd);
    if (r < 1e-3 || f < 1e-9) return false;
    bm->fwd = bm->fwd * (1.0 / f);
    bm->up = bm->up - bm->fwd * Dot(bm->up, bm->fwd);
    double u = Len(bm->up);
    if (u < 1e-9) return false;
    bm->up = bm->up * (1.0 / u);
    return true;
}

static std::vector<Bookmark> LoadBookmarks(const App& app) {
    std::vector<Bookmark> v;
    for (int i = 1; i <= MAX_BOOKMARKS; i++) {
        Bookmark bm;
        if (ParseBookmark(app, GetSetting("bm" + std::to_string(i)), &bm)) v.push_back(bm);
    }
    return v;
}

static std::string BookmarkLabel(const App& app, const Bookmark& bm) {
    const Body& b = app.B[bm.body];
    double r = Len(bm.pos);
    V3 dir = bm.pos * (1.0 / r);
    V3 X, Z;
    BodyFixedAxes(b, &X, &Z);
    double alt = r - SurfaceRadius(b, X * dir.x + b.pole * dir.y + Z * dir.z);
    char buf[160];
    snprintf(buf, sizeof buf, "%s  %s %.1f°  %s %.1f°  %s", TR(b.d.jp, b.d.en), dir.y >= 0 ? "N" : "S",
             std::fabs(MapLatDeg(b.d, dir)), std::atan2(-dir.z, dir.x) >= 0 ? "E" : "W",
             std::fabs(std::atan2(-dir.z, dir.x) / DEG), FmtDist(std::max(0.0, alt)).c_str());
    return buf;
}

static void SaveBookmark(App& app) {
    std::vector<Bookmark> v = LoadBookmarks(app);
    const Body& b = app.B[app.ship.ref];
    V3 X, Z;
    BodyFixedAxes(b, &X, &Z);
    auto toFixed = [&](V3 w) { return V3(Dot(w, X), Dot(w, b.pole), Dot(w, Z)); };
    Bookmark bm{app.ship.ref, toFixed(app.ship.local), toFixed(ShipFwd(app.ship)), toFixed(ShipUp(app.ship))};
    if ((int)v.size() >= MAX_BOOKMARKS) v.erase(v.begin());   // いっぱいなら一番古いものを消す
    v.push_back(bm);
    for (int i = 1; i <= MAX_BOOKMARKS; i++) {
        std::string val;
        if (i <= (int)v.size()) {
            const Bookmark& m = v[i - 1];
            char buf[400];
            snprintf(buf, sizeof buf, "%s,%.6f,%.6f,%.6f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f", app.B[m.body].d.en, m.pos.x, m.pos.y, m.pos.z,
                     m.fwd.x, m.fwd.y, m.fwd.z, m.up.x, m.up.y, m.up.z);
            val = buf;
        }
        SetSetting("bm" + std::to_string(i), val);
    }
    SaveSettings();
    app.nextBookmark = 0;
    char msg[256];
    snprintf(msg, sizeof msg, T(UI_BM_SAVED), (int)v.size(), BookmarkLabel(app, bm).c_str());
    Toast(app, msg);
}

// ワープの瞬間に呼ぶ: ブックマークの位置・向きへ移す
static void ApplyBookmark(App& app, const Bookmark& bm) {
    const Body& b = app.B[bm.body];
    V3 X, Z;
    BodyFixedAxes(b, &X, &Z);
    auto toWorld = [&](V3 f) { return X * f.x + b.pole * f.y + Z * f.z; };
    app.ship.ref = bm.body;
    app.ship.speed = 0;
    app.ship.local = toWorld(bm.pos);
    app.ship.q = QuatFromBasis(toWorld(bm.fwd), toWorld(bm.up));
    app.target = bm.body;
    UpdateReferenceFrame(app);
}

// ---------------------------------------------------------------- 共有リンク
// 場面を URL の ? 以降に入れる: t=時刻（J2000 からの日数）, at=ブックマークと同じ形式の位置と向き,
// tg=ターゲットの英語名, ts=時間倍率の番号, p=1 なら時間停止
static const char* const SHARE_BASE = "https://hubstacksea.github.io/Solar-System-Simulator/";

#if defined(__EMSCRIPTEN__)
EM_JS(char*, WebQuery, (), { return stringToNewUTF8(location.search || ""); });
EM_JS(char*, WebPageUrl, (), { return stringToNewUTF8(location.origin + location.pathname); });
// クリップボードへ書き、アドレス欄も同じ URL にする（再読み込みでも同じ場面になる）。書けなければ 0
EM_JS(int, WebShare, (const char* url), {
    var u = UTF8ToString(url);
    try { history.replaceState(null, "", u); } catch (e) {}
    if (!navigator.clipboard || !window.isSecureContext) return 0;
    navigator.clipboard.writeText(u).catch(function () {});
    return 1;
});
#endif

static std::string UrlEncode(const std::string& v) {
    std::string out;
    for (unsigned char ch : v) {
        if (isalnum(ch) || strchr("-_.~,", ch)) out += (char)ch;
        else {
            char h[4];
            snprintf(h, sizeof h, "%%%02X", ch);
            out += h;
        }
    }
    return out;
}

static std::string UrlDecode(const std::string& v) {
    std::string out;
    for (size_t i = 0; i < v.size(); i++) {
        if (v[i] == '%' && i + 2 < v.size() && isxdigit((unsigned char)v[i + 1]) && isxdigit((unsigned char)v[i + 2])) {
            out += (char)strtol(v.substr(i + 1, 2).c_str(), nullptr, 16);
            i += 2;
        } else out += v[i] == '+' ? ' ' : v[i];
    }
    return out;
}

static std::string ShareQuery(const App& app) {
    const Body& b = app.B[app.ship.ref];
    V3 X, Z;
    BodyFixedAxes(b, &X, &Z);
    auto toFixed = [&](V3 w) { return V3(Dot(w, X), Dot(w, b.pole), Dot(w, Z)); };
    V3 p = toFixed(app.ship.local), f = toFixed(ShipFwd(app.ship)), u = toFixed(ShipUp(app.ship));
    char at[400];
    snprintf(at, sizeof at, "%s,%.3f,%.3f,%.3f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f", b.d.en, p.x, p.y, p.z, f.x, f.y, f.z, u.x, u.y, u.z);
    char q[700];
    snprintf(q, sizeof q, "?t=%.6f&at=%s&tg=%s&ts=%d%s", app.tDays, UrlEncode(at).c_str(), UrlEncode(app.B[app.target].d.en).c_str(),
             app.timeIdx, app.paused ? "&p=1" : "");
    if (app.eventZoom) {   // ズームした視野（ガリレオの見た木星、小惑星への接近など）
        size_t n = strlen(q);
        snprintf(q + n, sizeof q - n, "&fov=%.4g", app.eventZoomFov);
    }
    return q;
}

// 探査機の旅の中の場面: 知らせを出している場面なら場面の番号（到着は arrive）、ほかは時刻で
static std::string VoyageQuery(const App& app) {
    char q[200];
    if (app.apolloMission >= 0) {   // アポロ計画: ?apollo=11&scene=場面
        std::string n = APOLLO_MISSIONS[app.apolloMission].name;
        n = n.substr(n.find(' ') + 1);
        if (app.voyCard >= 0) snprintf(q, sizeof q, "?apollo=%s&scene=%d", n.c_str(), app.voyCard + 1);
        else snprintf(q, sizeof q, "?apollo=%s&t=%.6f", n.c_str(), app.tDays);
        return q;
    }
    std::string name = UrlEncode(app.B[app.voyProbe].d.en);
    if (app.voyLeg > 1) name += "&leg=" + std::to_string(app.voyLeg);
    int last = (int)app.voySteps.size() - 1;
    if (app.voyCard == last) snprintf(q, sizeof q, "?voyage=%s&scene=arrive", name.c_str());
    else if (app.voyCard >= 0) snprintf(q, sizeof q, "?voyage=%s&scene=%d", name.c_str(), app.voyCard + 1);
    else snprintf(q, sizeof q, "?voyage=%s&t=%.6f", name.c_str(), app.tDays);
    return q;
}

static void ShareScene(App& app) {
    std::string q = app.voyageOn ? VoyageQuery(app) : ShareQuery(app);
#if defined(__EMSCRIPTEN__)
    char* base = WebPageUrl();
    std::string url = base + q;
    free(base);
    Toast(app, T(WebShare(url.c_str()) ? UI_SHARED : UI_SHARED_BAR));
#else
    SetClipboardText((SHARE_BASE + q).c_str());
    Toast(app, T(UI_SHARED));
#endif
}

// 前回の続き（設定の resume に共有リンクと同じ形で保存）。場所だけの時は日時・時間倍率・停止を除く
static std::string ResumeQuery(const std::string& saved, bool withTime) {
    if (withTime) return saved;
    std::string out;
    size_t i = 0;
    while (i < saved.size()) {
        size_t amp = saved.find('&', i);
        if (amp == std::string::npos) amp = saved.size();
        std::string item = saved.substr(i, amp - i), key = item.substr(0, item.find('='));
        if (key != "t" && key != "ts" && key != "p") out += (out.empty() ? "" : "&") + item;
        i = amp + 1;
    }
    return out;
}

static void SaveResume(const App& app) {
    SetSetting("resume", ShareQuery(app).substr(1));   // 先頭の ? を除く
    SaveSettings();
}

// 共有リンクの ? の後を キー → 値 に
static std::map<std::string, std::string> ParseQuery(std::string q) {
    size_t qm = q.find('?');
    if (qm != std::string::npos) q = q.substr(qm + 1);
    std::map<std::string, std::string> kv;
    size_t i = 0;
    while (i <= q.size()) {
        size_t amp = q.find('&', i);
        if (amp == std::string::npos) amp = q.size();
        std::string item = q.substr(i, amp - i);
        size_t eq = item.find('=');
        if (eq != std::string::npos) kv[item.substr(0, eq)] = UrlDecode(item.substr(eq + 1));
        i = amp + 1;
    }
    return kv;
}

// 共有リンクの場面にする（起動時に一度）。読めた項目だけ使う。場面の項目が無ければ false
static bool ApplyShareQuery(App& app, std::string q, UiId msgId = UI_SHARE_OPENED) {
    std::map<std::string, std::string> kv = ParseQuery(q);
    if (kv.count("apollo")) {   // アポロ計画: 天体の一覧ができてから StartPendingVoyage で始める
        app.pendingApollo = kv["apollo"];
        app.pendingScene = kv["scene"];
        if (kv.count("t")) app.pendingVoyageT = atof(kv["t"].c_str());
        return true;
    }
    if (kv.count("voyage")) {   // 探査機の旅: 探査機の一覧ができてから StartPendingVoyage で始める
        app.pendingVoyage = kv["voyage"];
        app.pendingScene = kv["scene"];
        app.pendingLeg = kv.count("leg") ? std::max(1, atoi(kv["leg"].c_str())) : 1;
        if (kv.count("t")) app.pendingVoyageT = atof(kv["t"].c_str());
        return true;
    }
    if (!kv.count("t") && !kv.count("at")) return false;
    if (kv.count("t")) {
        double t = atof(kv["t"].c_str());
        if (InDateRange(t)) app.tDays = t;
        UpdateBodies(app.B, app.tDays);
    }
    if (kv.count("fov")) {
        double f = atof(kv["fov"].c_str());
        if (std::isfinite(f) && f >= 0.05 && f < 60) {
            app.eventZoom = true;
            app.eventZoomFov = f;
        }
    }
    if (kv.count("ts")) app.timeIdx = std::max(0, std::min(TIME_SCALE_COUNT - 1, atoi(kv["ts"].c_str())));
    app.paused = kv["p"] == "1";
    Bookmark bm;
    if (kv.count("at") && ParseBookmark(app, kv["at"], &bm) && app.B[bm.body].active) {
        ApplyBookmark(app, bm);
        char msg[256];
        snprintf(msg, sizeof msg, T(msgId), BookmarkLabel(app, bm).c_str());
        Toast(app, msg);
    }
    for (size_t k = 0; k < app.B.size(); k++)
        if (kv["tg"] == app.B[k].d.en && app.B[k].active) app.target = (int)k;
    return true;
}

// ---------------------------------------------------------------- 地上の場所（検索の「地上」・流星群）
// 地上から空を見る場所（天体, 緯度・経度 deg, 標準時 UTC+時間（夏時間は考えない。NAN は UTC で表示））。
// 選ぶと地表に降り、赤道側（北半球なら南）の空を見上げる。月面では地球が出ていれば地球を見る
struct City { const char* jp; const char* en; double lat, lon; int body; double tz; };
// 番号が決まっていない天体（衛星の一部・太陽系の外の惑星）は、英語名で探す（City::body に負の値で）
static const int CITY_PROXIMA_B = -2, CITY_TITAN = -3;
static const City CITIES[] = {
    {"東京", "Tokyo", 35.68, 139.69, B_EARTH, 9},         {"札幌", "Sapporo", 43.06, 141.35, B_EARTH, 9},
    {"那覇", "Naha", 26.21, 127.68, B_EARTH, 9},          {"北京", "Beijing", 39.90, 116.41, B_EARTH, 8},
    {"シンガポール", "Singapore", 1.35, 103.82, B_EARTH, 8}, {"ムンバイ", "Mumbai", 19.08, 72.88, B_EARTH, 5.5},
    {"カイロ", "Cairo", 30.04, 31.24, B_EARTH, 2},        {"モスクワ", "Moscow", 55.76, 37.62, B_EARTH, 3},
    {"ロンドン", "London", 51.51, -0.13, B_EARTH, 0},      {"パリ", "Paris", 48.86, 2.35, B_EARTH, 1},
    {"レイキャビク", "Reykjavik", 64.15, -21.94, B_EARTH, 0}, {"ニューヨーク", "New York", 40.71, -74.01, B_EARTH, -5},
    {"ロサンゼルス", "Los Angeles", 34.05, -118.24, B_EARTH, -8},
    {"マウナケア山頂（ハワイ）", "Mauna Kea (Hawaii)", 19.82, -155.47, B_EARTH, -10},
    {"アタカマ砂漠（チリ）", "Atacama Desert (Chile)", -24.63, -70.40, B_EARTH, -4},
    {"リオデジャネイロ", "Rio de Janeiro", -22.91, -43.17, B_EARTH, -3}, {"ケープタウン", "Cape Town", -33.92, 18.42, B_EARTH, 2},
    {"シドニー", "Sydney", -33.87, 151.21, B_EARTH, 10},
    {"南極点", "South Pole", -89.99, 0.0, B_EARTH, NAN},  {"北極点", "North Pole", 89.99, 0.0, B_EARTH, NAN},
    // 月面（経度は東が正）
    {"静かの海（アポロ11号の着陸地点）", "Sea of Tranquility (Apollo 11)", 0.674, 23.473, B_MOON, NAN},
    {"タウルス・リットロウ谷（アポロ17号）", "Taurus-Littrow (Apollo 17)", 20.19, 30.77, B_MOON, NAN},
    {"月の南極", "Lunar south pole", -89.9, 0.0, B_MOON, NAN},
    {"フォン・カルマン・クレーター（嫦娥4号、月の裏側）", "Von Karman crater (Chang'e 4, far side)", -45.44, 177.6, B_MOON, NAN},
    // 火星（経度は東が正）
    {"ジェゼロ・クレーター（パーシビアランス）", "Jezero crater (Perseverance)", 18.44, 77.45, B_MARS, NAN},
    {"ゲール・クレーター（キュリオシティ）", "Gale crater (Curiosity)", -4.59, 137.44, B_MARS, NAN},
    {"クリュセ平原（バイキング1号）", "Chryse Planitia (Viking 1)", 22.27, -47.95, B_MARS, NAN},
    // 金星の着陸機の着陸地点（経度は東が正。Wikipedia の List of landings on extraterrestrial bodies による。ベネラ 8〜10 号は
    // 半径 150 km ほどの範囲。ベガ 1 号は記事によって北緯 8.05°・東経 176.89° とも）
    {"ベネラ7号の着陸地点（1970年12月15日、初めての金星着陸）", "Venera 7 landing site (15 Dec 1970, first landing on Venus)", -5.0, -9.0, B_VENUS, NAN},
    {"ベネラ8号の着陸地点（1972年7月22日）", "Venera 8 landing site (22 Jul 1972)", -10.70, -24.75, B_VENUS, NAN},
    {"ベネラ9号の着陸地点（1975年10月22日、初めて地表を撮影）", "Venera 9 landing site (22 Oct 1975, first images of the surface)", 31.01, -68.36, B_VENUS, NAN},
    {"ベネラ10号の着陸地点（1975年10月25日）", "Venera 10 landing site (25 Oct 1975)", 15.42, -68.49, B_VENUS, NAN},
    {"ベネラ11号の着陸地点（1978年12月25日）", "Venera 11 landing site (25 Dec 1978)", -14.0, -61.0, B_VENUS, NAN},
    {"ベネラ12号の着陸地点（1978年12月21日）", "Venera 12 landing site (21 Dec 1978)", -7.0, -66.0, B_VENUS, NAN},
    {"ベネラ13号の着陸地点（1982年3月1日、初めてカラーで撮影）", "Venera 13 landing site (1 Mar 1982, first color images)", -7.5, -57.0, B_VENUS, NAN},
    {"ベネラ14号の着陸地点（1982年3月5日）", "Venera 14 landing site (5 Mar 1982)", -13.25, -50.0, B_VENUS, NAN},
    {"ベガ1号の着陸地点（1985年6月11日）", "Vega 1 landing site (11 Jun 1985)", 7.2, 177.8, B_VENUS, NAN},
    {"ベガ2号の着陸地点（1985年6月15日）", "Vega 2 landing site (15 Jun 1985)", -7.14, 177.67, B_VENUS, NAN},
    // タイタン（経度は東が正。Wikipedia の Huygens (spacecraft) の 10°34′23″S 192°20′06″W）
    {"ホイヘンスの着陸地点（タイタン、2005年1月14日）", "Huygens landing site (Titan, 14 Jan 2005)", -10.573, 167.665, CITY_TITAN, NAN},
    // セルク・クレーター: ドラゴンフライ（2028 年 7 月に打ち上げ、2034 年にタイタンに着く予定の回転翼機）の目的地。
    // 座標は Wikipedia の Selk (crater) の 7°N 199°W。着陸の予定地はこのそばの砂丘の間（資料によって座標が違う）
    {"セルク・クレーター（ドラゴンフライの目的地、2034年到着予定）", "Selk crater (Dragonfly's destination, arriving 2034)", 7.0, 161.0, CITY_TITAN, NAN},
    // プロキシマ・ケンタウリ b（自転が公転と同期していると考え、経度 0° が恒星の真下。経度 70° では恒星が高さ 20° に止まって見える）
    {"プロキシマ・ケンタウリ b（想像図）", "Proxima Centauri b (artist's impression)", 0, 70, CITY_PROXIMA_B, NAN},
};
static int CityBody(const App& app, const City& c) {
    if (c.body >= 0) return c.body;
    const char* en = c.body == CITY_TITAN ? "Titan" : "Proxima Centauri b";
    for (size_t i = 0; i < app.B.size(); i++)
        if (std::strcmp(app.B[i].d.en, en) == 0) return (int)i;
    return B_EARTH;
}
static const int CITY_COUNT = sizeof CITIES / sizeof CITIES[0];
static void GoToCity(App& app, int k, double elDeg = 35);

// ---------------------------------------------------------------- 今いる所の地名（地上のパネル）
// assets/place_names.txt（tools/make_place_names.py）: 地球は都市（市街地の広さの円）、ほかの天体は IAU の地名
// （中心と直径の円と、緯度・経度の範囲。実際の形ではない）
struct PlaceName {
    V3 dir;            // 天体固定座標の向き
    double cosR;       // 中心から縁までの角度の cos
    float rad;         // 半径 km
    float box[4];      // 南端・北端・西端・東端（度）。南端 > 北端なら範囲なし
    int kind;          // 種類（g_placeKinds の番号、-1: 無し）
    std::string name, ja;
};
struct PlaceKind { std::string ja, en; };
static std::vector<PlaceKind> g_placeKinds;
static std::vector<std::vector<PlaceName>> g_placeNames;   // 天体の番号ごと
static bool g_placeNamesLoaded = false;

static V3 LatLonDir(double latDeg, double lonDeg) {
    double la = latDeg * DEG, lo = lonDeg * DEG;
    return V3(std::cos(la) * std::cos(lo), std::sin(la), -std::cos(la) * std::sin(lo));
}

// 天体の英語名の比べ方: 英字だけを小文字で（「52246 Donaldjohanson」と「Donaldjohanson」を同じに）
static std::string PlaceBodyKey(const char* s) {
    std::string k;
    for (; *s; s++)
        if (std::isalpha((unsigned char)*s)) k += (char)std::tolower((unsigned char)*s);
    return k;
}

// 読めたら true（ブラウザ版は、ファイルが届くまで false）
static bool LoadPlaceNames(const App& app) {
    if (g_placeNamesLoaded) return true;
    EmbeddedFile f = PlaceNamesTxt();
    if (!f.data || !f.size) return false;
    g_placeNamesLoaded = true;
    g_placeNames.assign(app.B.size(), {});
    std::map<std::string, int> bodies;
    for (size_t i = 0; i < app.B.size(); i++) bodies.emplace(PlaceBodyKey(app.B[i].d.en), (int)i);
    const char* p = (const char*)f.data;
    const char* end = p + f.size;
    int body = -1;
    while (p < end) {
        const char* e = (const char*)std::memchr(p, '\n', end - p);
        if (!e) e = end;
        std::vector<std::string> col;
        for (const char* a = p;;) {
            const char* t = (const char*)std::memchr(a, '\t', e - a);
            col.emplace_back(a, t ? t : e);
            if (!t) break;
            a = t + 1;
        }
        p = e + 1;
        if (col[0] == "K" && col.size() >= 3) g_placeKinds.push_back({col[1], col[2]});
        else if (col[0] == "B" && col.size() >= 2) {
            auto it = bodies.find(PlaceBodyKey(col[1].c_str()));
            body = it == bodies.end() ? -1 : it->second;
        } else if (body >= 0 && col.size() >= 6 && col[0][0] != '#') {
            PlaceName n;
            double lat = atof(col[0].c_str()), lon = atof(col[1].c_str());
            n.dir = LatLonDir(lat, lon);
            n.rad = (float)atof(col[2].c_str());
            n.cosR = std::cos(std::min<double>(PI, n.rad / app.B[body].d.radius));
            n.box[0] = 1, n.box[1] = 0;
            if (!col[3].empty()) sscanf(col[3].c_str(), "%f,%f,%f,%f", &n.box[0], &n.box[1], &n.box[2], &n.box[3]);
            n.kind = atoi(col[4].c_str());
            if (n.kind >= (int)g_placeKinds.size()) n.kind = -1;
            n.name = col[5];
            if (col.size() >= 7) n.ja = col[6];
            g_placeNames[body].push_back(std::move(n));
        }
    }
    return true;
}

// フォントに入れる文字（地名の日本語・英語に出てくる ASCII 以外の文字を 1 つずつ）
static std::string PlaceNameChars() {
    std::set<int> cps;
    auto add = [&](const std::string& s) {
        for (const char* c = s.c_str(); *c;) {
            int n = 0, cp = GetCodepointNext(c, &n);
            if (cp > 127) cps.insert(cp);
            c += n > 0 ? n : 1;
        }
    };
    for (const PlaceKind& k : g_placeKinds) add(k.ja + k.en);
    for (const auto& v : g_placeNames)
        for (const PlaceName& n : v) add(n.name + n.ja);
    std::string s;
    for (int cp : cps) {
        int len = 0;
        const char* u = CodepointToUTF8(cp, &len);
        s.append(u, len);
    }
    return s;
}

static std::string PlaceLabel(const PlaceName& n) {
    const PlaceKind* k = n.kind >= 0 ? &g_placeKinds[n.kind] : nullptr;
    if (g_jp) return !n.ja.empty() ? n.ja : k && !k->ja.empty() ? n.name + "（" + k->ja + "）" : n.name;
    return k && !k->en.empty() ? n.name + " " + k->en : n.name;
}

// 緯度・経度（度）の所の地名。そこを含む一番小さい地名と、地球以外ではそれを含む 1.5 倍以上大きい地名。無ければ空
static std::string PlaceNameAt(const App& app, int body, double lat, double lon) {
    if (!LoadPlaceNames(app) || body < 0 || body >= (int)g_placeNames.size()) return "";
    V3 d = LatLonDir(lat, lon);
    auto inside = [&](const PlaceName& n) {
        if (Dot(d, n.dir) < n.cosR) return false;
        if (n.box[0] > n.box[1]) return true;
        if (lat < n.box[0] || lat > n.box[1]) return false;
        return n.box[2] <= n.box[3] ? lon >= n.box[2] && lon <= n.box[3] : lon >= n.box[2] || lon <= n.box[3];
    };
    const PlaceName *a = nullptr, *b = nullptr;
    for (const PlaceName& n : g_placeNames[body])
        if ((!a || n.rad < a->rad) && inside(n)) a = &n;
    if (!a) return "";
    if (body != B_EARTH)
        for (const PlaceName& n : g_placeNames[body])
            if (n.rad >= 1.5f * a->rad && (!b || n.rad < b->rad) && inside(n)) b = &n;
    return b ? PlaceLabel(*a) + "  ·  " + PlaceLabel(*b) : PlaceLabel(*a);
}
