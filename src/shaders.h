// Solar System Voyager — Copyright (c) 2026 hubstacksea
// MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
// 天体描画用シェーダ (GLSL 330)。テクスチャ無しで手続き的に模様を生成する。
//  - 近づくほど細かいオクターブを足す（画素より小さい模様は打ち切る）ので、地表付近でも模様が潰れない
//  - 地形の高さの勾配を解析的に求めて法線を揺らし、山やクレーターの陰影を付ける
//  - 天体ごとの特徴（月の海、火星の極冠、木星の大赤斑、地球の大陸・雲・夜景など）を detail.x で描き分ける
#pragma once

// 頂点・フラグメントシェーダで共通の部分（隣り合う文字列リテラルとしてシェーダの文字列につなぐ）
#define NOISE_GLSL R"(
#define MAP_STYLE int(mapInfo2.x + 0.5)   // 地図の表面模様の種類
// ---- 整数ハッシュ（座標が大きくても劣化しないので、地表付近の細かいオクターブでも使える）
uint hashu(ivec3 c) {
    uvec3 v = uvec3(c) * uvec3(1597334673u, 3812015801u, 2798796415u);
    uint n = (v.x ^ v.y ^ v.z) * 1597334673u;
    n ^= n >> 15; n *= 2246822519u; n ^= n >> 13; n *= 3266489917u; n ^= n >> 16;
    return n;
}
float hash1(ivec3 c) { return float(hashu(c) >> 8u) * (1.0 / 16777216.0); }

float vnoise(vec3 x) {
    vec3 fl = floor(x);
    ivec3 i = ivec3(fl);
    vec3 f = x - fl;
    vec3 u = f * f * (3.0 - 2.0 * f);
    float a = hash1(i), b = hash1(i + ivec3(1, 0, 0)), c = hash1(i + ivec3(0, 1, 0)), d = hash1(i + ivec3(1, 1, 0));
    float e = hash1(i + ivec3(0, 0, 1)), g = hash1(i + ivec3(1, 0, 1)), h = hash1(i + ivec3(0, 1, 1)), k = hash1(i + ivec3(1, 1, 1));
    return mix(mix(mix(a, b, u.x), mix(c, d, u.x), u.y), mix(mix(e, g, u.x), mix(h, k, u.x), u.y), u.z);
}

// 値ノイズと解析的な勾配 (x: 値 0..1, yzw: 勾配)
vec4 noised(vec3 x) {
    vec3 fl = floor(x);
    ivec3 i = ivec3(fl);
    vec3 w = x - fl;
    vec3 u = w * w * (3.0 - 2.0 * w);
    vec3 du = 6.0 * w * (1.0 - w);
    float a = hash1(i), b = hash1(i + ivec3(1, 0, 0)), c = hash1(i + ivec3(0, 1, 0)), d = hash1(i + ivec3(1, 1, 0));
    float e = hash1(i + ivec3(0, 0, 1)), f = hash1(i + ivec3(1, 0, 1)), g = hash1(i + ivec3(0, 1, 1)), h = hash1(i + ivec3(1, 1, 1));
    float k1 = b - a, k2 = c - a, k3 = e - a;
    float k4 = a - b - c + d, k5 = a - c - e + g, k6 = a - b - e + f;
    float k7 = -a + b + c - d + e - f - g + h;
    float v = a + k1 * u.x + k2 * u.y + k3 * u.z + k4 * u.x * u.y + k5 * u.y * u.z + k6 * u.z * u.x + k7 * u.x * u.y * u.z;
    vec3 dv = du * vec3(k1 + k4 * u.y + k6 * u.z + k7 * u.y * u.z,
                        k2 + k5 * u.z + k4 * u.x + k7 * u.z * u.x,
                        k3 + k6 * u.x + k5 * u.y + k7 * u.x * u.y);
    return vec4(v, dv);
}

// 3D 格子のセルに 1 つずつ置く円形の模様（クレーター・火山・黒点）。無ければ false
bool cellSpot(vec3 p, float F, float density, vec3 salt, float rmin, float rmax, out vec3 d, out float r, out float rnd) {
    vec3 q = p * F + salt;
    vec3 fl = floor(q);
    ivec3 c = ivec3(fl);
    vec3 f = q - fl;
    d = vec3(0.0); r = 1.0; rnd = 0.0;
    if (hash1(c + ivec3(5, 71, 29)) > density) return false;
    float sz = hash1(c + ivec3(41, 13, 97));
    r = mix(rmin, rmax, sz * sz);
    vec3 ctr = vec3(r * 1.7) + vec3(hash1(c + ivec3(1, 2, 3)), hash1(c + ivec3(4, 5, 6)), hash1(c + ivec3(7, 8, 9))) * (1.0 - 3.4 * r);
    d = f - ctr;
    rnd = hash1(c + ivec3(11, 12, 13));
    return true;
}

)"

// 標高データより細かい手続きの起伏の高さ（地形の格子の頂点用。値はフラグメントシェーダの陰影の起伏と同じ式の、周波数 fLo より
// 細かい部分）。sp: 格子点の間隔（単位球の長さ）。それより細かいものは省く。bodies.h の CPU 版（BodyBumps）と同じ計算
#define BUMPS_GLSL R"(
float bandW(float f, float sp, float fLo) { return (1.0 - smoothstep(0.15, 0.5, f * sp)) * smoothstep(0.5, 1.0, f / fLo); }
// fbm（オクターブはすべて）
float fbm0(vec3 p, float f, int oct, float gain, vec3 o) {
    float s = 0.0, a = 1.0;
    for (int i = 0; i < oct; i++) {
        s += a * (vnoise(p * f + o + float(i) * 7.13) * 2.0 - 1.0);
        a *= gain; f *= 2.03;
    }
    return s;
}
float fbmBand(vec3 p, float f, int oct, float gain, vec3 o, float sp, float fLo) {
    float s = 0.0, a = 1.0;
    for (int i = 0; i < oct && f * sp < 0.5; i++) {
        float w = bandW(f, sp, fLo);
        if (w > 0.0) s += a * w * (vnoise(p * f + o + float(i) * 7.13) * 2.0 - 1.0);
        a *= gain; f *= 2.03;
    }
    return s;
}
// クレーターの断面: x（中心からの距離 / 半径）の高さと傾き dh（深さ 1 あたり）。s: 新しさ。1 は新しいお椀形（深さ / 直径
// 約 0.2）。小さいほど崩れて浅く（0 で 0.3 倍）、底は埋まって平らに、縁は低くなる（火星: 風で運ばれた塵・砂が積もり、削られる）
float craterShape(float x, float s, out float dh) {
    float k = mix(0.3, 1.0, s), rim = 0.25 * mix(0.3, 1.0, s), fl = (1.0 - s) * 0.55;
    if (x < 1.0) { dh = x * x > fl ? 2.0 * x * k : 0.0; return (max(x * x, fl) - 1.0 + rim) * k; }
    float t = (x - 1.0) * 3.5, e = exp(-t * t);
    dh = -rim * 7.0 * t * e * k;
    return rim * e * k;
}
// クレーターの新しさ: worn = 0 はすべて新しい。worn = 1（火星）はほとんどが崩れていて、新しいものは 4 分の 1 ほど（rnd^2 > 0.56 は
// rnd > 0.75 で、光条のある新しいクレーターと同じもの）
float craterFresh(float rnd, float worn) { return mix(1.0, rnd * rnd, worn); }
// クレーターの多さに層ごと（小さい層へ）に掛ける値: worn = 1 では小さいものほど埋まって消え、0.8 倍ずつ（目安）
float craterLayerK(float worn) { return mix(1.0, 0.8, worn); }
// 崩れたクレーターの天体: 火星（塵・砂）と、カリスト（氷が昇華して縁が崩れ、直径 1 km より小さいクレーターが少ない）
float craterWorn(int style) { return style == 3 || style == 7 ? 1.0 : 0.0; }
// 冥王星のスプートニク平原（中心 北緯 20°・東経 180°、南北 約 1050 km・東西 約 800 km の窒素の氷の盆地）の割合。
// 縁は場所で ±7% ほど揺らす
float plutoSP(vec3 p, vec3 o) {
    vec3 n = normalize(p);
    float la = degrees(asin(clamp(n.y, -1.0, 1.0))) - 20.0, lo = mod(degrees(atan(-n.z, n.x)) + 360.0, 360.0) - 180.0;
    float e = (la / 26.0) * (la / 26.0) + (lo / 20.0) * (lo / 20.0) + (vnoise(n * 12.0 + o + 5.0) - 0.5) * 0.3;
    return 1.0 - smoothstep(0.85, 1.0, e);
}
// 地図のある天体の、地図より細かいクレーターの多さに掛ける値。表面が若いほど少ない（値はクレーターの数からの目安）:
// イオは火山の噴出物で埋まり 0、エウロパ（年代 約 6000 万年）・タイタン（雨・風で削られる）・トリトン（約 1 億年）は
// 月の高地の数 % ほど、エンケラドゥスの南極地域（南緯 55° より南）とスプートニク平原は 0
float mapCraterK(int style, vec3 p, vec3 o) {
    if (style == 4) return 0.0;
    if (style == 5) return 0.03;
    if (style == 25) return 0.02;
    if (style == 12) return 0.08;
    if (style == 11) return smoothstep(-0.87, -0.77, normalize(p).y);
    if (style == 10) return 1.0 - plutoSP(p, o);
    return 1.0;
}
float cratersBand(vec3 p, float density, float F0, vec3 salt, float sp, float fLo, float worn) {
    float s = 0.0, F = F0, dk = density;
    for (int i = 0; i < 16 && F * 1.7 * sp < 0.5; i++, F *= 2.2, dk *= craterLayerK(worn)) {
        vec3 d; float r, rnd;
        if (!cellSpot(p, F, dk, salt + float(i) * 31.7, 0.08, 0.29, d, r, rnd)) continue;
        float x = length(d) / r;
        if (x >= 1.7) continue;
        float dh, h = craterShape(x, craterFresh(rnd, worn), dh);
        s += bandW(F / (2.0 * r), sp, fLo) * h * 0.4 * r / F;
    }
    return s;
}
// 火星の崖（細かい標高の傾き s（tan）が大きい所。マリネリス渓谷の壁・クレーターの内壁など）の、データより細かい形（手続き。
// 値はどれも HiRISE などの画像の見た目からの目安）:
//  - 地層の段: 標高 STRATA_M ごとに、3 割が平らな棚、7 割が急な崖（傾きは最大でデータの 2.1 倍）。層の高さは場所で ±90 m ほど波打つ
//  - 尾根と溝: 鋭い尾根の並ぶノイズ（振幅は傾きに比例し、傾き 0.5 で最初のオクターブ ±570 m ほど）
// 崖の割合は傾き 7°〜19° で 0 から 1 へ
const float STRATA_M = 250.0;
float cliffWall(float s) { return smoothstep(0.12, 0.35, s); }
float cliffRidgeA(float s) { return cliffWall(s) * 0.8 * min(s, 0.8) / 1200.0; }   // 尾根と溝の振幅（単位球の長さ）
// 点 p（単位球）・標高 hm m の地層の番号（小数）
float strataT(float hm, vec3 p, vec3 o) { return hm / STRATA_M + (vnoise(p * 300.0 + o + 91.0) - 0.5) * 0.7; }
// 段にした地層の番号と、傾きの倍率 dd
float strataStep(float t, out float dd) {
    float i = floor(t), x = clamp((t - i - 0.3) / 0.7, 0.0, 1.0);
    dd = 6.0 * x * (1.0 - x) / 0.7;
    return i + x * x * (3.0 - 2.0 * x);
}
// 段の強さ: 崖の割合。間隔 spM m より細かい段（急な崖（傾き 0.8）での幅 STRATA_M / 0.8）は消す（傾きは遠くでは画素の
// 大きさでならされて小さく出るので、傾きによらず消す）
float strataK(float s, float spM) { return cliffWall(s) * (1.0 - smoothstep(0.15, 0.5, spM * 0.8 / STRATA_M)); }
// 尾根の鋭いノイズ（値 −0.5..0.5）の、周波数 fLo より細かい部分
float ridgeBand(vec3 p, float f, int oct, float gain, vec3 o, float sp, float fLo) {
    float s = 0.0, a = 1.0;
    for (int i = 0; i < oct && f * sp < 0.5; i++) {
        float w = bandW(f, sp, fLo);
        if (w > 0.0) s += a * w * (0.5 - abs(vnoise(p * f + o + float(i) * 7.13) * 2.0 - 1.0));
        a *= gain; f *= 2.03;
    }
    return s;
}
// 岩石・氷天体の手続きの表面の値: 起伏の強さ・クレーターの多さ・月と水星の海・冥王星のハート
void rockyParams(vec3 p, int style, vec3 o, out float rough, out float dens, out float mare, out float heart) {
    rough = 1.0; dens = 0.55; mare = 0.0; heart = 0.0;
    if (style == 1 || style == 2) {   // 月・水星: 海（平らで暗い溶岩平原）
        mare = smoothstep(0.05, 0.25, fbm0(p, 1.3, 4, 0.55, o + 9.0));
        if (style == 2) mare *= 0.45;
        dens = mix(0.6, 0.22, mare);
        rough = mix(1.0, 0.3, mare);
    }
    else if (style == 3) { dens = 0.35; rough = 1.3; }   // 火星
    else if (style == 4) { dens = 0.0; rough = 0.6; }    // イオ: 火山活動でクレーターが無い
    else if (style == 5) { dens = 0.06; rough = 0.35; }  // エウロパ: 若い氷の表面
    else if (style == 6) { dens = 0.4; }
    else if (style == 7) { dens = 0.85; }                // カリスト: クレーターだらけ
    else if (style == 10) { dens = 0.3; }
    else if (style == 12) { dens = 0.12; rough = 0.7; }
    else if (style == 24) { dens = 0.02; rough = 0.8; }  // 金星: 若い溶岩平原でクレーターがほとんど無い
    else if (style == 25) { dens = 0.02; rough = 0.5; }  // タイタン: 雨と風で削られ、クレーターがほとんど無い
    if (style == 10) {   // 冥王星のハート（スプートニク平原）は平らでクレーターが無い
        vec3 h0 = normalize(vec3(0.82, 0.30, 0.48)), h1 = normalize(vec3(0.70, 0.12, 0.70));
        float edge = fbm0(p, 9.0, 4, 0.5, o + 21.0) * 0.012;
        heart = max(smoothstep(0.955, 0.965, dot(p, h0) + edge), smoothstep(0.935, 0.945, dot(p, h1) + edge));
        rough *= 1.0 - 0.85 * heart;
    }
}
float rockyBand(vec3 p, int style, vec3 o, float sp, float fLo) {
    float rough, dens, mare, heart;
    rockyParams(p, style, o, rough, dens, mare, heart);
    float T = fbmBand(p, 3.0, 26, 0.48, o, sp, fLo) * (0.022 * rough);
    return T + (dens > 0.0 ? cratersBand(p, dens * (1.0 - heart), 5.0, o * 0.37, sp, fLo, 0.0) : 0.0);
}
// 月の海（暗い所）の割合。c は地表の色
float moonDark(vec3 c) { return 1.0 - smoothstep(0.12, 0.3, dot(pow(c, vec3(1.3)) * 1.25, vec3(0.333))); }
)"

