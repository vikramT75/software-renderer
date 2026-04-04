#pragma once
#include "../math/vec3.h"
#include "../pipeline/vertex.h"
#include "../scene/light.h"
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
    Vec3 position; // World-space position
    Vec3 normal;   // World-space normal
    Vec2 uv;
    Vec3 tangent; // World-space tangent
    float invW;   // For perspective-correct interpolation
    float depth;  // MISSING MEMBER RE-ADDED
};

struct Shader
{
    // Global toggle for the entire engine
    inline static DebugMode globalDebugMode = DebugMode::None;

    // Individual override for this specific shader instance
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