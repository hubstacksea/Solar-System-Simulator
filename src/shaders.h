// 天体描画用シェーダ (GLSL 330)。テクスチャ無しで手続き的に模様を生成する。
//  - 近づくほど細かいオクターブを足す（画素より小さい模様は打ち切る）ので、地表付近でも模様が潰れない
//  - 地形の高さの勾配を解析的に求めて法線を揺らし、山やクレーターの陰影を付ける
//  - 天体ごとの特徴（月の海、火星の極冠、木星の大赤斑、地球の大陸・雲・夜景など）を detail.x で描き分ける
#pragma once

static const char* PLANET_VS = R"(
#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
out vec3 fragObj;
out vec3 fragNormal;
out vec3 fragWorld;
void main() {
    fragObj = vertexPosition;
    fragWorld = vec3(matModel * vec4(vertexPosition, 1.0));
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

static const char* PLANET_FS = R"(
#version 330
in vec3 fragObj;
in vec3 fragNormal;
in vec3 fragWorld;
uniform vec3 lightDir;     // ワールド空間（カメラ相対）での太陽方向
uniform vec3 lightDirObj;  // 本体（自転込み）/ 環の座標系での太陽方向
uniform vec4 occ[4];       // 影を落とす天体: xyz 中心（fragWorld と同じ座標）, w 半径（負なら大気あり）
uniform int occN;
uniform float sunAng;      // 太陽の視半径 rad
uniform vec3 color1;
uniform vec3 color2;
uniform vec3 atmColor;
uniform vec4 params;       // x: 種別, y: 縞の数 / 環の基準半径km, z: シード, w: 大気の強さ
uniform vec4 detail;       // x: 表面模様の種類, y: 1 画素の大きさの係数, z: 時刻（日）, w: 雲の濃さ
uniform mat4 matNormal;
uniform sampler2D texture0;  // 地球・月: 地表の色 (Blue Marble / LROC)
uniform sampler2D texture1;  // 地球: R 標高・水深, G 夜景, B 海陸マスク, A 海氷 / 月: R 標高上位, A 下位
out vec4 finalColor;

const float PI = 3.14159265;
float fw;      // 1 画素の大きさ（単位球の長さ）
float g_ray;   // 新しいクレーターの明るさ（光条）

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

// クレーター 1 層 (x: 高さ[半径単位], yzw: 勾配)。お椀形の凹みと盛り上がった縁
vec4 craterLayer(vec3 p, float F, float density, vec3 salt) {
    vec3 d; float r, rnd;
    if (!cellSpot(p, F, density, salt, 0.08, 0.29, d, r, rnd)) return vec4(0.0);
    float dist = length(d);
    float x = dist / r;
    if (x >= 1.7) return vec4(0.0);
    float h, dh;
    if (x < 1.0) { h = x * x - 0.75; dh = 2.0 * x; }
    else { float t = (x - 1.0) * 3.5; float e = exp(-t * t); h = 0.25 * e; dh = -0.25 * 2.0 * t * 3.5 * e; }
    float w = lodW(F / (2.0 * r));
    float depth = 0.4 * r;
    // 若いクレーターほど明るい（内部と周囲の噴出物）
    if (rnd > 0.75) g_ray += w * (x < 1.0 ? 0.5 : 0.8 * exp(-(x - 1.0) * 3.0)) * (rnd - 0.75) * 4.0;
    else if (x < 1.0) g_ray -= w * 0.08;
    vec3 dir = dist > 1e-6 ? d / dist : vec3(0.0);
    return w * vec4(h * depth / F, dh / r * depth * dir);
}

vec4 craters(vec3 p, float density, float F0, vec3 salt) {
    vec4 s = vec4(0.0);
    float F = F0;
    for (int i = 0; i < 16; i++) {
        if (lodW(F * 1.7) <= 0.0) break;
        s += craterLayer(p, F, density, salt + float(i) * 31.7);
        F *= 2.2;
    }
    return s;
}

// 緯度・経度 (lon は -PI..PI)
vec2 latLon(vec3 p) { return vec2(asin(clamp(p.y, -1.0, 1.0)), atan(p.z, p.x)); }
float lonDiff(float a, float b) { float d = a - b; return d - 2.0 * PI * floor((d + PI) / (2.0 * PI)); }

// ---------------------------------------------------------------- 岩石・氷天体
// base: 地表の色, 戻り値: 高さと勾配
vec4 rocky(vec3 p, int style, vec3 o, out vec3 base) {
    g_ray = 0.0;
    float macro = fbmL(p, 2.2, 5, 0.5, o + 3.0);
    float rough = 1.0, dens = 0.55;
    float mare = 0.0;
    if (style == 1 || style == 2) {   // 月・水星: 海（平らで暗い溶岩平原）
        mare = smoothstep(0.05, 0.25, fbmL(p, 1.3, 4, 0.55, o + 9.0));
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

    // 冥王星のハート（スプートニク平原）は平らでクレーターが無い
    float heart = 0.0;
    if (style == 10) {
        vec3 h0 = normalize(vec3(0.82, 0.30, 0.48)), h1 = normalize(vec3(0.70, 0.12, 0.70));
        float edge = fbmL(p, 9.0, 4, 0.5, o + 21.0) * 0.012;
        heart = max(smoothstep(0.955, 0.965, dot(p, h0) + edge), smoothstep(0.935, 0.945, dot(p, h1) + edge));
        rough *= 1.0 - 0.85 * heart;
    }

    vec4 T = fbmD(p, 3.0, 26, 0.48, o) * (0.022 * rough);
    vec4 C = dens > 0.0 ? craters(p, dens * (1.0 - heart), 5.0, o * 0.37) : vec4(0.0);
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
    } else if (style == 9) {   // イアペトゥス: 先行半球が真っ黒な二色の衛星
        float dark = smoothstep(-0.12, 0.12, p.x + fbmL(p, 5.0, 5, 0.5, o) * 0.15);
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

// 高さコード → m。陸 128..255 = 0..8900 m、海 127..0 = 0..-11000 m（平方根で詰めてある）
float earthHeightM(float c) {
    float v = c * 255.0;
    float t = v >= 127.5 ? (v - 128.0) / 127.0 : (127.0 - v) / 127.0;
    return v >= 127.5 ? t * t * 8900.0 : -t * t * 11000.0;
}

// 実データの地球。大陸・標高・夜景は NASA のデータ、テクスチャより細かい部分は手続きで補う
vec4 earthTex(vec3 p, vec3 o, float ndlG, out vec3 base, out float specMask, out float lights, out float cloud) {
    // 本体座標: X 軸がグリニッジ子午線、Y 軸が北極、東経は -Z 方向
    float lat = asin(clamp(p.y, -1.0, 1.0));
    float lon = atan(-p.z, p.x);
    vec2 uv = vec2(lon / (2.0 * PI) + 0.5, 0.5 - lat / PI);
    // 経度 ±180° の継ぎ目で微分が跳ばないようにする
    vec2 dx = dFdx(uv), dy = dFdy(uv);
    dx.x -= floor(dx.x + 0.5); dy.x -= floor(dy.x + 0.5);
    vec3 tc = textureGrad(texture0, uv, dx, dy).rgb;
    vec4 td = textureGrad(texture1, uv, dx, dy);

    // 標高の勾配（中心差分。間隔は 1 テクセルか 1 画素の大きい方）
    vec2 texel = vec2(1.0 / 4096.0, 1.0 / 2048.0);
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

    // 海岸線: ぼかしたマスクの 0.5 を境にし、テクセルより細かい所はノイズで入り組ませる
    float coastNoise = fbmL(p, 700.0, 16, 0.55, o + 41.0);
    float m = td.b + coastNoise * 0.22;
    float aa = max(fwidth(m), 0.002);
    float land = smoothstep(0.5 - aa, 0.5 + aa, m);

    // 陸の色: テクスチャ（海の部分は周囲の陸の色で埋めてあるので、海岸でも青が混ざらない）
    vec3 lc = tc * (0.88 + 0.24 * (fbmL(p, 700.0, 20, 0.5, o + 5.0) * 0.5 + 0.5));
    // 海の色: 水深で浅瀬を明るく。海氷はデータの A
    float depth = clamp(-hm / 3000.0, 0.0, 1.0);
    vec3 oc = mix(vec3(0.08, 0.24, 0.34), vec3(0.03, 0.07, 0.20), smoothstep(0.0, 0.4, depth));
    float ice = td.a * (0.85 + 0.3 * fbmL(p, 900.0, 10, 0.5, o + 47.0));
    oc = mix(oc, vec3(0.88, 0.91, 0.95), clamp(ice, 0.0, 1.0));
    base = mix(oc, lc, land);
    specMask = (1.0 - land) * (1.0 - ice);

    // 夜景: 実データ。近づくと街の明かりの粒に分かれる
    lights = td.g * land * clamp(1.0 + 1.2 * fbmL(p, 900.0, 12, 0.6, o + 13.0), 0.0, 2.0);
    cloud = earthClouds(p, o, 0.12);   // 実データの地表が見えるよう少なめ

    // 起伏: データの勾配 + テクセルより細かい山肌（標高が高いほど険しい）
    float rough = (0.03 + 0.3 * clamp(hm / 4000.0, 0.0, 1.0)) / 1400.0;
    vec4 h = vec4(hm / R * EXAG, grad) * land + fbmD(p, 700.0, 18, 0.5, o) * (rough * land);
    if (land < 1.0) h += fbmD(p, 20000.0, 14, 0.55, o + 31.0) * (1.5e-6 * (1.0 - land));
    return h;
}

// 土星の環の密度（中心からの距離 km）
float ringDensity(float rkm) {
    if (rkm < 74658.0) return 0.0;
    if (rkm < 92000.0) return 0.22;     // C環
    if (rkm < 117580.0) return 0.92;    // B環
    if (rkm < 122170.0) return 0.06;    // カッシーニの間隙
    if (rkm < 133423.0) return 0.62;    // A環
    if (rkm < 133745.0) return 0.0;     // エンケの間隙
    if (rkm < 136775.0) return 0.55;    // A環外側
    return 0.0;
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

vec4 moonTex(vec3 p, vec3 o, out vec3 base) {
    g_ray = 0.0;
    // 本体座標: X 軸が経度 0°（地球を向く側）、Y 軸が北極、東経は -Z 方向
    float lat = asin(clamp(p.y, -1.0, 1.0));
    float lon = atan(-p.z, p.x);
    vec2 uv = vec2(lon / (2.0 * PI) + 0.5, 0.5 - lat / PI);
    vec2 dx = dFdx(uv), dy = dFdy(uv);
    dx.x -= floor(dx.x + 0.5); dy.x -= floor(dy.x + 0.5);
    vec3 tc = pow(textureGrad(texture0, uv, dx, dy).rgb, vec3(1.3)) * 1.25;   // 海と高地の明暗差を少し強調

    vec2 texel = vec2(1.0 / 2048.0, 1.0 / 1024.0);
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

    // 海（暗い所）はクレーターが少なく平ら
    float dark = 1.0 - smoothstep(0.12, 0.3, dot(tc, vec3(0.333)));
    vec4 C = craters(p, mix(0.55, 0.2, dark), 260.0, o * 0.37);
    vec4 T = fbmD(p, 600.0, 20, 0.48, o) * (mix(0.004, 0.0015, dark) / 260.0);
    base = tc * (0.92 + 0.16 * (fbmL(p, 900.0, 16, 0.5, o + 5.0) * 0.5 + 0.5)) * (1.0 + g_ray * 0.35);
    return vec4(hm / R * EXAG, grad) + C + T;
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
vec3 sun(vec3 p, float mu, vec3 o) {
    float g = fbmL(p, 45.0, 18, 0.55, o);                 // 粒状斑
    float sg = fbmL(p, 6.0, 4, 0.5, o + 3.0);             // 超粒状斑
    vec3 c = mix(color2, color1, clamp(0.65 + g * 0.45 + sg * 0.15, 0.0, 1.0));
    // 黒点（低緯度帯）
    for (int k = 0; k < 2; k++) {
        vec3 d; float r, rnd;
        if (cellSpot(p, 9.0 * float(k + 1), 0.35, o + float(k) * 5.3, 0.06, 0.2, d, r, rnd)) {
            float x = length(d) / r;
            float belt = smoothstep(0.08, 0.15, abs(p.y)) * (1.0 - smoothstep(0.45, 0.55, abs(p.y)));
            c = mix(c, c * 0.55, (1.0 - smoothstep(0.8, 1.0, x)) * belt);
            c = mix(c, vec3(0.25, 0.10, 0.03), (1.0 - smoothstep(0.35, 0.5, x)) * belt);
        }
    }
    // 白斑（周辺部で目立つ）
    float fac = smoothstep(0.2, 0.55, fbmL(p, 18.0, 5, 0.5, o + 7.0)) * pow(1.0 - mu, 2.0);
    c *= 1.0 + fac * 0.5;
    c *= 0.42 + 0.58 * pow(mu, 0.5);                       // 周辺減光
    c *= vec3(1.0, 0.94 + 0.06 * mu, 0.82 + 0.18 * mu);    // 縁ほど赤い
    return c;
}

void main() {
    int kind = int(params.x + 0.5);
    int style = int(detail.x + 0.5);
    vec3 V = normalize(-fragWorld);

    if (kind == 4) {   // 環
        float rkm = length(fragObj.xz) * params.y;
        float d = ringDensity(rkm);
        fw = length(fragWorld) * detail.y;
        // 細い環の筋（近づくほど細かく）
        float ringlets = 0.0, a = 0.5, f = 0.004;
        for (int i = 0; i < 9; i++) {
            float w = lodW(f * params.y);
            if (w <= 0.0) break;
            ringlets += a * w * vnoise(vec3(rkm * f, 0.0, float(i) * 3.1));
            a *= 0.6; f *= 2.7;
        }
        d *= 0.55 + 0.6 * ringlets;
        if (d <= 0.002) discard;
        vec3 Lo = normalize(lightDirObj);
        vec3 P = fragObj;
        float b = dot(P, Lo);
        float c = dot(P, P) - 1.0;
        float h = b * b - c;
        float shadow = (h > 0.0 && (-b - sqrt(h)) > 0.0) ? 0.06 : 1.0;
        float lit = 0.2 + 0.8 * pow(abs(Lo.y), 0.35);
        vec3 col = mix(color2, color1, clamp(d, 0.0, 1.0)) * lit * shadow;
        finalColor = vec4(col, clamp(d, 0.0, 1.0) * 0.95);
        return;
    }

    vec3 Ng = normalize(fragNormal);
    vec3 p = normalize(fragObj);
    vec3 o = vec3(params.z, params.z * 1.37, params.z * 0.71);
    float mu = max(dot(Ng, V), 0.0);
    fw = max(length(fragWorld) * detail.y / max(mu, 0.25), 1e-8);

    if (kind == 0) {
        finalColor = vec4(sun(p, mu, o), 1.0);
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
    if (kind == 1) hgt = style == 13 ? moonTex(p, o, base) : rocky(p, style, o, base);
    else if (kind == 2) base = gas(p, style, o);
    else if (style == 31) hgt = earthTex(p, o, ndlG, base, specMask, lights, cloud);
    else hgt = earth(p, o, ndlG, base, specMask, lights, cloud);

    // 高さの勾配で法線を揺らす（球面の接線成分だけ使う）
    vec3 gt = hgt.yzw - p * dot(hgt.yzw, p);
    vec3 n0 = normalize(mat3(matNormal) * p);
    vec3 nb = normalize(mat3(matNormal) * normalize(p - gt));
    vec3 N = normalize(Ng + (nb - n0));
    float ndl = max(dot(N, L), 0.0) * smoothstep(-0.06, 0.02, ndlG) * sh;
    float muN = max(dot(N, V), 0.05);

    vec3 col;
    if (kind == 1 && style != 3) {
        // 大気の無い天体: ランベルトとロンメル・ゼーリガーの中間（満月が縁まで明るく見える）
        float ls = 2.0 * ndl / (ndl + muN);
        col = base * (mix(ndl, ls * 0.5, 0.5) * 1.15 + 0.012);
    } else if (kind == 2) {
        col = base * (ndl * 1.05 + 0.015) * (0.8 + 0.2 * sqrt(mu));
    } else {
        col = base * (ndl * 1.05 + 0.012);
    }

    if (kind == 3) {
        vec3 H = normalize(L + V);
        float nh = max(dot(N, H), 0.0);
        col += vec3(1.0, 0.96, 0.88) * specMask * (pow(nh, 90.0) * 0.6 + pow(nh, 12.0) * 0.04) * step(0.0, ndlG) * sh;
        col += vec3(1.0, 0.72, 0.38) * lights * 1.4 * (1.0 - smoothstep(-0.18, 0.02, ndlG)) * (1.0 - cloud);
        float cl = max(ndlG, 0.0) * sh + 0.01;
        col = mix(col, vec3(cl), cloud * 0.92);
        col = mix(col, atmColor * max(ndlG, 0.0), 0.12 * (1.0 - mu) * params.w);   // 大気による青み
    }

    // 月食: 地球の大気で曲げられた赤い光がわずかに届く
    col += base * vec3(0.16, 0.05, 0.02) * eclRed * max(ndlG, 0.0);
    float rim = pow(1.0 - mu, 3.0) * params.w;
    col += atmColor * rim * clamp(ndlG + 0.25, 0.0, 1.0);
    finalColor = vec4(col, 1.0);
}
)";
