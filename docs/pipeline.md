# Rendering Pipeline

## Overview

```
 CPU side                          Per-vertex                 Per-fragment
┌──────────┐   ┌──────────────┐  ┌───────────────────────┐  ┌────────────────┐
│  Mesh    │──▶│ Vertex stage │─▶│  Rasterization        │─▶│ Fragment stage │
│ Vertex[] │   │  (M * V * P) │  │  (barycentric, z-test)│  │  (shading)     │
└──────────┘   └──────────────┘  └───────────────────────┘  └────────────────┘
                                                                      │
                                                              ┌───────▼───────┐
                                                              │  Framebuffer  │
                                                              └───────────────┘
```

## Stage 1 — Vertex transform

**Input:** `Vertex` — model-space position, normal, UV  
**Output:** `ClipVertex` — clip-space position (before divide), eye-space normal

```cpp
ClipVertex cv;
cv.clip   = MVP * Vec4(v.position, 1.f);   // clip space
cv.normal = ModelMatrix * v.normal;        // world/eye space normal
cv.uv     = v.uv;
```

The transform chain is: **Model → World → Eye (View) → Clip**

```
clip = Projection * View * Model * local_position
```

## Stage 2 — Viewport transform

**Input:** `ClipVertex`  
**Output:** `ScreenVertex`

Perspective divide converts clip → NDC:

```
ndc.x = clip.x / clip.w
ndc.y = clip.y / clip.w
ndc.z = clip.z / clip.w    // depth, kept for z-buffer
```

Viewport maps NDC → pixel coordinates:

```
sx = (ndc.x * 0.5 + 0.5) * width
sy = (1 - (ndc.y * 0.5 + 0.5)) * height   // Y flipped (top-left origin)
```

Attributes are premultiplied by `1/w` for perspective-correct interpolation
(see `docs/math.md`).

## Stage 3 — Rasterization

For each triangle, the rasterizer:

1. Computes the pixel-space bounding box (clamped to the framebuffer).
2. Iterates every pixel centre `(x+0.5, y+0.5)` in the bounding box.
3. Evaluates the edge function for each pixel to compute barycentric coords.
4. Skips pixels outside the triangle.
5. Interpolates NDC z and runs the **depth test** against `Depthbuffer`.
6. Passes surviving fragments to the fragment stage.

**Z-buffer convention:** NDC z ∈ [-1, +1]; depth buffer initialised to +1.
A fragment passes if `z < stored_depth` (closer wins, less-than test).

## Stage 4 — Fragment / shading (stub)

Currently writes a flat `uint32_t` colour.  
The `Shader` base class in `shading/shader.h` defines the interface for future
Lambert, Phong, and PBR implementations.

```cpp
struct Shader {
    virtual uint32_t shade(const FragmentInput& frag) const = 0;
};
```

## Known limitations (to be fixed)

| Issue | Symptom | Fix |
|---|---|---|
| No near-plane clipping | Triangles crossing near-plane are discarded, not clipped | Cohen-Sutherland in clip space |
| No back-face culling | Both sides of geometry are rasterized | Check sign of area2 vs winding |
| No frustum culling | Off-screen triangles still rasterized | Test against all 6 planes |
| Flat colour only | No lighting | Implement `lambert.cpp` |
