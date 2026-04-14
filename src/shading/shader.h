#pragma once
#include "../math/math_utils.h"
#include "../math/vec3.h"
#include "../math/vec4.h"
#include "../pipeline/vertex.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

// Restore the missing CullMode enum
enum class CullMode
{
    None,
    Back,
    Front
};

enum class DebugMode
{
    None = 0,
    Normals,
    UVs,
    Shadows,
    Tangents,
    WorldPos
};

struct FragmentInput
{
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
    Vec3 tangent;
    float invW;
    float depth;
};

// Tonemap a linear HDR Vec3 and pack to ARGB8888.
// Called exclusively by Renderer::applyBloom() — NOT inside shade().
struct ShaderUtils
{
    static float exposure;
    static float saturation;

    static Vec3 tonemapACES(Vec3 v)
    {
        v = v * exposure;

        // 1. Standard Per-Channel ACES
        // This naturally desaturates extremely bright highlights (like the sun) to white.
        float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
        float r = (v.x * (a * v.x + b)) / (v.x * (c * v.x + d) + e);
        float g = (v.y * (a * v.y + b)) / (v.y * (c * v.y + d) + e);
        float b_ch = (v.z * (a * v.z + b)) / (v.z * (c * v.z + d) + e);
        Vec3 result = {r, g, b_ch};

        // 2. Apply Custom Saturation (User tuned down to 1.0 default)
        float finalLum = result.x * 0.2126f + result.y * 0.7152f + result.z * 0.0722f;
        result = Vec3(finalLum, finalLum, finalLum) + (result - Vec3(finalLum, finalLum, finalLum)) * saturation;

        return result;
    }

    static uint32_t packColor(const Vec3 &color)
    {
        Vec3 mapped = tonemapACES(color);

        // Fast gamma ~2.0 approximation (sqrt is a HW instruction, pow is not).
        // True gamma 2.2 costs 3 transcendental pow calls per pixel.
        float r = std::sqrt(std::max(mapped.x, 0.0f));
        float g = std::sqrt(std::max(mapped.y, 0.0f));
        float b = std::sqrt(std::max(mapped.z, 0.0f));

        uint8_t ir = (uint8_t)(MathUtils::clamp(r, 0.f, 1.f) * 255);
        uint8_t ig = (uint8_t)(MathUtils::clamp(g, 0.f, 1.f) * 255);
        uint8_t ib = (uint8_t)(MathUtils::clamp(b, 0.f, 1.f) * 255);
        return 0xFF000000 | (ir << 16) | (ig << 8) | ib;
    }
};

class ShadowMap;
struct LightList;

struct Shader
{
    static DebugMode globalDebugMode;
    DebugMode instanceDebugMode = DebugMode::None;

    virtual void setFrameState(const Vec3 &camPos, const LightList *lts, const ShadowMap *sm) = 0;
    // Returns a raw linear HDR Vec4 (rgb, alpha). Tonemapping is done once by the renderer.
    virtual Vec4 shade(const FragmentInput &frag) const = 0;
    virtual ~Shader() = default;

  protected:
    DebugMode getActiveMode() const
    {
        return (instanceDebugMode != DebugMode::None) ? instanceDebugMode : globalDebugMode;
    }
};