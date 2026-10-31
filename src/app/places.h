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
    bm->pos = V3(a[0], a[1], a[2]);
    bm->fwd = V3(a[3], a[4], a[5]);
    bm->up = V3(a[6], a[7], a[8]);
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
    double alt = r - SurfaceRadius(b, b.ex * dir.x + b.pole * dir.y + b.ez * dir.z);  // 楕円体は自転に依らない
    char buf[160];
    snprintf(buf, sizeof buf, "%s  %s %.1f°  %s %.1f°  %s", TR(b.d.jp, b.d.en), dir.y >= 0 ? "N" : "S",
             std::fabs(std::asin(std::max(-1.0, std::min(1.0, dir.y))) / DEG), std::atan2(-dir.z, dir.x) >= 0 ? "E" : "W",
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
    return q;
}

static void ShareScene(App& app) {
    std::string q = ShareQuery(app);
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

// 共有リンクの場面にする（起動時に一度）。読めた項目だけ使う
static bool ApplyShareQuery(App& app, std::string q, UiId msgId = UI_SHARE_OPENED) {
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
    if (!kv.count("t") && !kv.count("at")) return false;
    if (kv.count("t")) {
        double t = atof(kv["t"].c_str());
        if (std::isfinite(t) && std::fabs(t) < 365.25 * 400) app.tDays = t;
        UpdateBodies(app.B, app.tDays);
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
// 太陽系の外の惑星は番号が決まっていないので、英語名で探す（City::body に負の値で）
static const int CITY_PROXIMA_B = -2;
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
    // プロキシマ・ケンタウリ b（自転が公転と同期していると考え、経度 0° が恒星の真下。経度 70° では恒星が高さ 20° に止まって見える）
    {"プロキシマ・ケンタウリ b（想像図）", "Proxima Centauri b (artist's impression)", 0, 70, CITY_PROXIMA_B, NAN},
};
static int CityBody(const App& app, const City& c) {
    if (c.body >= 0) return c.body;
    for (size_t i = 0; i < app.B.size(); i++)
        if (std::strcmp(app.B[i].d.en, "Proxima Centauri b") == 0) return (int)i;
    return B_EARTH;
}
static const int CITY_COUNT = sizeof CITIES / sizeof CITIES[0];
static void GoToCity(App& app, int k, double elDeg = 35);
