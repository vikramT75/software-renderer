# Rendering Pipeline

## Overview

The engine implements a modern, heavily optimized CPU graphics pipeline roughly mimicking the logical stages of hardware GPUs like OpenGL or DirectX.

```text
 CPU side                          Per-vertex                 Per-fragment
┌──────────┐   ┌──────────────┐  ┌───────────────────────┐  ┌────────────────┐
│  Mesh    │──▶│ Vertex Stage │─▶│ Near-Plane Clipping   │─▶│  Rasterization │
│ Vertex[] │   │  (M * V * P) │  │ (Sutherland-Hodgman)  │  │  (Multi-Tile)  │
└──────────┘   └──────────────┘  └───────┬───────────────┘  └──────┬─────────┘
                                         │                         │
                               ┌─────────▼─────────────┐   ┌───────▼─────────┐
                               │ Perspective Divide    │   │ HDR Shading     │
                               │ & Viewport Transform  │   │ (PBR & Blending)│
                               └───────────────────────┘   └───────┬─────────┘
                                                                   │
┌──────────────┐  ┌───────────────┐                        ┌───────▼─────────┐
│ SDL Window   │◀─│ Tonemapping & │◀───────────────────────│  Framebuffer    │
│ Present      │  │ Post-Process  │                        │  (Vec3 Float)   │
└──────────────┘  └───────────────┘                        └─────────────────┘
```

---

## 1. Vertex Transform

**Input:** `Vertex` (Model-space position, normal, UV, tangent)  
**Output:** `ClipVertex` (Clip-space, Eye-space normal, World-space position)

All incoming vertices are multiplied by the transformation matrices:
`clipSpace = ProjectionMatrix * ViewMatrix * ModelMatrix * localPosition`

## 2. Near-Plane Clipping
**File:** `pipeline/clipping.h`

Triangles that cross the near-plane (`z = -w`) must be precisely clipped to prevent geometry tearing and division-by-zero artifacts. 
We use the **Sutherland-Hodgman** algorithm to intersect triangle edges against the near-plane, potentially spawning new vertices and sub-triangles dynamically before rasterization.

## 3. Perspective Divide & Viewport Transform
**File:** `pipeline/projection.h`

**Perspective divide** converts 4D clip space into Normalized Device Coordinates (NDC):
```cpp
ndc.x = clip.x / clip.w
ndc.y = clip.y / clip.w
ndc.z = clip.z / clip.w
```

**Viewport transform** maps NDC to raw screen pixels. At this stage, all attributes (UVs, Normals, Tangents, WorldPositions) are divided by `w` to prepare for mathematically sound **perspective-correct interpolation**.

## 4. Tiled Multi-Threaded Rasterization
**File:** `core/renderer.h`, `rasterizer/rasterizer.h`

The screen is logically divided into multiple bins/tiles (e.g., 32x32 pixels). Instead of iterating every pixel on a single CPU thread, the engine dispatches rasterizing chunks into a robust multithreaded lock-free execution pool.

1. **Backface Culling:** Triangles facing away from the camera are discarded via edge-function winding orders.
2. **Edge Functions:** Barycentric coordinates evaluate if a pixel center falls strictly inside the triangle edges.
3. **Depth Testing:** A highly accurate `std::atomic<float>` lock-free Depth Buffer safely enforces painter's algorithm rules and occlusion for opaque objects.

## 5. Shading Stage
**File:** `shading/pbr.h`

Surviving fragments execute the Shader pass. The attributes are inversely multiplied back by `w` (perspective-correct interpolation). 
The Shader evaluates lighting, evaluates `.hdr` irradiance buffers, and runs Physical Material logic yielding an unclamped continuous `Vec4`.

## 6. Post-Processing & Presentation
**File:** `core/renderer.h`

Because the engine natively outputs unbounded HDR geometry:
1. **Bloom:** Bright pixels (`> 1.0f`) are extracted and blurred safely.
2. **Tonemapping:** The **ACES** filmic curve gracefully rolls off highlights.
3. **Gamma Correction:** Linear space is compressed into sRGB `2.2` (via fast sqrt approximation).
4. **Packing:** Floats are bitwise converted to `ARGB8888` for SDL display.
