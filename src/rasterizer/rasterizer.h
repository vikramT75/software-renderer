#pragma once
#include "barycentric.h"
#include "../pipeline/triangle.h"
#include "../core/framebuffer.h"
#include "../core/depthbuffer.h"
#include "../shading/shader.h"
#include <algorithm>
#include <cmath>

enum class CullMode
{
    None,
    Back,
    Front
};

inline void rasterizeTriangle(const Triangle &tri,
                              Framebuffer &fb,
                              Depthbuffer &db,
                              CullMode cull = CullMode::Back,
                              const Shader *shader = nullptr)
{
    ScreenVertex v0 = tri.v[0];
    ScreenVertex v1 = tri.v[1];
    ScreenVertex v2 = tri.v[2];

    float area2 = edgeFunction(v0.sx, v0.sy, v1.sx, v1.sy, v2.sx, v2.sy);

    if (cull == CullMode::Back && area2 <= 0.f)
        return;
    if (cull == CullMode::Front && area2 >= 0.f)
        return;
    if (area2 == 0.f)
        return;

    if (area2 < 0.f && cull == CullMode::None)
    {
        std::swap(v1, v2);
        area2 = -area2;
    }

    int minX = std::max(0, (int)std::floor(std::min({v0.sx, v1.sx, v2.sx})));
    int maxX = std::min(fb.width - 1, (int)std::ceil(std::max({v0.sx, v1.sx, v2.sx})));
    int minY = std::max(0, (int)std::floor(std::min({v0.sy, v1.sy, v2.sy})));
    int maxY = std::min(fb.height - 1, (int)std::ceil(std::max({v0.sy, v1.sy, v2.sy})));

    for (int y = minY; y <= maxY; ++y)
    {
        for (int x = minX; x <= maxX; ++x)
        {
            auto b = Barycentric::compute(
                v0.sx, v0.sy,
                v1.sx, v1.sy,
                v2.sx, v2.sy,
                static_cast<float>(x) + 0.5f,
                static_cast<float>(y) + 0.5f,
                area2);

            if (!b.inside)
                continue;

            float z = b.w0 * v0.z + b.w1 * v1.z + b.w2 * v2.z;
            if (!db.test(x, y, z))
                continue;

            uint32_t color = tri.color;

            if (shader)
            {
                float invW = b.w0 * v0.invW + b.w1 * v1.invW + b.w2 * v2.invW;
                float w = 1.f / invW;

                FragmentInput frag;
                frag.position = {
                    (b.w0 * v0.worldPos.x + b.w1 * v1.worldPos.x + b.w2 * v2.worldPos.x) * w,
                    (b.w0 * v0.worldPos.y + b.w1 * v1.worldPos.y + b.w2 * v2.worldPos.y) * w,
                    (b.w0 * v0.worldPos.z + b.w1 * v1.worldPos.z + b.w2 * v2.worldPos.z) * w,
                };
                frag.normal = {
                    (b.w0 * v0.normal.x + b.w1 * v1.normal.x + b.w2 * v2.normal.x) * w,
                    (b.w0 * v0.normal.y + b.w1 * v1.normal.y + b.w2 * v2.normal.y) * w,
                    (b.w0 * v0.normal.z + b.w1 * v1.normal.z + b.w2 * v2.normal.z) * w,
                };
                frag.uv = {
                    (b.w0 * v0.uv.x + b.w1 * v1.uv.x + b.w2 * v2.uv.x) * w,
                    (b.w0 * v0.uv.y + b.w1 * v1.uv.y + b.w2 * v2.uv.y) * w,
                };
                frag.tangent = {
                    (b.w0 * v0.tangent.x + b.w1 * v1.tangent.x + b.w2 * v2.tangent.x) * w,
                    (b.w0 * v0.tangent.y + b.w1 * v1.tangent.y + b.w2 * v2.tangent.y) * w,
                    (b.w0 * v0.tangent.z + b.w1 * v1.tangent.z + b.w2 * v2.tangent.z) * w,
                };
                frag.depth = z;

                color = shader->shade(frag);
            }

            fb.setPixel(x, y, color);
        }
    }
}