#pragma once
#include "../math/vec3.h"

enum class LightType { Directional, Point };

struct Light
{
    LightType type      = LightType::Directional;
    Vec3      direction = {0.f, -1.f, 0.f};   // world-space, for Directional
    Vec3      position  = {0.f, 5.f,  0.f};   // world-space, for Point
    Vec3      color     = {1.f,  1.f,  1.f};  // linear RGB
    float     intensity = 1.f;
};
