#pragma once
#include <cmath>
#include <algorithm>

namespace MathUtils
{
    constexpr float PI      = 3.14159265358979323846f;
    constexpr float TWO_PI  = 2.f * PI;
    constexpr float HALF_PI = PI * 0.5f;

    inline float toRadians(float degrees) { return degrees * (PI / 180.f); }
    inline float toDegrees(float radians) { return radians * (180.f / PI); }

    inline float clamp(float v, float lo, float hi)
    {
        return std::max(lo, std::min(hi, v));
    }

    inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

    // Remap v from [inLo,inHi] to [outLo,outHi]
    inline float remap(float v, float inLo, float inHi, float outLo, float outHi)
    {
        return outLo + (v - inLo) / (inHi - inLo) * (outHi - outLo);
    }
}
