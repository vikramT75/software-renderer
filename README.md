# swr — Software Renderer

A from-scratch CPU rasterizer written in C++17, using SDL2 only for window
management and pixel upload. No GPU APIs. No third-party math libraries.

## What's implemented

| Feature | File(s) |
|---|---|
| Window + SDL2 pixel upload | `platform/sdl_window.h` |
| ARGB framebuffer | `core/framebuffer.h` |
| Floating-point depth buffer | `core/depthbuffer.h` |
| Vec2 / Vec3 / Vec4 | `math/vec*.h` |
| Row-major Mat4 (col-vector convention) | `math/mat4.h` |
| Perspective projection (OpenGL NDC) | `math/mat4.h` |
| LookAt view matrix | `math/mat4.h` |
| SRT transform hierarchy | `scene/transform.h` |
| Camera abstraction | `scene/camera.h` |
| Vertex / ClipVertex / ScreenVertex | `pipeline/vertex.h` |
| Viewport transform | `pipeline/projection.h` |
| Edge-function rasterizer | `rasterizer/rasterizer.h` |
| Perspective-correct depth (1/w trick) | `rasterizer/rasterizer.h` |
| Flat-color triangle draw | `core/renderer.h` |

## Planned

- [ ] Near-plane triangle clipping (currently skips tris with w ≤ 0)
- [ ] Back-face culling (CCW winding)
- [ ] Gouraud / Lambert shading (`shading/lambert.cpp`)
- [ ] Phong shading (`shading/phong.cpp`)
- [ ] Texture mapping + bilinear filtering
- [ ] OBJ mesh loader (`loaders/obj_loader.cpp`)
- [ ] PBR shading (`shading/pbr.cpp`)
- [ ] Shadow mapping
- [ ] MSAA

## Math convention

- **Storage:** row-major (`m[row][col]`)
- **Multiplication:** column-vector — `v' = M * v`
- **Coordinate system:** right-handed (camera looks down -Z in eye space)
- **NDC:** x,y,z ∈ [-1, +1]  (OpenGL convention)
- **Screen origin:** top-left

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

### Windows (MinGW)

```bash
# Point CMake at your SDL2 installation:
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH="C:/SDL2-2.x.x/x86_64-w64-mingw32" \
  -G "MinGW Makefiles" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build
# SDL2.dll is copied automatically next to swr.exe
```

### Windows (MSVC)

```bash
cmake -S . -B build -DSDL2_DIR="C:/SDL2/cmake"
cmake --build build --config Release
```

### Old MinGW one-liner (no CMake)

```bash
g++ src/main.cpp \
  -Isrc \
  -I/c/SDL2-2.x.x/x86_64-w64-mingw32/include \
  -L/c/SDL2-2.x.x/x86_64-w64-mingw32/lib \
  -lmingw32 -lSDL2main -lSDL2 \
  -std=c++17 -O2 -o swr
```

## Project layout

```
swr/
├─ src/
│   ├─ core/
│   │   ├─ renderer.h       ← main draw API
│   │   ├─ framebuffer.h
│   │   └─ depthbuffer.h
│   ├─ math/
│   │   ├─ vec2.h / vec3.h / vec4.h
│   │   ├─ mat4.h
│   │   └─ math_utils.h
│   ├─ pipeline/
│   │   ├─ vertex.h         ← Vertex / ClipVertex / ScreenVertex
│   │   ├─ triangle.h
│   │   └─ projection.h     ← viewport transform
│   ├─ rasterizer/
│   │   ├─ barycentric.h
│   │   └─ rasterizer.h
│   ├─ shading/
│   │   └─ shader.h         ← base interface (stubs ready)
│   ├─ scene/
│   │   ├─ camera.h
│   │   ├─ transform.h
│   │   └─ light.h
│   ├─ loaders/             ← OBJ loader (coming)
│   ├─ platform/
│   │   └─ sdl_window.h
│   └─ main.cpp
├─ assets/
│   ├─ models/
│   └─ textures/
├─ screenshots/
├─ docs/
│   ├─ pipeline.md
│   ├─ math.md
│   └─ shading.md
├─ CMakeLists.txt
└─ README.md
```
