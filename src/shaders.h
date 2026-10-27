// 天体描画用シェーダ (GLSL 330)。テクスチャ無しで手続き的に模様を生成する。
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
uniform vec3 lightDirObj;  // 環の座標系での太陽方向
uniform vec3 color1;
uniform vec3 color2;
uniform vec3 atmColor;
uniform vec4 params;       // x: 種別, y: 縞の数 / 環の基準半径km, z: シード, w: 大気の強さ
out vec4 finalColor;

float hash(vec3 p) {
    p = fract(p * 0.3183099 + vec3(0.1, 0.2, 0.3));
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}
float noise(vec3 x) {
    vec3 i = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hash(i + vec3(0,0,0)), hash(i + vec3(1,0,0)), f.x),
                   mix(hash(i + vec3(0,1,0)), hash(i + vec3(1,1,0)), f.x), f.y),
               mix(mix(hash(i + vec3(0,0,1)), hash(i + vec3(1,0,1)), f.x),
                   mix(hash(i + vec3(0,1,1)), hash(i + vec3(1,1,1)), f.x), f.y), f.z);
}
float fbm(vec3 p) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 5; i++) { v += a * noise(p); p *= 2.03; a *= 0.5; }
    return v;
}

// 土星の環の光学的厚さ（おおよそ）
float ringDensity(float r) {
    float d = 0.0;
    if (r < 74658.0) d = 0.0;
    else if (r < 92000.0) d = 0.22;        // C環
    else if (r < 117580.0) d = 0.92;       // B環
    else if (r < 122170.0) d = 0.06;       // カッシーニの間隙
    else if (r < 133423.0) d = 0.62;       // A環
    else if (r < 133745.0) d = 0.0;        // エンケの間隙
    else if (r < 136775.0) d = 0.55;       // A環外側
    float fine = noise(vec3(r * 0.004, 0.0, 0.0)) * 0.5 + noise(vec3(r * 0.0007, 3.0, 0.0)) * 0.5;
    return d * (0.65 + 0.35 * fine);
}

void main() {
    int kind = int(params.x + 0.5);
    vec3 V = normalize(-fragWorld);

    if (kind == 4) {   // 環
        float rkm = length(fragObj.xz) * params.y;
        float d = ringDensity(rkm);
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

    vec3 N = normalize(fragNormal);
    vec3 p = normalize(fragObj);
    float seed = params.z;
    float mu = max(dot(N, V), 0.0);

    if (kind == 0) {   // 恒星: 粒状斑 + 周辺減光
        float g = fbm(p * 28.0 + seed);
        vec3 c = mix(color2, color1, smoothstep(0.2, 0.8, g) * 0.8 + 0.2);
        c *= 0.45 + 0.55 * pow(mu, 0.45);
        finalColor = vec4(c, 1.0);
        return;
    }

    vec3 base;
    if (kind == 1) {          // 岩石天体
        float n = fbm(p * 3.0 + seed);
        float n2 = fbm(p * 14.0 + seed * 2.0);
        base = mix(color2, color1, clamp(smoothstep(0.3, 0.7, n) * 0.7 + n2 * 0.4, 0.0, 1.0));
    } else if (kind == 2) {   // ガス惑星: 縞模様
        float t = fbm(p * vec3(2.0, 9.0, 2.0) + seed);
        float b = sin(p.y * params.y * 3.14159 + t * 4.0) * 0.5 + 0.5;
        base = mix(color1, color2, b);
    } else {                  // 地球
        float n = fbm(p * 2.2 + seed);
        vec3 land = mix(color2, vec3(0.55, 0.47, 0.32), smoothstep(0.5, 0.75, fbm(p * 7.0 + 4.0)));
        base = (n > 0.53) ? land : color1;
        float ice = smoothstep(0.86, 0.9, abs(p.y));
        base = mix(base, vec3(0.93), ice);
        float cl = fbm(p * 5.0 + vec3(seed * 3.0, 1.0, 2.0));
        base = mix(base, vec3(1.0), smoothstep(0.55, 0.72, cl) * 0.85);
    }

    float ndl = max(dot(N, normalize(lightDir)), 0.0);
    vec3 col = base * (ndl * 1.05 + 0.015);
    float rim = pow(1.0 - mu, 3.0) * params.w;
    col += atmColor * rim * clamp(dot(N, normalize(lightDir)) + 0.25, 0.0, 1.0);
    finalColor = vec4(col, 1.0);
}
)";