// 細かい標高（地球 1 分角・月 16 ピクセル/度・火星 32 ピクセル/度）。CPU がカメラの真下を中心に展開した窓（texture4。bodies.h の DetailMap と
// 同じ計算）
#define DETAIL_GLSL R"(
uniform sampler2D texture4;
uniform vec4 detA;   // xy: 窓の左上（全球の 1 分角の画素）, z: 窓の一辺の画素数, w: 縁でなじませる幅（画素）
uniform vec4 detB;   // xy: 全球の画素数, z: 1 で窓あり, w: 値の形式（1 地球の高さコード, 2 上位・下位バイト 0.5 m 単位で −10000 m から）
const float EARTH_RM = 6378137.0;
// 窓の中の位置 l（画素）と重み（窓の外は 0、縁から detA.w 画素で 1）。uv は全球の画像の座標
float detWeight(vec2 uv, out vec2 l) {
    l = vec2(0.0);
    if (detB.z < 0.5) return 0.0;
    l = uv * detB.xy - 0.5 - detA.xy;
    l.x -= detB.x * floor(l.x / detB.x + 0.5);
    vec2 e = min(l, detA.z - 1.0 - l);
    return clamp(min(e.x, e.y) / detA.w, 0.0, 1.0);
}
// 窓の画像の値 → m（地球は陸 128..255 = 0..8900 m、海は海面）
float detDecode(vec4 t) {
    if (detB.w > 1.5) return (t.r * 255.0 * 256.0 + t.a * 255.0) * 0.5 - 10000.0;
    float v = t.r * 255.0, s = (v - 128.0) / 127.0;
    return v >= 127.5 ? s * s * 8900.0 : 0.0;
}
// 窓の画素 l の標高 m（4 画素の双線形。CPU と同じ）
float detHeight(vec2 l) {
    ivec2 i = ivec2(floor(l)), n = ivec2(int(detA.z) - 1);
    vec2 f = l - floor(l);
    ivec2 a = clamp(i, ivec2(0), n), b = clamp(i + 1, ivec2(0), n);
    float h00 = detDecode(texelFetch(texture4, a, 0)), h10 = detDecode(texelFetch(texture4, ivec2(b.x, a.y), 0));
    float h01 = detDecode(texelFetch(texture4, ivec2(a.x, b.y), 0)), h11 = detDecode(texelFetch(texture4, b, 0));
    return mix(mix(h00, h10, f.x), mix(h01, h11, f.x), f.y);
}
// 窓の画素 l の標高の傾き（tan。両隣の画素の差。CPU の DetailMap::Slope と同じ）。lat: 緯度 rad、R: 半径 m
float detSlope(vec2 l, float lat, float R) {
    float dy = 3.1415927 * R / detB.y, dx = 6.2831853 * R / detB.x * max(cos(lat), 0.05);
    float gx = (detHeight(l + vec2(1.0, 0.0)) - detHeight(l - vec2(1.0, 0.0))) / (2.0 * dx);
    float gy = (detHeight(l + vec2(0.0, 1.0)) - detHeight(l - vec2(0.0, 1.0))) / (2.0 * dy);
    return length(vec2(gx, gy));
}
// 地域だけのもっと細かい標高（火星のオリンポス山など。texture5。bodies.h の DetailMap の boxes と同じ計算）。細かい標高の窓の
// 中にあり、その上に重ねる。値の形式は 0.5 m 単位で −10000 m から
uniform sampler2D texture5;
uniform vec4 detC;   // xy: 窓の左上（全球の画素）, z: 窓の一辺の画素数, w: 縁でなじませる幅（画素）
uniform vec4 detD;   // xy: 全球の画素数, z: 1 で窓あり
uniform vec4 detE;   // 地域の長方形（全球の画素）: xy 左上, zw 右下
// 窓の中の位置 l（画素）と重み（窓と地域の外は 0、縁から detC.w 画素で 1）
float det2Weight(vec2 uv, out vec2 l) {
    l = vec2(0.0);
    if (detD.z < 0.5) return 0.0;
    vec2 g = uv * detD.xy - 0.5;
    l = g - detC.xy;
    l.x -= detD.x * floor(l.x / detD.x + 0.5);
    vec2 b = g - detE.xy;
    b.x -= detD.x * floor(b.x / detD.x + 0.5);
    vec2 e = min(min(l, detC.z - 1.0 - l), min(b, detE.zw - detE.xy - 1.0 - b));
    return clamp(min(e.x, e.y) / detC.w, 0.0, 1.0);
}
float det2Decode(vec4 t) { return (t.r * 255.0 * 256.0 + t.a * 255.0) * 0.5 - 10000.0; }
float det2Height(vec2 l) {
    ivec2 i = ivec2(floor(l)), n = ivec2(int(detC.z) - 1);
    vec2 f = l - floor(l);
    ivec2 a = clamp(i, ivec2(0), n), b = clamp(i + 1, ivec2(0), n);
    float h00 = det2Decode(texelFetch(texture5, a, 0)), h10 = det2Decode(texelFetch(texture5, ivec2(b.x, a.y), 0));
    float h01 = det2Decode(texelFetch(texture5, ivec2(a.x, b.y), 0)), h11 = det2Decode(texelFetch(texture5, b, 0));
    return mix(mix(h00, h10, f.x), mix(h01, h11, f.x), f.y);
}
float det2Slope(vec2 l, float lat, float R) {
    float dy = 3.1415927 * R / detD.y, dx = 6.2831853 * R / detD.x * max(cos(lat), 0.05);
    float gx = (det2Height(l + vec2(1.0, 0.0)) - det2Height(l - vec2(1.0, 0.0))) / (2.0 * dx);
    float gy = (det2Height(l + vec2(0.0, 1.0)) - det2Height(l - vec2(0.0, 1.0))) / (2.0 * dy);
    return length(vec2(gx, gy));
}
// 標高 h（m）に、地域だけのもっと細かい標高を重みで混ぜる。s: 間隔（単位球の長さ。それに合わせた縮小画像から引く）
float det2Mix(vec2 uv, float s, float h, out float w2, out vec2 l2) {
    w2 = det2Weight(uv, l2);
    if (w2 <= 0.0) return h;
    float lod = log2(max(s * detD.y / 3.1415927, 1.0));
    return mix(h, lod > 0.0 ? det2Decode(textureLod(texture5, (l2 + 0.5) / detC.z, lod)) : det2Height(l2), w2);
}
// 1 分角より細かい山肌（手続き。地形の格子の高さと陰影の両方に使う）。x: 高さ（単位球の長さ）, yzw: 勾配。
// 振幅は標高の傾き s（tan）で決め、急な山ほど険しく、平野は穏やかにする。h: 標高 m（海面の近くでは 0 へ）。
// sp: 頂点・画素の間隔（単位球の長さ）。それより細かいオクターブは省く。fLo: 標高データにある周波数（地域だけのもっと
// 細かい標高の中では、それより粗いオクターブは実際の地形と重なるので省く。bandW）
vec4 earthBumps(vec3 p, float s, float h, float sp, float fLo, int oct, vec3 o) {
    float a = (0.003 + 0.6 * min(s, 1.0)) / 3200.0 * clamp(h / 50.0, 0.0, 1.0), f = 3200.0;
    vec4 r = vec4(0.0);
    for (int i = 0; i < oct; i++) {
        if (f * sp >= 0.5 || a <= 0.0) break;
        float w = bandW(f, sp, fLo);
        if (w > 0.0) {
            vec4 n = noised(p * f + o + float(i) * 7.13);
            r += a * w * vec4(n.x * 2.0 - 1.0, n.yzw * 2.0 * f);
        }
        a *= 0.5; f *= 2.03;
    }
    return r;
}
)"

static const char* PLANET_VS = R"(
#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform mat4 matView;
uniform mat4 matProjection;
uniform float relBeta;   // 光速に近い時の見え方の β（0 なら何もしない）
uniform vec3 relDir;     // 進む向き（カメラからの相対座標）
// 地表近くの地形の格子（terrA.x > 0 の時）: 頂点は (u, φ)。カメラの真下の向き c を中心に、c からの角度 θ の輪
// （輪ごとの θ は terrRing。CPU で決める）を φ の向きに並べ、標高の分だけ持ち上げる。座標は赤道半径を 1 とし、原点は真下の
// 楕円体の上の点（カメラの近くでも桁落ちしないよう、そこからの差を求める）
uniform vec4 terrA;      // x: 1 で格子, y: θa, z: lnK（輪の θ の基本の並び θa (exp(u lnK) − 1)。シェーダでは使わない）,
                         // w: 高さの形式（1 地球の高さコード, 2 上位・下位バイト 0.5 m 単位, 3 上位・下位バイト 1 m 単位）
uniform vec4 terrRing[61];   // 輪 i の θ（4 つずつ詰める）
uniform vec4 terrStep[61];   // 輪 i の 1 周の頂点の間引き（2 の累乗。その数ずつの頂点を同じ位置に重ねる）
uniform vec4 terrB;      // xyz: 真下の向き c（本体の座標系の球面の点）, w: 極半径 / 赤道半径
uniform vec4 terrC;      // xyz: c に垂直な向き, w: 1 m あたりの長さ（1 / 赤道半径 m）
uniform vec4 terrD;      // x, y: 標高の画像の幅と高さ, z: 輪の数, w: 1 周の頂点の数
uniform vec4 terrE;      // データより細かい手続きの起伏: x 半径 m, y 全球の標高の周波数, z 種類（0 なし, 1 月, 2 地図のある天体）,
                         // w 細かい標高（窓）の周波数
uniform sampler2D texture0;  // 月: 地表の色（海はクレーターが少ない）
uniform sampler2D texture1;
uniform sampler2D texture2;  // 地図のある天体: 撮影された範囲（R）と液体の湖・海（A）
uniform vec4 params;     // z: 天体ごとのシード（フラグメントシェーダと同じ）
uniform vec4 mapInfo;    // y: 地図の種類（2 模様だけ）
uniform vec4 mapInfo2;   // x: 撮影されていない所の表面模様の種類, z: 1 なら撮影範囲の画像のアルファが液体
uniform float mapLatK;
out vec3 fragObj;
out vec3 fragNormal;
out vec3 fragWorld;
out float fragAlt;   // 地形の高さ km（金星のもやの計算に使う。地形でなければ 0）
out float fragShade; // 地形の影: 日なたの割合（地形の格子の頂点で求めて補間する。地形でなければ 1）
uniform vec3 lightDirObj;  // 本体（自転込み）の座標系での太陽方向
uniform float sunAng;      // 太陽の視半径 rad

// 地図（正距円筒図法）を引く向き: 球のメッシュの点 p（楕円体では y を縮める前）の y に 1 + mapLatK を掛ける（地球の地図は
// 測地緯度、ほかの天体は惑星中心緯度。CPU の MapLatScale）
vec3 mapDir(vec3 p) { return normalize(vec3(p.x, p.y * (1.0 + mapLatK), p.z)); }

float terrDecode(vec4 t) {
    if (terrA.w < 1.5) {   // 陸 128..255 = 0..8900 m（平方根で詰めてある）。海は海面（0）
        float v = t.r * 255.0, s = (v - 128.0) / 127.0;
        return v >= 127.5 ? s * s * 8900.0 : 0.0;
    }
    float v = t.r * 255.0 * 256.0 + t.a * 255.0;
    return terrA.w > 2.5 ? v - 32768.0 : v * 0.5 - 10000.0;
}
// 標高 m。頂点の間隔が画素より細かい所は CPU 側と同じく 4 画素の値から双線形に補間し、粗い所は縮小した画像を使う
float terrHeight(vec3 d, float lod) {
    float lat = asin(clamp(mapDir(d).y, -1.0, 1.0)), lon = atan(-d.z, d.x);
    vec2 uv = vec2(lon / 6.2831853 + 0.5, 0.5 - lat / 3.1415927);
    if (lod > 0.0) return terrDecode(textureLod(texture1, uv, lod));
    vec2 xy = uv * terrD.xy - 0.5, f = fract(xy);
    ivec2 i0 = ivec2(floor(xy)), wh = ivec2(terrD.xy);
    int x0 = i0.x < 0 ? i0.x + wh.x : (i0.x >= wh.x ? i0.x - wh.x : i0.x), x1 = x0 + 1 >= wh.x ? 0 : x0 + 1;
    int y0 = clamp(i0.y, 0, wh.y - 1), y1 = clamp(i0.y + 1, 0, wh.y - 1);
    float h00 = terrDecode(texelFetch(texture1, ivec2(x0, y0), 0)), h10 = terrDecode(texelFetch(texture1, ivec2(x1, y0), 0));
    float h01 = terrDecode(texelFetch(texture1, ivec2(x0, y1), 0)), h11 = terrDecode(texelFetch(texture1, ivec2(x1, y1), 0));
    return mix(mix(h00, h10, f.x), mix(h01, h11, f.x), f.y);
}
)" NOISE_GLSL BUMPS_GLSL DETAIL_GLSL R"(
// 火星の崖（頂点ごとに bodyHeight で決める）: 崖の割合（cliffWall。急な斜面ではクレーターが崩れて残らない）と尾根と溝の振幅（cliffRidgeA）
float g_cliff = 0.0, g_wall = 0.0;
float g_craterK = 1.0;   // 地図のある天体のクレーターの多さに掛ける値（mapCraterK。頂点ごとに bodyHeight で決める）
// データより細かい手続きの起伏（単位球の長さ。地球は山肌の振幅を掛ける前の値）を、点 q で、L より細かいものを省いて求める
float bumpAt(vec3 q, float L, float fLo, vec3 o) {
    if (terrA.w < 1.5) {   // 地球の山肌（earthBumps の値を振幅で割ったもの）
        float r = 0.0, a = 1.0, f = 3200.0;
        for (int i = 0; i < 7; i++) {
            if (f * L >= 0.5) break;
            float w = bandW(f, L, fLo);
            if (w > 0.0) r += a * w * (vnoise(q * f + o + float(i) * 7.13) * 2.0 - 1.0);
            a *= 0.5; f *= 2.03;
        }
        return r;
    }
    vec3 n = mapDir(q);
    vec2 uv = vec2(atan(-n.z, n.x) / 6.2831853 + 0.5, 0.5 - asin(clamp(n.y, -1.0, 1.0)) / 3.1415927);
    float h;
    if (terrE.z < 1.5) {   // 月: 海（暗い所）はクレーターが少なく平ら
        float dark = moonDark(textureLod(texture0, uv, 0.0).rgb);
        h = cratersBand(q, mix(0.55, 0.2, dark), 260.0, o * 0.37, L, fLo, 0.0) +
            fbmBand(q, 600.0, 20, 0.48, o, L, fLo) * (mix(0.004, 0.0015, dark) / 260.0);
    } else {               // 地図のある天体: 撮影された所は細かいクレーターと山肌、撮影されていない所は手続きの表面
        vec4 m = textureLod(texture2, uv, 0.0);
        float fine = 0.0, ph = 0.0;
        int st = MAP_STYLE;
        if (m.r > 0.0) fine = cratersBand(q, 0.35 * g_craterK, 260.0, o * 0.37, L, fLo, craterWorn(st)) * (1.0 - g_cliff) +
                              fbmBand(q, 600.0, 20, 0.48, o, L, fLo) * (0.003 / 260.0) +
                              (g_wall > 0.0 ? g_wall * ridgeBand(q, 1200.0, 7, 0.5, o + 43.0, L, fLo) : 0.0);
        if (m.r < 1.0 || mapInfo.y > 1.5) ph = rockyBand(q, MAP_STYLE, o, L, fLo);
        h = mix(ph, fine, m.r);
        if (mapInfo2.z > 0.5) h *= 1.0 - m.a;   // 液体の湖・海は平ら
    }
    return h;
}
// 地形の格子の頂点の手続きの起伏。頂点はカメラと一緒に地表の上を滑るので、頂点の位置で直接求めると細かい起伏を拾ったり
// 外したりして、飛ぶと縁が波打つ。標高の縮小画像と同じく、地表に固定した格子点（間隔は一番細かい Lmin の 2 の累乗倍で、
// 頂点の間隔の LAT_K 倍以上）で求めて補間し、隣の段となめらかにつなぐ。sp: 頂点の間隔、Lmin: 20 m（CPU と同じ）
const float LAT_K = 1.0;
// 格子点（3 次元の格子、間隔 L）の値を 3 重線形に補間する。bumpAt を呼ぶ所は 1 つだけにする（8 点 × 2 段を別々に書くと、
// ブラウザ（Windows の Chrome は WebGL を Direct3D に変換する）でシェーダの変換が終わらず起動しなくなった）
float vertexBumps(vec3 d, float sp, float Lmin, float fLo, vec3 o) {
    float lv = log2(max(LAT_K * sp / Lmin, 1.0)), n = floor(lv), t = lv - n;
    int levels = t < 0.001 ? 1 : 2;
    float r = 0.0;
    for (int k = 0; k < 16; k++) {
        int lvl = k >> 3;
        if (lvl >= levels) break;
        float L = Lmin * exp2(n + float(lvl));
        vec3 g = d / L, i = floor(g), f = g - i;
        vec3 c = vec3(float(k & 1), float((k >> 1) & 1), float((k >> 2) & 1));
        vec3 w3 = mix(1.0 - f, f, c);
        float w = w3.x * w3.y * w3.z * (lvl == 0 ? 1.0 - (levels == 2 ? t : 0.0) : t);
        r += w * bumpAt((i + c) * L, L, fLo, o);
    }
    return r;
}
// 地球: カメラの周り（1 分角の窓の中）は細かい標高と、それより細かい山肌を足した高さ m。sp は頂点の間隔、hm は全球の標高
float earthDetailHeight(vec3 d, float sp, float hm) {
    float lat = asin(clamp(mapDir(d).y, -1.0, 1.0)), lon = atan(-d.z, d.x);
    vec2 l;
    float w = detWeight(vec2(lon / 6.2831853 + 0.5, 0.5 - lat / 3.1415927), l);
    if (w <= 0.0) return hm;
    float lod = log2(max(sp * detB.y / 3.1415927, 1.0));
    float hd = lod > 0.0 ? detDecode(textureLod(texture4, (l + 0.5) / detA.z, lod)) : detHeight(l), w2;
    vec2 l2;
    float hb = det2Mix(vec2(lon / 6.2831853 + 0.5, 0.5 - lat / 3.1415927), sp, mix(hm, hd, w), w2, l2);
    if (3200.0 * LAT_K * sp >= 0.5) return hb;   // 山肌の一番粗いオクターブも格子点の間隔より細かい
    vec3 o = vec3(params.z, params.z * 1.37, params.z * 0.71);
    float sl = mix(detSlope(l, lat, EARTH_RM), w2 > 0.0 ? det2Slope(l2, lat, EARTH_RM) : 0.0, w2);
    float a = (0.003 + 0.6 * min(sl, 1.0)) / 3200.0 * clamp(hb / 50.0, 0.0, 1.0);   // earthBumps の振幅
    float fLo = mix(detB.y, detD.y, w2) / 6.2831853;
    return max(0.0, hb + a * vertexBumps(d, sp, 20.0 / EARTH_RM, fLo, o) * EARTH_RM * w);
}
// 地球以外: カメラの周り（月・火星の窓の中）は細かい標高。データより細かい手続きの起伏（クレーター・山肌）を足した高さ m
float bodyHeight(vec3 d, float sp, float hm) {
    float lat = asin(clamp(mapDir(d).y, -1.0, 1.0)), lon = atan(-d.z, d.x);
    vec2 uv = vec2(lon / 6.2831853 + 0.5, 0.5 - lat / 3.1415927), l;
    float w = detWeight(uv, l), w2 = 0.0;
    vec2 l2 = vec2(0.0);
    if (w > 0.0) {
        float lod = log2(max(sp * detB.y / 3.1415927, 1.0));
        hm = mix(hm, lod > 0.0 ? detDecode(textureLod(texture4, (l + 0.5) / detA.z, lod)) : detHeight(l), w);
        hm = det2Mix(uv, sp, hm, w2, l2);
    }
    if (terrE.z < 0.5) return hm;
    vec3 o = vec3(params.z, params.z * 1.37, params.z * 0.71);
    g_cliff = 0.0; g_wall = 0.0;
    g_craterK = terrE.z > 1.5 ? mapCraterK(MAP_STYLE, d, o) : 1.0;
    if (w > 0.0 && MAP_STYLE == 3) {   // 火星の崖: 地層の段と尾根・溝（細かい標高の窓の中だけ）
        float s = mix(detSlope(l, lat, terrE.x), w2 > 0.0 ? det2Slope(l2, lat, terrE.x) : 0.0, w2);
        float k = strataK(s, sp * terrE.x) * w, t = strataT(hm, d, o), dd;
        if (k > 0.0) hm += k * STRATA_M * (strataStep(t, dd) - t);
        g_cliff = cliffWall(s) * w; g_wall = cliffRidgeA(s) * w;
    }
    return hm + vertexBumps(d, sp, 20.0 / terrE.x, mix(mix(terrE.y, terrE.w, w), detD.y / 6.2831853, w2), o) * terrE.x;
}
// 地形の影に使う標高 m: 全球の標高と細かい標高の窓だけ（手続きの起伏は入れない。bumpAt を呼ぶ所を増やさないためでもある）。
// s: 調べる間隔（単位球の長さ）。それに合わせて縮小した画像から引く
float dataHeight(vec3 d, float s) {
    float h = terrHeight(d, log2(max(s * terrD.y / 3.1415927, 1.0)));
    float lat = asin(clamp(mapDir(d).y, -1.0, 1.0)), lon = atan(-d.z, d.x);
    vec2 l;
    vec2 uv = vec2(lon / 6.2831853 + 0.5, 0.5 - lat / 3.1415927), l2;
    float w = detWeight(uv, l), w2;
    if (w > 0.0) {
        float lod = log2(max(s * detB.y / 3.1415927, 1.0));
        h = det2Mix(uv, s, mix(h, lod > 0.0 ? detDecode(textureLod(texture4, (l + 0.5) / detA.z, lod)) : detHeight(l), w), w2, l2);
    }
    return h;
}
// 地形の影（太陽高度 20° より低い時）: 頂点から太陽の方向へ地面に沿って 16 歩たどり（歩幅は頂点の間隔か標高の 1 画素から
// 広げ、最大で 150 km か高さ 8 km の山の影の長さまで）、太陽へ向かう光線より高い所があれば影にする。光線の高さは球面が
// 下がる分も入れ、影の縁は太陽の視半径と標高の 1 画素分の高さの不確かさでぼかす。高さは実際の値（陰影の起伏の強調は入れない）。
// 画素ごとに求めると描画が 2 倍ほど重くなったので頂点で求める。d: 頂点の向き（単位球）、sp: 頂点の間隔。戻り値は日なたの割合
float vertexShadow(vec3 d, float sp) {
    vec3 Lo = normalize(lightDirObj);
    float se = dot(d, Lo);
    if (se > 0.34 || se < -0.03) return 1.0;
    vec3 sh = Lo - d * se;
    float ce = length(sh);
    if (ce < 1e-4) return 1.0;
    sh /= ce;
    float te = se / ce, R = terrE.x;
    float pxm = max(sp, 3.1415927 / max(max(detB.z > 0.5 ? detB.y : 0.0, detD.z > 0.5 ? detD.y : 0.0), terrD.y)) * R;
    float h0 = dataHeight(d, pxm / R);
    float dmax = max(min(150000.0, 8000.0 / max(te, 0.01)), pxm * 2.0), q = pow(dmax / pxm, 1.0 / 15.0), dist = pxm, lit = 1.0;
    for (int k = 0; k < 16; k++) {
        float a = dist / R;
        vec3 dk = d * cos(a) + sh * sin(a);
        float hr = h0 + dist * te + dist * dist / (2.0 * R);
        lit = min(lit, clamp(0.5 + (hr - dataHeight(dk, dist * (1.0 - 1.0 / q) / R)) / (dist * 2.0 * sunAng + 0.15 * pxm), 0.0, 1.0));
        if (lit <= 0.0) break;
        dist *= q;
    }
    return lit;
}

void main() {
    vec3 pos = vertexPosition, nrm = vertexNormal;
    fragObj = vertexPosition;
    fragAlt = 0.0;
    fragShade = 1.0;
    if (terrA.x > 0.5) {
        int ri = int(vertexPosition.x * terrD.z + 0.5);
        int si = int(vertexPosition.y / 6.2831853 * terrD.w + 0.5), stp = int(terrStep[ri >> 2][ri & 3] + 0.5);
        float th = terrRing[ri >> 2][ri & 3], ph = float(si / stp * stp) * 6.2831853 / terrD.w;
        vec3 c = terrB.xyz, e1 = terrC.xyz, e2 = cross(e1, c);
        vec3 t = cos(ph) * e1 + sin(ph) * e2;
        float st = sin(th), s2 = sin(0.5 * th), cm1 = -2.0 * s2 * s2;   // cos θ − 1
        vec3 d = c * (1.0 + cm1) + t * st;
        // 頂点の間隔（間引いた後の、隣の輪との間と 1 周の向きの大きい方）が標高の 1 画素の何倍か。間隔より細かい起伏まで
        // 引くと、カメラが動いて頂点が地表の上を滑る時に遠くの起伏が波打つので、間隔に合わせて縮小した画像から引く
        int rp = max(ri - 1, 0), rn = min(ri + 1, int(terrD.z + 0.5));
        float dth = max(terrRing[rn >> 2][rn & 3] - th, th - terrRing[rp >> 2][rp & 3]);
        float sp = max(dth, st * 6.2831853 * float(stp) / terrD.w);
        float lod = log2(max(sp * terrD.y / 3.1415927, 1.0));
        vec3 E = vec3(1.0, terrB.w, 1.0);
        float hm = terrHeight(d, lod);
        hm = terrA.w < 1.5 ? earthDetailHeight(d, sp, hm) : bodyHeight(d, sp, hm);
        float h = hm * terrC.w;
        fragAlt = hm * 0.001;
        pos = E * (c * cm1 + t * st) + normalize(E * d) * h;
        nrm = vec3(d.x, d.y / terrB.w, d.z);
        fragObj = d;
        fragShade = vertexShadow(d, sp);
    }
    fragWorld = vec3(matModel * vec4(pos, 1.0));
    fragNormal = normalize(vec3(matNormal * vec4(nrm, 0.0)));
    if (relBeta <= 0.0) {
        gl_Position = mvp * vec4(pos, 1.0);
        return;
    }
    // 光行差: カメラから見た頂点の向きを、距離はそのままで進む向きへ寄せる（cos θ′ = (cos θ + β) / (1 + β cos θ)）。
    // 陰影は元の位置（fragWorld）で計算する
    vec3 w = fragWorld;
    float L = length(w);
    vec3 d = w / max(L, 1e-30);
    float ct = dot(d, relDir), ct2 = (ct + relBeta) / (1.0 + relBeta * ct);
    vec3 perp = d - relDir * ct;
    float pl = length(perp);
    if (pl > 1e-7) d = relDir * ct2 + perp * (sqrt(max(0.0, 1.0 - ct2 * ct2)) / pl);
    gl_Position = matProjection * matView * vec4(d * L, 1.0);
}
)";

