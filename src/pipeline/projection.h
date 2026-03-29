#pragma once
#include "vertex.h"

inline ScreenVertex toScreen(const ClipVertex &cv, int width, int height)
{
    float invW = 1.f / cv.clip.w;
    float ndcX = cv.clip.x * invW;
    float ndcY = cv.clip.y * invW;
    float ndcZ = cv.clip.z * invW;

    ScreenVertex sv;
    sv.sx = (ndcX * 0.5f + 0.5f) * static_cast<float>(width);
    sv.sy = (1.f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(height);
    sv.z = ndcZ;
    sv.invW = invW;

    // Premultiply by invW for perspective-correct interpolation
    sv.worldPos = cv.worldPos * invW;
    sv.normal = cv.normal * invW;
    sv.uv = cv.uv * invW;

    return sv;
}