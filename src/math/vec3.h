#pragma once
#include <cmath>

struct Vec3
{
    float x, y, z;

    Vec3() : x(0.f), y(0.f), z(0.f) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    Vec3 operator+(const Vec3& v) const { return {x + v.x, y + v.y, z + v.z}; }
    Vec3 operator-(const Vec3& v) const { return {x - v.x, y - v.y, z - v.z}; }
    Vec3 operator*(float s)       const { return {x * s,   y * s,   z * s};   }
    Vec3 operator-()              const { return {-x, -y, -z}; }

    float dot(const Vec3& v)   const { return x*v.x + y*v.y + z*v.z; }
    float length()             const { return std::sqrt(dot(*this)); }
    Vec3  normalized()         const { float l = length(); return {x/l, y/l, z/l}; }

    Vec3 cross(const Vec3& v) const
    {
        return {
            y * v.z - z * v.y,
            z * v.x - x * v.z,
            x * v.y - y * v.x
        };
    }
};