static const char* PLANET_FS = R"(
#version 330
in vec3 fragObj;
in vec3 fragNormal;
in vec3 fragWorld;
in float fragAlt;
uniform vec3 lightDir;     // ワールド空間（カメラ相対）での太陽方向
uniform vec3 lightDirObj;  // 本体（自転込み）/ 環の座標系での太陽方向
uniform float relBeta;     // 光速に近い時の見え方の β と進む向き（頂点シェーダと同じ。色のドップラー効果に使う）
uniform vec3 relDir;
uniform float oppB0;       // 衝効果の強さ B0（大気の無い天体は 1、ほかは 0）
uniform vec4 sunInfo;      // 太陽の黒点: x 活動の強さ（負なら既定の模様）, y 黒点の平均の緯度 deg, z 緯度の広がり deg
uniform vec4 scars[20];    // 木星の衝突痕（シューメーカー・レビー第 9 彗星）: xyz 本体座標の向き, w 半径 rad
uniform vec4 scarInfo;     // x 衝突痕の数, y 濃さ
uniform vec4 occ[4];       // 影を落とす天体: xyz 中心（fragWorld と同じ座標）, w 半径（負なら大気あり）
uniform int occN;
uniform float sunAng;      // 太陽の視半径 rad
uniform vec3 color1;
uniform vec3 color2;
uniform vec3 atmColor;
uniform vec4 params;       // x: 種別（4 環, 5 上空のもや）, y: 縞の数 / 環の基準半径km, z: シード, w: 大気の強さ
uniform vec4 ringInfo;     // 環: x 不透明度の表の内側 km, y 表の幅 km, z 海王星のアダムズ環の弧の位置 rad（無ければ -1）,
                           //     w 1 なら画素より細い環を濃くする（天王星・海王星）
uniform vec4 detail;       // x: 表面模様の種類, y: 1 画素の大きさの係数, z: 時刻（日）, w: 雲の濃さ
uniform mat4 matNormal;
uniform sampler2D texture0;  // 地球・月: 地表の色 (Blue Marble / LROC)
uniform sampler2D texture1;  // 地球: R 標高・水深, G 夜景, B 海陸マスク, A 海氷 / 月・火星: R 標高上位, A 下位
uniform sampler2D texture2;  // 地図のある天体: 撮影された範囲（1: あり）
uniform sampler2D texture3;  // 土星と環: 環の不透明度（半径 72,000〜142,500 km） / 地球: 12 か月の地表の色
uniform vec4 mapInfo;        // 地図: x 1 / 平均の明るさ, y 種類（0 カラー, 1 白黒, 2 模様だけ）, z 標高（0 なし, 1 0.5 m 単位, 2 1 m 単位）, w 半径 km
uniform vec4 mapInfo2;       // 地図: x 撮影されていない所に使う表面模様の種類, y 地図を東へずらす角度 rad（木星の大赤斑）,
                             // z 1 なら撮影範囲の画像のアルファが液体の湖・海の割合（タイタン）
                             // 地球: z 今の月（0 = 1 月中旬、12 で一周）, w 季節の色と雲の画像あり
// 大気（地球・火星）: atmBeta は天頂方向の空気 1 つ分で散乱する割合（0 なら大気なし）。日光は太陽が低いほど赤くなる
// atmHaze: x カメラの位置の空気の濃さ（0 ならかすみなし）, y 描画座標 1 あたりの km, z スケールハイト km,
//          w 見えている太陽の割合（日食）
uniform vec3 atmBeta;
uniform vec4 atmExt;     // 地面の照らし方: xyz 日光を遮る天頂方向の光学的厚さ（色ごと）, w 散らされた光のうち地面へ向かう割合
uniform vec3 atmAlb;     // 散らす割合（単一散乱アルベド。火星の塵は青い光を吸収する）
uniform vec3 atmMie;
uniform vec3 atmOzone;
uniform vec4 atmHaze;
uniform vec4 hazeAir;    // 金星の雲・タイタンのもやの下の地表: x 1 で有効, y 描画の 1 単位の長さ km, z カメラの高度 km,
                         // w もやの粒の散乱 1/km（カメラのいる高さの。層の中か下か）
uniform vec3 hazeSky;    // 層の下の空の明るさ（色ごと。雲と厚い大気で散らされた日光）
uniform vec4 hazeBeta;   // 空気のレイリー散乱（地表、1/km、色ごと）, w: 空気の濃さが 1/e になる高さ km
uniform vec3 camUp;
in float fragShade;  // 地形の影（頂点シェーダの vertexShadow）
out vec4 finalColor;
uniform float exposure;    // 明るさの自動調整の倍率（UpdateExposure）。0.75 までは掛けたまま、それより明るい所はなめらかに 1 へ寄せる（白く飛ばない）
vec3 expo(vec3 c) {
    if (exposure <= 1.0) return c;
    vec3 e = c * exposure;
    return mix(e, 0.75 + 0.25 * (1.0 - exp(-(e - 0.75) / 0.25)), step(0.75, e));
}

const float PI = 3.14159265;
float fw;      // 1 画素の大きさ（単位球の長さ）
float g_ray;   // 新しいクレーターの明るさ（光条）
float g_liquid = 0.0;   // 液体の湖・海の割合（タイタン）
float g_ice = 0.0;      // 氷と雪の割合（地球の氷床・海氷、火星の極冠）
float g_floor = 0.0;    // 崩れたクレーターの埋まった平らな底の割合（火星。暗い砂がたまる）

