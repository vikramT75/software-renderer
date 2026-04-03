#pragma once
#include "vertex.h"
#include <vector>

// Sutherland-Hodgman clip against a single plane defined by:
//   dot(v, plane) + plane.w >= 0  → inside
//
// We clip in clip space against the 6 frustum planes.
// Only the near plane (w + z >= 0) is strictly required to prevent
// divide-by-zero in perspective divide, but we clip all 6 for correctness.

// Returns the interpolation factor t where the edge (a→b) crosses the plane.
// Plane equation: dot(plane_normal, v) = 0 in homogeneous clip space.
inline float intersectPlane(const Vec4 &a, const Vec4 &b,
                            float na, float nb) // signed distances
{
    return na / (na - nb);
}

// Interpolate a ClipVertex between two endpoints at factor t.
inline ClipVertex lerpClip(const ClipVertex &a, const ClipVertex &b, float t)
{
    ClipVertex out;

    out.clip.x = a.clip.x + (b.clip.x - a.clip.x) * t;
    out.clip.y = a.clip.y + (b.clip.y - a.clip.y) * t;
    out.clip.z = a.clip.z + (b.clip.z - a.clip.z) * t;
    out.clip.w = a.clip.w + (b.clip.w - a.clip.w) * t;

    //  World positions
    out.worldPos.x = a.worldPos.x + (b.worldPos.x - a.worldPos.x) * t;
    out.worldPos.y = a.worldPos.y + (b.worldPos.y - a.worldPos.y) * t;
    out.worldPos.z = a.worldPos.z + (b.worldPos.z - a.worldPos.z) * t;

    out.normal.x = a.normal.x + (b.normal.x - a.normal.x) * t;
    out.normal.y = a.normal.y + (b.normal.y - a.normal.y) * t;
    out.normal.z = a.normal.z + (b.normal.z - a.normal.z) * t;

    out.uv.x = a.uv.x + (b.uv.x - a.uv.x) * t;
    out.uv.y = a.uv.y + (b.uv.y - a.uv.y) * t;
    return out;
}

// Clip a polygon (as a list of ClipVertices) against one plane.
// Plane index:
//   0: w + x >= 0  (left)
//   1: w - x >= 0  (right)
//   2: w + y >= 0  (bottom)
//   3: w - y >= 0  (top)
//   4: w + z >= 0  (near)
//   5: w - z >= 0  (far)
inline std::vector<ClipVertex> clipAgainstPlane(
    const std::vector<ClipVertex> &poly, int plane)
{
    if (poly.empty())
        return {};

    auto signedDist = [&](const ClipVertex &v) -> float
    {
        switch (plane)
        {
        case 0:
            return v.clip.w + v.clip.x;
        case 1:
            return v.clip.w - v.clip.x;
        case 2:
            return v.clip.w + v.clip.y;
        case 3:
            return v.clip.w - v.clip.y;
        case 4:
            return v.clip.w + v.clip.z;
        case 5:
            return v.clip.w - v.clip.z;
        }
        return 0.f;
    };

    std::vector<ClipVertex> out;
    out.reserve(poly.size() + 1);

    for (size_t i = 0; i < poly.size(); ++i)
    {
        const ClipVertex &cur = poly[i];
        const ClipVertex &next = poly[(i + 1) % poly.size()];

        float dc = signedDist(cur);
        float dn = signedDist(next);

        bool curInside = dc >= 0.f;
        bool nextInside = dn >= 0.f;

        if (curInside)
            out.push_back(cur);

        if (curInside != nextInside)
        {
            float t = dc / (dc - dn);
            out.push_back(lerpClip(cur, next, t));
        }
    }

    return out;
}

// Clip a triangle (3 ClipVertices) against all 6 frustum planes.
// Returns a triangle fan (may produce 0, 1, or more triangles).
inline std::vector<ClipVertex> clipTriangle(const ClipVertex tri[3])
{
    std::vector<ClipVertex> poly = {tri[0], tri[1], tri[2]};

    for (int plane = 0; plane < 6; ++plane)
    {
        poly = clipAgainstPlane(poly, plane);
        if (poly.empty())
            return {};
    }

    return poly; // convex polygon — caller fans it into triangles
}