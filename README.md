# swr — Software Renderer

A from-scratch, high-performance CPU rasterizer written in modern C++17. It uses SDL2 exclusively for window management and pixel upload to the screen. No GPU APIs (OpenGL/Vulkan/DirectX) and no third-party math libraries are used. 

This project serves as an advanced portfolio piece demonstrating a deep understanding of rendering mathematics, multi-threaded performance engineering, and state-of-the-art physically based rendering (PBR).

## Engine Features

This software renderer punches significantly above its weight class, implementing features typically reserved for hardware-accelerated engines:

*   **Physically Based Rendering (PBR):** Complete Cook-Torrance BRDF implementation featuring GGX Normal Distribution, Smith Geometry, and Fresnel-Schlick approximations.
*   **Linear HDR Pipeline:** Native floating-point `Vec3` radiance buffers, operating entirely in linear color space. Supports floating-point `.hdr` textures directly via `stbi_loadf`.
*   **Image-Based Lighting (IBL):** Monte Carlo hemispherical integration bakes physical irradiance maps from high-dynamic-range equirectangular skyboxes at startup for accurate ambient diffuse lighting.
*   **Cinematic Post-Processing:** Implements ACES filmic tonemapping (per-channel), adjustable saturation/exposure, and a luminosity-threshold Bloom effect extracting highlights `> 1.0`.
*   **Alpha Blending & Transparency:** Solves depth-sorting via the Painter's Algorithm. The renderer intelligently separates opaque and transparent geometry into distinct bins automatically sorting and rendering transparent triangles back-to-front.
*   **Tiled Multi-Threading:** Rasterization is aggressively multithreaded. The screen is partitioned into isolated tiles processed by a custom, lock-free thread pool to maximize CPU utilization without risking frame-buffer race conditions.
*   **Data-Driven Architecture:** Scenes, entities, hierarchies, materials, and HDR lighting are completely driven via JSON deserialization (`nlohmann/json`), moving configuration entirely out of the C++ source code.
*   **Zero-Overhead Abstractions:** Uses advanced C++ templating (e.g., `rasterizeTriangle<IsShadowPass, WriteDepth>`) to eliminate runtime branching inside the hyper-critical inner rendering loops.

---

## What's Implemented

| Feature | File(s) |
|---|---|
| Window + SDL2 pixel upload | `platform/sdl_window.h` |
| JSON Scene Deserializer | `loaders/scene_loader.h` |
| Floating-Point HDR Framebuffer | `core/framebuffer.h` |
| Multi-threaded Tiled Renderer | `core/renderer.h` |
| Depth buffer & Depth Testing | `core/depthbuffer.h` |
| Shadow mapping (PCF filtered) | `core/shadow_map.h` |
| Row-major Mat4 & Fast Math | `math/mat4.h`, `math/math_utils.h` |
| Scene Hierarchy & Transforms | `scene/scene.h`, `scene/entity.h` |
| Camera & Free Look | `scene/camera.h` |
| Near-plane Clipping | `pipeline/clipping.h` |
| Edge-function Rasterization | `rasterizer/rasterizer.h` |
| Perspective-correct interpolation | `rasterizer/rasterizer.h` |
| Monte Carlo Irradiance Bake | `core/asset_manager.h` |
| Cook–Torrance PBR Shading | `shading/pbr.h` |
| HDR & LDR Texture Sampling | `shading/texture.h` |
| OBJ mesh loader | `loaders/obj_loader.h` |

---

## Architecture

The engine follows a strict **Data-Driven Ownership Model**:

```text
SceneLoader
├── Parses JSON → Allocates PBRShaders, Materials
└── Populates Scene Data

Scene (Data Container)
├── Camera          (position, orientation, projection)
├── LightList       (directional + point lights)
├── ShadowMap       (depth buffer for the key light)
└── Entity[]        (owns meshes, references materials)
    ├── Transform   (position, rotation, scale, parent-child inheritance)
    ├── Mesh*       (shared, owned by AssetManager)
    └── Material*   (shared, owned by SceneLoader)
```

- **AssetManager** owns raw resources (Meshes, HDR/LDR Textures).
- **SceneLoader** owns dynamic render state (Materials, Shaders).
- **Scene** owns the scene graph (Entities, Lights).
- **Renderer** consumes a `Scene&` and executes the pipeline:
  1. **Shadow Pass:** Depth-only rasterization into the `ShadowMap`.
  2. **Geometry Binning:** Triangles are sorted into `Opaque` or `Transparent` arrays.
  3. **Opaque Pass:** Multi-threaded PBR rasterization with depth writing.
  4. **Transparent Pass:** Sorted back-to-front rasterization with Alpha Blending.
  5. **Post-Process Pass:** Bloom extraction, ACES tonemapping, and Gamma 2.2 approximation.

---

## Math Convention

- **Storage:** Row-major (`m[row][col]`)
- **Multiplication:** Column-vector — `v' = M * v`
- **Coordinate system:** Right-handed (camera looks down `-Z`)
- **NDC:** x,y,z ∈ `[-1, +1]` (OpenGL convention)
- **Screen origin:** Top-left

---

## Controls

| Key | Action |
|---|---|
| `W`, `A`, `S`, `D` | Move Camera (Forward, Left, Back, Right) |
| `E`, `Q` | Move Up / Down |
| `Shift` | Sprint (4× speed) |
| `F` | Toggle point light attached to camera (Flashlight) |
| `P`, `L` | Step Saturation UP/DOWN (Color grading) |
| `1` - `5` | Toggle Debug Views (None, Normals, UVs, Shadows, Tangents) |
| `Mouse` | Free Look |

---

## Building

### Prerequisites

- CMake ≥ 3.16
- C++17 compiler (GCC, Clang, MSVC)
- SDL2 development libraries

### Recommended Build Flags
The codebase is heavily engineered to benefit from compiler auto-vectorization (SIMD) and fast math. The provided `CMakeLists.txt` automatically applies:
- **GCC/Clang:** `-O3 -march=native -ffast-math`
- **MSVC:** `/O2 /arch:AVX2 /fp:fast`

### Linux / macOS

```bash
sudo apt install libsdl2-dev   # or: brew install sdl2
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/swr
```
Or use the included script: `./build.sh`

### Windows (WSL)

```bash
# From WSL (Debian/Ubuntu):
sudo apt install libsdl2-dev
bash build.sh
```

### Windows (MSVC)

```bash
cmake -S . -B build -DSDL2_DIR="C:/SDL2/cmake"
cmake --build build --config Release
```

---

## Project Layout

```text
swr/
├─ src/
│   ├─ core/          ← Renderer loop, IBL baking, Frame/Depth/Shadow buffers
│   ├─ loaders/       ← JSON Scene parser, Wavefront OBJ parser
│   ├─ math/          ← Custom vector, matrix, and accelerated math functions
│   ├─ pipeline/      ← Sutherland-Hodgman clipping, projection
│   ├─ platform/      ← SDL window, input system
│   ├─ rasterizer/    ← Barycentric math, edge-function definitions
│   ├─ scene/         ← Camera, Lights, Transforms, Entities, Materials
│   ├─ shading/       ← Texture mapping, BRDF logic, Shaders
│   ├─ third_party/   ← stb_image (textures), nlohmann (JSON)
│   └─ main.cpp       ← Entry point, OS loop
├─ assets/
│   ├─ models/        ← .obj meshes
│   ├─ scenes/        ← .json scene descriptions
│   └─ textures/      ← HDR / LDR texture maps
├─ CMakeLists.txt
└─ build.sh
```
