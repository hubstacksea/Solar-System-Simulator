// Solar System Voyager — Copyright (c) 2026 hubstacksea
// MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
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

    // a から b への円柱（両端にふた）
    void Tube(Vector3 a, Vector3 b, float r, int segs, Color c, ShipMat m) {
        Vector3 ax = Vector3Normalize(Vector3Subtract(b, a));
        Vector3 t = fabsf(ax.y) < 0.9f ? Vector3{0, 1, 0} : Vector3{1, 0, 0};
        Vector3 u = Vector3Normalize(Vector3CrossProduct(ax, t)), v = Vector3CrossProduct(ax, u);
        std::vector<std::vector<Vector3>> P(2);
        for (int i = 0; i < segs; i++) {
            float ang = 2 * PI * i / segs;
            Vector3 o = Vector3Add(Vector3Scale(u, cosf(ang) * r), Vector3Scale(v, sinf(ang) * r));
            P[0].push_back(Vector3Add(a, o));
            P[1].push_back(Vector3Add(b, o));
        }
        Grid(P, true, std::vector<std::vector<Color>>(2, std::vector<Color>(segs, c)), m);
        Disc(a, Vector3Negate(ax), r, segs, c, m);
        Disc(b, ax, r, segs, c, m);
    }

    // a から b への角柱（側面は平らに陰影を付ける）。r は外接円の半径、rot は最初の頂点の角度 rad
    void Prism(Vector3 a, Vector3 b, float r, int sides, float rot, Color c, ShipMat m) {
        Vector3 ax = Vector3Normalize(Vector3Subtract(b, a));
        Vector3 t = fabsf(ax.y) < 0.9f ? Vector3{0, 1, 0} : Vector3{1, 0, 0};
        Vector3 u = Vector3Normalize(Vector3CrossProduct(ax, t)), v = Vector3CrossProduct(ax, u);
        auto corner = [&](int i) {
            float ang = rot + 2 * PI * i / sides;
            return Vector3Add(Vector3Scale(u, cosf(ang) * r), Vector3Scale(v, sinf(ang) * r));
        };
        for (int i = 0; i < sides; i++) {
            Vector3 o0 = corner(i), o1 = corner(i + 1);
            Vector3 n = Vector3Normalize(Vector3Add(o0, o1));
            int k = Add(Vector3Add(a, o0), n, c, m);
            Add(Vector3Add(a, o1), n, c, m);
            Add(Vector3Add(b, o1), n, c, m);
            Add(Vector3Add(b, o0), n, c, m);
            Tri(k, k + 1, k + 2);
            Tri(k, k + 2, k + 3);
        }
        for (int e = 0; e < 2; e++) {
            Vector3 p = e ? b : a, n = e ? ax : Vector3Negate(ax);
            int ci = Add(p, n, c, m);
            for (int i = 0; i <= sides; i++) Add(Vector3Add(p, corner(i)), n, c, m);
            for (int i = 0; i < sides; i++) Tri(ci, ci + 1 + i, ci + 2 + i);
        }
    }

    // 直方体（中心と各軸方向の半分の長さ）
    void Box(Vector3 c0, Vector3 h, Color c, ShipMat m) {
        const Vector3 N[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        for (const Vector3& n : N) {
            Vector3 a = fabsf(n.x) > 0 ? Vector3{0, 1, 0} : Vector3{1, 0, 0};
            Vector3 b = Vector3CrossProduct(n, a);
            auto P = [&](float sa, float sb) {
                return Vector3{c0.x + (n.x + a.x * sa + b.x * sb) * h.x, c0.y + (n.y + a.y * sa + b.y * sb) * h.y,
                               c0.z + (n.z + a.z * sa + b.z * sb) * h.z};
            };
            int k = Add(P(-1, -1), n, c, m);
            Add(P(1, -1), n, c, m);
            Add(P(1, 1), n, c, m);
            Add(P(-1, 1), n, c, m);
            Tri(k, k + 1, k + 2);
            Tri(k, k + 2, k + 3);
        }
    }

    // 向きのある直方体。ax, ay, az は中心から各面までのベクトル（互いに直交）
    void OBox(Vector3 c0, Vector3 ax, Vector3 ay, Vector3 az, Color c, ShipMat m) {
        const Vector3 A[3] = {ax, ay, az};
        for (int f = 0; f < 6; f++) {
            Vector3 n = Vector3Scale(A[f / 2], f % 2 ? -1.0f : 1.0f), a = A[(f / 2 + 1) % 3], b = A[(f / 2 + 2) % 3];
            Vector3 cn = Vector3Add(c0, n), nn = Vector3Normalize(n);
            int k = Add(Vector3Subtract(Vector3Subtract(cn, a), b), nn, c, m);
            Add(Vector3Subtract(Vector3Add(cn, a), b), nn, c, m);
            Add(Vector3Add(Vector3Add(cn, a), b), nn, c, m);
            Add(Vector3Add(Vector3Subtract(cn, a), b), nn, c, m);
            Tri(k, k + 1, k + 2);
            Tri(k, k + 2, k + 3);
        }
    }

    // 放物面アンテナ: 縁の中心 rimCenter、開口の向き dir（凹面が向く方向）、半径 r、深さ depth
    void Dish(Vector3 rimCenter, Vector3 dir, float r, float depth, Color c, ShipMat m) {
        dir = Vector3Normalize(dir);
        Vector3 t = fabsf(dir.y) < 0.9f ? Vector3{0, 1, 0} : Vector3{1, 0, 0};
        Vector3 u = Vector3Normalize(Vector3CrossProduct(dir, t)), v = Vector3CrossProduct(dir, u);
        const int R = 8, S = 32;
        std::vector<std::vector<Vector3>> P;
        for (int i = 0; i <= R; i++) {
            float rr = r * i / R, z = depth * (1 - (rr * rr) / (r * r));
            std::vector<Vector3> row;
            for (int k = 0; k < S; k++) {
                float ang = 2 * PI * k / S;
                Vector3 o = Vector3Add(Vector3Scale(u, cosf(ang) * rr), Vector3Scale(v, sinf(ang) * rr));
                row.push_back(Vector3Add(Vector3Add(rimCenter, o), Vector3Scale(dir, -z)));
            }
            P.push_back(row);
        }
        Grid(P, true, std::vector<std::vector<Color>>(R + 1, std::vector<Color>(S, c)), m);
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

// 探査機の機体モデル。単位 m。-Z が姿勢の基準（パーカーは耐熱シールドを太陽へ、ほかは高利得アンテナを地球へ向ける）
enum ProbeModel {
    PM_VOYAGER = 0, PM_PIONEER, PM_GALILEO, PM_GALILEO_PROBE, PM_CASSINI, PM_CASSINI_HUYGENS, PM_NEW_HORIZONS, PM_PARKER,
    PM_JUNO, PM_ROSETTA, PM_HAYABUSA2, PM_OSIRIS_REX, PM_DART, PM_COUNT
};

inline Mesh GenProbeMesh(int model) {
    ShipMeshBuilder mb;
    const Color white = {226, 224, 216, 255}, gold = {176, 140, 62, 255}, metal = {120, 122, 128, 255};
    const Color dark = {44, 44, 48, 255}, black = {28, 28, 30, 255}, panel = {40, 52, 92, 255};
    // 原子力電池 (RTG): フィンの付いた黒い円柱
    auto rtg = [&](Vector3 a, Vector3 b, float r) {
        mb.Tube(a, b, r * 0.55f, 10, dark, SM_METAL);
        Vector3 d = Vector3Subtract(b, a);
        Vector3 t = fabsf(d.y) < 0.9f * Vector3Length(d) ? Vector3{0, 1, 0} : Vector3{1, 0, 0};
        Vector3 u = Vector3Normalize(Vector3CrossProduct(d, t)), v = Vector3Normalize(Vector3CrossProduct(d, u));
        for (int i = 0; i < 4; i++) {   // 放熱フィン
            float ang = PI * i / 4;
            Vector3 o = Vector3Add(Vector3Scale(u, cosf(ang)), Vector3Scale(v, sinf(ang)));
            for (int sgn = -1; sgn <= 1; sgn += 2) {
                Vector3 off = Vector3Scale(o, sgn * r * 0.8f);
                mb.Tube(Vector3Add(a, off), Vector3Add(b, off), r * 0.12f, 4, dark, SM_METAL);
            }
        }
    };
    switch (model) {
    case PM_PIONEER: {
        // パイオニア 10/11 号: 直径 2.74 m の浅いアンテナ、後ろに六角形の機器室、2 本のブームに RTG を 2 基ずつ
        mb.Dish({0, 0, -0.45f}, {0, 0, -1}, 1.37f, 0.45f, white, SM_PAINT);
        for (int i = 0; i < 3; i++) {   // 中利得アンテナの三脚
            float ang = 2 * PI * i / 3;
            mb.Tube({cosf(ang) * 0.9f, sinf(ang) * 0.9f, -0.2f}, {0, 0, -1.3f}, 0.012f, 4, metal, SM_METAL);
        }
        mb.Tube({0, 0, -1.3f}, {0, 0, -1.5f}, 0.07f, 10, white, SM_PAINT);
        mb.Prism({0, 0, 0.0f}, {0, 0, 0.36f}, 0.72f, 6, 0, gold, SM_METAL);
        mb.Prism({0.72f, 0, 0.0f}, {0.72f, 0, 0.36f}, 0.36f, 6, 0, gold, SM_METAL);   // 科学機器室
        for (int i = 0; i < 2; i++) {
            float ang = PI * (0.5f + 0.67f * (i ? 1 : -1));
            Vector3 dir = {cosf(ang), sinf(ang), 0};
            mb.Tube(Vector3Scale(dir, 0.6f), Vector3Add(Vector3Scale(dir, 3.0f), {0, 0, 0.2f}), 0.03f, 6, metal, SM_METAL);
            rtg(Vector3Add(Vector3Scale(dir, 2.0f), {0, 0, 0.13f}), Vector3Add(Vector3Scale(dir, 2.45f), {0, 0, 0.16f}), 0.2f);
            rtg(Vector3Add(Vector3Scale(dir, 2.5f), {0, 0, 0.16f}), Vector3Add(Vector3Scale(dir, 2.95f), {0, 0, 0.2f}), 0.2f);
        }
        mb.Tube({0, 0.6f, 0.2f}, {0, 6.6f, 0.2f}, 0.015f, 5, metal, SM_METAL);   // 磁力計のブーム（RTG と 120° 離す）
        mb.Tube({0, 6.6f, 0.2f}, {0, 6.8f, 0.2f}, 0.06f, 8, white, SM_PAINT);
        break;
    }
    case PM_GALILEO:
    case PM_GALILEO_PROBE: {
        // ガリレオ: 傘のような高利得アンテナ（実際は開ききらなかった）、十角形の回転部、2 本の RTG ブーム、
        // 長い磁力計ブーム、下の非回転部とスキャン・プラットフォーム。大気圏突入機は 1995 年 7 月に切り離し
        for (int i = 0; i < 18; i++) {   // アンテナのリブ
            float a0 = 2 * PI * i / 18;
            mb.Tube({0, 0, -1.0f}, {cosf(a0) * 2.4f, sinf(a0) * 2.4f, -1.9f}, 0.02f, 4, metal, SM_METAL);
        }
        mb.Dish({0, 0, -1.9f}, {0, 0, -1}, 2.4f, 0.9f, white, SM_PAINT);
        mb.Tube({0, 0, -1.0f}, {0, 0, -2.6f}, 0.05f, 8, metal, SM_METAL);
        mb.Prism({0, 0, -1.0f}, {0, 0, 0.2f}, 0.9f, 10, 0, gold, SM_METAL);
        for (int i = 0; i < 2; i++) {
            float sx = i ? 1.0f : -1.0f;
            mb.Tube({sx * 0.8f, 0, -0.3f}, {sx * 4.6f, 0, 0.6f}, 0.04f, 6, metal, SM_METAL);
            rtg({sx * 3.4f, 0, 0.32f}, {sx * 4.4f, 0, 0.55f}, 0.24f);
        }
        mb.Tube({0, 0.8f, -0.4f}, {0, 11.0f, -0.4f}, 0.03f, 5, metal, SM_METAL);
        mb.Tube({0, 0, 0.2f}, {0, 0, 1.3f}, 0.45f, 12, gold, SM_METAL);    // 非回転部
        mb.Tube({0.5f, 0, 1.1f}, {1.6f, 0, 1.1f}, 0.06f, 6, metal, SM_METAL);
        mb.Box({1.7f, 0, 1.1f}, {0.3f, 0.35f, 0.3f}, dark, SM_METAL);      // スキャン・プラットフォーム
        if (model == PM_GALILEO_PROBE)
            mb.Revolve(0, 0, {{1.3f, 0.0f}, {1.3f, 0.6f}, {1.55f, 0.62f}, {1.8f, 0.3f}, {1.85f, 0.0f}}, 20, {120, 84, 50, 255}, SM_METAL);
        break;
    }
    case PM_CASSINI:
    case PM_CASSINI_HUYGENS: {
        // カッシーニ: 直径 4 m の高利得アンテナ、十二角形の電子機器室、背の高い推進モジュール、底の 3 基の RTG と
        // 2 基の主エンジン、11 m の磁力計ブーム。ホイヘンス・プローブ（側面の円盤）は 2004 年 12 月に切り離し
        mb.Dish({0, 0, -0.75f}, {0, 0, -1}, 2.0f, 0.75f, white, SM_PAINT);
        for (int i = 0; i < 4; i++) {
            float ang = PI * 0.5f * i + PI / 4;
            mb.Tube({cosf(ang) * 1.2f, sinf(ang) * 1.2f, -0.3f}, {0, 0, -1.6f}, 0.015f, 4, metal, SM_METAL);
        }
        mb.Tube({0, 0, -1.6f}, {0, 0, -1.75f}, 0.22f, 12, white, SM_PAINT);
        mb.Prism({0, 0, 0.0f}, {0, 0, 0.6f}, 1.0f, 12, 0, gold, SM_METAL);          // 電子機器室
        mb.Tube({0, 0, 0.6f}, {0, 0, 4.4f}, 0.82f, 20, gold, SM_METAL);            // 推進モジュール
        mb.Revolve(0, 0, {{4.4f, 0.82f}, {4.6f, 0.6f}, {4.65f, 0.0f}}, 20, gold, SM_METAL);
        for (int i = 0; i < 2; i++) {   // 主エンジンのノズル
            float x = i ? 0.35f : -0.35f;
            mb.Revolve(x, -0.3f, {{4.5f, 0.08f}, {4.9f, 0.14f}, {5.2f, 0.22f}}, 12, metal, SM_METAL);
        }
        for (int i = 0; i < 3; i++) {
            float ang = PI * 0.5f + 2 * PI * i / 3;
            Vector3 dir = {cosf(ang), sinf(ang), 0};
            rtg(Vector3Add(Vector3Scale(dir, 0.95f), {0, 0, 3.6f}), Vector3Add(Vector3Scale(dir, 1.25f), {0, 0, 4.7f}), 0.24f);
        }
        mb.Box({0.0f, 1.05f, 2.0f}, {0.35f, 0.25f, 0.6f}, dark, SM_METAL);       // 機器を載せたパレット
        mb.Tube({-0.8f, 0, 0.3f}, {-11.0f, 0, 0.3f}, 0.025f, 5, metal, SM_METAL);
        if (model == PM_CASSINI_HUYGENS)
            mb.Revolve(1.0f, 0, {{1.4f, 0.0f}, {1.4f, 1.35f}, {1.55f, 1.35f}, {1.75f, 0.9f}, {2.1f, 0.5f}, {2.15f, 0.0f}}, 24,
                       {200, 186, 160, 255}, SM_METAL);
        break;
    }
    case PM_NEW_HORIZONS: {
        // ニュー・ホライズンズ: 三角形の本体（金色の断熱材）、直径 2.1 m のアンテナ、頂点から突き出た 1 基の RTG
        mb.Dish({0, 0, -0.75f}, {0, 0, -1}, 1.05f, 0.35f, white, SM_PAINT);
        mb.Tube({0, 0, -0.4f}, {0, 0, -1.1f}, 0.02f, 5, metal, SM_METAL);
        mb.Disc({0, 0, -1.1f}, {0, 0, -1}, 0.12f, 12, white, SM_PAINT);
        mb.Prism({0, 0, -0.4f}, {0, 0, 0.3f}, 1.05f, 3, PI / 2, gold, SM_METAL);
        mb.Box({0.0f, 0.2f, -0.05f}, {0.25f, 0.18f, 0.3f}, dark, SM_METAL);
        rtg({0, -0.55f, 0.0f}, {0, -1.6f, 0.0f}, 0.25f);
        break;
    }
    case PM_PARKER: {
        // パーカー・ソーラー・プローブ: 直径 2.3 m・厚さ 11.4 cm の耐熱シールド（太陽側は白）、六角形の本体、
        // シールドの陰に収まる冷却付きの太陽電池パネル 2 枚、後ろの磁力計ブーム
        mb.Tube({0, 0, -1.2f}, {0, 0, -1.086f}, 1.15f, 40, black, SM_METAL);
        mb.Disc({0, 0, -1.201f}, {0, 0, -1}, 1.15f, 40, white, SM_PAINT);
        for (int i = 0; i < 6; i++) {   // シールドを支える支柱
            float ang = 2 * PI * i / 6;
            mb.Tube({cosf(ang) * 0.85f, sinf(ang) * 0.85f, -1.08f}, {cosf(ang) * 0.5f, sinf(ang) * 0.5f, -0.45f}, 0.02f, 4, metal, SM_METAL);
        }
        mb.Prism({0, 0, -0.45f}, {0, 0, 1.0f}, 0.55f, 6, 0, gold, SM_METAL);
        for (int i = 0; i < 2; i++) {
            float sx = i ? 1.0f : -1.0f;
            mb.Tube({sx * 0.48f, 0, -0.2f}, {sx * 0.75f, 0, -0.2f}, 0.025f, 5, metal, SM_METAL);
            mb.Box({sx * 0.95f, 0, 0.05f}, {0.2f, 0.015f, 0.38f}, panel, SM_GLASS);
        }
        mb.Box({0, 0.5f, 0.4f}, {0.25f, 0.06f, 0.25f}, white, SM_PAINT);   // 冷却用ラジエーター
        mb.Tube({0, 0, 1.0f}, {0, 0, 3.0f}, 0.02f, 5, metal, SM_METAL);
        break;
    }
    case PM_JUNO: {
        // ジュノー: 六角形の本体の上に直径 2.5 m のアンテナ、120° おきに 3 枚の太陽電池パネル（1 枚 約 9 m、
        // 1 枚の先は磁力計のブーム）。パネルは本体の側面から水平に伸びる
        mb.Dish({0, 0, -1.25f}, {0, 0, -1}, 1.25f, 0.4f, white, SM_PAINT);
        mb.Prism({0, 0, -0.85f}, {0, 0, 0.6f}, 1.75f, 6, 0, gold, SM_METAL);
        mb.Box({0, 0, 0.75f}, {0.8f, 0.8f, 0.15f}, dark, SM_METAL);      // 放射線から守る電子機器室
        for (int i = 0; i < 3; i++) {
            float ang = PI / 6 + 2 * PI * i / 3;
            Vector3 dir = {cosf(ang), sinf(ang), 0}, side = {-sinf(ang), cosf(ang), 0};
            int segs = i == 0 ? 3 : 4;   // 磁力計の付く 1 枚は 3 枚つなぎ + 磁力計の台
            for (int k = 0; k < segs; k++) {
                Vector3 c0 = Vector3Add(Vector3Scale(dir, 1.6f + k * 2.25f + 1.1f), {0, 0, -0.1f});
                mb.OBox(c0, Vector3Scale(dir, 1.1f), Vector3Scale(side, 1.35f), {0, 0, 0.03f}, panel, SM_GLASS);
            }
            if (i == 0) {
                Vector3 c0 = Vector3Add(Vector3Scale(dir, 1.6f + 3 * 2.25f + 0.6f), {0, 0, -0.1f});
                mb.OBox(c0, Vector3Scale(dir, 0.6f), Vector3Scale(side, 0.3f), {0, 0, 0.25f}, metal, SM_METAL);
            }
        }
        break;
    }
    case PM_ROSETTA: {
        // ロゼッタ: 箱形の本体（2.8 × 2.1 × 2.0 m）、側面に直径 2.2 m の高利得アンテナ、両側に長さ 14 m の
        // 太陽電池パネル（5 枚つなぎ）。フィラエ（着陸機）は 2014 年 11 月 12 日に切り離し
        mb.Box({0, 0, 0}, {1.0f, 1.05f, 1.4f}, gold, SM_METAL);
        mb.Tube({0, 0, -1.0f}, {0, 0, -1.6f}, 0.05f, 8, metal, SM_METAL);
        mb.Dish({0, 0, -2.0f}, {0, 0, -1}, 1.1f, 0.35f, white, SM_PAINT);
        for (int i = 0; i < 2; i++) {
            float sx = i ? 1.0f : -1.0f;
            mb.Tube({sx * 1.0f, 0, 0}, {sx * 1.6f, 0, 0}, 0.05f, 6, metal, SM_METAL);
            for (int k = 0; k < 5; k++)
                mb.Box({sx * (2.9f + k * 2.55f), 0, 0}, {1.25f, 0.03f, 1.1f}, panel, SM_GLASS);
        }
        break;
    }
    case PM_HAYABUSA2: {
        // はやぶさ2: 箱形の本体（1.0 × 1.6 × 1.25 m）、上面に 2 枚の平面アンテナ、両側に 3 枚つなぎの太陽電池パネル、
        // 下面にサンプラーホーン（円錐の筒）。-Z（地球）を向くのはアンテナのある面
        mb.Box({0, 0, 0}, {0.8f, 0.5f, 0.62f}, gold, SM_METAL);
        mb.Box({-0.35f, 0, -0.66f}, {0.3f, 0.3f, 0.04f}, white, SM_PAINT);   // 平面アンテナ（Ka 帯・X 帯）
        mb.Box({0.35f, 0, -0.66f}, {0.25f, 0.25f, 0.04f}, white, SM_PAINT);
        mb.Tube({0, -0.5f, 0}, {0, -1.5f, 0}, 0.08f, 10, metal, SM_METAL);   // サンプラーホーン
        mb.Tube({0, -1.5f, 0}, {0, -1.55f, 0}, 0.14f, 10, metal, SM_METAL);
        for (int i = 0; i < 2; i++) {
            float sx = i ? 1.0f : -1.0f;
            for (int k = 0; k < 3; k++)
                mb.Box({sx * (1.4f + k * 1.15f), 0.3f, 0}, {0.55f, 0.02f, 0.55f}, panel, SM_GLASS);
        }
        mb.Tube({0, 0, 0.62f}, {0, 0, 0.78f}, 0.3f, 12, dark, SM_METAL);   // イオンエンジン
        break;
    }
    case PM_OSIRIS_REX: {
        // オシリス・レックス: 箱形の本体（2.4 × 2.4 × 3.2 m）、上に直径 2 m の高利得アンテナと試料回収カプセル、
        // 両側に太陽電池パネル（端から端まで 6.2 m）、底から伸びる試料採取用のアーム（TAGSAM）
        mb.Box({0, 0, 0}, {1.2f, 1.2f, 1.6f}, white, SM_PAINT);
        mb.Dish({0, 0, -2.05f}, {0, 0, -1}, 1.0f, 0.3f, white, SM_PAINT);
        mb.Tube({0, 0, -1.6f}, {0, 0, -1.8f}, 0.08f, 8, metal, SM_METAL);
        mb.Revolve(0.75f, 0.6f, {{-1.6f, 0.4f}, {-1.95f, 0.25f}, {-2.0f, 0.0f}}, 16, {200, 190, 170, 255}, SM_METAL);  // 回収カプセル
        for (int i = 0; i < 2; i++) {
            float sx = i ? 1.0f : -1.0f;
            mb.Tube({sx * 1.2f, 0, 0.3f}, {sx * 1.5f, 0, 0.3f}, 0.05f, 6, metal, SM_METAL);
            mb.Box({sx * 2.4f, 0, 0.3f}, {0.9f, 0.02f, 0.85f}, panel, SM_GLASS);
        }
        mb.Tube({0.4f, -1.2f, 1.6f}, {0.4f, -1.2f, 4.6f}, 0.05f, 6, metal, SM_METAL);   // TAGSAM
        mb.Revolve(0.4f, -1.2f, {{4.6f, 0.0f}, {4.6f, 0.15f}, {4.75f, 0.15f}, {4.75f, 0.0f}}, 12, metal, SM_METAL);
        break;
    }
    case PM_DART: {
        // DART: 1.3 m の箱形の本体、両側に長さ 8.5 m の巻き取り式太陽電池パネル（ROSA）、
        // 本体の側面に平らな円形のアンテナ、前（-Z）に小惑星を撮るカメラ（DRACO）の筒
        mb.Box({0, 0, 0}, {0.65f, 0.65f, 0.65f}, gold, SM_METAL);
        mb.Tube({0, 0, -0.65f}, {0, 0, -1.2f}, 0.17f, 12, dark, SM_METAL);
        mb.Disc({0, 0, -1.201f}, {0, 0, -1}, 0.17f, 12, black, SM_METAL);
        mb.Tube({0, 0.66f, 0}, {0, 0.72f, 0}, 0.5f, 24, white, SM_PAINT);
        for (int i = 0; i < 2; i++) {
            float sx = i ? 1.0f : -1.0f;
            mb.Tube({sx * 0.65f, 0, 0}, {sx * 0.9f, 0, 0}, 0.06f, 6, metal, SM_METAL);
            mb.Box({sx * 5.15f, 0, 0}, {4.25f, 0.015f, 1.2f}, panel, SM_GLASS);
            mb.Tube({sx * 0.9f, 0, -1.2f}, {sx * 9.4f, 0, -1.2f}, 0.04f, 6, metal, SM_METAL);
            mb.Tube({sx * 0.9f, 0, 1.2f}, {sx * 9.4f, 0, 1.2f}, 0.04f, 6, metal, SM_METAL);
        }
        break;
    }
    default: {
        // ボイジャー 1/2 号: 直径 3.7 m のアンテナと副反射鏡、十角形の本体、RTG 3 基のブーム、科学機器のブーム、
        // 13 m の磁力計ブーム
        std::vector<Vector2> dish;
        for (int i = 0; i <= 8; i++) {
            float r = 1.85f * i / 8;
            dish.push_back({-r * r / (4 * 1.6f), r});
        }
        mb.Revolve(0, 0, dish, 32, white, SM_PAINT);
        mb.Tube({0, 0, -0.05f}, {0, 0, -1.0f}, 0.025f, 6, metal, SM_METAL);
        mb.Disc({0, 0, -1.0f}, {0, 0, -1}, 0.16f, 16, white, SM_PAINT);
        mb.Prism({0, 0, 0.05f}, {0, 0, 0.5f}, 0.95f, 10, 0, gold, SM_METAL);
        mb.Tube({-0.9f, 0, 0.3f}, {-3.0f, 0, 0.3f}, 0.04f, 6, metal, SM_METAL);
        for (int i = 0; i < 3; i++) {
            float x = -2.0f - i * 0.45f;
            rtg({x, 0, 0.3f}, {x - 0.4f, 0, 0.3f}, 0.2f);
        }
        mb.Tube({0.9f, 0, 0.3f}, {3.0f, 0, 0.3f}, 0.04f, 6, metal, SM_METAL);
        mb.Tube({2.7f, -0.3f, 0.3f}, {2.7f, 0.4f, 0.3f}, 0.28f, 12, metal, SM_METAL);
        mb.Tube({0, 0.9f, 0.4f}, {0, 13.0f, 0.4f}, 0.02f, 5, metal, SM_METAL);
        break;
    }
    }
    return mb.Build();
}

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