// 地図の色が白い（明るく色みが少ない）割合。氷と雪の目安
float whiteness(vec3 c) {
    float mx = max(max(c.r, c.g), c.b), mn = min(min(c.r, c.g), c.b);
    return smoothstep(0.55, 0.75, mn) * (1.0 - smoothstep(0.12, 0.25, (mx - mn) / max(mx, 1e-3)));
}

)" NOISE_GLSL BUMPS_GLSL DETAIL_GLSL R"(
// 周波数 f の模様の重み: 画素より細かくなったら消す
float lodW(float f) { return 1.0 - smoothstep(0.15, 0.5, f * fw); }

// 近づくほどオクターブが増える fbm（値はおおよそ -1..1）。o はシード（拡大後に足すので精度を落とさない）
float fbmL(vec3 p, float f, int oct, float gain, vec3 o) {
    float s = 0.0, a = 1.0;
    for (int i = 0; i < oct; i++) {
        float w = lodW(f);
        if (w <= 0.0) break;
        s += a * w * (vnoise(p * f + o + float(i) * 7.13) * 2.0 - 1.0);
        a *= gain; f *= 2.03;
    }
    return s;
}

// 勾配付き fbm (x: 値, yzw: p についての勾配)
vec4 fbmD(vec3 p, float f, int oct, float gain, vec3 o) {
    vec4 s = vec4(0.0);
    float a = 1.0;
    for (int i = 0; i < oct; i++) {
        float w = lodW(f);
        if (w <= 0.0) break;
        vec4 n = noised(p * f + o + float(i) * 7.13);
        s += a * w * vec4(n.x * 2.0 - 1.0, n.yzw * 2.0 * f);
        a *= gain; f *= 2.03;
    }
    return s;
}
// 勾配付きの尾根の鋭いノイズ（ridgeBand の全部のオクターブ。x: 値, yzw: p についての勾配）
vec4 ridgeD(vec3 p, float f, int oct, float gain, vec3 o) {
    vec4 s = vec4(0.0);
    float a = 1.0;
    for (int i = 0; i < oct; i++) {
        float w = lodW(f);
        if (w <= 0.0) break;
        vec4 n = noised(p * f + o + float(i) * 7.13);
        float v = n.x * 2.0 - 1.0;
        s += a * w * vec4(0.5 - abs(v), -sign(v) * n.yzw * 2.0 * f);
        a *= gain; f *= 2.03;
    }
    return s;
}

// クレーター 1 層 (x: 高さ[半径単位], yzw: 勾配)。お椀形の凹みと盛り上がった縁
vec4 craterLayer(vec3 p, float F, float density, vec3 salt, float worn) {
    vec3 d; float r, rnd;
    if (!cellSpot(p, F, density, salt, 0.08, 0.29, d, r, rnd)) return vec4(0.0);
    float dist = length(d);
    float x = dist / r;
    if (x >= 1.7) return vec4(0.0);
    float fr = craterFresh(rnd, worn), dh, h = craterShape(x, fr, dh);
    float w = lodW(F / (2.0 * r));
    if (x * x < (1.0 - fr) * 0.55) g_floor += w * (1.0 - fr);
    float depth = 0.4 * r;
    // 若いクレーターほど明るい（内部と周囲の噴出物）
    if (rnd > 0.75) g_ray += w * (x < 1.0 ? 0.5 : 0.8 * exp(-(x - 1.0) * 3.0)) * (rnd - 0.75) * 4.0;
    else if (x < 1.0) g_ray -= w * 0.08;
    vec3 dir = dist > 1e-6 ? d / dist : vec3(0.0);
    return w * vec4(h * depth / F, dh / r * depth * dir);
}

vec4 craters(vec3 p, float density, float F0, vec3 salt, float worn) {
    vec4 s = vec4(0.0);
    float F = F0, dk = density;
    for (int i = 0; i < 16; i++) {
        if (lodW(F * 1.7) <= 0.0) break;
        s += craterLayer(p, F, dk, salt + float(i) * 31.7, worn);
        dk *= craterLayerK(worn);
        F *= 2.2;
    }
    return s;
}

// ---- 地表の素材ごとの、地図より細かい起伏（陰影だけで、地形の格子の高さには入れない）。値は vec4(高さ, p についての勾配)（単位球の長さ）
// 岩塊・樹冠: 格子のセルに 1 つずつ置く丸い塊（3 段の大きさ。小さい段ほど数が多い）。F: 一番大きい段のセルの細かさ,
// cover: 地表を覆う割合（セルに置く確率 × 塊の体積の割合 ≒ 0.033。断面の面積の割合は体積の割合に等しい）, tall: 高さ / 半径
vec4 boulders(vec3 p, float F, float cover, float tall, vec3 salt) {
    vec4 s = vec4(0.0);
    float f = F, dens = cover * 30.0;
    for (int k = 0; k < 3; k++) {
        float w = lodW(f);
        if (w <= 0.0 || dens <= 0.0) break;
        vec3 d; float r, rnd;
        if (cellSpot(p, f, min(dens, 1.0), salt + float(k) * 17.3, 0.12, 0.3, d, r, rnd)) {
            float q = r * r - dot(d, d);
            if (q > 0.0) {
                float z = sqrt(q);
                s += w * vec4(z * tall / f, -d * tall / max(z, r * 0.15));
            }
        }
        f *= 2.3;
    }
    return s;
}

// 砂の波（さざ波・砂丘）: 決まった 2 つの向きの波を、場所ごとにゆっくり変わる重みで混ぜる（向きそのものを場所で変えると、
// 細かい波の位相が乱れるため）。断面は風下側が急な非対称。峰は数波長ごとに曲がり、高さは峰の曲がりと同じノイズでむらにする。
// 大小 2 つの波（さざ波と砂丘）をまとめて求める。F1, F2: 波数（単位球の長さあたり、波長 = 2π / F）、k1, k2: 傾きの大きさ
// （値の傾きは最大でこの 1.6 倍ほど）
vec4 sandWaves(vec3 p, float F1, float k1, float F2, float k2, vec3 o) {
    float m = smoothstep(0.45, 0.55, vnoise(p * 6.0 + o + 3.0));   // 向きの切り替わりは狭く（重なると格子に見える）
    vec4 bend = noised(p * (F2 * 0.05) + o);   // 峰の曲がり（大きい波の約 3 波長ごと。小さい波も同じ曲がりで重さを抑える）
    vec4 s = vec4(0.0);
    for (int j = 0; j < 2; j++) {
        float F = j == 0 ? F1 : F2, w = lodW(F * 0.16);
        if (w <= 0.0) continue;
        float amp = (j == 0 ? k1 : k2) * w * (0.3 + 1.4 * bend.x);
        for (int k = 0; k < 2; k++) {
            vec3 a = normalize(k == 0 ? vec3(0.8, 0.15, 0.58) : vec3(-0.3, 0.2, 0.93));
            float ph = dot(p, a) * F + bend.x * 5.0 * F / F2;   // 峰は大小の波で同じ距離だけずれる
            float h = sin(ph) + 0.3 * sin(2.0 * ph), dh = cos(ph) + 0.6 * cos(2.0 * ph);
            s += (k == 0 ? m : 1.0 - m) * amp * vec4(h / F, dh * (a + bend.yzw * 0.25));
        }
    }
    return s;
}

// 平行な筋（尾根・溝）の 1 組: 向き n に垂直な面に沿う筋（球面では n を軸とする小円で、近くではほぼ直線）。F: 筋の間隔の
// 逆数（単位球の長さあたり）。間隔ごとに frac の割合だけ置き、bend（0..1。lineBend の値）で ゆるく曲げる。kind 0: 二重の
// 尾根（2 本の尾根の間に溝）、1: 尾根と溝が交互に並ぶ。値は vec4(高さ 0..1, p についての勾配)
vec4 lineSet(vec3 p, vec3 n, float F, float frac, int kind, vec3 o, float bend) {
    float w = lodW(F * (kind == 0 ? 8.0 : 2.0));
    if (w <= 0.0) return vec4(0.0);
    float u = dot(p, n) * F + 2.5 * bend;
    float i = floor(u), f = u - i;
    uint hr = hashu(ivec3(int(i), 53, int(o.y)));   // 置くか（下の 16 ビット）と高さ（上の 16 ビット）
    if (float(hr & 65535u) * (1.0 / 65536.0) > frac) return vec4(0.0);
    float h, dh;
    if (kind == 0) {
        float a = (f - 0.38) / 0.07, b = (f - 0.62) / 0.07, ea = exp(-a * a), eb = exp(-b * b);
        h = ea + eb; dh = -2.0 * (a * ea + b * eb) / 0.07;
    } else {
        h = 0.5 - 0.5 * cos(6.2831853 * f); dh = 3.1415927 * sin(6.2831853 * f);
    }
    w *= 0.3 + 0.7 * float(hr >> 16) * (1.0 / 65536.0);   // 筋ごとに高さを変える
    return w * vec4(h, dh * F * n);
}
// 筋の曲がり（間隔の逆数 F の筋の約 25 本ごとに変わる）。重いので、同じ間隔の筋の組（向きが違う）で同じ値を使う
float lineBend(vec3 p, float F, vec3 o) { return lodW(F * 2.0) > 0.0 ? vnoise(p * (F * 0.04) + o) : 0.0; }
vec3 lineDir(float k) { return normalize(vec3(sin(k * 2.4 + 0.3), cos(k * 1.7 + 1.1), sin(k * 3.1 + 2.0))); }
// 区画 i の隣（k = 0..8 で 3x3）の区画の点から q へのベクトル。点は区画の 0.18〜0.82 に置く（上と下の 16 ビット）。こうすると、
// 縁を作る点が必ず 3x3 の区画の中にある（2 つ先の区画の点までの最短の距離と、3x3 の区画の点までの最長の距離から数値で確かめた。
// 0.17 から）
vec2 cellVec(vec2 q, vec2 i, int k) {
    vec2 c = i + vec2(float(k % 3 - 1), float(k / 3 - 1));
    uint h = hashu(ivec3(ivec2(c), 11));
    return q - c - (0.18 + 0.64 * vec2(float(h & 65535u), float(h >> 16)) * (1.0 / 65536.0));
}
// 多角形の網目（冥王星のスプートニク平原の溝・トリトンのカンタロープ地形）は、中心の経度 lon0 を 0 とする経度と緯度の平面に
// 点を散らしたボロノイで作る（平面は地域の中では球面とほぼ同じ。経度の向きは kx（中心の緯度の cos）で縮める。3 次元の
// ボロノイは調べる点が 27 個で重かった）。F: 多角形の大きさの逆数（単位球の長さあたり）、lat・lon: 緯度・経度
vec2 cellQ(float lat, float lon, float lon0, float kx, float F, vec3 o) {
    float lo = mod(lon - lon0 + 9.4247780, 6.2831853) - 3.1415927;
    return vec2(lo * kx, lat) * F + o.xy;
}
// q の平面での勾配 g を p についての勾配に
vec3 cellGrad(float lat, float lon, float kx, vec2 g) {
    vec3 east = vec3(-sin(lon), 0.0, -cos(lon)), north = vec3(-sin(lat) * cos(lon), cos(lat), sin(lat) * sin(lon));
    return east * g.x * kx / max(cos(lat), 0.05) + north * g.y;
}
// ボロノイの多角形の網目。pits = false: 冥王星のスプートニク平原の、対流の多角形の境目の溝（多角形の縁（一番近い点と、ほかの
// 点との垂直二等分線）までの距離が小さい所。2 番目と 1 番目に近い点の距離の差を使うと、近い 2 点の間で溝が扇形に広がり、
// 3 本の溝が集まる所で陰影が折れた）。pits = true: トリトンのカンタロープ地形の、丸みのある窪みと間の尾根（点からの距離 r と
// 縁までの距離 e から、点で 0・縁で 1 になる r / (r + e) の上の窪み。縁で傾きが 0 なので、尾根の頂は折れ目にならない）。
// 値は vec4(高さ −1..0（溝の底・窪みの底で −1）, p についての勾配)
vec4 cellNet(float lat, float lon, float lon0, float kx, float F, bool pits, vec3 o) {
    float w = lodW(F * 10.0);
    if (w <= 0.0) return vec4(0.0);
    vec2 q = cellQ(lat, lon, lon0, kx, F, o), i = floor(q), v1 = vec2(0.0);
    float d1 = 1e9, d2 = 1e9;
    for (int k = 0; k < 9; k++) {
        vec2 v = cellVec(q, i, k);
        float d = dot(v, v);
        if (d < d1) { d2 = d1; d1 = d; v1 = v; }
        else d2 = min(d2, d);
    }
    // 縁までの距離は、2 番目と 1 番目に近い点の距離の差の半分より小さくない。なめらかな最小で縮むのは 0.015 × log 8 = 0.031 まで
    // なので、溝では差が 2 × (0.06 + 0.031) を超える所（多角形の内側の大部分）は溝の外
    if (!pits && sqrt(d2) - sqrt(d1) > 0.183) return vec4(0.0);
    // 縁（垂直二等分線）ごとの距離 ek を、なめらかな最小（幅 0.015）でまとめる（ただの最小だと、溝が集まる所で壁が折れ目で
    // 切り替わって見える）。今までの最小 m を基準に重みを足していく（配列を使わない）
    float m = 1e9, sw = 0.0;
    vec2 n = vec2(0.0);
    for (int k = 0; k < 9; k++) {
        vec2 dv = cellVec(q, i, k) - v1;
        float L = length(dv);
        if (L < 1e-6) continue;   // 一番近い点そのもの
        float ek = (dot(dv + v1, dv + v1) - d1) / (2.0 * L);
        if (ek < m) {
            float r = exp((ek - m) / 0.015);
            sw = sw * r + 1.0; n = n * r + dv / L; m = ek;
        } else {
            float r = exp((m - ek) / 0.015);
            sw += r; n += r * dv / L;
        }
    }
    float e = m - 0.015 * log(sw);   // 縁の上では負になるが、そこは溝の底・尾根の頂
    n /= sw;   // e の q についての勾配
    // 形を決める値 x とその勾配 nx: 溝は e、窪みは r / (r + e)
    float x = e, x0 = 0.0, xw = 0.06;
    vec2 nx = n;
    if (pits) {
        float r = sqrt(d1), ep = max(e, 0.0), D = r + ep + 1e-6;
        x = r / D; x0 = 0.15; xw = 0.85;
        nx = (ep * v1 / max(r, 1e-6) - (e > 0.0 ? r : 0.0) * n) / (D * D);
    }
    float s = clamp((x - x0) / xw, 0.0, 1.0);
    vec2 g = 6.0 * s * (1.0 - s) / xw * nx * F;   // q の平面での勾配
    return w * vec4(s * s * (3.0 - 2.0 * s) - 1.0, cellGrad(lat, lon, kx, g));
}
// 地図のある氷の衛星などの、地図より細かい地形（陰影だけ。値は vec4(高さ, 勾配)、単位球の長さ）。大きさ・高さは探査機の
// 画像と地形の研究からの目安:
//  - エウロパ: 二重の尾根（1 本の幅 約 1.5 km、高さ 約 200 m。小さいものは 60 m）が、場所ごとに違う向きで重なる。
//    カオス地形（地図の暗い所）には少ない
//  - ガニメデ: 明るい溝状地形に、尾根と溝が間隔 約 8 km（起伏 約 350 m）と 1.5 km（60 m）で並ぶ。帯ごとに向きが違う
//  - エンケラドゥス: 南極地域（南緯 55° より南）に、間隔 約 1.5 km・起伏 約 60 m の尾根と溝
//  - 冥王星: スプートニク平原の、窒素の氷の対流でできた多角形（大きさ 約 25 km）の境目の溝（深さ 約 100 m）
//  - トリトン: カンタロープ地形（ボイジャー 2 号の画像で東経 300°〜30°・赤道から北の撮影範囲の端まで。USGS の
//    ブベンベ地域の位置は北緯 18°・東経 335°）の、直径 30〜40 km の楕円の窪み（cavi）が並ぶ網目。窪みの深さ 約 500 m
//    （Schenk ほか 2021 で 300〜600 m、間の尾根は 400 m 以上。ボイジャーの立体視・陰影からの推定で不確か）
// R: 半径 m、rel: 地図の画素の明るさ（地図の平均を 1）、lat・lon: 緯度・経度、kC: mapCraterK の値（冥王星では平原の外の割合）
vec4 bodyRelief(vec3 p, int style, vec3 o, float R, float rel, float lat, float lon, float kC) {
    vec4 r = vec4(0.0);
    if (style == 5) {
        float c = smoothstep(0.75, 0.95, rel);   // カオス地形には少ない
        float bL = c > 0.0 ? lineBend(p, R / 12000.0, o) : 0.0, bS = c > 0.0 ? lineBend(p, R / 2500.0, o + 40.0) : 0.0;
        for (int k = 0; k < 3 && c > 0.0; k++) {
            float m = smoothstep(0.35, 0.6, vnoise(p * 5.0 + o + float(k) * 17.0));
            if (m > 0.0) r += m * (lineSet(p, lineDir(float(k)), R / 12000.0, 0.5, 0, o + float(k) * 3.1, bL) * (200.0 / R) +
                                   lineSet(p, lineDir(float(k + 4)), R / 2500.0, 0.6, 0, o + float(k) * 5.3 + 40.0, bS) * (60.0 / R));
        }
        r *= c;
    } else if (style == 6) {
        float b = smoothstep(0.95, 1.25, rel);
        float bL = b > 0.0 ? lineBend(p, R / 8000.0, o) : 0.0, bS = b > 0.0 ? lineBend(p, R / 1500.0, o + 60.0) : 0.0;
        for (int k = 0; k < 3 && b > 0.0; k++) {
            float m = smoothstep(0.5, 0.7, vnoise(p * 14.0 + o + float(k) * 23.0));
            vec3 n = lineDir(float(k + 8));
            if (m > 0.0) r += m * (lineSet(p, n, R / 8000.0, 1.0, 1, o + float(k) * 7.7, bL) * (350.0 / R) +
                                   lineSet(p, n, R / 1500.0, 1.0, 1, o + float(k) * 9.1 + 60.0, bS) * (60.0 / R));
        }
        r *= b;
    } else if (style == 11) {
        float s = 1.0 - smoothstep(-0.87, -0.77, normalize(p).y), m = smoothstep(0.4, 0.6, vnoise(p * 20.0 + o + 7.0));
        float bn = s > 0.0 ? lineBend(p, R / 1500.0, o) : 0.0;
        if (s > 0.0) r = s * (60.0 / R) * (lineSet(p, lineDir(12.0), R / 1500.0, 0.8, 1, o, bn) * m +
                                           lineSet(p, lineDir(13.0), R / 1500.0, 0.8, 1, o + 3.0, bn) * (1.0 - m));
    } else if (style == 10 || style == 12) {
        // トリトンの地域: 中心 北緯 22°・東経 345°、南北 ±24°・東西 ±48° の楕円。縁は場所で揺らす
        bool tr = style == 12;
        float la = degrees(lat) - 22.0, lo = mod(degrees(lon) + 555.0, 360.0) - 180.0;
        float s = tr ? 1.0 - smoothstep(0.8, 1.0, (la / 24.0) * (la / 24.0) + (lo / 48.0) * (lo / 48.0) +
                                                   (vnoise(p * 10.0 + o + 9.0) - 0.5) * 0.4) : 1.0 - kC;
        if (s > 0.0) r = s * ((tr ? 500.0 : 100.0) / R) * cellNet(lat, lon, tr ? -0.2617994 : 3.1415927, tr ? 0.93 : 0.94,
                                                                 R / (tr ? 35000.0 : 25000.0), tr, o + (tr ? 47.0 : 33.0));
    }
    return r;
}

// 樹冠: 丸い冠がすき間なく並ぶ凹凸（値ノイズの山を 2 段）。F: 細かさ（冠の大きさ ≒ 単位球の長さ 1 / F）
vec4 canopy(vec3 p, float F, vec3 o) {
    vec4 s = vec4(0.0);
    float a = 1.0;
    for (int k = 0; k < 2; k++) {
        float w = lodW(F);
        if (w <= 0.0) break;
        vec4 n = noised(p * F + o + float(k) * 5.3);
        s += a * w * vec4(n.x / F, n.yzw);
        a *= 0.5; F *= 2.1;
    }
    return s;
}

// 雪の風紋（サスツルギ）: 風の向き a に沿って伸ばしたノイズ。F: 細かさ
vec4 sastrugi(vec3 p, float F, vec3 a, vec3 o) {
    float w = lodW(F);
    if (w <= 0.0) return vec4(0.0);
    vec3 q = p * F;
    vec4 n = noised(q - a * (dot(q, a) * 0.85) + o);
    return w * vec4((n.x - 0.5) / F, n.yzw - a * (dot(n.yzw, a) * 0.85));
}

uniform float mapLatK;
// 地図（正距円筒図法）を引く向き: 球のメッシュの点 p（楕円体では y を縮める前）の y に 1 + mapLatK を掛ける（地球の地図は
// 測地緯度、ほかの天体は惑星中心緯度。CPU の MapLatScale）
vec3 mapDir(vec3 p) { return normalize(vec3(p.x, p.y * (1.0 + mapLatK), p.z)); }

// 緯度・経度 (lon は -PI..PI)
vec2 latLon(vec3 p) { return vec2(asin(clamp(p.y, -1.0, 1.0)), atan(p.z, p.x)); }
float lonDiff(float a, float b) { float d = a - b; return d - 2.0 * PI * floor((d + PI) / (2.0 * PI)); }

// ---------------------------------------------------------------- 岩石・氷天体
// base: 地表の色, 戻り値: 高さと勾配
vec4 rocky(vec3 p, int style, vec3 o, out vec3 base) {
    g_ray = 0.0;
    float macro = fbmL(p, 2.2, 5, 0.5, o + 3.0);
    float rough, dens, mare, heart;   // 起伏の強さ・クレーターの多さ・月と水星の海・冥王星のハート（スプートニク平原）
    rockyParams(p, style, o, rough, dens, mare, heart);

    vec4 T = fbmD(p, 3.0, 26, 0.48, o) * (0.022 * rough);
    vec4 C = dens > 0.0 ? craters(p, dens * (1.0 - heart), 5.0, o * 0.37, 0.0) : vec4(0.0);
    float tone = clamp(0.5 + macro * 0.6 + T.x * 6.0, 0.0, 1.0);
    vec3 c1 = color1, c2 = color2;
    base = mix(c2, c1, tone);

    if (style == 1 || style == 2) {
        base = mix(base, c2 * 0.72, mare * 0.85);
    } else if (style == 3) {   // 火星: 暗色の地域と極冠
        float dark = smoothstep(0.0, 0.3, fbmL(p, 1.6, 6, 0.55, o + 15.0));
        base = mix(base, c2 * 0.7, dark * 0.7);
        float cap = smoothstep(0.87, 0.9, abs(p.y) + fbmL(p, 8.0, 5, 0.5, o + 4.0) * 0.03);
        base = mix(base, vec3(0.95, 0.93, 0.90), cap);
    } else if (style == 4) {   // イオ: 硫黄の斑と火山（黒い火口と赤い噴出物の輪）
        base = mix(base, vec3(0.98, 0.96, 0.82), smoothstep(0.1, 0.4, macro) * 0.6);
        base = mix(base, c2, smoothstep(0.0, 0.35, fbmL(p, 4.0, 6, 0.5, o + 17.0)) * 0.7);
        for (int k = 0; k < 2; k++) {
            vec3 d; float r, rnd;
            if (cellSpot(p, 7.0 * float(k + 1), 0.55, o + float(k) * 9.1, 0.12, 0.3, d, r, rnd)) {
                float x = length(d) / r;
                base = mix(base, vec3(0.80, 0.30, 0.12), (1.0 - smoothstep(0.4, 1.6, x)) * 0.7 * rnd);
                base = mix(base, vec3(0.07, 0.05, 0.04), 1.0 - smoothstep(0.15, 0.3, x));
            }
        }
    } else if (style == 5) {   // エウロパ: 赤茶色の筋（リネア）とカオス地形
        base = mix(c1, c2, smoothstep(0.15, 0.5, macro) * 0.45);
        float lin = 0.0;
        for (int k = 0; k < 4; k++) {
            float f = 2.5 * pow(2.3, float(k));
            float n = fbmL(p, f, 3, 0.45, o + float(k) * 13.0);
            float width = 0.012 + fw * f * 0.5;
            lin = max(lin, (1.0 - smoothstep(0.0, width, abs(n))) * lodW(f * 8.0) * (1.0 - float(k) * 0.18));
        }
        base = mix(base, c2 * 0.85, lin * 0.75);
    } else if (style == 6) {   // ガニメデ: 暗い古い地域と明るい溝状地形
        float dark = smoothstep(-0.05, 0.15, macro);
        float groove = sin(dot(p, normalize(vec3(0.3, 1.0, 0.5))) * 140.0 + fbmL(p, 6.0, 3, 0.5, o) * 8.0);
        base = mix(c1 * (0.92 + 0.08 * groove * lodW(25.0)), c2, dark);
    } else if (style == 7) {   // カリスト: 暗い表面に明るいクレーター
        base = mix(c2, c1, tone * 0.45);
        g_ray *= 2.0;
    } else if (style == 9) {   // イアペトゥス: 先行半球（西経 90°、本体 +Z 側）が真っ黒な二色の衛星
        float dark = smoothstep(-0.12, 0.12, p.z + fbmL(p, 5.0, 5, 0.5, o) * 0.15);
        base = mix(c1, c2, dark);
    } else if (style == 10) {  // 冥王星: 明るいハートと赤黒い赤道帯
        float band = (1.0 - smoothstep(0.0, 0.35, abs(p.y + 0.05))) * smoothstep(-0.15, 0.2, macro + 0.15);
        base = mix(base, vec3(0.30, 0.17, 0.12), band * 0.85);
        base = mix(base, vec3(0.96, 0.93, 0.88), heart);
    } else if (style == 11) {  // エンケラドゥス: 南極の「虎の縞」
        float st = sin(dot(p.xz, vec2(0.8, 0.6)) * 36.0 + fbmL(p, 10.0, 3, 0.5, o) * 2.0);
        float tiger = (1.0 - smoothstep(0.0, 0.25, abs(st) - 0.0)) * (1.0 - smoothstep(-0.86, -0.78, p.y));
        base = mix(base, vec3(0.55, 0.72, 0.82), tiger * 0.8);
    } else if (style == 12) {  // トリトン: ピンク色の南極冠
        base = mix(base, vec3(0.92, 0.80, 0.76), (1.0 - smoothstep(-0.4, -0.2, p.y + macro * 0.15)));
    }
    base *= clamp(1.0 + g_ray * 0.5, 0.5, 1.8);
    return T + C;
}

// ---------------------------------------------------------------- ガス惑星・厚い大気
vec3 gas(vec3 p, int style, vec3 o) {
    if (style == 24) {
        // 金星の雲は自転（243 日）よりずっと速く、約 4 日で一周する（超回転）。自転と同じ向きに、差の分だけ回す
        float a = fract(detail.z * (1.0 / 4.0 - 1.0 / 243.025)) * 6.2831853;
        float ca = cos(a), sa = sin(a);
        p = vec3(ca * p.x + sa * p.z, p.y, -sa * p.x + ca * p.z);
    }
    vec2 ll = latLon(p);
    float lat = ll.x, lon = ll.y;
    float warp = fbmL(p * vec3(1.0, 3.5, 1.0), 2.5, 6, 0.55, o);
    float y = p.y + warp * 0.035 + fbmL(p * vec3(1.0, 2.0, 1.0), 14.0, 6, 0.5, o + 5.0) * 0.006;
    float b1 = sin(y * params.y * PI) * 0.5 + 0.5;
    float b2 = sin(y * params.y * PI * 2.7 + 1.3) * 0.5 + 0.5;
    vec3 base = mix(color1, color2, b1 * 0.75 + b2 * 0.25);
    // 細かな乱流（近づくほど細かく）
    float fine = fbmL(p * vec3(1.0, 2.5, 1.0), 24.0, 16, 0.55, o + 9.0);
    base *= 1.0 + fine * 0.07;

    if (style == 20) {        // 木星: 茶色の縞・大赤斑・白斑
        base = mix(base, vec3(0.55, 0.38, 0.27), smoothstep(0.6, 0.95, b1) * 0.45);
        float la = -22.4 * PI / 180.0;
        float dl = lonDiff(lon, 1.2) * cos(la);
        float e = length(vec2(dl / 0.13, (lat - la) / 0.065));
        float swirl = fbmL(vec3(dl * 8.0, (lat - la) * 14.0, e * 2.0), 3.0, 6, 0.5, o + 2.0);
        vec3 grs = mix(vec3(0.78, 0.40, 0.26), vec3(0.88, 0.58, 0.42), swirl * 0.5 + 0.5);
        base = mix(base, grs, 1.0 - smoothstep(0.85, 1.05, e));
        base = mix(base, vec3(0.95, 0.90, 0.82), smoothstep(0.9, 1.05, e) * (1.0 - smoothstep(1.05, 1.3, e)) * 0.5);
        for (int k = 0; k < 3; k++) {
            float lo = -33.0 * PI / 180.0;
            float d2 = length(vec2(lonDiff(lon, 2.4 + float(k) * 0.55) * cos(lo) / 0.035, (lat - lo) / 0.022));
            base = mix(base, vec3(0.97, 0.95, 0.92), (1.0 - smoothstep(0.7, 1.0, d2)) * 0.85);
        }
    } else if (style == 21) { // 土星: 淡い縞と北極の六角形
        base = mix(base, color1, 0.25);
        float colat = acos(clamp(p.y, -1.0, 1.0));
        float seg = mod(lon + PI / 6.0, PI / 3.0) - PI / 6.0;
        float hexR = 0.27 / cos(seg);
        base = mix(base, vec3(0.55, 0.60, 0.62), (1.0 - smoothstep(hexR - 0.01, hexR + 0.01, colat)) * 0.6);
    } else if (style == 22) { // 天王星: ほぼ一様
        base = mix(color1, color2, (b1 * 0.75 + b2 * 0.25) * 0.25) * (1.0 + fine * 0.02);
    } else if (style == 23) { // 海王星: 大暗斑と白い筋雲
        float la = -20.0 * PI / 180.0;
        float e = length(vec2(lonDiff(lon, -0.8) * cos(la) / 0.11, (lat - la) / 0.06));
        base = mix(base, vec3(0.10, 0.17, 0.48), (1.0 - smoothstep(0.8, 1.0, e)) * 0.8);
        float streak = smoothstep(0.35, 0.6, fbmL(p * vec3(0.6, 6.0, 0.6), 4.0, 6, 0.5, o + 11.0));
        base = mix(base, vec3(0.92, 0.95, 1.0), streak * 0.6 * smoothstep(0.1, 0.4, abs(p.y)));
    } else if (style == 24) { // 金星: 雲の V 字模様
        float wave = sin(lon * 2.0 - abs(lat) * 5.0 + warp * 3.0);
        base = mix(color2, color1, clamp(0.55 + wave * 0.18 + fine * 0.6, 0.0, 1.0));
    } else if (style == 25) { // タイタン: 一様なもや、極付近がやや暗い
        base = mix(color1, color2, smoothstep(0.3, 0.9, abs(p.y)) * 0.6) * (1.0 + fine * 0.03);
    }
    return base;
}

// ---------------------------------------------------------------- 地球
// 雲: 経度方向にゆっくり流れる。地表付近では見えなくする（detail.w）
float earthClouds(vec3 p, vec3 o, float bias) {
    if (detail.w <= 0.0) return 0.0;
    float ang = detail.z * 0.15;
    float cs = cos(ang), sn = sin(ang);
    vec3 pc = vec3(cs * p.x - sn * p.z, p.y, sn * p.x + cs * p.z);
    float w = fbmL(pc, 3.0, 4, 0.5, o + 17.0);
    float cl = fbmL(pc + w * 0.06, 4.0, 18, 0.52, o + 23.0);
    float wisp = fbmL(pc, 30.0, 12, 0.55, o + 29.0);
    return smoothstep(0.15 + bias, 0.55 + bias, cl + wisp * 0.25 + 0.12 * cos(p.y * PI * 3.0)) * detail.w;
}

// カメラの周りの細かい標高の窓（地球・月・火星）の、uv の標高 m（hd）と勾配（sl: 東・北の向きの tan。半径 R m）。
// 間隔は 1 画素か画面の 1 画素の大きい方。戻り値は窓の重み（窓の外は 0）
float detGradient(vec2 uv, vec2 dx, vec2 dy, float lat, float R, out vec2 sl, out float hd) {
    vec2 l;
    float w = detWeight(uv, l);
    sl = vec2(0.0); hd = 0.0;
    if (w <= 0.0) return 0.0;
    vec2 k = detB.xy / detA.z, gx = dx * k, gy = dy * k;
    vec2 sd = max(1.0 / detB.xy, vec2(fw / (2.0 * PI * max(cos(lat), 0.05)), fw / PI));
    vec2 q = (l + 0.5) / detA.z, sq = sd * k;
    float hE = detDecode(textureGrad(texture4, q + vec2(sq.x, 0.0), gx, gy));
    float hW = detDecode(textureGrad(texture4, q - vec2(sq.x, 0.0), gx, gy));
    float hN = detDecode(textureGrad(texture4, q - vec2(0.0, sq.y), gx, gy));
    float hS = detDecode(textureGrad(texture4, q + vec2(0.0, sq.y), gx, gy));
    hd = detDecode(textureGrad(texture4, q, gx, gy));
    sl = vec2((hE - hW) / (2.0 * sd.x) / (2.0 * PI * max(cos(lat), 0.05)), (hN - hS) / (2.0 * sd.y) / PI) / R;
    vec2 l2;
    float w2 = det2Weight(uv, l2);
    if (w2 > 0.0) {   // 地域だけのもっと細かい標高
        k = detD.xy / detC.z; gx = dx * k; gy = dy * k;
        sd = max(1.0 / detD.xy, vec2(fw / (2.0 * PI * max(cos(lat), 0.05)), fw / PI));
        q = (l2 + 0.5) / detC.z; sq = sd * k;
        hE = det2Decode(textureGrad(texture5, q + vec2(sq.x, 0.0), gx, gy));
        hW = det2Decode(textureGrad(texture5, q - vec2(sq.x, 0.0), gx, gy));
        hN = det2Decode(textureGrad(texture5, q - vec2(0.0, sq.y), gx, gy));
        hS = det2Decode(textureGrad(texture5, q + vec2(0.0, sq.y), gx, gy));
        hd = mix(hd, det2Decode(textureGrad(texture5, q, gx, gy)), w2);
        sl = mix(sl, vec2((hE - hW) / (2.0 * sd.x) / (2.0 * PI * max(cos(lat), 0.05)), (hN - hS) / (2.0 * sd.y) / PI) / R, w2);
    }
    return w;
}

// 高さコード → m。陸 128..255 = 0..8900 m、海 127..0 = 0..-11000 m（平方根で詰めてある）
float earthHeightM(float c) {
    float v = c * 255.0;
    float t = v >= 127.5 ? (v - 128.0) / 127.0 : (127.0 - v) / 127.0;
    return v >= 127.5 ? t * t * 8900.0 : -t * t * 11000.0;
}

// 実データの地球。大陸・標高・夜景は NASA のデータ、テクスチャより細かい部分は手続きで補う
vec4 earthTex(vec3 p, vec3 o, float ndlG, out vec3 base, out float specMask, out float lights, out float cloud) {
    // 本体座標: X 軸がグリニッジ子午線、Y 軸が北極、東経は -Z 方向
    float lat = asin(clamp(mapDir(p).y, -1.0, 1.0));
    float lon = atan(-p.z, p.x);
    vec2 uv = vec2(lon / (2.0 * PI) + 0.5, 0.5 - lat / PI);
    // 経度 ±180° の継ぎ目で微分が跳ばないようにする
    vec2 dx = dFdx(uv), dy = dFdy(uv);
    dx.x -= floor(dx.x + 0.5); dy.x -= floor(dy.x + 0.5);
    vec3 tc = textureGrad(texture0, uv, dx, dy).rgb;
    vec4 td = textureGrad(texture1, uv, dx, dy);
    if (mapInfo2.w > 0.5) {
        // 季節: 12 か月の地表の色（texture3 に 4 列 × 3 行）から今の月の色と 4 月の色の差を足す（雪・緑・乾季）
        vec2 tuv = clamp(uv, vec2(0.001, 0.002), vec2(0.999, 0.998));
        vec2 gx = dx / vec2(4.0, 3.0), gy = dy / vec2(4.0, 3.0);
        float m0 = floor(mapInfo2.z), f = mapInfo2.z - m0;
        int a = int(m0) % 12, b = (a + 1) % 12;
        vec3 ca = textureGrad(texture3, (vec2(float(a % 4), float(a / 4)) + tuv) / vec2(4.0, 3.0), gx, gy).rgb;
        vec3 cb = textureGrad(texture3, (vec2(float(b % 4), float(b / 4)) + tuv) / vec2(4.0, 3.0), gx, gy).rgb;
        vec3 apr = textureGrad(texture3, (vec2(3.0, 0.0) + tuv) / vec2(4.0, 3.0), gx, gy).rgb;
        tc = clamp(tc + mix(ca, cb, f) - apr, 0.0, 1.0);
    }

    // 標高の勾配（中心差分。間隔は 1 テクセルか 1 画素の大きい方）
    vec2 texel = 1.0 / vec2(textureSize(texture1, 0));
    vec2 st = max(texel, vec2(fw / (2.0 * PI * max(cos(lat), 0.05)), fw / PI));
    float hE = earthHeightM(textureGrad(texture1, uv + vec2(st.x, 0.0), dx, dy).r);
    float hW = earthHeightM(textureGrad(texture1, uv - vec2(st.x, 0.0), dx, dy).r);
    float hN = earthHeightM(textureGrad(texture1, uv - vec2(0.0, st.y), dx, dy).r);
    float hS = earthHeightM(textureGrad(texture1, uv + vec2(0.0, st.y), dx, dy).r);
    float hm = earthHeightM(td.r);
    const float R = 6378137.0, EXAG = 2.5;   // 起伏は 2.5 倍に強調
    float dhdLon = (hE - hW) / (2.0 * st.x) / (2.0 * PI) / R * EXAG;
    float dhdLat = (hN - hS) / (2.0 * st.y) / PI / R * EXAG;
    vec3 east = vec3(-sin(lon), 0.0, -cos(lon));
    vec3 north = vec3(-sin(lat) * cos(lon), cos(lat), sin(lat) * sin(lon));
    vec3 grad = east * dhdLon / max(cos(lat), 0.05) + north * dhdLat;
    // カメラの周り（1 分角の窓の中）: 細かい標高の勾配。傾き s は細かい山肌の振幅に使う
    vec2 sl;
    float dm, dw = detGradient(uv, dx, dy, lat, R, sl, dm), s = length(sl);
    if (dw > 0.0) {
        grad = mix(grad, (east * sl.x + north * sl.y) * EXAG, dw);
        if (hm > 0.0 || dm > 0.0) hm = mix(hm, dm, dw);
    }

    // 海岸線: ぼかしたマスクの 0.5 を境にし、テクセルより細かい所はノイズで入り組ませる（マスクが 0 か 1 の所、
    // つまり海岸から離れた所はノイズで境を越えないので求めない）。以下の模様も、掛ける重みが 0 の所では求めない
    float coastNoise = td.b > 0.0 && td.b < 1.0 ? fbmL(p, 700.0, 16, 0.55, o + 41.0) : 0.0;
    float m = td.b + coastNoise * 0.22;
    float aa = max(fwidth(m), 0.002);
    float land = smoothstep(0.5 - aa, 0.5 + aa, m);

    // 陸の色: テクスチャ（海の部分は周囲の陸の色で埋めてあるので、海岸でも青が混ざらない）
    vec3 lc = land > 0.0 ? tc * (0.88 + 0.24 * (fbmL(p, 700.0, 20, 0.5, o + 5.0) * 0.5 + 0.5)) : tc;
    // 海の色: 水深で浅瀬を明るく。海氷はデータの A
    float depth = clamp(-hm / 3000.0, 0.0, 1.0);
    vec3 oc = mix(vec3(0.08, 0.24, 0.34), vec3(0.03, 0.07, 0.20), smoothstep(0.0, 0.4, depth));
    float ice = td.a > 0.0 ? td.a * (0.85 + 0.3 * fbmL(p, 900.0, 10, 0.5, o + 47.0)) : 0.0;
    oc = mix(oc, vec3(0.88, 0.91, 0.95), clamp(ice, 0.0, 1.0));
    base = mix(oc, lc, land);
    specMask = (1.0 - land) * (1.0 - ice);
    g_ice = max(clamp(ice, 0.0, 1.0) * (1.0 - land), whiteness(tc) * land);   // 海氷と、白い陸（氷床・積雪）

    // 夜景: 実データ。近づくと街の明かりの粒に分かれる
    lights = td.g * land > 0.0 ? td.g * land * clamp(1.0 + 1.2 * fbmL(p, 900.0, 12, 0.6, o + 13.0), 0.0, 2.0) : 0.0;
    if (mapInfo2.w > 0.5) {
        // 雲: NASA Blue Marble の雲の分布（texture2）に、画像より細かい筋を手続きで足す
        float cv = textureGrad(texture2, uv, dx, dy).r;
        float wisp = cv > 0.0 && detail.w > 0.0 ? fbmL(p, 300.0, 12, 0.55, o + 29.0) : 0.0;
        cloud = smoothstep(0.12, 0.8, cv + wisp * 0.1 * cv) * detail.w;
    } else {
        cloud = earthClouds(p, o, 0.12);   // 実データの地表が見えるよう少なめ
    }

    // 起伏: データの勾配 + テクセルより細かい山肌（標高が高いほど険しい）
    float rough = (0.03 + 0.3 * clamp(hm / 4000.0, 0.0, 1.0)) / 1400.0;
    vec4 h = vec4(hm / R * EXAG, grad) * land;
    // 窓の中は 1 分角より細かい山肌（地形の格子の高さと同じ。陰影は画素まで細かく）
    vec2 l2;
    float fLo = dw > 0.0 ? mix(detB.y, detD.y, det2Weight(uv, l2)) / (2.0 * PI) : 0.0;
    if (land > 0.0) h += mix(dw < 1.0 ? fbmD(p, 700.0, 18, 0.5, o) * rough : vec4(0.0),
                             dw > 0.0 ? earthBumps(p, s, max(hm, 0.0), fw, fLo, 16, o) : vec4(0.0), dw) * land;
    // 地表の色から素材を推定して細かい起伏を足す（土地被覆の実測ではなく色からの推定）: 明るい黄褐色は砂（砂丘 波長 120 m・800 m）、
    // 暗い緑は森（樹冠 約 8 m と 4 m の凹凸。くぼみは暗い）、白い所は雪と氷（風に沿って伸びた風紋 約 20 m と 5 m）
    if (land > 0.0) {
        float lum = dot(tc, vec3(0.333));
        float sand = smoothstep(0.38, 0.5, lum) * smoothstep(0.1, 0.2, tc.r - tc.b) * (1.0 - g_ice);
        float forest = (1.0 - smoothstep(0.16, 0.26, lum)) * smoothstep(-0.03, 0.02, tc.g - tc.r);
        vec4 m = vec4(0.0);
        if (sand > 0.0)
            m += sandWaves(p, 6.2832 * R / 120.0, 0.12, 6.2832 * R / 800.0, 0.35, o + 61.0) * sand;
        if (forest > 0.0) {
            vec4 cr = canopy(p, R / 8.0, o * 0.61);
            m += cr * (0.5 * forest);
            base *= mix(1.0, 0.6 + 0.35 * clamp(cr.x * (R / 8.0), 0.0, 1.4), forest * land);
        }
        if (g_ice > 0.0) {
            vec3 e = cross(vec3(0.0, 1.0, 0.0), p);
            e = length(e) > 1e-3 ? normalize(e) : vec3(1.0, 0.0, 0.0);
            vec3 wa = normalize(e * 0.6 + cross(p, e) * 0.8);
            m += (sastrugi(p, R / 20.0, wa, o + 71.0) * 0.2 + sastrugi(p, R / 5.0, wa, o + 73.0) * 0.3) * g_ice;
        }
        h += m * land;
    }
    if (land < 1.0) h += fbmD(p, 20000.0, 14, 0.55, o + 31.0) * (1.5e-6 * (1.0 - land));
    return h;
}

// 環を垂直に見た時の不透明度 1 - exp(-τ)（中心からの距離 km）。土星は Voyager 2 PPS の掩蔽観測
// （assets/saturn_rings.png）、ほかの惑星は起動時に作った表
float ringDensity(float rkm) {
    // 範囲の判定より先に読む（分岐の中で読むと縮小画像の段階が決まらないことがある）
    float u = (rkm - ringInfo.x) / ringInfo.y;
    float v = texture(texture3, vec2(clamp(u, 0.0, 1.0), 0.5)).r;
    return (u <= 0.0 || u >= 1.0) ? 0.0 : v;
}

// 海王星のアダムズ環の弧（勇気・自由・平等・友愛。弧の並びと長さは目安）。弧のある所で τ を何倍にするか
float adamsArcs(vec3 q) {
    float a = degrees(mod(atan(q.z, q.x) - ringInfo.z + 6.2831853, 6.2831853));
    float k = 0.0;
    k += 1.0 - smoothstep(1.0, 2.0, abs(a - 2.0));     // 勇気
    k += 1.0 - smoothstep(2.0, 3.0, abs(a - 11.0));    // 自由
    k += 1.0 - smoothstep(1.0, 2.0, abs(a - 19.0));    // 平等
    k += 1.0 - smoothstep(0.5, 1.5, abs(a - 23.0));
    k += 1.0 - smoothstep(4.0, 6.0, abs(a - 33.0));    // 友愛
    return 1.0 + 30.0 * k;
}

// 他の天体の影（日食・月食・衛星の影）。太陽の視半径を考えて本影と半影を分ける
// red: 大気のある天体の本影の深さ（月食の赤銅色に使う）
float occShadow(vec3 P, vec3 L, out float red) {
    float lit = 1.0;
    red = 0.0;
    for (int i = 0; i < occN; i++) {
        vec3 v = occ[i].xyz - P;
        float t = dot(v, L);
        if (t <= 0.0) continue;
        float sep = length(v - L * t) / t;   // 太陽の中心と天体の中心の角距離
        float ao = abs(occ[i].w) / t;        // 天体の視半径
        float cover = min(1.0, ao * ao / (sunAng * sunAng));
        float k = smoothstep(abs(ao - sunAng), ao + sunAng, sep);
        lit *= mix(1.0 - cover, 1.0, k);
        if (occ[i].w < 0.0) red = max(red, (1.0 - k) * cover);
    }
    return lit;
}

// 実データの月。色は LROC、標高は LOLA。テクスチャより細かいクレーターは手続きで補う
float moonHeightM(vec4 t) { return (t.r * 255.0 * 256.0 + t.a * 255.0) * 0.5 - 10000.0; }
// 地図の標高 m（mapInfo.z が 2 なら 1 m 単位で -32768 m から。凹凸の大きいベスタ・カロン）
float mapHeightM(vec4 t) { return mapInfo.z > 1.5 ? (t.r * 255.0 * 256.0 + t.a * 255.0) - 32768.0 : moonHeightM(t); }

vec4 moonTex(vec3 p, vec3 o, out vec3 base) {
    g_ray = 0.0;
    // 本体座標: X 軸が経度 0°（地球を向く側）、Y 軸が北極、東経は -Z 方向
    float lat = asin(clamp(mapDir(p).y, -1.0, 1.0));
    float lon = atan(-p.z, p.x);
    vec2 uv = vec2(lon / (2.0 * PI) + 0.5, 0.5 - lat / PI);
    vec2 dx = dFdx(uv), dy = dFdy(uv);
    dx.x -= floor(dx.x + 0.5); dy.x -= floor(dy.x + 0.5);
    vec3 tc = pow(textureGrad(texture0, uv, dx, dy).rgb, vec3(1.3)) * 1.25;   // 海と高地の明暗差を少し強調

    vec2 texel = 1.0 / vec2(textureSize(texture1, 0));
    vec2 st = max(texel, vec2(fw / (2.0 * PI * max(cos(lat), 0.05)), fw / PI));
    float hE = moonHeightM(textureGrad(texture1, uv + vec2(st.x, 0.0), dx, dy));
    float hW = moonHeightM(textureGrad(texture1, uv - vec2(st.x, 0.0), dx, dy));
    float hN = moonHeightM(textureGrad(texture1, uv - vec2(0.0, st.y), dx, dy));
    float hS = moonHeightM(textureGrad(texture1, uv + vec2(0.0, st.y), dx, dy));
    float hm = moonHeightM(textureGrad(texture1, uv, dx, dy));
    const float R = 1737400.0, EXAG = 1.5;
    float dhdLon = (hE - hW) / (2.0 * st.x) / (2.0 * PI) / R * EXAG;
    float dhdLat = (hN - hS) / (2.0 * st.y) / PI / R * EXAG;
    vec3 east = vec3(-sin(lon), 0.0, -cos(lon));
    vec3 north = vec3(-sin(lat) * cos(lon), cos(lat), sin(lat) * sin(lon));
    vec3 grad = east * dhdLon / max(cos(lat), 0.05) + north * dhdLat;
    // カメラの周り（16 ピクセル/度の窓の中）: 細かい標高
    vec2 sl;
    float hd, dw = detGradient(uv, dx, dy, lat, R, sl, hd);
    if (dw > 0.0) {
        grad = mix(grad, (east * sl.x + north * sl.y) * EXAG, dw);
        hm = mix(hm, hd, dw);
    }

    // 海（暗い所）はクレーターが少なく平ら（moonDark と同じ）
    float dark = 1.0 - smoothstep(0.12, 0.3, dot(tc, vec3(0.333)));
    vec4 C = craters(p, mix(0.55, 0.2, dark), 260.0, o * 0.37, 0.0);
    vec4 T = fbmD(p, 600.0, 20, 0.48, o) * (mix(0.004, 0.0015, dark) / 260.0);
    base = tc * (0.92 + 0.16 * (fbmL(p, 900.0, 16, 0.5, o + 5.0) * 0.5 + 0.5)) * (1.0 + g_ray * 0.35);
    // 岩塊: Diviner の岩の多さ（地表を 1 m ほどより大きな岩が覆う割合）だけ置く。直径 1〜12 m。新しい岩は砂より明るい
    vec4 Bk = vec4(0.0);
    if (mapInfo2.w > 0.5) {
        float ra = pow(textureGrad(texture2, uv, dx, dy).r, 2.0) * 0.5;
        Bk = boulders(p, R / 20.0, ra, 0.6, o * 0.53);
        base *= 1.0 + 0.2 * step(1e-10, Bk.x);
    }
    return vec4(hm / R * EXAG, grad) + C + T + Bk;
}

// 探査機の画像による地図のある天体（水星・火星・ケレス・ベスタ・衛星・冥王星など）。月と同じ並びの地図を貼る。
// 色は天体の色に合わせて明るさをそろえ、撮影されていない所は手続き的な表面にする。
// 細かい起伏は手続き的に足す（撮影された範囲では大きなクレーターを作らず、地図の模様と食い違わないようにする）
// 地図の色: 地図の 1 画素が画面の 1 画素より大きく写る近さでは、双線形補間だと画素の境目で明るさの傾きが折れて
// 四角いモザイクに見えるので、Catmull-Rom の 3 次補間（双線形の 9 回の読み出しで求める）に切り替える
vec3 mapColor(vec2 uv, vec2 dx, vec2 dy) {
    vec2 ts = vec2(textureSize(texture0, 0));
    vec3 c = textureGrad(texture0, uv, dx, dy).rgb;
    float k = 1.0 - smoothstep(0.5, 1.0, max(length(dx * ts), length(dy * ts)));
    if (k <= 0.0) return c;
    vec2 sp = uv * ts;
    vec2 t1 = floor(sp - 0.5) + 0.5, f = sp - t1;
    vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f)), w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    vec2 w2 = f * (0.5 + f * (2.0 - 1.5 * f)), w3 = f * f * (-0.5 + 0.5 * f);
    vec2 w12 = w1 + w2;
    vec2 a = (t1 - 1.0) / ts, b = (t1 + w2 / w12) / ts, e = (t1 + 2.0) / ts;
    vec3 r = (textureLod(texture0, vec2(a.x, a.y), 0.0).rgb * w0.x + textureLod(texture0, vec2(b.x, a.y), 0.0).rgb * w12.x
            + textureLod(texture0, vec2(e.x, a.y), 0.0).rgb * w3.x) * w0.y
           + (textureLod(texture0, vec2(a.x, b.y), 0.0).rgb * w0.x + textureLod(texture0, vec2(b.x, b.y), 0.0).rgb * w12.x
            + textureLod(texture0, vec2(e.x, b.y), 0.0).rgb * w3.x) * w12.y
           + (textureLod(texture0, vec2(a.x, e.y), 0.0).rgb * w0.x + textureLod(texture0, vec2(b.x, e.y), 0.0).rgb * w12.x
            + textureLod(texture0, vec2(e.x, e.y), 0.0).rgb * w3.x) * w3.y;
    return mix(c, clamp(r, 0.0, 1.0), k);
}

