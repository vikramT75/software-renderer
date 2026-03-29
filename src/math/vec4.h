#pragma once
#include "vec3.h"

struct Vec4
{
    float x, y, z, w;

    Vec4() : x(0.f), y(0.f), z(0.f), w(0.f) {}
    Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    Vec4(const Vec3& v, float w) : x(v.x), y(v.y), z(v.z), w(w) {}

    Vec4 operator+(const Vec4& v) const { return {x+v.x, y+v.y, z+v.z, w+v.w}; }
    Vec4 operator*(float s)       const { return {x*s,   y*s,   z*s,   w*s};   }

    // Perspective divide — returns NDC xyz
    Vec3 perspectiveDivide() const { return {x/w, y/w, z/w}; }

    // Drop w
    Vec3 xyz() const { return {x, y, z}; }
};
