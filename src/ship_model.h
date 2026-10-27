// 自機モデル: 手続き的に生成するメッシュと専用シェーダ
//  単位: m、機首は -Z、上は +Y
//  マテリアル番号は texcoord.x に入れる（0: 塗装, 1: 金属, 2: ガラス, 3: ノズル発光, 4: 航法灯）
#pragma once
#include "raylib.h"
#include "raymath.h"
#include <cmath>
#include <vector>

enum ShipMat { SM_PAINT = 0, SM_METAL = 1, SM_GLASS = 2, SM_NOZZLE = 3, SM_NAVLIGHT = 4 };

class ShipMeshBuilder {
public:
    // 格子状の曲面。P[r][c] を三角形でつなぎ、法線は隣接面の平均（滑らか）
    void Grid(const std::vector<std::vector<Vector3>>& P, bool wrap, const std::vector<std::vector<Color>>& C, ShipMat m) {
        int rows = (int)P.size(), cols = (int)P[0].size();
        int base = (int)pos.size();
        for (int r = 0; r < rows; r++)
            for (int c = 0; c < cols; c++) Add(P[r][c], {0, 0, 0}, C[r][c], m);
        int cmax = wrap ? cols : cols - 1;
        for (int r = 0; r + 1 < rows; r++) {
            for (int c = 0; c < cmax; c++) {
                int c1 = (c + 1) % cols;
                int a = base + r * cols + c, b = base + r * cols + c1, d = base + (r + 1) * cols + c, e = base + (r + 1) * cols + c1;
                SmoothTri(a, b, e);
                SmoothTri(a, e, d);
            }
        }
        for (int i = base; i < (int)pos.size(); i++) nrm[i] = Vector3Normalize(nrm[i]);
    }

    // 平面の円板（ノズル・吸気口など）
    void Disc(Vector3 center, Vector3 axis, float radius, int segs, Color c, ShipMat m) {
        axis = Vector3Normalize(axis);
        Vector3 t = fabsf(axis.y) < 0.9f ? Vector3{0, 1, 0} : Vector3{1, 0, 0};
        Vector3 u = Vector3Normalize(Vector3CrossProduct(axis, t)), v = Vector3CrossProduct(axis, u);
        int ci = Add(center, axis, c, m);
        int first = (int)pos.size();
        for (int i = 0; i <= segs; i++) {
            float a = 2 * PI * i / segs;
            Add(Vector3Add(center, Vector3Add(Vector3Scale(u, cosf(a) * radius), Vector3Scale(v, sinf(a) * radius))), axis, c, m);
        }
        for (int i = 0; i < segs; i++) Tri(ci, first + i, first + i + 1);
    }

    // z 軸に沿った回転体。profile は (z, 半径) の列
    void Revolve(float cx, float cy, const std::vector<Vector2>& profile, int segs, Color c, ShipMat m) {
        std::vector<std::vector<Vector3>> P;
        std::vector<std::vector<Color>> C;
        for (const Vector2& zr : profile) {
            std::vector<Vector3> row;
            for (int s = 0; s < segs; s++) {
                float a = 2 * PI * s / segs;
                row.push_back({cx + cosf(a) * zr.y, cy + sinf(a) * zr.y, zr.x});
            }
            P.push_back(row);
            C.push_back(std::vector<Color>(segs, c));
        }
        Grid(P, true, C, m);
    }

    // 翼型断面を持つ翼面。root から spanDir 方向へ span だけ伸び、翼弦は z 方向
    void Wing(Vector3 root, Vector3 spanDir, Vector3 thickDir, float span, float leadRoot, float trailRoot, float leadTip,
              float trailTip, float thickRoot, float thickTip, Color c, ShipMat m) {
        const int N = 12, S = 8;
        std::vector<std::vector<Vector3>> P;
        std::vector<std::vector<Color>> C;
        for (int j = 0; j <= S; j++) {
            float v = (float)j / S;
            float lead = leadRoot + (leadTip - leadRoot) * v, trail = trailRoot + (trailTip - trailRoot) * v;
            float th = thickRoot + (thickTip - thickRoot) * v;
            Vector3 o = Vector3Add(root, Vector3Scale(spanDir, span * v));
            std::vector<Vector3> row;
            for (int k = 0; k <= 2 * N; k++) {
                // 後縁(上面) → 前縁 → 後縁(下面) の順に一周。前縁付近を細かくする
                float s = k <= N ? 1.0f - (float)k / N : (float)(k - N) / N;
                float u = 0.5f * (1 - cosf(PI * s));
                float yt = 5 * th * (0.2969f * sqrtf(u) - 0.126f * u - 0.3516f * u * u + 0.2843f * u * u * u - 0.1036f * u * u * u * u);
                float side = k <= N ? 1.0f : -1.0f;
                Vector3 p = Vector3Add(o, Vector3Scale(thickDir, yt * side));
                p.z = lead + (trail - lead) * u;
                row.push_back(p);
            }
            P.push_back(row);
            C.push_back(std::vector<Color>(2 * N + 1, c));
        }
        Grid(P, false, C, m);
    }

