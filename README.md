# swr — Software Renderer

A from-scratch CPU rasterizer written in C++17, using SDL2 only for window
management and pixel upload. No GPU APIs. No third-party math libraries.

## What's implemented

| Feature | File(s) |
|---|---|
| Window + SDL2 pixel upload | `platform/sdl_window.h` |
| ARGB framebuffer | `core/framebuffer.h` |
| Floating-point depth buffer | `core/depthbuffer.h` |
| Shadow mapping (depth-only pass) | `core/shadow_map.h` |
| Scene-driven renderer | `core/renderer.h` |
| Vec2 / Vec3 / Vec4 | `math/vec*.h` |
| Row-major Mat4 (col-vector convention) | `math/mat4.h` |
| Perspective projection (OpenGL NDC) | `math/mat4.h` |
| LookAt view matrix | `math/mat4.h` |
| Math utilities (clamp, radians, PI) | `math/math_utils.h` |
| Scene container (camera, lights, entities) | `scene/scene.h` |
| Entity (mesh + material + transform) | `scene/entity.h` |
| Material (shader + cull mode + shadow flag) | `scene/material.h` |
| SRT transform → model matrix | `scene/transform.h` |
| FPS camera with mouse look | `scene/camera.h` |
| Directional + point lights | `scene/light.h` |
| Vertex / ClipVertex / ScreenVertex | `pipeline/vertex.h` |
| Sutherland–Hodgman near-plane clipping | `pipeline/clipping.h` |
| Viewport transform | `pipeline/projection.h` |
| Edge-function rasterizer with back-face culling | `rasterizer/rasterizer.h` |
| Perspective-correct interpolation (1/w) | `rasterizer/rasterizer.h` |
| Barycentric coordinate computation | `rasterizer/barycentric.h` |
| Shader base class + per-frame state injection | `shading/shader.h` |
| PBR shading (Cook–Torrance BRDF) | `shading/pbr.h` |
| Phong shading | `shading/phong.h` |
| Lambert shading | `shading/lambert.h` |
| Texture loading (BMP/PNG/JPG via stb_image) | `shading/texture.h` |
| Bilinear texture filtering | `shading/texture.h` |
| OBJ mesh loader (with normals + UVs) | `loaders/obj_loader.h` |

## Architecture

The renderer follows a **Scene → Entity → Material** data-driven design:

```
Scene
├── Camera          (position, orientation, projection)
├── LightList       (directional + point lights)
├── ShadowMap       (depth buffer for the key light)
└── Entity[]
    ├── name
    ├── Transform   (position, rotation, scale → model matrix)
    ├── Mesh*       (shared, not owned — vertices + indices)
    └── Material*   (shared, not owned)
        ├── Shader* (PBR / Phong / Lambert instance with material params)
        ├── CullMode
        └── castsShadow
```

- **Scene** is a pure data container with zero rendering logic.
- **Entities** bind geometry, appearance, and placement. Multiple entities can
  share the same `Mesh*` and `Material*` to save memory.
- **Renderer** consumes a `Scene&` and produces a complete frame in two passes:
  1. **Shadow pass** — depth-only rasterization into the shadow map for all
     shadow-casting entities.
  2. **Main pass** — full shading with PBR lighting, shadow lookup, and texture
     sampling.

## Planned

- [ ] MSAA
- [ ] Normal mapping
- [ ] IBL ambient (currently a flat 0.08 ambient term)
- [ ] Transparency / alpha blending
- [ ] Frustum culling

## Math convention

- **Storage:** row-major (`m[row][col]`)
- **Multiplication:** column-vector — `v' = M * v`
- **Coordinate system:** right-handed (camera looks down -Z in eye space)
- **NDC:** x,y,z ∈ [-1, +1]  (OpenGL convention)
- **Screen origin:** top-left

## Controls

| Key | Action |
|---|---|
| W / A / S / D | Move forward / left / back / right |
| E / Q | Move up / down |
| Shift | Sprint (4× speed) |
| F | Toggle flashlight (point light attached to camera) |
| Mouse | Free look |

## Building

### Prerequisites

- CMake ≥ 3.16
- C++17 compiler (GCC, Clang, MSVC)
- SDL2 development libraries

### Linux / macOS

```bash
sudo apt install libsdl2-dev   # or: brew install sdl2
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/swr
```

Or use the included build script:

```bash
./build.sh
```

### Windows (WSL)

```bash
# From WSL (Debian/Ubuntu):
sudo apt install libsdl2-dev
bash build.sh
```

### Windows (MinGW)

```bash
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH="C:/SDL2-2.x.x/x86_64-w64-mingw32" \
  -G "MinGW Makefiles" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Windows (MSVC)

```bash
cmake -S . -B build -DSDL2_DIR="C:/SDL2/cmake"
cmake --build build --config Release
```

## Project layout

```
swr/
├─ src/
│   ├─ core/
│   │   ├─ renderer.h       ← scene-driven render loop (shadow + main pass)
│   │   ├─ shadow_map.h     ← depth-only shadow map
│   │   ├─ framebuffer.h
│   │   └─ depthbuffer.h
│   ├─ math/
│   │   ├─ vec2.h / vec3.h / vec4.h
│   │   ├─ mat4.h
│   │   └─ math_utils.h
│   ├─ pipeline/
│   │   ├─ vertex.h         ← Vertex / ClipVertex / ScreenVertex
│   │   ├─ clipping.h       ← Sutherland–Hodgman near-plane clipper
│   │   ├─ triangle.h
│   │   └─ projection.h     ← viewport transform
│   ├─ rasterizer/
│   │   ├─ barycentric.h
│   │   └─ rasterizer.h
│   ├─ shading/
│   │   ├─ shader.h         ← base Shader interface
│   │   ├─ pbr.h            ← Cook–Torrance PBR (GGX + Smith + Fresnel)
│   │   ├─ phong.h          ← Blinn-Phong
│   │   ├─ lambert.h        ← Lambertian diffuse
│   │   └─ texture.h        ← image loading + bilinear sampling
│   ├─ scene/
│   │   ├─ scene.h          ← top-level data container
│   │   ├─ entity.h         ← mesh + material + transform binding
│   │   ├─ material.h       ← shader + render state
│   │   ├─ camera.h         ← FPS camera with mouse look
│   │   ├─ transform.h      ← SRT → model matrix
│   │   └─ light.h          ← directional + point light
│   ├─ loaders/
│   │   └─ obj_loader.h     ← Wavefront OBJ parser
│   ├─ third_party/
│   │   └─ stb_image.h
│   ├─ platform/
│   │   └─ sdl_window.h
│   └─ main.cpp             ← scene setup + game loop
├─ assets/
│   ├─ models/              ← .obj meshes
│   └─ textures/            ← .bmp / .jpg / .png textures
├─ screenshots/
├─ docs/
│   ├─ pipeline.md
│   ├─ math.md
│   └─ shading.md
├─ CMakeLists.txt
├─ build.sh
└─ README.md
```