vec4 mapTex(vec3 p, vec3 o, out vec3 base) {
    g_ray = 0.0;
    float lat = asin(clamp(mapDir(p).y, -1.0, 1.0));
    float lon = atan(-p.z, p.x);
    vec2 uv = vec2(lon / (2.0 * PI) + 0.5, 0.5 - lat / PI);
    vec2 dx = dFdx(uv), dy = dFdy(uv);
    dx.x -= floor(dx.x + 0.5); dy.x -= floor(dy.x + 0.5);
    vec3 tc = mapColor(uv, dx, dy);
    float cov = textureGrad(texture2, uv, dx, dy).r;
    // 手続き的な表面は撮影されていない所（cov < 1）か、明暗を手続き的に塗り分ける天体だけ、地図の細かい起伏は撮影
    // された所（cov > 0）だけ求める（混ぜる重みが 0 の側は結果に効かない。火星などほぼ全体が撮影された天体で重い rocky を省く）
    vec3 pb = vec3(0.0);
    vec4 ph = vec4(0.0);
    if (cov < 1.0 || mapInfo.y > 1.5) ph = rocky(p, MAP_STYLE, o, pb);
    float L1 = dot(color1, vec3(0.333));
    vec3 mb;
    if (mapInfo.y > 1.5) mb = pb * tc.r * mapInfo.x;          // 模様だけ（明暗は手続き的な塗り分け）
    else if (mapInfo.y > 0.5) mb = color1 * tc.r * mapInfo.x;  // 白黒: 天体の色で着色
    else mb = tc * L1 * mapInfo.x;                             // カラー: 明るさだけ天体に合わせる
    bool mars = MAP_STYLE == 3;
    // 火星: TES の夜の熱慣性から素材を見分け、細かい起伏を足す。熱慣性が低い所は細かい塵が積もって小さな凹凸が埋まり、
    // 高い所は岩・岩盤（岩塊 直径 1〜12 m）。砂の波（さざ波 波長 25 m・砂丘 300 m）は、観測で砂丘地帯と分かっている所
    // （USGS の砂丘データベース。地図のアルファ）だけ（熱慣性が中ほどでも、砂とは限らず固まった地面のことも多い）。
    // 砂丘は動いて新しいので、小さなクレーターは描かない。クレーターは塵・砂で埋まり削られて、ほとんどが浅く崩れている
    // （craterFresh）。極冠は表面が若くクレーターがほとんど無い。
    // 崖（cliffWall）は岩がむき出し: クレーターは崩れて残らず、塵も積もらず、岩塊（崩れた岩くず）が多い
    vec4 mat = vec4(0.0), C = vec4(0.0);
    float dust = 0.0, sand = 0.0, rock = 0.0, R = mapInfo.w * 1000.0;
    g_floor = 0.0;
    vec2 sl = vec2(0.0);   // カメラの周り（火星の 32 ピクセル/度の窓の中）の細かい標高 hd と勾配 sl
    float hd = 0.0, dw = mapInfo.z > 0.5 ? detGradient(uv, dx, dy, lat, R, sl, hd) : 0.0;
    float cs = length(sl), cliff = mars ? cliffWall(cs) * dw : 0.0;
    if (mars) {
        g_ice = whiteness(tc) * smoothstep(55.0, 65.0, abs(degrees(lat)));   // 極冠: 白い所（緯度 60° より極側）
        if (mapInfo2.w > 0.5) {
            vec2 tm = textureGrad(texture3, uv, dx, dy).ra;
            float ti = 20.0 * pow(125.0, tm.x);
            dust = 1.0 - smoothstep(120.0, 220.0, ti);
            sand = smoothstep(0.1, 0.5, tm.y) * (1.0 - g_ice);
            rock = smoothstep(350.0, 700.0, ti) * (1.0 - sand);   // 砂丘の砂は岩を覆う
        }
        dust *= 1.0 - cliff;
        rock = max(rock, 0.4 * cliff * (1.0 - sand));
    }
    // 地図の画素の明るさ（地図の平均を 1）。ガニメデの明るい溝状地形（約 20 億年）は暗い地域（約 40 億年）よりクレーターが
    // 半分ほど（頂点の高さには入れない。ガニメデは標高データが無く地形の格子を作らない）
    int st = MAP_STYLE;
    float rel = dot(tc, vec3(0.333)) * mapInfo.x, kC0 = mapCraterK(st, p, o), kC = kC0;
    if (st == 6) kC *= 1.0 - 0.55 * smoothstep(0.95, 1.25, rel);
    if (cov > 0.0) C = craters(p, 0.35 * kC, 260.0, o * 0.37, craterWorn(st)) * (1.0 - g_ice) * (1.0 - sand) * (1.0 - cliff);
    vec4 Tn = fbmD(p, 600.0, 20, 0.48, o);
    if (mars) {
        // 地図（1 画素 約 5 km）より細かい明るさのむら mot（値はどれも HiRISE などの画像の見た目からの目安）:
        //  - 暗い砂の吹きだまり: 低い所（手続きの起伏の谷）にたまる、境目のはっきりした斑（地表の 4 分の 1 ほど、周りの 0.85 倍）。
        //    塵の積もった所には無い
        //  - 崩れたクレーターの埋まった底: 暗い砂がたまる（0.75 倍）
        //  - 岩塊・砂丘の砂: 玄武岩で周りより暗い（0.82 倍・0.85 倍）
        //  - ゆるやかなむら ±8%
        //  - 崖: むき出しの岩で 0.9 倍。地層ごとに ±12% の縞
        // 地図の画素の平均の明るさが変わらないよう、斑の分は割り戻す
        float Rm = R;
        if (rock > 0.0) mat = boulders(p, Rm / 20.0, 0.12 * rock * (1.0 - g_ice), 0.6, o * 0.53);
        float bare = (1.0 - dust) * (1.0 - g_ice);
        float drift = smoothstep(0.25, 0.45, fbmL(p, 9000.0, 12, 0.5, o + 71.0) * 0.6 - Tn.x * 0.5) * bare;
        float mot = (1.0 - 0.15 * drift) / (1.0 - 0.15 * 0.25 * bare);
        mot *= 1.0 - 0.25 * clamp(g_floor, 0.0, 1.0) * (1.0 - g_ice);
        mot *= 1.0 - 0.18 * step(1e-10, mat.x);
        if (sand > 0.0) mat += sandWaves(p, 6.2832 * Rm / 25.0, 0.1, 6.2832 * Rm / 300.0, 0.35, o + 61.0) * sand;
        mot *= (1.0 - 0.15 * sand) * (1.0 + 0.08 * fbmL(p, 2000.0, 14, 0.55, o + 83.0));
        if (cliff > 0.0) {   // 地層の縞（隣の層と境目の 2 割でなじませる。段と同じく細かすぎる所は消す）
            float t = strataT(hd, p, o), i = floor(t);
            float b = mix(hash1(ivec3(int(i), 17, 29)), hash1(ivec3(int(i) + 1, 17, 29)), smoothstep(0.8, 1.0, t - i));
            mot *= (1.0 - 0.1 * cliff) * (1.0 + strataK(cs, fw * R) * dw * (0.24 * b - 0.12));
        }
        mot = mix(mot, 1.0, g_ice);
        // 火星の地図（Viking MDIM 2.1 に NASA Ames が色を付けたもの）は暗い地域が青灰色に塗られているので、色合いだけを明るさに
        // 応じた反射スペクトルの目安に置き換える（反射率の比 赤:緑:青 は明るい塵 1 : 0.55 : 0.29、暗い砂 1 : 0.73 : 0.55。Mars
        // Pathfinder・ローバーのパノラマカメラなどの観測の概数。地図と同じ画面の色の値（sRGB、およそ 1/2.2 乗）に直して使う）。
        // 明るさ（3 色の平均）は地図の値に細かいむらを掛けたもの。極冠の氷は変えない
        vec3 hue = mix(vec3(1.0, 0.87, 0.76), vec3(1.0, 0.76, 0.57), smoothstep(0.6, 1.1, dot(tc, vec3(0.333)) * mapInfo.x * mot));
        mb = mix(hue * (dot(mb, vec3(0.333)) * mot / dot(hue, vec3(0.333))), mb, g_ice);
    } else if (cov > 0.0) mat = bodyRelief(p, st, o, R, rel, lat, lon, kC0);
    vec4 fine = vec4(0.0);
    if (cov > 0.0) fine = (C + Tn * (0.003 / 260.0)) * (1.0 - 0.6 * dust) + mat;
    vec4 h = mix(ph, fine, cov);
    if (mapInfo.z > 0.5) {
        vec2 texel = 1.0 / vec2(textureSize(texture1, 0));
        vec2 st = max(texel, vec2(fw / (2.0 * PI * max(cos(lat), 0.05)), fw / PI));
        float hE = mapHeightM(textureGrad(texture1, uv + vec2(st.x, 0.0), dx, dy));
        float hW = mapHeightM(textureGrad(texture1, uv - vec2(st.x, 0.0), dx, dy));
        float hN = mapHeightM(textureGrad(texture1, uv - vec2(0.0, st.y), dx, dy));
        float hS = mapHeightM(textureGrad(texture1, uv + vec2(0.0, st.y), dx, dy));
        float hm = mapHeightM(textureGrad(texture1, uv, dx, dy));
        float dhdLon = (hE - hW) / (2.0 * st.x) / (2.0 * PI) / R;
        float dhdLat = (hN - hS) / (2.0 * st.y) / PI / R;
        vec3 east = vec3(-sin(lon), 0.0, -cos(lon));
        vec3 north = vec3(-sin(lat) * cos(lon), cos(lat), sin(lat) * sin(lon));
        vec3 grad = east * dhdLon / max(cos(lat), 0.05) + north * dhdLat;
        if (dw > 0.0) {
            grad = mix(grad, east * sl.x + north * sl.y, dw);
            hm = mix(hm, hd, dw);
        }
        if (cliff > 0.0) {   // 火星の崖: 地層の段（頂点の bodyHeight と同じ）と尾根・溝
            float k = strataK(cs, fw * R) * dw, t = strataT(hm, p, o), dd, st = strataStep(t, dd);
            h += vec4(k * STRATA_M / R * (st - t), 0.0, 0.0, 0.0) + cov * cliffRidgeA(cs) * dw * ridgeD(p, 1200.0, 7, 0.5, o + 43.0);
            grad *= 1.0 + k * (dd - 1.0);
        }
        h += vec4(hm / R, grad);
    }
    base = mix(pb, mb, cov) * (0.94 + 0.12 * (fbmL(p, 900.0, 16, 0.5, o + 5.0) * 0.5 + 0.5));
    if (mapInfo2.z > 0.5) {   // 液体の湖・海: 平らで暗い（光の映り込みは照明のところで）
        g_liquid = textureGrad(texture2, uv, dx, dy).a;
        h = mix(h, vec4(h.x, 0.0, 0.0, 0.0), g_liquid);
        base = mix(base, vec3(0.02), g_liquid);
    }
    return h;
}

