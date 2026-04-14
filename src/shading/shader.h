#pragma once
#include "../math/math_utils.h"
#include "../math/vec3.h"
#include "../pipeline/vertex.h"
#include <algorithm>
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
    static uint32_t packColor(const Vec3 &color)
    {
        // Reinhard-family approximation: sqrt(x/(x+1)) maps [0,∞) → [0,1)
        float r = std::sqrt(color.x / (color.x + 1.0f));
        float g = std::sqrt(color.y / (color.y + 1.0f));
        float b = std::sqrt(color.z / (color.z + 1.0f));

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
    // Returns a raw linear HDR Vec3. Tonemapping is done once by the renderer.
    virtual Vec3 shade(const FragmentInput &frag) const = 0;
    virtual ~Shader() = default;

  protected:
    DebugMode getActiveMode() const
    {
        return (instanceDebugMode != DebugMode::None) ? instanceDebugMode : globalDebugMode;
    }
};