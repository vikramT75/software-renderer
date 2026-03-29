#pragma once
#include <cmath>

struct Vec2
{
    float x, y;

    Vec2() : x(0.f), y(0.f) {}
    Vec2(float x, float y) : x(x), y(y) {}

    Vec2 operator+(const Vec2& v) const { return {x + v.x, y + v.y}; }
    Vec2 operator-(const Vec2& v) const { return {x - v.x, y - v.y}; }
    Vec2 operator*(float s)       const { return {x * s,   y * s};   }

    float dot(const Vec2& v)  const { return x * v.x + y * v.y; }
    float length()            const { return std::sqrt(dot(*this)); }
    Vec2  normalized()        const { float l = length(); return {x / l, y / l}; }
};