    Mesh Build() {
        Mesh mesh = {0};
        mesh.vertexCount = (int)pos.size();
        mesh.triangleCount = (int)idx.size() / 3;
        mesh.vertices = (float*)MemAlloc(mesh.vertexCount * 3 * sizeof(float));
        mesh.normals = (float*)MemAlloc(mesh.vertexCount * 3 * sizeof(float));
        mesh.texcoords = (float*)MemAlloc(mesh.vertexCount * 2 * sizeof(float));
        mesh.colors = (unsigned char*)MemAlloc(mesh.vertexCount * 4);
        mesh.indices = (unsigned short*)MemAlloc(idx.size() * sizeof(unsigned short));
        for (int i = 0; i < mesh.vertexCount; i++) {
            mesh.vertices[i * 3] = pos[i].x; mesh.vertices[i * 3 + 1] = pos[i].y; mesh.vertices[i * 3 + 2] = pos[i].z;
            mesh.normals[i * 3] = nrm[i].x; mesh.normals[i * 3 + 1] = nrm[i].y; mesh.normals[i * 3 + 2] = nrm[i].z;
            mesh.texcoords[i * 2] = (float)mat[i]; mesh.texcoords[i * 2 + 1] = 0;
            mesh.colors[i * 4] = col[i].r; mesh.colors[i * 4 + 1] = col[i].g; mesh.colors[i * 4 + 2] = col[i].b; mesh.colors[i * 4 + 3] = col[i].a;
        }
        for (size_t i = 0; i < idx.size(); i++) mesh.indices[i] = idx[i];
        UploadMesh(&mesh, false);
        return mesh;
    }

private:
    std::vector<Vector3> pos, nrm;
    std::vector<Color> col;
    std::vector<int> mat;
    std::vector<unsigned short> idx;

    int Add(Vector3 p, Vector3 n, Color c, ShipMat m) {
        pos.push_back(p); nrm.push_back(n); col.push_back(c); mat.push_back(m);
        return (int)pos.size() - 1;
    }
    void Tri(int a, int b, int c) { idx.push_back((unsigned short)a); idx.push_back((unsigned short)b); idx.push_back((unsigned short)c); }
    void SmoothTri(int a, int b, int c) {
        Tri(a, b, c);
        Vector3 n = Vector3CrossProduct(Vector3Subtract(pos[b], pos[a]), Vector3Subtract(pos[c], pos[a]));
        nrm[a] = Vector3Add(nrm[a], n); nrm[b] = Vector3Add(nrm[b], n); nrm[c] = Vector3Add(nrm[c], n);
    }
};

// 胴体断面（超楕円）の寸法表: z, 半幅, 半高, 中心の高さ
struct HullStation { float z, w, h, cy; };

