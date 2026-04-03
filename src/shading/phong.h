#pragma once
#include "shader.h"
#include "texture.h"
#include "../scene/light.h"
#include "../core/shadow_map.h"
#include "../math/math_utils.h"
#include <cmath>

struct PhongShader : Shader
{
    const LightList *lights = nullptr;
    const ShadowMap *shadowMap = nullptr; // null = no shadows
    Vec3 cameraPos = {0.f, 0.f, 0.f};

    Vec3 albedo = {1.f, 1.f, 1.f};
    Vec3 specular = {1.f, 1.f, 1.f};
    float shininess = 32.f;
    float ambient = 0.05f;

    const Texture *diffuseMap = nullptr;

    void setFrameState(const Vec3& camPos,
                       const LightList* lts,
                       const ShadowMap* sm) override
    {
        cameraPos = camPos;
        lights = lts;
        shadowMap = sm;
    }

    uint32_t shade(const FragmentInput &frag) const override
    {
        if (!lights || lights->lights.empty())
            return 0xFF888888;

        Vec3 N = frag.normal.normalized();
        Vec3 V = (cameraPos - frag.position).normalized();

        Vec3 diff = albedo;
        if (diffuseMap && diffuseMap->loaded)
        {
            uint32_t t = diffuseMap->sampleBilinear(frag.uv.x, frag.uv.y);
            diff = {
                ((t >> 16) & 0xFF) / 255.f,
                ((t >> 8) & 0xFF) / 255.f,
                ((t) & 0xFF) / 255.f};
        }

        // Check shadow — only applied to key light (index 0)
        bool shadowed = false;
        if (shadowMap)
            shadowed = shadowMap->inShadow(frag.position);

        Vec3 result = diff * ambient;

        for (size_t li = 0; li < lights->lights.size(); ++li)
        {
            const Light &light = lights->lights[li];

            // Shadow only affects key light (first light)
            if (li == 0 && shadowed)
                continue;

            Vec3 L;
            float attenuation = 1.f;

            if (light.type == LightType::Directional)
            {
                L = Vec3{-light.direction.x,
                         -light.direction.y,
                         -light.direction.z}
                        .normalized();
            }
            else
            {
                Vec3 toLight = light.position - frag.position;
                float dist = toLight.length();
                L = toLight * (1.f / dist);
                attenuation = 1.f / (light.attConstant + light.attLinear * dist + light.attQuadratic * dist * dist);
            }

            float NdotL = MathUtils::clamp(N.dot(L), 0.f, 1.f);

            result.x += diff.x * light.color.x * NdotL * light.intensity * attenuation;
            result.y += diff.y * light.color.y * NdotL * light.intensity * attenuation;
            result.z += diff.z * light.color.z * NdotL * light.intensity * attenuation;

            if (NdotL > 0.f)
            {
                Vec3 R = N * (2.f * N.dot(L)) - L;
                float spec = std::pow(MathUtils::clamp(R.dot(V), 0.f, 1.f), shininess);
                result.x += specular.x * light.color.x * spec * light.intensity * attenuation;
                result.y += specular.y * light.color.y * spec * light.intensity * attenuation;
                result.z += specular.z * light.color.z * spec * light.intensity * attenuation;
            }
        }

        float r = MathUtils::clamp(result.x, 0.f, 1.f);
        float g = MathUtils::clamp(result.y, 0.f, 1.f);
        float b = MathUtils::clamp(result.z, 0.f, 1.f);

        return 0xFF000000 | (uint32_t(r * 255.f) << 16) | (uint32_t(g * 255.f) << 8) | uint32_t(b * 255.f);
    }
};