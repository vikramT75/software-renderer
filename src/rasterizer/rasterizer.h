#pragma once
#include "../core/depthbuffer.h"
#include "../core/framebuffer.h"
#include "../pipeline/triangle.h"
#include "../shading/shader.h"
#include "barycentric.h"
#include <algorithm>
#include <cmath>


inline void rasterizeTriangle(const Triangle &tri, Framebuffer &fb, Depthbuffer &db, CullMode cull,
                              const Shader *shader, int tileMinX, int tileMaxX, int tileMinY, int tileMaxY)
{
    ScreenVertex v0 = tri.v[0];
    ScreenVertex v1 = tri.v[1];
    ScreenVertex v2 = tri.v[2];

    // 1. Edge Function Definition
    auto edgeFunc = [](const ScreenVertex &a, const ScreenVertex &b, float cx, float cy)
    { return (b.sx - a.sx) * (cy - a.sy) - (b.sy - a.sy) * (cx - a.sx); };

    // 2. Culling Phase (Original Winding)
    float originalArea = edgeFunc(v0, v1, v2.sx, v2.sy);

    // FIXED: Culling conditions restored. (Positive area = Backface in Y-down screen space)
    if (cull == CullMode::Back && originalArea >= 0.f)
        return;
    if (cull == CullMode::Front && originalArea <= 0.f)
        return;
    if (std::abs(originalArea) < 0.00001f)
        return;

    // 3. Setup Phase (Force Positive Winding for the inner loop)
    float area2 = originalArea;
    if (area2 < 0.f)
    {
        std::swap(v1, v2);
        area2 = -area2;
    }

    // 4. Bounding Box
    int minX = std::max(tileMinX, (int)std::floor(std::min({v0.sx, v1.sx, v2.sx})));
    int maxX = std::min(tileMaxX, (int)std::ceil(std::max({v0.sx, v1.sx, v2.sx})));
    int minY = std::max(tileMinY, (int)std::floor(std::min({v0.sy, v1.sy, v2.sy})));
    int maxY = std::min(tileMaxY, (int)std::ceil(std::max({v0.sy, v1.sy, v2.sy})));

    // 5. Incremental Deltas: dE/dx = a.y - b.y | dE/dy = b.x - a.x
    float stepX0 = v1.sy - v2.sy, stepY0 = v2.sx - v1.sx;
    float stepX1 = v2.sy - v0.sy, stepY1 = v0.sx - v2.sx;
    float stepX2 = v0.sy - v1.sy, stepY2 = v1.sx - v0.sx;

    float px = (float)minX + 0.5f, py = (float)minY + 0.5f;

    float w0_row = edgeFunc(v1, v2, px, py);
    float w1_row = edgeFunc(v2, v0, px, py);
    float w2_row = edgeFunc(v0, v1, px, py);

    float invArea = 1.0f / area2;

    for (int y = minY; y <= maxY; ++y)
    {
        float w0 = w0_row, w1 = w1_row, w2 = w2_row;
        for (int x = minX; x <= maxX; ++x)
        {
            if (w0 >= 0 && w1 >= 0 && w2 >= 0)
            {
                float b0 = w0 * invArea, b1 = w1 * invArea, b2 = w2 * invArea;
                float z = b0 * v0.z + b1 * v1.z + b2 * v2.z;

                if (db.test(x, y, z))
                {
                    float w = 1.f / (b0 * v0.invW + b1 * v1.invW + b2 * v2.invW);

                    FragmentInput frag;
                    frag.position = (v0.worldPos * b0 + v1.worldPos * b1 + v2.worldPos * b2) * w;
                    frag.normal = (v0.normal * b0 + v1.normal * b1 + v2.normal * b2) * w;
                    frag.uv = (v0.uv * b0 + v1.uv * b1 + v2.uv * b2) * w;
                    frag.tangent = (v0.tangent * b0 + v1.tangent * b1 + v2.tangent * b2) * w;
                    frag.depth = z;

                    fb.setPixel(x, y, shader ? shader->shade(frag) : tri.color);
                }
            }
            w0 += stepX0;
            w1 += stepX1;
            w2 += stepX2;
        }
        w0_row += stepY0;
        w1_row += stepY1;
        w2_row += stepY2;
    }
}