// 地図のあるガス惑星（木星・土星）: 雲の模様を貼る。撮影されていない所は手続き的な雲。地図は mapInfo2.y だけ回す（大赤斑を今の経度に合わせる）
vec3 gasMap(vec3 p, vec3 o) {
    vec3 pb = gas(p, MAP_STYLE, o);
    float lat = asin(clamp(mapDir(p).y, -1.0, 1.0));
    float lon = atan(-p.z, p.x) + mapInfo2.y;
    vec2 uv = vec2(lon / (2.0 * PI) + 0.5, 0.5 - lat / PI);
    vec2 dx = dFdx(uv), dy = dFdy(uv);
    dx.x -= floor(dx.x + 0.5); dy.x -= floor(dy.x + 0.5);
    vec3 tc = textureGrad(texture0, uv, dx, dy).rgb;
    float cov = textureGrad(texture2, uv, dx, dy).r;
    return mix(pb, tc * dot(color1, vec3(0.333)) * mapInfo.x, cov);
}

// base: 地表の色, specMask: 海の反射, lights: 夜景, 戻り値: 高さと勾配
vec4 earth(vec3 p, vec3 o, float ndlG, out vec3 base, out float specMask, out float lights, out float cloud) {
    vec4 e = fbmD(p, 1.6, 28, 0.5, o);
    float el = e.x + 0.04;                       // 海面からの高さ（ノイズ単位）
    float aa = length(e.yzw) * fw + 1e-6;        // 海岸線を画素幅でなめらかに
    float land = smoothstep(-aa, aa, el);
    float lat = abs(p.y);
    float moist = fbmL(p, 2.5, 6, 0.5, o + 40.0);
    float temp = 1.0 - pow(lat, 1.5) * 1.25 - max(el, 0.0) * 1.6;
    float arid = smoothstep(0.12, 0.25, lat) * (1.0 - smoothstep(0.42, 0.55, lat));
    float dry = clamp(arid * 0.9 - moist * 1.4 + 0.1, 0.0, 1.0);

    vec3 forest = vec3(0.09, 0.24, 0.08), grass = vec3(0.30, 0.40, 0.17), desert = vec3(0.80, 0.67, 0.46);
    vec3 tundra = vec3(0.42, 0.40, 0.34), rock = vec3(0.42, 0.38, 0.34), snow = vec3(0.94);
    vec3 lc = mix(forest, grass, smoothstep(-0.15, 0.2, -moist));
    lc = mix(lc, desert, smoothstep(0.35, 0.6, dry));
    lc = mix(lc, rock, smoothstep(0.35, 0.6, el) * 0.7);
    lc = mix(lc, tundra, (1.0 - smoothstep(0.25, 0.45, temp)));
    lc = mix(lc, snow, (1.0 - smoothstep(0.06, 0.18, temp)));
    lc *= 0.88 + 0.24 * (fbmL(p, 70.0, 20, 0.5, o + 5.0) * 0.5 + 0.5);   // 細かい地表の模様

    float depth = clamp(-el * 5.0, 0.0, 1.0);
    vec3 oc = mix(vec3(0.07, 0.36, 0.45), color1 * 0.75, smoothstep(0.0, 0.35, depth));
    float seaIce = smoothstep(0.90, 0.93, lat + fbmL(p, 12.0, 4, 0.5, o + 8.0) * 0.02);
    oc = mix(oc, vec3(0.90, 0.93, 0.96), seaIce);
    base = mix(oc, lc, land);
    specMask = (1.0 - land) * (1.0 - seaIce);

    // 夜景: 温暖で乾燥していない陸地に集まる
    lights = 0.0;
    float pop = land * (1.0 - dry) * smoothstep(0.15, 0.45, temp) * smoothstep(0.0, 0.3, fbmL(p, 5.0, 4, 0.5, o + 11.0) + 0.1);
    if (pop > 0.0 && ndlG < 0.02) lights = pop * smoothstep(0.05, 0.5, fbmL(p, 40.0, 16, 0.6, o + 13.0));

    cloud = earthClouds(p, o, 0.0);

    float hk = 0.035 * land * (0.3 + 3.0 * max(el, 0.0));
    // 海面の波（近づいた時だけ現れる細かいうねり）
    vec4 h = vec4(e.x * hk, e.yzw * hk);
    if (land < 1.0) h += fbmD(p, 20000.0, 14, 0.55, o + 31.0) * (1.5e-6 * (1.0 - land));
    return h;
}

