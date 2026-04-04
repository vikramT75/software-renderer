#pragma once
#include "../math/math_utils.h"
#include "../math/vec3.h"
#include "../pipeline/vertex.h"
#include "../scene/light.h"
#include <algorithm>
#include <cstdint>

class ShadowMap;

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

// Helper for common shading operations
struct ShaderUtils
{
    static uint32_t packColor(const Vec3 &color)
    {
        // Simple Reinhard tonemapping and gamma 2.2 approximation
        float r = std::sqrt(color.x / (color.x + 1.0f));
        float g = std::sqrt(color.y / (color.y + 1.0f));
        float b = std::sqrt(color.z / (color.z + 1.0f));

        uint8_t ir = (uint8_t)(MathUtils::clamp(r, 0.f, 1.f) * 255);
        uint8_t ig = (uint8_t)(MathUtils::clamp(g, 0.f, 1.f) * 255);
        uint8_t ib = (uint8_t)(MathUtils::clamp(b, 0.f, 1.f) * 255);
        return 0xFF000000 | (ir << 16) | (ig << 8) | ib;
    }
};

struct Shader
{
    static DebugMode globalDebugMode;
    DebugMode instanceDebugMode = DebugMode::None;

    virtual void setFrameState(const Vec3 &camPos, const LightList *lts, const ShadowMap *sm) = 0;
    virtual uint32_t shade(const FragmentInput &frag) const = 0;
    virtual ~Shader() = default;

  protected:
    DebugMode getActiveMode() const
    {
        return (instanceDebugMode != DebugMode::None) ? instanceDebugMode : globalDebugMode;
    }
};