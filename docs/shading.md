# Shading Architecture

## Overview

The renderer operates in a **Linear HDR (High Dynamic Range) Color Space**. 
Unlike basic rasterizers that output clamped `[0, 255]` pixel colors directly, our shaders return floating-point native `Vec4` values (RGB + Alpha) representing physical irradiance.

All shaders implement the `Shader` interface from `shading/shader.h`:

```cpp
struct FragmentInput {
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
    Vec3 tangent;
    float invW;
    float depth;
};

struct Shader {
    // Injected once per frame (by Renderer) for the pass
    virtual void setFrameState(const Vec3& camPos, const LightList* lts, const ShadowMap* sm) = 0;
    
    // Evaluated per fragment. Returns a linear HDR Vec4 (rgb, alpha)
    virtual Vec4 shade(const FragmentInput& frag) const = 0;
};
```

Because shaders return linear HDR geometry, **Tonemapping** and **Gamma Correction** are decoupled from the shading step. They are handled globally during the post-processing phase (`Renderer::applyBloom()`).

---

## PBR (Cook-Torrance)

**File:** `shading/pbr.h`

Our primary material shader implements a physically-based microfacet BRDF.

Components:
- **NDF (Normal Distribution Function):** Trowbridge-Reitz GGX
- **Geometry (G):** Smith's Schlick-GGX
- **Fresnel (F):** Fresnel-Schlick approximation with Roughness injection

```cpp
Vec3 f_r = (D * G * F) / max(4.0f * dot(N,L) * dot(N,V), 0.001f);
// Total Radiance = (Diffuse + Specular f_r) * Radiance * NdotL
```

### Image-Based Lighting (IBL)
The PBR shader evaluates ambient light using a baked irradiance map originating from a floating-point `.hdr` equirectangular skybox. The skybox acts as a giant area light encapsulating the scene.

---

## Legacy Models
While PBR is highly recommended, the engine retains legacy shading models for benchmarking and stylistic purposes:

- **Phong (`shading/phong.h`):** Standard Blinn-Phong specular shading.
- **Lambert (`shading/lambert.h`):** Purely diffuse shading.

---

## Tonemapping & Post-Processing

### Film-like ACES
To display HDR values (which can easily exceed `1.0`) on a standard LDR monitor, the renderer passes all completed floating-point framebuffers through the **ACES (Academy Color Encoding System)** filmic operator. 

This ensures that extremely bright spots (like the sun or specular specular highlights) smoothly roll off into white rather than clamping unnaturally, preserving color hue in high-luminance areas.

### Bloom
Because we preserve unclamped floats (`> 1.0`), the post-processor can cleanly extract emission and physical highlights thresholding the physical intensity buffer to generate bloom via symmetric Gaussian blurs.

### Hardware-Accelerated Gamma Approximation
True `std::pow(color, 1.0f / 2.2f)` costs three expensive transcendental calls per pixel. We approximate this in the `ShaderUtils::packColor` packing stage using `std::sqrt()` (a fast hardware instruction representing Gamma 2.0) to aggressively maximize framerates.

```cpp
// shading/shader.h (Conceptual workflow)
Vec3 hdrColor = pbrShader.shade(frag);
Vec3 mapped = ShaderUtils::tonemapACES(hdrColor);

// Fast Gamma ~2.0
float r = std::sqrt(mapped.x);
float g = std::sqrt(mapped.y);
float b = std::sqrt(mapped.z);
```
