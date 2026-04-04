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
    // Per-frame state injected by the Renderer
    const LightList *lights = nullptr;
    const ShadowMap *shadowMap = nullptr;
    Vec3 cameraPos = {0.f, 0.f, 0.f};

    // Material parameters
    Vec3 albedo = {1.f, 1.f, 1.f};
    float metallic = 0.f;   // 0 = dielectric, 1 = metal
    float roughness = 0.5f; // 0 = mirror, 1 = fully rough
    float ao = 1.f;         // ambient occlusion

    // Texture maps
    const Texture *albedoMap = nullptr;
    const Texture *roughnessMap = nullptr;
    const Texture *metallicMap = nullptr;
    const Texture *normalMap = nullptr; // For normal mapping

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

        // --- 1. Normal Mapping (TBN Matrix) ---
        Vec3 N = frag.normal.normalized();
        if (normalMap && normalMap->loaded)
        {
            Vec3 T = frag.tangent.normalized();
            // Orthogonalize tangent
            T = (T - N * N.dot(T)).normalized();
            Vec3 B = N.cross(T); // Reconstruct bitangent

            uint32_t t = normalMap->sampleBilinear(frag.uv.x, frag.uv.y);
            Vec3 texNormal = {((t >> 16) & 0xFF) / 255.f * 2.f - 1.f, ((t >> 8) & 0xFF) / 255.f * 2.f - 1.f,
                              (t & 0xFF) / 255.f * 2.f - 1.f};
            // Transform normal from tangent space to world space
            N = (T * texNormal.x + B * texNormal.y + N * texNormal.z).normalized();
        }

        Vec3 V = (cameraPos - frag.position).normalized();

        // Resolve material properties
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

        Vec3 F0 = {0.04f, 0.04f, 0.04f};
        F0 = F0 + (baseColor - F0) * metal;

        Vec3 Lo = {0.f, 0.f, 0.f};

        // --- 2. Lighting Loop ---
        for (size_t i = 0; i < lights->lights.size(); ++i)
        {
            const auto &light = lights->lights[i];

            // PCF Shadow multiplier for Key Light
            float shadowMultiplier = 1.0f;
            if (light.castsShadow && shadowMap)
            {
                shadowMultiplier = shadowMap->shadowFactor(frag.position);
            }

            Vec3 L;
            float attenuation = 1.0f;
            if (light.type == LightType::Directional)
            {
                L = Vec3{-light.direction.x, -light.direction.y, -light.direction.z}.normalized();
            }
            else
            {
                Vec3 toLight = light.position - frag.position;
                float distance = toLight.length();
                L = toLight * (1.f / distance);
                attenuation = 1.0f / (light.attConstant + light.attLinear * distance +
                                      light.attQuadratic * (distance * distance));
            }

            Vec3 H = (V + L).normalized();
            float NdotL = MathUtils::clamp(N.dot(L), 0.0f, 1.0f);
            float NdotV = MathUtils::clamp(N.dot(V), 0.0f, 1.0f);

            if (NdotL > 0.0f)
            {
                // Cook-Torrance BRDF
                float NdotH = MathUtils::clamp(N.dot(H), 0.0f, 1.0f);
                float HdotV = MathUtils::clamp(H.dot(V), 0.0f, 1.0f);

                float a = rough * rough;
                float a2 = a * a;
                float D = a2 / (MathUtils::PI * std::pow(NdotH * NdotH * (a2 - 1.0f) + 1.0f, 2.0f));

                float k = std::pow(rough + 1.0f, 2.0f) / 8.0f;
                float G = (NdotV / (NdotV * (1.0f - k) + k)) * (NdotL / (NdotL * (1.0f - k) + k));

                Vec3 F = F0 + (Vec3{1.0f, 1.0f, 1.0f} - F0) * std::pow(1.0f - HdotV, 5.0f);

                Vec3 specular = F * (D * G) * (1.0f / (4.0f * NdotV * NdotL + 0.0001f));
                Vec3 kD = (Vec3{1.0f, 1.0f, 1.0f} - F) * (1.0f - metal);

                Vec3 radiance = {light.color.x * light.intensity * attenuation,
                                 light.color.y * light.intensity * attenuation,
                                 light.color.z * light.intensity * attenuation};

                Lo.x += (kD.x * baseColor.x / MathUtils::PI + specular.x) * radiance.x * NdotL * shadowMultiplier;
                Lo.y += (kD.y * baseColor.y / MathUtils::PI + specular.y) * radiance.y * NdotL * shadowMultiplier;
                Lo.z += (kD.z * baseColor.z / MathUtils::PI + specular.z) * radiance.z * NdotL * shadowMultiplier;
            }
        }

        // Ambient, Tone Mapping, Gamma Correction
        Vec3 ambientColor = baseColor * (0.05f * ao);
        Vec3 finalColor = ambientColor + Lo;
        finalColor.x /= (finalColor.x + 1.0f);
        finalColor.y /= (finalColor.y + 1.0f);
        finalColor.z /= (finalColor.z + 1.0f);

        float gamma = 1.0f / 2.2f;
        uint8_t r_out = static_cast<uint8_t>(MathUtils::clamp(std::pow(finalColor.x, gamma) * 255.f, 0.f, 255.f));
        uint8_t g_out = static_cast<uint8_t>(MathUtils::clamp(std::pow(finalColor.y, gamma) * 255.f, 0.f, 255.f));
        uint8_t b_out = static_cast<uint8_t>(MathUtils::clamp(std::pow(finalColor.z, gamma) * 255.f, 0.f, 255.f));

        return 0xFF000000 | (r_out << 16) | (g_out << 8) | b_out;
    }
};