inline Mesh GenShipMesh() {
    ShipMeshBuilder mb;
    const Color paint = {214, 218, 226, 255}, accent = {205, 62, 44, 255}, antiGlare = {48, 52, 60, 255};
    const Color metal = {92, 96, 106, 255}, dark = {28, 30, 34, 255}, glass = {20, 34, 52, 255};

    // ---- 胴体: 断面を補間しながらロフト
    const HullStation st[] = {
        {-19.0f, 0.02f, 0.02f, -0.30f}, {-18.2f, 0.55f, 0.42f, -0.26f}, {-16.5f, 1.05f, 0.82f, -0.16f}, {-13.5f, 1.55f, 1.20f, -0.05f},
        {-9.5f, 1.90f, 1.48f, 0.0f},    {-2.0f, 2.20f, 1.50f, 0.0f},    {5.0f, 2.30f, 1.40f, 0.0f},     {10.0f, 1.95f, 1.15f, 0.08f},
        {12.0f, 1.60f, 0.95f, 0.12f},
    };
    const int NS = sizeof(st) / sizeof(st[0]);
    const int rings = 64, segs = 40;
    std::vector<std::vector<Vector3>> P;
    std::vector<std::vector<Color>> C;
    for (int r = 0; r <= rings; r++) {
        float z = st[0].z + (st[NS - 1].z - st[0].z) * r / rings;
        int k = 0;
        while (k < NS - 2 && z > st[k + 1].z) k++;
        float t = (z - st[k].z) / (st[k + 1].z - st[k].z);
        t = t * t * (3 - 2 * t);
        float w = st[k].w + (st[k + 1].w - st[k].w) * t, h = st[k].h + (st[k + 1].h - st[k].h) * t;
        float cy = st[k].cy + (st[k + 1].cy - st[k].cy) * t;
        std::vector<Vector3> row;
        std::vector<Color> crow;
        for (int s = 0; s < segs; s++) {
            float a = 2 * PI * s / segs, ca = cosf(a), sa = sinf(a);
            const float n = 2.6f;  // 超楕円の指数（角の丸い箱型）
            float x = w * copysignf(powf(fabsf(ca), 2 / n), ca), y = h * copysignf(powf(fabsf(sa), 2 / n), sa) + cy;
            row.push_back({x, y, z});
            Color c = paint;
            if (z > -12.6f && z < -11.6f) c = accent;                         // 機首の帯
            if (z > -17.5f && z < -11.0f && sa > 0.55f) c = antiGlare;        // 防眩塗装
            if (z > 1.0f && z < 9.0f && fabsf(ca) > 0.92f && sa < 0.2f && sa > -0.4f) c = accent;  // 側面のライン
            crow.push_back(c);
        }
        P.push_back(row);
        C.push_back(crow);
    }
    mb.Grid(P, true, C, SM_PAINT);
    mb.Disc({0, 0.12f, 12.0f}, {0, 0, 1}, 1.2f, 24, dark, SM_METAL);

    // ---- キャノピー（上半分の楕円体）
    {
        std::vector<std::vector<Vector3>> CP;
        std::vector<std::vector<Color>> CC;
        const int cr = 12, cs = 28;
        for (int r = 0; r <= cr; r++) {
            float th = (PI * 0.5f) * r / cr;  // 0: 頂点 → π/2: 縁
            std::vector<Vector3> row;
            for (int s = 0; s < cs; s++) {
                float a = 2 * PI * s / cs;
                row.push_back({1.05f * sinf(th) * cosf(a), 1.05f + 0.95f * cosf(th), -9.2f + 3.4f * sinf(th) * sinf(a)});
            }
            CP.push_back(row);
            CC.push_back(std::vector<Color>(cs, glass));
        }
        mb.Grid(CP, true, CC, SM_GLASS);
    }

    for (int side = -1; side <= 1; side += 2) {
        float sx = (float)side;
        // ---- 主翼（わずかな下反角）
        Vector3 span = Vector3Normalize({sx, -0.06f, 0});
        mb.Wing({1.8f * sx, -0.35f, 0}, span, {0, 1, 0}, 10.8f, -4.0f, 10.5f, 6.4f, 9.6f, 0.75f, 0.22f, paint, SM_PAINT);
        Vector3 tip = Vector3Add({1.8f * sx, -0.35f, 0}, Vector3Scale(span, 10.8f));

        // ---- 翼端ポッドと航法灯（左: 赤, 右: 緑）
        mb.Revolve(tip.x, tip.y, {{3.2f, 0.02f}, {3.6f, 0.28f}, {4.6f, 0.42f}, {9.5f, 0.42f}, {10.8f, 0.25f}, {11.2f, 0.02f}}, 14, metal, SM_METAL);
        Color nav = side < 0 ? Color{255, 40, 30, 255} : Color{40, 255, 90, 255};
        mb.Revolve(tip.x, tip.y, {{2.75f, 0.01f}, {2.85f, 0.16f}, {3.1f, 0.22f}, {3.35f, 0.16f}, {3.45f, 0.01f}}, 10, nav, SM_NAVLIGHT);

        // ---- エンジンナセル
        float nx = 3.35f * sx, ny = -0.3f;
        mb.Revolve(nx, ny, {{1.6f, 0.95f}, {1.8f, 1.12f}, {2.6f, 1.22f}, {6.0f, 1.28f}, {11.0f, 1.26f}, {13.2f, 1.12f}, {14.2f, 1.0f}, {14.4f, 0.98f}},
                   28, metal, SM_METAL);
        mb.Disc({nx, ny, 1.65f}, {0, 0, -1}, 0.92f, 24, dark, SM_METAL);    // 吸気口
        mb.Disc({nx, ny, 14.35f}, {0, 0, 1}, 0.9f, 28, {255, 255, 255, 255}, SM_NOZZLE);
        // ノズル外周のリング
        mb.Revolve(nx, ny, {{14.2f, 1.02f}, {14.6f, 1.06f}, {15.0f, 0.98f}}, 28, dark, SM_METAL);

        // ---- 尾翼（外側へ傾けた双垂直尾翼）
        float cant = 0.32f;
        Vector3 finSpan = {sinf(cant) * sx, cosf(cant), 0}, finThick = {cosf(cant) * sx, -sinf(cant), 0};
        mb.Wing({1.25f * sx, 1.05f, 0}, finSpan, finThick, 4.6f, 4.6f, 11.8f, 9.4f, 12.4f, 0.3f, 0.12f, paint, SM_PAINT);
    }
    return mb.Build();
}

