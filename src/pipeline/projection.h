#pragma once
#include "triangle.h"
#include "vertex.h"

inline ScreenVertex toScreen(const ClipVertex &v, int w, int h)
{
    ScreenVertex out;
    float invW = 1.0f / v.clip.w;

    // Screen coordinates
    out.sx = (v.clip.x * invW + 1.0f) * 0.5f * (float)w;
    out.sy = (1.0f - v.clip.y * invW) * 0.5f * (float)h;
    out.z = v.clip.z * invW;
    out.invW = invW;

    // CRITICAL MATH FIX: Pre-multiply attributes by 1/W for perspective correctness
    out.worldPos = v.worldPos * invW;
    out.normal = v.normal * invW;
    out.tangent = v.tangent * invW;
    out.uv = v.uv * invW;

    return out;
}