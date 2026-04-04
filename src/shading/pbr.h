#pragma once
#include "../core/shadow_map.h"
#include "../math/math_utils.h"
#include "../scene/light.h"
#include "shader.h"
#include "texture.h"
#include <algorithm>
#include <cmath>

struct PBRShader : Shader
{
    const LightList *lights = nullptr;
    const ShadowMap *shadowMap = nullptr;
    Vec3 cameraPos = {0.f, 0.f, 0.f};

    Vec3 albedo = {1.f, 1.f, 1.f};
    float metallic = 0.f;
    float roughness = 0.5f;
    float ao = 1.f;

    const Texture *albedoMap = nullptr;
    const Texture *roughnessMap = nullptr;
    const Texture *metallicMap = nullptr;
    const Texture *normalMap = nullptr;

    // IBL Irradiance map
    const Texture *irradianceMap = nullptr;

    void setFrameState(const Vec3 &camPos, const LightList *lts, const ShadowMap *sm) override
    {
        cameraPos = camPos;
        lights = lts;
        shadowMap = sm;
    }

    uint32_t shade(const FragmentInput &frag) const override
    {
        // --- 1. Debug Visualizations ---
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
                debugCol = {frag.uv.x, frag.uv.y, 0.f};
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
            else if (activeMode == DebugMode::WorldPos)
            {
                debugCol = {std::abs(std::sin(frag.position.x)), std::abs(std::sin(frag.position.y)),
                            std::abs(std::sin(frag.position.z))};
            }
            uint8_t r = (uint8_t)(MathUtils::clamp(debugCol.x, 0.f, 1.f) * 255);
            uint8_t g = (uint8_t)(MathUtils::clamp(debugCol.y, 0.f, 1.f) * 255);
            uint8_t b = (uint8_t)(MathUtils::clamp(debugCol.z, 0.f, 1.f) * 255);
            return 0xFF000000 | (r << 16) | (g << 8) | b;
        }

        // --- 2. Normal Mapping ---
        Vec3 N = frag.normal.normalized();
        if (normalMap && normalMap->loaded)
        {
            Vec3 T = frag.tangent.normalized();
            T = (T - N * N.dot(T)).normalized();
            Vec3 B = N.cross(T);
            uint32_t t = normalMap->sampleBilinear(frag.uv.x, frag.uv.y);
            Vec3 texN = {((t >> 16) & 0xFF) / 255.f * 2.f - 1.f, ((t >> 8) & 0xFF) / 255.f * 2.f - 1.f,
                         (t & 0xFF) / 255.f * 2.f - 1.f};
            N = (T * texN.x + B * texN.y + N * texN.z).normalized();
        }

        Vec3 V = (cameraPos - frag.position).normalized();
        Vec3 baseColor = albedo;
        if (albedoMap && albedoMap->loaded)
        {
            uint32_t t = albedoMap->sampleBilinear(frag.uv.x, frag.uv.y);
            baseColor = {((t >> 16) & 0xFF) / 255.f, ((t >> 8) & 0xFF) / 255.f, (t & 0xFF) / 255.f};
        }

        float rough = roughness;
        if (roughnessMap && roughnessMap->loaded)
        {
            uint32_t t = roughnessMap->sampleBilinear(frag.uv.x, frag.uv.y);
            rough = ((t >> 16) & 0xFF) / 255.f;
        }

        float metal = metallic;
        if (metallicMap && metallicMap->loaded)
        {
            uint32_t t = metallicMap->sampleBilinear(frag.uv.x, frag.uv.y);
            metal = ((t >> 16) & 0xFF) / 255.f;
        }

        Vec3 F0 = Vec3{0.04f, 0.04f, 0.04f} + (baseColor - Vec3{0.04f, 0.04f, 0.04f}) * metal;

        // --- 3. Direct Lighting (Directional/Point) ---
        Vec3 Lo = {0.f, 0.f, 0.f};
        for (const auto &light : lights->lights)
        {
            float shadow = (light.castsShadow && shadowMap) ? shadowMap->shadowFactor(frag.position) : 1.0f;
            Vec3 L;
            float att = 1.0f;
            if (light.type == LightType::Directional)
                L = -light.direction.normalized();
            else
            {
                Vec3 toL = light.position - frag.position;
                float d = toL.length();
                L = toL * (1.f / d);
                att = 1.0f / (light.attConstant + light.attLinear * d + light.attQuadratic * d * d);
            }
            Vec3 H = (V + L).normalized();
            float nL = std::max(N.dot(L), 0.0f), nV = std::max(N.dot(V), 0.0f);
            if (nL > 0.0f)
            {
                float nH = std::max(N.dot(H), 0.0f), hV = std::max(H.dot(V), 0.0f);
                float a2 = std::pow(rough * rough, 2);
                float D = a2 / (MathUtils::PI * std::pow(nH * nH * (a2 - 1.f) + 1.f, 2));
                float k = std::pow(rough + 1.f, 2) / 8.f;
                float G = (nV / (nV * (1.f - k) + k)) * (nL / (nL * (1.f - k) + k));
                Vec3 F = F0 + (Vec3{1.f, 1.f, 1.f} - F0) * std::pow(1.f - hV, 5.f);
                Vec3 spec = F * (D * G) / (4.f * nV * nL + 0.0001f);
                Vec3 kD = (Vec3{1.f, 1.f, 1.f} - F) * (1.f - metal);
                Lo += (kD * baseColor / MathUtils::PI + spec) * (light.color * light.intensity * att) * nL * shadow;
            }
        }

        // --- 4. IBL Ambient (Irradiance) ---
        Vec3 irradiance = {0.03f, 0.03f, 0.04f}; // Deep blue fallback
        if (irradianceMap && irradianceMap->loaded)
        {
            irradiance = irradianceMap->sampleSpherical(N);
        }

        // Approximate Fresnel for ambient term
        float nV = std::max(N.dot(V), 0.0f);
        Vec3 F =
            F0 + (Vec3{std::max(1.0f - rough, F0.x), std::max(1.0f - rough, F0.y), std::max(1.0f - rough, F0.z)} - F0) *
                     std::pow(1.0f - nV, 5.0f);
        Vec3 kD = (Vec3{1.0f, 1.0f, 1.0f} - F) * (1.0f - metal);
        Vec3 ambient = kD * baseColor * irradiance * ao;

        Vec3 final = ambient + Lo;

        // Tone Mapping & Gamma Correction
        final = final / (final + Vec3{1.f, 1.f, 1.f});
        float g = 1.0f / 2.2f;
        return 0xFF000000 | ((uint8_t)(std::pow(final.x, g) * 255) << 16) |
               ((uint8_t)(std::pow(final.y, g) * 255) << 8) | (uint8_t)(std::pow(final.z, g) * 255);
    }
};