static const char* SHIP_VS = R"(
#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec2 vertexTexCoord;
in vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
out vec3 fragObj;
out vec3 fragWorld;
out vec3 fragNormal;
out vec4 fragColor;
flat out int fragMat;
void main() {
    fragObj = vertexPosition;
    fragWorld = vec3(matModel * vec4(vertexPosition, 1.0));
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));
    fragColor = vertexColor;
    fragMat = int(vertexTexCoord.x + 0.5);
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

static const char* SHIP_FS = R"(
#version 330
in vec3 fragObj;
in vec3 fragWorld;
in vec3 fragNormal;
in vec4 fragColor;
flat in int fragMat;
uniform vec3 sunDir;      // ワールド（カメラ相対）での太陽方向
uniform vec3 sunColor;    // 影に入ると 0
uniform vec3 fillDir;     // 近くの天体からの照り返し
uniform vec3 fillColor;
uniform vec3 engineColor;
uniform float engineGlow; // 0..1
uniform float navBlink;   // 0..1
out vec4 finalColor;

void main() {
    vec3 N = normalize(fragNormal);
    if (!gl_FrontFacing) N = -N;
    vec3 V = normalize(-fragWorld);
    vec3 L = normalize(sunDir);
    vec3 H = normalize(L + V);
    vec3 albedo = fragColor.rgb;

    if (fragMat == 3) {   // ノズル: 中心ほど明るく
        float r = length(fragObj.xy - vec2(sign(fragObj.x) * 3.35, -0.3)) / 0.9;
        vec3 hot = mix(vec3(1.0), engineColor, smoothstep(0.0, 0.8, r));
        float k = 0.15 + 1.6 * engineGlow;
        finalColor = vec4(hot * k * (1.1 - 0.5 * r), 1.0);
        return;
    }
    if (fragMat == 4) {   // 航法灯
        finalColor = vec4(albedo * (0.25 + 1.6 * navBlink), 1.0);
        return;
    }

    float shininess = 48.0, specK = 0.35;
    vec3 specTint = vec3(1.0);
    if (fragMat == 0) {
        // パネルライン
        float lz = 1.0 - smoothstep(0.0, 0.03, abs(fract(fragObj.z / 2.6 + 0.5) - 0.5));
        float lx = 1.0 - smoothstep(0.0, 0.025, abs(fract(fragObj.x / 1.9 + 0.5) - 0.5));
        albedo *= 1.0 - 0.18 * max(lz, lx * step(0.9, abs(N.y)));
    } else if (fragMat == 1) {
        shininess = 70.0; specK = 0.7; specTint = albedo * 1.6 + 0.25;
    } else if (fragMat == 2) {
        shininess = 220.0; specK = 1.4;
    }

    float ndl = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), shininess) * specK * step(0.0, dot(N, L));
    vec3 col = albedo * sunColor * ndl + specTint * sunColor * spec;
    col += albedo * fillColor * max(dot(N, normalize(fillDir)), 0.0);
    // 柔らかい補助光（視線方向から）。宇宙では影側が真っ黒になり形が読めないため
    col += albedo * (0.03 + 0.09 * max(dot(N, V), 0.0));
    col += vec3(0.22, 0.30, 0.45) * pow(1.0 - max(dot(N, V), 0.0), 3.0) * 0.28;  // 輪郭光

    if (fragMat == 2) {   // ガラス: 簡易な環境反射 + フレネル
        vec3 R = reflect(-V, N);
        vec3 env = mix(vec3(0.01, 0.015, 0.03), vec3(0.10, 0.14, 0.22), R.y * 0.5 + 0.5);
        float fres = 0.08 + 0.92 * pow(1.0 - max(dot(N, V), 0.0), 5.0);
        col = mix(col, env + sunColor * pow(max(dot(R, L), 0.0), 400.0) * 3.0, fres);
    }
    col = col / (1.0 + 0.15 * col);   // ハイライトの白飛びを抑える
    finalColor = vec4(col, 1.0);
}
)";
