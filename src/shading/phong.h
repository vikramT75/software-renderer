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
    const Texture *normalMap = nullptr;

    void setFrameState(const Vec3 &camPos, const LightList *lts, const ShadowMap *sm) override
    {
        cameraPos = camPos;
        lights = lts;
        shadowMap = sm;
    }

    uint32_t shade(const FragmentInput &frag) const override
    {
        DebugMode activeMode = getActiveMode();
        if (activeMode != DebugMode::None)
        {
            Vec3 debugCol = {0.f, 0.f, 0.f};
            if (activeMode == DebugMode::Normals)
            {
                Vec3 n = frag.normal.normalized();
                debugCol = {n.x * 0.5f + 0.5f, n.y * 0.5f + 0.5f, n.z * 0.5f + 0.5f};
            }
            else if (activeMode == DebugMode::UVs)
            {
                debugCol = {frag.uv.x, frag.uv.y, 0.f};
            }
            else if (activeMode == DebugMode::Tangents)
            {
                Vec3 t = frag.tangent.normalized();
                debugCol = {t.x * 0.5f + 0.5f, t.y * 0.5f + 0.5f, t.z * 0.5f + 0.5f};
            }
            else if (activeMode == DebugMode::Shadows && shadowMap)
            {
                float s = shadowMap->shadowFactor(frag.position);
                debugCol = {s, s, s};
            }

            uint8_t r = (uint8_t)(MathUtils::clamp(debugCol.x, 0.f, 1.f) * 255);
            uint8_t g = (uint8_t)(MathUtils::clamp(debugCol.y, 0.f, 1.f) * 255);
            uint8_t b = (uint8_t)(MathUtils::clamp(debugCol.z, 0.f, 1.f) * 255);
            return 0xFF000000 | (r << 16) | (g << 8) | b;
        }

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
        for (const auto &light : lights->lights)
        {
            float shadowMultiplier = (light.castsShadow && shadowMap) ? shadowMap->shadowFactor(frag.position) : 1.0f;
            Vec3 L;
            float attenuation = 1.f;
            if (light.type == LightType::Directional)
            {
                L = -light.direction.normalized();
            }
            else
            {
                Vec3 toLight = light.position - frag.position;
                float dist = toLight.length();
                L = toLight * (1.f / dist);
                attenuation = 1.f / (light.attConstant + light.attLinear * dist + light.attQuadratic * dist * dist);
            }

            float NdotL = std::max(N.dot(L), 0.f);
            float combined = light.intensity * attenuation * shadowMultiplier;
            totalLighting += light.color * diffColor * NdotL * combined;

            if (NdotL > 0.f)
            {
                Vec3 R = (N * (2.f * N.dot(L)) - L).normalized();
                float specPower = std::pow(std::max(R.dot(V), 0.f), shininess);
                totalLighting += light.color * specular * specPower * combined;
            }
        }

        Vec3 final = diffColor * ambientIntensity + totalLighting;
        final.x /= (final.x + 1.f);
        final.y /= (final.y + 1.f);
        final.z /= (final.z + 1.f);
        float g = 1.0f / 2.2f;
        return 0xFF000000 | ((uint8_t)(std::pow(final.x, g) * 255) << 16) |
               ((uint8_t)(std::pow(final.y, g) * 255) << 8) | ((uint8_t)(std::pow(final.z, g) * 255));
    }
};