// ---------------------------------------------------------------- 太陽
// 恒星の表面。surf: 0 太陽型, 1 赤色矮星, 2 高温の星, 3 白色矮星, 4 褐色矮星, 5 速く自転する高温の星。flare: 整数部がフレアの通し番号、小数部が明るさ
vec3 sun(vec3 p, float mu, vec3 o, int surf, float flare) {
    if (surf == 4) {
        // 褐色矮星: 木星のような雲の帯と、明るさの変化から分かっている濃淡のまだらな雲（自ら弱く光る）
        float warp = fbmL(p * vec3(1.0, 3.5, 1.0), 2.5, 6, 0.55, o);
        float y = p.y + warp * 0.05;
        float band = sin(y * 9.0 * PI) * 0.5 + 0.5;
        float patchy = fbmL(p * vec3(1.0, 2.2, 1.0), 5.0, 6, 0.5, o + 4.0);
        float fine = fbmL(p * vec3(1.0, 2.5, 1.0), 24.0, 10, 0.55, o + 9.0);
        vec3 c = mix(color2 * 0.45, color1 * 1.2, clamp(0.3 + band * 0.45 + patchy * 0.8, 0.0, 1.0)) * (1.0 + fine * 0.1);
        return c * (0.35 + 0.65 * pow(mu, 0.6));
    }
    if (surf == 2 || surf == 3 || surf == 5) {
        // 高温の星・白色矮星: 対流による粒状斑がほとんどなく、なめらか。白色矮星は周辺減光が弱く、縁まで青白い
        float g = fbmL(p, 20.0, 6, 0.5, o);
        vec3 c = mix(color2, color1, clamp(0.85 + g * 0.08, 0.0, 1.0));
        float limb = surf == 3 ? 0.75 + 0.25 * pow(mu, 0.5) : 0.5 + 0.5 * pow(mu, 0.5);
        c *= limb * (surf == 3 ? vec3(0.92 + 0.08 * mu, 0.96 + 0.04 * mu, 1.05) : vec3(1.0));
        if (surf == 5) {
            // 重力減光: 速い自転で赤道がふくらみ、重力が弱い赤道は冷たく暗く（アルタイルは約 6,900 K）、極は熱く明るく
            // 青白い（約 8,700 K）。明るさは温度の 4 乗で、赤道は極の 4 割ほど（Monnier et al. 2007 の干渉計の像）
            float w = pow(abs(p.y), 1.5);
            c *= mix(vec3(1.0, 0.88, 0.74) * 0.62, vec3(0.95, 1.02, 1.15) * 1.2, w);
        }
        return c;
    }
    float g = fbmL(p, 45.0, 18, 0.55, o);                 // 粒状斑
    float sg = fbmL(p, 6.0, 4, 0.5, o + 3.0);             // 超粒状斑
    vec3 c = mix(color2, color1, clamp(0.65 + g * 0.45 + sg * 0.15, 0.0, 1.0));
    if (surf == 1) {
        // 赤色矮星: 表面の数割を覆う大きな黒点が高緯度まで広がり、極のまわりにも黒点の帽子がある
        for (int k = 0; k < 2; k++) {
            vec3 d; float r, rnd;
            if (cellSpot(p, 2.2 + 2.0 * float(k), 0.4, o + float(k) * 4.1, 0.18, 0.4, d, r, rnd)) {
                float x = length(d) / r;
                c = mix(c, c * 0.5, 1.0 - smoothstep(0.7, 1.0, x));
                c = mix(c, c * 0.25, 1.0 - smoothstep(0.3, 0.55, x));
            }
        }
        float cap = smoothstep(0.78, 0.9, abs(p.y) + 0.06 * fbmL(p, 8.0, 3, 0.5, o + 9.0));
        c = mix(c, c * 0.45, cap);
    }
    // 黒点（低緯度帯）。太陽は活動周期に合わせて数と緯度が変わる（周期の初めは高い緯度、終わりは赤道近く）
    bool cyc = sunInfo.x >= 0.0;
    float dens = cyc ? 0.5 * sunInfo.x : 0.35;
    for (int k = 0; k < (surf == 1 || dens < 0.003 ? 0 : 2); k++) {
        vec3 d; float r, rnd;
        if (cellSpot(p, 9.0 * float(k + 1), dens, o + float(k) * 5.3, 0.06, 0.2, d, r, rnd)) {
            float x = length(d) / r;
            float lat = degrees(asin(clamp(abs(p.y), 0.0, 1.0)));
            float belt = cyc ? exp(-pow((lat - sunInfo.y) / sunInfo.z, 2.0))
                             : smoothstep(0.08, 0.15, abs(p.y)) * (1.0 - smoothstep(0.45, 0.55, abs(p.y)));
            c = mix(c, c * 0.55, (1.0 - smoothstep(0.8, 1.0, x)) * belt);
            c = mix(c, vec3(0.25, 0.10, 0.03), (1.0 - smoothstep(0.35, 0.5, x)) * belt);
        }
    }
    // 白斑（周辺部で目立つ）
    float fac = smoothstep(0.2, 0.55, fbmL(p, 18.0, 5, 0.5, o + 7.0)) * pow(1.0 - mu, 2.0) * (cyc ? 0.3 + 0.7 * min(1.0, sunInfo.x) : 1.0);
    c *= 1.0 + fac * 0.5;
    c *= 0.42 + 0.58 * pow(mu, 0.5);                       // 周辺減光
    c *= vec3(1.0, 0.94 + 0.06 * mu, 0.82 + 0.18 * mu);    // 縁ほど赤い
    if (flare > 0.0) {
        // フレア: 低〜中緯度の 1 か所が青白く強く光り、まわりに 2 本の光の帯（フレアリボン）が伸びる
        int n = int(flare);
        float a = fract(flare);
        float lon = hash1(ivec3(n, 17, 3)) * 6.2832, lat = (hash1(ivec3(n, 5, 11)) - 0.5) * 1.6;
        vec3 fc = vec3(cos(lat) * cos(lon), sin(lat), cos(lat) * sin(lon));
        vec3 side = normalize(cross(fc, vec3(0.0, 1.0, 0.0)));
        vec3 q = p - fc * dot(p, fc);
        float u = dot(q, side), v = length(q - side * u);
        float core = exp(-dot(p - fc, p - fc) / 0.004);
        float ribbon = exp(-pow((v - 0.035) / 0.012, 2.0)) * exp(-u * u / 0.006) * (0.6 + 0.4 * fbmL(p, 60.0, 3, 0.5, o + float(n)));
        c += vec3(1.3, 1.4, 1.7) * a * (core * 2.5 + ribbon * 1.2) * step(0.0, dot(p, fc));
    }
    return c;
}

// ドップラー効果（光速に近い時）: 進む向きの前方は青く明るく、後方は赤く暗く（背景の星と同じ見た目の近似）
vec3 doppler(vec3 c) {
    if (relBeta <= 0.0) return c;
    float ct = dot(normalize(fragWorld), relDir), ct2 = (ct + relBeta) / (1.0 + relBeta * ct);
    float D = sqrt(1.0 - relBeta * relBeta) / (1.0 - relBeta * ct2);
    float kb = clamp((D - 1.0) * 0.5, 0.0, 0.85), kr = clamp((1.0 - D) * 1.5, 0.0, 1.0);
    vec3 blue = mix(c, vec3(0.59, 0.75, 1.0) * max(max(c.r, c.g), c.b), kb);
    return mix(blue, vec3(1.0, 0.35, 0.24) * dot(c, vec3(0.333)), kr) * min(1.0, D * D * D);
}

