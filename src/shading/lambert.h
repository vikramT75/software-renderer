#pragma once
#include "shader.h"
#include "../scene/light.h"
#include "../math/math_utils.h"
#include <cstdint>

struct LambertShader : Shader
{
    Light light;
    Vec3 albedo = {1.f, 1.f, 1.f}; // base colour, linear [0,1]
    float ambient = 0.05f;         // minimum brightness

    uint32_t shade(const FragmentInput &frag) const override
    {
        // Light direction — towards the light (negate for directional)
        Vec3 L = (light.type == LightType::Directional)
                     ? Vec3{-light.direction.x,
                            -light.direction.y,
                            -light.direction.z}
                           .normalized()
                     : (light.position - frag.position).normalized();

        Vec3 N = frag.normal.normalized();

        float diff = MathUtils::clamp(N.dot(L), 0.f, 1.f);
        float intensity = ambient + diff * light.intensity;

        float r = MathUtils::clamp(albedo.x * light.color.x * intensity, 0.f, 1.f);
        float g = MathUtils::clamp(albedo.y * light.color.y * intensity, 0.f, 1.f);
        float b = MathUtils::clamp(albedo.z * light.color.z * intensity, 0.f, 1.f);

        return (0xFF000000) | (static_cast<uint32_t>(r * 255.f) << 16) | (static_cast<uint32_t>(g * 255.f) << 8) | static_cast<uint32_t>(b * 255.f);
    }
};