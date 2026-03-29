#pragma once
#include "shader.h"
#include "texture.h"
#include "../scene/light.h"
#include "../math/math_utils.h"

struct PhongShader : Shader
{
    Light light;

    Vec3 albedo = {1.f, 1.f, 1.f};
    Vec3 specular = {1.f, 1.f, 1.f};
    float shininess = 32.f;
    float ambient = 0.05f;

    Vec3 cameraPos = {0.f, 0.f, 0.f};

    const Texture *diffuseMap = nullptr; // optional — null = use albedo colour

    uint32_t shade(const FragmentInput &frag) const override
    {
        Vec3 N = frag.normal.normalized();

        Vec3 L = (light.type == LightType::Directional)
                     ? Vec3{-light.direction.x,
                            -light.direction.y,
                            -light.direction.z}
                           .normalized()
                     : (light.position - frag.position).normalized();

        Vec3 V = (cameraPos - frag.position).normalized();

        float NdotL = MathUtils::clamp(N.dot(L), 0.f, 1.f);
        Vec3 R = N * (2.f * N.dot(L)) - L;
        float spec = std::pow(MathUtils::clamp(R.dot(V), 0.f, 1.f), shininess);

        // Resolve diffuse colour — texture overrides albedo if present
        Vec3 diff = albedo;
        if (diffuseMap && diffuseMap->loaded)
        {
            uint32_t texel = diffuseMap->sampleBilinear(frag.uv.x, frag.uv.y);
            diff.x = static_cast<float>((texel >> 16) & 0xFF) / 255.f;
            diff.y = static_cast<float>((texel >> 8) & 0xFF) / 255.f;
            diff.z = static_cast<float>((texel) & 0xFF) / 255.f;
        }

        float li = light.intensity;
        float r = MathUtils::clamp(diff.x * light.color.x * (ambient + NdotL * li) + specular.x * light.color.x * spec * li, 0.f, 1.f);
        float g = MathUtils::clamp(diff.y * light.color.y * (ambient + NdotL * li) + specular.y * light.color.y * spec * li, 0.f, 1.f);
        float b = MathUtils::clamp(diff.z * light.color.z * (ambient + NdotL * li) + specular.z * light.color.z * spec * li, 0.f, 1.f);

        return (0xFF000000) | (static_cast<uint32_t>(r * 255.f) << 16) | (static_cast<uint32_t>(g * 255.f) << 8) | static_cast<uint32_t>(b * 255.f);
    }
};