void main() {
    int kind = int(params.x + 0.5);
    int style = int(detail.x + 0.5);
    vec3 V = normalize(-fragWorld);

    if (kind == 6) {   // 金星の雲・タイタンのもやの層を下から見た空（一様に明るい曇り空）
        finalColor = vec4(doppler(hazeSky), 1.0);
        return;
    }

    if (kind == 5) {   // タイタンの上空のもや（本体より一回り大きな殻。縁でだけ見える青い層）
        vec3 Nh = normalize(fragNormal);
        float muH = abs(dot(Nh, V));
        float litH = clamp(dot(Nh, normalize(lightDir)) + 0.35, 0.0, 1.0);
        float aH = pow(1.0 - muH, 6.0) * params.w * litH;
        if (aH <= 0.003) discard;
        finalColor = vec4(doppler(color1 * (0.4 + 0.6 * litH)), clamp(aH, 0.0, 1.0));
        return;
    }

    if (kind == 4) {   // 環
        float rkm = length(fragObj.xz) * params.y;
        float d = ringDensity(rkm);
        fw = length(fragWorld) * detail.y;
        // 観測（34 km ごと）より細かい環の筋（近づくほど細かく）
        float ringlets = 0.0, a = 0.5, f = 0.03;
        for (int i = 0; i < 9; i++) {
            float w = lodW(f * params.y);
            if (w <= 0.0) break;
            ringlets += a * w * vnoise(vec3(rkm * f, 0.0, float(i) * 3.1));
            a *= 0.6; f *= 2.7;
        }
        d *= 0.85 + 0.3 * ringlets;
        if (ringInfo.z > -0.5 && abs(rkm - 62932.0) < 60.0) d = 1.0 - exp(log(1.0 - min(d, 0.99)) * adamsArcs(fragObj));
        if (ringInfo.w > 0.5) {
            // 細い環は遠くで画素の中に薄まって消えるので、表の 1 目盛りより画素が大きいほど濃くする（写真で細い線に見えるように）
            float spread = clamp(fwidth(rkm) / (ringInfo.y / 4096.0), 1.0, 64.0);
            d = 1.0 - exp(log(1.0 - min(d, 0.99)) * pow(spread, 0.75));
        }
        if (d <= 0.002) discard;
        // 斜めに見るほど光が環を通る距離が長くなり、不透明になる
        vec3 Nr = normalize(mat3(matNormal) * vec3(0.0, 1.0, 0.0));
        float muV = abs(dot(normalize(-fragWorld), Nr));
        float tau = -log(1.0 - min(d, 0.993));
        float alpha = 1.0 - exp(-tau / max(muV, 0.03));
        vec3 Lo = normalize(lightDirObj);
        vec3 P = fragObj;
        float b = dot(P, Lo);
        float c = dot(P, P) - 1.0;
        float h = b * b - c;
        float shadow = (h > 0.0 && (-b - sqrt(h)) > 0.0) ? 0.06 : 1.0;
        float lit = 0.2 + 0.8 * pow(abs(Lo.y), 0.35);
        vec3 col = mix(color2, color1, clamp(d, 0.0, 1.0)) * lit * shadow;
        finalColor = vec4(doppler(col), clamp(alpha, 0.0, 1.0) * 0.97);
        return;
    }

    vec3 Ng = normalize(fragNormal);
    vec3 p = normalize(fragObj);
    vec3 o = vec3(params.z, params.z * 1.37, params.z * 0.71);
    float mu = max(dot(Ng, V), 0.0);
    fw = max(length(fragWorld) * detail.y / max(mu, 0.25), 1e-8);

    if (kind == 0) {
        finalColor = vec4(doppler(sun(p, mu, o, int(params.y + 0.5), params.w)), 1.0);
        return;
    }

    vec3 L = normalize(lightDir);
    float ndlG = dot(Ng, L);
    float eclRed;
    float sh = occShadow(fragWorld, L, eclRed);
    if (style == 21) {   // 土星: 環の影
        vec3 Lo = normalize(lightDirObj);
        if (abs(Lo.y) > 1e-4 && p.y * Lo.y < 0.0) {
            vec3 q = p - Lo * (p.y / Lo.y);
            sh *= 1.0 - 0.85 * clamp(ringDensity(length(q.xz) * 60268.0), 0.0, 1.0);
        }
    }
    vec3 base;
    vec4 hgt = vec4(0.0);
    float specMask = 0.0, lights = 0.0, cloud = 0.0;
    if (kind == 1) hgt = style == 13 ? moonTex(p, o, base) : style == 14 ? mapTex(p, o, base) : rocky(p, style, o, base);
    else if (kind == 2) {
        base = style == 14 ? gasMap(p, o) : gas(p, style, o);
        // 衝突痕: 黒っぽい芯と、そのまわりの暗い輪（大きな破片の痕で見られた形）
        for (int i = 0; i < 20; i++) {
            if (float(i) >= scarInfo.x) break;
            float r = scars[i].w;
            float a = acos(clamp(dot(p, scars[i].xyz), -1.0, 1.0)) / max(r, 1e-4);
            float core = 1.0 - smoothstep(0.35, 0.6, a), ring = exp(-pow((a - 0.95) / 0.12, 2.0)) * 0.6;
            float dk = clamp(core + ring, 0.0, 1.0) * scarInfo.y * (0.8 + 0.2 * fbmL(p, 40.0, 4, 0.5, o + float(i)));
            base = mix(base, vec3(0.16, 0.12, 0.10), dk);
        }
    }
    else if (style == 31) hgt = earthTex(p, o, ndlG, base, specMask, lights, cloud);
    else hgt = earth(p, o, ndlG, base, specMask, lights, cloud);

    // 高さの勾配で法線を揺らす（球面の接線成分だけ使う）
    vec3 gt = hgt.yzw - p * dot(hgt.yzw, p);
    vec3 n0 = normalize(mat3(matNormal) * p);
    vec3 nb = normalize(mat3(matNormal) * normalize(p - gt));
    vec3 N = normalize(Ng + (nb - n0));
    float ndl = max(dot(N, L), 0.0) * smoothstep(-0.06, 0.02, ndlG) * sh;
    ndl *= fragShade;   // 地形の影（頂点シェーダ）
    float muN = max(dot(N, V), 0.05);
    // 大気を通ってきた日光（太陽が真上の時を白として、低いほど道のりが長く弱まる。地球は青が散らされて朝夕は赤く、
    // 火星は塵が色によらず遮るのでほぼ白いまま暗くなる）
    float am = min(1.0 / (max(ndlG, 0.0) + 0.06), 7.0);
    vec3 sunTint = exp(-atmExt.xyz * max(am - 1.0, 0.0) - atmOzone * max(min(1.0 / (max(ndlG, 0.0) + 0.03), 25.0) - 1.0, 0.0));
    // 空からの光: 遮られた日光のうち散らされて地面へ向かう分（1 回散乱の目安。地球は青く、火星は塵が青を吸収して赤茶）。
    // 上を向いた面ほど多く受ける。太陽が真上の時の直射と空の光の和を色ごとに白とする（カメラが晴れた昼の光に色を合わせる
    // のと同じ。真昼の地面の色と明るさは今までと同じで、日陰は空の色、朝夕は直射が弱まって空の光の色に寄る）
    vec3 skyL = vec3(0.0), gnorm = vec3(1.0);
    if (atmExt.w > 0.0) {
        vec3 tz = exp(-atmExt.xyz);
        gnorm = 1.0 / (1.0 + atmExt.w * atmAlb * (1.0 - tz) / tz);
        float el = max(ndlG, 0.0) + 0.02 * smoothstep(-0.1, 0.0, ndlG);   // 日が沈んだ後も薄明の空がしばらく照らす
        skyL = atmExt.w * atmAlb * (1.0 - exp(-atmExt.xyz * am)) / tz * el * (0.5 + 0.5 * dot(N, n0)) * sh;
    }

    vec3 col;
    if (kind == 1 && style != 3) {
        // 大気の無い天体: ランベルトとロンメル・ゼーリガーの中間（満月が縁まで明るく見える）
        float ls = 2.0 * ndl / (ndl + muN);
        // 衝効果: 砂粒の影が太陽を背にすると隠れて急に明るくなる（Hapke の影隠しの項 B0 / (1 + tan(g/2) / h)。
        // 月の目安 B0 = 1, h = 0.06。位相角 45° で 1 になるよう割る）。大気のある天体（火星）は oppB0 = 0
        float tg = tan(acos(clamp(dot(L, V), -1.0, 1.0)) * 0.5);
        float opp = (1.0 + oppB0 / (1.0 + tg / 0.06)) / (1.0 + oppB0 / (1.0 + 0.41421 / 0.06));
        col = base * ((mix(ndl, ls * 0.5, 0.5) * 1.15 * opp * sunTint + skyL) * gnorm + 0.012);
    } else if (kind == 2) {
        col = base * (ndl * 1.05 + 0.015) * (0.8 + 0.2 * sqrt(mu));
    } else {
        col = base * ((ndl * 1.05 * sunTint + skyL) * gnorm + 0.012);
    }

    if (g_ice > 0.0) {
        // 氷と雪: 太陽の方を（逆光で）浅く見るほど明るい前方散乱（雪の反射の強い向き）と、なめらかな面の照り返し
        float fwdS = pow(max(-dot(L, V), 0.0), 3.0) * (1.0 - muN);
        float nh = max(dot(N, normalize(L + V)), 0.0);
        col += (base * fwdS * 1.5 + pow(nh, 60.0) * 0.12) * sunTint * ndl * g_ice;
    }

    if (kind == 3) {
        vec3 H = normalize(L + V);
        float nh = max(dot(N, H), 0.0);
        col += vec3(1.0, 0.96, 0.88) * sunTint * specMask * (pow(nh, 90.0) * 0.6 + pow(nh, 12.0) * 0.04) * step(0.0, ndlG) * sh;
        // 夜景の画像は 1 画素が数 km なので、地上近く（1 画素が数百 m より細かい）では引き伸ばされた光の塊になる。そこでは弱める
        lights *= mix(0.04, 1.0, smoothstep(1e-5, 8e-5, fw));
        col += vec3(1.0, 0.72, 0.38) * lights * 1.4 * (1.0 - smoothstep(-0.18, 0.02, ndlG)) * (1.0 - cloud);
        vec3 cl = max(ndlG, 0.0) * sh * sunTint + 0.01;
        col = mix(col, cl, cloud * 0.92);
        col = mix(col, atmColor * max(ndlG, 0.0), 0.12 * (1.0 - mu) * params.w);   // 大気による青み
    }

    if (hazeAir.x > 0.5) {
        // 金星の雲・タイタンのもやの下: 日光は散らされて空全体から届く（影はできず、上を向いた面ほど少し明るい）
        col = base * hazeSky * (0.75 + 0.25 * dot(N, n0)) * 1.6;
        // 液体の湖・海: 平らな液面に空の光が映る（液体メタンの屈折率 約 1.29 のフレネル反射。斜めに見るほど明るい）
        if (g_liquid > 0.0) {
            float fr = 0.016 + 0.984 * pow(1.0 - max(dot(n0, V), 0.0), 5.0);
            col = mix(col, hazeSky * fr, g_liquid);
        }
        // もや: 空気のレイリー散乱（地表の値 hazeBeta、高さ hazeBeta.w ごとに 1/e。視線の通る高さで平均）と、もやの粒の散乱。
        // 遠いほど地面の色が消え、空と同じ光になる（光が何度も散らされるので、散らされた光は空の明るさとする）
        float dist = length(fragWorld) * hazeAir.y, hc = hazeAir.z, hp = fragAlt, dh = hc - hp, H = hazeBeta.w;
        float rho = abs(dh) > 0.01 ? (exp(-hp / H) - exp(-hc / H)) * H / dh : exp(-hc / H);
        vec3 tr = exp(-(hazeBeta.rgb * rho + hazeAir.w) * dist);
        finalColor = vec4(doppler(col * tr + hazeSky * (1.0 - tr)), 1.0);
        return;
    }

    // 月食: 地球の大気で曲げられた赤い光がわずかに届く
    col += base * vec3(0.32, 0.10, 0.04) * eclRed * max(ndlG, 0.0);
    float rim = pow(1.0 - mu, 3.0) * params.w;
    col += atmColor * rim * clamp(ndlG + 0.25, 0.0, 1.0);

    // 露出は地面の照り返しだけに掛ける（かすみは空と同じ明るさのまま重ね、地平線で空とつながるように）
    col = expo(col);
    // 大気の中から見た地面のかすみ: 遠いほど地面の色が薄れ、その方向の空の色（SKY_FS と同じ近似。地面の向こうまで空気が
    // 続く時の明るさ）に近づく。地面の光が通り抜ける割合 T で、地面の色と空の色を混ぜる（空の光を足すだけだと、空の光は
    // 空気が少なくても頭打ちに近づくので、遠くの山肌に地面の光と空の光が両方乗り、地平線の空より明るく白く飛んでいた）
    if (atmHaze.x > 0.0) {
        float air = atmHaze.x * length(fragWorld) * atmHaze.y / atmHaze.z;
        vec3 v = normalize(fragWorld);
        float cosT = dot(v, L), sunEl = dot(camUp, L), mu = max(dot(v, camUp), 0.0);
        float airV = atmHaze.x / (mu + 0.06);
        float airS = min(atmHaze.x / (max(sunEl, 0.0) + 0.06), 7.0) * (1.0 - 0.7 * sqrt(mu));
        vec3 sunCol = exp(-atmBeta * airS - atmOzone * min(1.0 / (max(sunEl, 0.0) + 0.03), 25.0) * min(atmHaze.x * 4.0, 1.0));
        vec3 ray = sunCol * (1.0 - exp(-atmBeta * airV)) * 0.75 * (1.0 + cosT * cosT);
        float g = max(cosT, 0.0);
        vec3 mie = sunCol * atmMie * (0.12 * pow(g, 6.0) + 0.5 * pow(g, 60.0)) * (1.0 - exp(-0.3 * airV));
        vec3 sky = 1.0 - exp(-(ray * 3.0 + mie) * smoothstep(-0.17, 0.03, sunEl) * atmHaze.w);
        vec3 T = exp(-(atmBeta + 0.02) * air);
        col = col * T + sky * (1.0 - T);
    }
    finalColor = vec4(doppler(col), 1.0);
}
)";

// 大気の中から見た空の色（地球・火星）。カメラを囲む球に描き、視線の方向ごとに単一散乱の近似で色を求める:
//  - レイリー散乱（地球は青、火星は塵で黄褐色）: 視線方向の空気の量が多いほど、太陽の光の色で明るくなる
//  - ミー散乱: 太陽のまわりの明るい光（火星では夕方に青白くなる）
//  - 太陽の光は、太陽の方向の空気の量に応じて赤くなる（朝焼け・夕焼け）。太陽が沈むと暗くなる
// アルファは空の明るさから決め、昼は背景の星を隠し、夜は透明になる
static const char* SKY_VS = R"(
#version 330
in vec3 vertexPosition;
uniform mat4 mvp;
out vec3 fragDir;
void main() {
    fragDir = vertexPosition;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

static const char* SKY_FS = R"(
#version 330
in vec3 fragDir;
uniform vec3 up;        // その場所の真上
uniform vec3 sunDir;
uniform vec3 beta;      // 天頂方向の空気 1 つ分で散乱する割合（色ごと）
uniform vec3 mieTint;
uniform vec3 ozone;     // 天頂方向の空気 1 つ分でオゾンが吸収する割合（黄緑〜橙を吸収し、夕方の空を青紫に保つ）
uniform float density;  // 高さによる空気の濃さ（地表 1）
uniform float sunVis;   // 見えている太陽の割合（日食で欠けると減る）
out vec4 finalColor;
void main() {
    vec3 d = normalize(fragDir);
    float mu = max(dot(d, up), 0.0);
    float sunEl = dot(sunDir, up);
    float cosT = dot(d, sunDir);
    float airV = density / (mu + 0.06);
    // 地平線近くの太陽の光は、上空の薄い空気を通って散乱する分が多いので、空気の量に上限を付ける。
    // 高い空の光は上空で散乱したもので、太陽の光が通ってきた空気が少ない（夕方でも天頂は赤くならない）
    // オゾンの層は高い所にあるので、夕方は高い空の光も長い距離を通る（天頂が青紫になる）
    float airS = min(density / (max(sunEl, 0.0) + 0.06), 7.0) * (1.0 - 0.7 * sqrt(mu));
    float airO = min(1.0 / (max(sunEl, 0.0) + 0.03), 25.0) * min(density * 4.0, 1.0);
    vec3 sunCol = exp(-beta * airS - ozone * airO);
    vec3 ray = sunCol * (1.0 - exp(-beta * airV)) * 0.75 * (1.0 + cosT * cosT);
    float g = max(cosT, 0.0);
    vec3 mie = sunCol * mieTint * (0.12 * pow(g, 6.0) + 0.5 * pow(g, 60.0)) * (1.0 - exp(-0.3 * airV));
    float light = smoothstep(-0.17, 0.03, sunEl) * sunVis;   // 太陽が地平線の下 10° まで薄明が残る
    vec3 col = 1.0 - exp(-(ray * 3.0 + mie) * light);   // 明るい所が白く飛ばないように丸める
    float a = clamp(max(col.r, max(col.g, col.b)) * 2.5, 0.0, 1.0);
    finalColor = vec4(min(col / max(a, 1e-3), vec3(1.0)), a);
}
)";

// 小惑星帯などの粒（約 10 万個）: 軌道要素から GPU で位置を求め、画面に投影して 1 画素の点で描く。
// 位置は AU 単位の float（誤差は数十 km で点の位置には影響しない）。CPU の計算（UpdateAsteroids）と同じ式
static const char* BELT_VS = R"(
#version 330
in vec4 aOrbit;   // 軌道長半径 AU, 離心率, 平均運動 rad/日, J2000 での平均近点角 rad
in vec4 aP;       // 近日点の向き（描画座標）, 種類
in vec3 aQ;       // それに直交する向き
uniform float t;          // J2000 からの日数
uniform vec3 camPos;      // カメラの位置 AU（太陽中心）
uniform vec3 camRight;
uniform vec3 camUp;
uniform vec3 camFwd;
uniform vec3 view;        // 画面の幅, 高さ, 焦点距離（画素）
out vec4 vColor;
void main() {
    float e = aOrbit.y;
    float M = mod(aOrbit.w + aOrbit.z * t, 6.2831853);
    if (M > 3.1415927) M -= 6.2831853;
    float E = M + e * sin(M) * (1.0 + e * cos(M));
    int iters = e < 0.3 ? 2 : 6;
    for (int i = 0; i < iters; i++) E -= (E - e * sin(E) - M) / (1.0 - e * cos(E));
    vec3 pos = (aP.xyz * (cos(E) - e) + aQ * (sqrt(1.0 - e * e) * sin(E))) * aOrbit.x;
    vec3 rel = pos - camPos;
    float z = dot(rel, camFwd);
    gl_PointSize = 1.0;
    if (z <= 1e-9) { gl_Position = vec4(2.0, 2.0, 2.0, 1.0); vColor = vec4(0.0); return; }   // カメラの後ろ
    float sx = view.x * 0.5 + dot(rel, camRight) / z * view.z;
    float sy = view.y * 0.5 - dot(rel, camUp) / z * view.z;
    gl_Position = vec4(sx / view.x * 2.0 - 1.0, 1.0 - sy / view.y * 2.0, 0.0, 1.0);
    int k = int(aP.w + 0.5);
    vColor = k == 1 ? vec4(235.0, 150.0, 120.0, 150.0) / 255.0 : k == 2 ? vec4(160.0, 215.0, 150.0, 130.0) / 255.0
           : k == 3 ? vec4(150.0, 185.0, 235.0, 140.0) / 255.0 : vec4(205.0, 190.0, 165.0, 120.0) / 255.0;
}
)";

static const char* BELT_FS = R"(
#version 330
in vec4 vColor;
out vec4 finalColor;
void main() { finalColor = vColor; }
)";

// 左上のパネルの下の球のミニマップ（地表の近くを飛ぶ時）: 正射図法で、中心が今いる所、北が上。地図（正距円筒図法）を引く
static const char* MINIGLOBE_VS = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
uniform mat4 mvp;
out vec2 fragTexCoord;
void main() {
    fragTexCoord = vertexTexCoord;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";
static const char* MINIGLOBE_FS = R"(
#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform sampler2D texture1;   // 地球: 陸と海（地表のシェーダの earthData。b が陸、r が標高、a が海氷）
uniform vec4 miniCenter;   // x: 中心の緯度, y: 中心の経度（東経が正、rad）, z: 球の直径（画素）, w: 1 なら地球
uniform vec3 miniTint;     // 白黒の地図は天体の色で着色
uniform vec4 miniSun;      // xyz: 天体の座標系での太陽の向き, w: 薄明の幅（太陽高度の sin）
out vec4 finalColor;
void main() {
    vec2 q = vec2(fragTexCoord.x * 2.0 - 1.0, 1.0 - fragTexCoord.y * 2.0);   // 右が東、上が北
    float r = length(q), aa = 2.0 / miniCenter.z;
    if (r > 1.0 + aa) discard;
    vec2 qc = q / max(r, 1.0);
    float cz = sqrt(max(1.0 - dot(qc, qc), 0.0));   // 中心から見た角度の cos
    float s0 = sin(miniCenter.x), c0 = cos(miniCenter.x);
    float lat = asin(clamp(cz * s0 + qc.y * c0, -1.0, 1.0));
    float lon = miniCenter.y + atan(qc.x, cz * c0 - qc.y * s0);
    vec2 uv = vec2(fract(lon / 6.2831853 + 0.5), 0.5 - lat / 3.1415927);
    vec2 dx = dFdx(uv), dy = dFdy(uv);
    dx.x -= floor(dx.x + 0.5); dy.x -= floor(dy.x + 0.5);
    vec3 c = textureGrad(texture0, uv, dx, dy).rgb * miniTint;
    if (miniCenter.w > 0.5) {   // 地球の色の画像は海を周りの陸の色で埋めてあるので、海は陸と海のデータで塗る
        vec4 td = textureGrad(texture1, uv, dx, dy);
        float depth = clamp((127.0 - td.r * 255.0) / 60.0, 0.0, 1.0);
        vec3 oc = mix(mix(vec3(0.08, 0.24, 0.34), vec3(0.03, 0.07, 0.20), depth), vec3(0.88, 0.91, 0.95), td.a);
        c = mix(oc, c, smoothstep(0.45, 0.55, td.b));
    }
    // 昼と夜: 地図の緯度は面の向き（地球は測地緯度）なので、その向きと太陽の向きで太陽高度が決まる
    vec3 nrm = vec3(cos(lat) * cos(lon), sin(lat), -cos(lat) * sin(lon));
    float day = smoothstep(-miniSun.w, 0.0, dot(nrm, miniSun.xyz));
    c *= mix(vec3(0.50, 0.56, 0.72), vec3(1.0), day);   // 夜も地形が分かる明るさに
    c *= 0.55 + 0.45 * sqrt(cz);   // 縁を暗く
    finalColor = vec4(c, 0.7 * (1.0 - smoothstep(1.0 - aa, 1.0 + aa, r)));   // 少し透かして後ろの景色を見せる
}
)";
