#pragma once
#include "../math/vec2.h"
#include "../math/vec3.h"
#include "../math/vec4.h"

struct Vertex
{
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
    Vec3 tangent;
};

struct ClipVertex
{
    Vec4 clip;
    Vec3 worldPos;
    Vec3 normal;
    Vec3 tangent;
    Vec2 uv;
};

// Single definition for ScreenVertex
struct ScreenVertex
{
    float sx, sy, z, invW;
    Vec3 worldPos;
    Vec3 normal;
    Vec3 tangent;
    Vec2 uv;
};