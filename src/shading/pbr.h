#pragma once
#include "shader.h"
#include "texture.h"
#include "../scene/light.h"
#include "../core/shadow_map.h"
#include "../math/math_utils.h"
#include <cmath>

struct PBRShader : Shader
{
    const LightList *lights = nullptr;
    const ShadowMap *shadowMap = nullptr;
    Vec3 cameraPos = {0.f, 0.f, 0.f};

    // Material parameters
    Vec3 albedo = {1.f, 1.f, 1.f};
    float metallic = 0.f;   // 0 = dielectric, 1 = metal
    float roughness = 0.5f; // 0 = mirror, 1 = fully rough
    float ao = 1.f;         // ambient occlusion (1 = no occlusion)

    const Texture *albedoMap = nullptr;
    const Texture *roughnessMap = nullptr; // R channel
    const Texture *metallicMap = nullptr;  // R channel

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

        // Resolve material from textures or constants
        Vec3 baseColor = albedo;
        float metal = metallic;
        float rough = roughness;

        if (albedoMap && albedoMap->loaded)
        {
            uint32_t t = albedoMap->sampleBilinear(frag.uv.x, frag.uv.y);
            baseColor = {
                ((t >> 16) & 0xFF) / 255.f,
                ((t >> 8) & 0xFF) / 255.f,
                ((t) & 0xFF) / 255.f};
        }
        if (roughnessMap && roughnessMap->loaded)
        {
            uint32_t t = roughnessMap->sampleBilinear(frag.uv.x, frag.uv.y);
            rough = ((t >> 16) & 0xFF) / 255.f;
        }
        if (metallicMap && metallicMap->loaded)
        {
            uint32_t t = metallicMap->sampleBilinear(frag.uv.x, frag.uv.y);
            metal = ((t >> 16) & 0xFF) / 255.f;
        }

        // F0 — base reflectivity at normal incidence
        // Dielectrics: 0.04, metals: tinted by albedo
        Vec3 F0 = {0.04f, 0.04f, 0.04f};
        F0 = F0 * (1.f - metal) + baseColor * metal;

        // Shadow check — key light only (index 0)
        bool shadowed = shadowMap && shadowMap->inShadow(frag.position);

        Vec3 Lo = {0.f, 0.f, 0.f}; // outgoing radiance accumulator

        for (size_t li = 0; li < lights->lights.size(); ++li)
        {
            const Light &light = lights->lights[li];
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

            Vec3 H = (V + L).normalized(); // halfway vector
            float NdotL = MathUtils::clamp(N.dot(L), 0.f, 1.f);
            float NdotV = MathUtils::clamp(N.dot(V), 0.f, 1.f);
            float NdotH = MathUtils::clamp(N.dot(H), 0.f, 1.f);
            float HdotV = MathUtils::clamp(H.dot(V), 0.f, 1.f);

            if (NdotL <= 0.f)
                continue;

            // --- D: GGX normal distribution ---
            float a = rough * rough;
            float a2 = a * a;
            float d = NdotH * NdotH * (a2 - 1.f) + 1.f;
            float D = a2 / (MathUtils::PI * d * d + 1e-7f);

            // --- G: Smith-Schlick geometric attenuation ---
            float k = (rough + 1.f) * (rough + 1.f) / 8.f; // direct lighting k
            float G1V = NdotV / (NdotV * (1.f - k) + k);
            float G1L = NdotL / (NdotL * (1.f - k) + k);
            float G = G1V * G1L;

            // --- F: Fresnel-Schlick ---
            float fc = std::pow(1.f - HdotV, 5.f);
            Vec3 F = F0 + (Vec3{1.f, 1.f, 1.f} - F0) * fc;

            // Cook-Torrance specular BRDF
            Vec3 spec = F * (D * G / (4.f * NdotV * NdotL + 1e-7f));

            // Energy conservation:
            // kD = diffuse contribution (metals have no diffuse)
            Vec3 kD = (Vec3{1.f, 1.f, 1.f} - F) * (1.f - metal);

            Vec3 radiance = {
                light.color.x * light.intensity * attenuation,
                light.color.y * light.intensity * attenuation,
                light.color.z * light.intensity * attenuation};

            // Accumulate: diffuse (Lambertian) + specular (Cook-Torrance)
            Lo.x += (kD.x * baseColor.x / MathUtils::PI + spec.x) * radiance.x * NdotL;
            Lo.y += (kD.y * baseColor.y / MathUtils::PI + spec.y) * radiance.y * NdotL;
            Lo.z += (kD.z * baseColor.z / MathUtils::PI + spec.z) * radiance.z * NdotL;
        }

        // Ambient — simple approximation (no IBL yet)
        Vec3 ambient = baseColor * (0.08f * ao);
        Vec3 color = ambient + Lo;

        // Gamma correction (linear → sRGB)
        float gamma = 1.f / 2.2f;
        color.x = std::pow(MathUtils::clamp(color.x, 0.f, 1.f), gamma);
        color.y = std::pow(MathUtils::clamp(color.y, 0.f, 1.f), gamma);
        color.z = std::pow(MathUtils::clamp(color.z, 0.f, 1.f), gamma);

        return 0xFF000000 | (uint32_t(color.x * 255.f) << 16) | (uint32_t(color.y * 255.f) << 8) | uint32_t(color.z * 255.f);
    }
};