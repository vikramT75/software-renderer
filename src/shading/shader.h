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
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
    Vec3 tangent;
    float invW;
    float depth;
};

struct Shader
{
    // Removed 'inline' for older C++ compatibility.
    // This must now be defined in main.cpp
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