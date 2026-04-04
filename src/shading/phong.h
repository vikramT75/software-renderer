#pragma once
#include "../core/shadow_map.h"
#include "../math/math_utils.h"
#include "../scene/light.h"
#include "shader.h"
#include "texture.h"
#include <algorithm>
#include <cmath>

struct PhongShader : Shader
{
    const LightList *lights = nullptr;
    const ShadowMap *shadowMap = nullptr;
    Vec3 cameraPos = {0.f, 0.f, 0.f};

    Vec3 albedo = {1.f, 1.f, 1.f};
    Vec3 specular = {1.f, 1.f, 1.f};
    float shininess = 32.f;
    float ambientIntensity = 0.05f;

    const Texture *diffuseMap = nullptr;
    const Texture *normalMap = nullptr; // NEW

    void setFrameState(const Vec3 &camPos, const LightList *lts, const ShadowMap *sm) override
    {
        cameraPos = camPos;
        lights = lts;
        shadowMap = sm;
    }

    uint32_t shade(const FragmentInput &frag) const override
    {
        if (!lights || lights->lights.empty())
            return 0xFF888888;

        // --- Normal Mapping ---
        Vec3 N = frag.normal.normalized();
        if (normalMap && normalMap->loaded)
        {
            Vec3 T = frag.tangent.normalized();
            T = (T - N * N.dot(T)).normalized();
            Vec3 B = N.cross(T);

            uint32_t t = normalMap->sampleBilinear(frag.uv.x, frag.uv.y);
            Vec3 texNormal = {((t >> 16) & 0xFF) / 255.f * 2.f - 1.f, ((t >> 8) & 0xFF) / 255.f * 2.f - 1.f,
                              (t & 0xFF) / 255.f * 2.f - 1.f};
            N = (T * texNormal.x + B * texNormal.y + N * texNormal.z).normalized();
        }

        Vec3 V = (cameraPos - frag.position).normalized();
        Vec3 diffColor = albedo;
        if (diffuseMap && diffuseMap->loaded)
        {
            uint32_t t = diffuseMap->sampleBilinear(frag.uv.x, frag.uv.y);
            diffColor = {((t >> 16) & 0xFF) / 255.f, ((t >> 8) & 0xFF) / 255.f, (t & 0xFF) / 255.f};
        }

        Vec3 totalLighting = {0.f, 0.f, 0.f};

        for (size_t i = 0; i < lights->lights.size(); ++i)
        {
            const auto &light = lights->lights[i];

            float shadowMultiplier = (i == 0 && shadowMap) ? shadowMap->shadowFactor(frag.position) : 1.0f;

            Vec3 L;
            float attenuation = 1.f;
            if (light.type == LightType::Directional)
            {
                L = Vec3{-light.direction.x, -light.direction.y, -light.direction.z}.normalized();
            }
            else
            {
                Vec3 toLight = light.position - frag.position;
                float dist = toLight.length();
                L = toLight * (1.f / dist);
                attenuation = 1.f / (light.attConstant + light.attLinear * dist + light.attQuadratic * dist * dist);
            }

            float NdotL = MathUtils::clamp(N.dot(L), 0.f, 1.f);
            float combinedFactor = light.intensity * attenuation * shadowMultiplier;

            // Diffuse + Specular accumulation
            totalLighting.x += diffColor.x * light.color.x * NdotL * combinedFactor;
            totalLighting.y += diffColor.y * light.color.y * NdotL * combinedFactor;
            totalLighting.z += diffColor.z * light.color.z * NdotL * combinedFactor;

            if (NdotL > 0.f)
            {
                Vec3 R = N * (2.f * N.dot(L)) - L;
                float specPower = std::pow(MathUtils::clamp(R.dot(V), 0.f, 1.f), shininess);
                totalLighting.x += specular.x * light.color.x * specPower * combinedFactor;
                totalLighting.y += specular.y * light.color.y * specPower * combinedFactor;
                totalLighting.z += specular.z * light.color.z * specPower * combinedFactor;
            }
        }

        Vec3 finalColor = {diffColor.x * ambientIntensity + totalLighting.x,
                           diffColor.y * ambientIntensity + totalLighting.y,
                           diffColor.z * ambientIntensity + totalLighting.z};

        // Tone Mapping & Gamma correction for consistency
        finalColor.x /= (finalColor.x + 1.0f);
        finalColor.y /= (finalColor.y + 1.0f);
        finalColor.z /= (finalColor.z + 1.0f);
        float g = 1.0f / 2.2f;
        return 0xFF000000 | ((uint8_t)(std::pow(finalColor.x, g) * 255) << 16) |
               ((uint8_t)(std::pow(finalColor.y, g) * 255) << 8) | ((uint8_t)(std::pow(finalColor.z, g) * 255));
    }
};