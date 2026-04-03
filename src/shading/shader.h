#pragma once
#include <cstdint>
#include "../math/vec3.h"
#include "../math/vec2.h"

// Forward declarations for per-frame state injection.
struct LightList;
struct ShadowMap;

// Fragment data delivered to a shader from the rasterizer.
struct FragmentInput
{
    Vec3  position;     // world-space position
    Vec3  normal;       // world-space normal (normalised)
    Vec2  uv;
    float depth;        // NDC z
};

// Base interface all shaders implement.
// Returns packed ARGB8888 colour for the fragment.
struct Shader
{
    virtual uint32_t shade(const FragmentInput& frag) const = 0;

    // Called by Renderer before drawing each entity to inject per-frame state.
    // Default no-ops — override in shaders that need lights/camera/shadows.
    virtual void setFrameState(const Vec3& /*cameraPos*/,
                               const LightList* /*lights*/,
                               const ShadowMap* /*shadowMap*/) {}

    virtual ~Shader() = default;
};
