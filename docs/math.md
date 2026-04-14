# Math Reference

## Coordinate systems

```
World / Eye space (right-handed)

        +Y
        |
        |
        +-------> +X
       /
      /
    +Z  (towards viewer; camera looks down -Z)
```

Camera sits at the origin of eye space (after the view transform). Objects at
negative Z are in front of the camera.

## Matrix convention

| Property | Choice |
|---|---|
| Storage | Row-major: `m[row][col]` |
| Multiplication | Column-vector: `v' = M * v` |
| Composition | `MVP = Projection * View * Model` (rightmost applied first) |

### Why row-major + column-vector?

It matches OpenGL's mathematical convention while keeping array indexing
intuitive (`m[row][col]` reads naturally). The GLSL `mat4` is column-major in
*memory* but the algebra is the same — don't confuse storage order with
multiplication convention.

## Projection

We use a symmetric perspective projection mapping eye-space z ∈ `[-zNear, -zFar]`
to NDC z ∈ `[-1, +1]` (OpenGL convention).

```
        [ f/a    0       0          0       ]
P  =    [  0     f       0          0       ]
        [  0     0   (f+n)/(n-f)  2fn/(n-f) ]
        [  0     0      -1          0       ]

where f = 1/tan(fovY/2),  a = width/height,  n = zNear,  f = zFar
```

After multiplying: `w_clip = -z_eye`  (always positive for visible geometry).

## Perspective-correct interpolation

Attributes (UV, normals) must be divided by `w` before linear interpolation
across the triangle in screen space, then divided by the interpolated `1/w` to
recover the correct value.

```cpp
// At each vertex: store  attr / w
sv.uv = uv * invW;        // invW = 1.0 / clip.w

// At each fragment: interpolate both, then divide
float invW_frag = bary.w0 * sv0.invW + bary.w1 * sv1.invW + bary.w2 * sv2.invW;
Vec2  uv_frag   = (bary.w0 * sv0.uv  + bary.w1 * sv1.uv  + bary.w2 * sv2.uv)
                  / invW_frag;
```

Depth (NDC z) does **not** need this correction — it is already linear in NDC
space after the perspective divide.

## Barycentric coordinates

For a triangle with vertices v0, v1, v2 and a point p:

```
edge(a, b, p) = (p.x - a.x)(b.y - a.y) - (p.y - a.y)(b.x - a.x)

w0 = edge(v1, v2, p) / area2
w1 = edge(v2, v0, p) / area2
w2 = edge(v0, v1, p) / area2

area2 = edge(v0, v1, v2)   (signed twice the triangle area)
```

Point is inside when w0, w1, w2 are all `≥ 0` (CCW) or all `≤ 0` (CW).

## Rotation order

`Transform::matrix()` applies:  `T * Ry * Rx * Rz * S`

This means scale is applied first, then Z rotation, X rotation, Y rotation,
then translation — a common "YXZ Euler" convention used in many game engines.

---

## Spherical UV Mapping (HDR IBL)

To parse Equirectangular high-dynamic-range skyboxes seamlessly onto a virtual sphere and inversely calculate directional dependencies for lighting, we use standard trigonometric wrapping around the Y-axis.

### Direction Vector to UV mapping:
```cpp
// Given a normalized direction vector 'd'
float u = 0.5f - (atan2(d.z, d.x) / (2.0f * PI));
float v = 0.5f + (asin(d.y) / PI);
```

### UV to Direction Vector mapping:
```cpp
// Inverse math to find direction out of a pixel layout
float y = sin((v - 0.5f) * PI);
float angle = (0.5f - u) * 2.0f * PI;
float r = cos((v - 0.5f) * PI);
float x = cos(angle) * r;
float z = sin(angle) * r;

Vec3 direction_to_sun = Vec3(x, y, z);
```

---

## High-Performance Hardware Approximations

Because software rendering cannot offload mathematics to GPU ALUs, standard transcendental math functions (`pow`, `sin`, `atan2`, etc.) become massive frame-time bottlenecks inside pixel-level inner loops. 

1. **Fast-Math & Vectorization (`-ffast-math` / `/fp:fast`)**: 
   The renderer's CMake pipeline forcefully configures compilers to drop strict IEEE 754 precision adherence (trading absolute floating point standards for speed). This instructs the optimizer to dynamically substitute complex instructions with fused hardware CPU-level SIMD intrinsics (AVX2/SSE) wherever arrays and loops allow.

2. **Branchless Template Specialization (`<IsShadowPass, WriteDepth>`)**: 
   Standard branching `if (statement)` kills CPU instruction caching. Rendering logic is parameterized primarily via compile-time boolean templates (`rasterizeTriangle<bool>`), stripping branching paths entirely and flattening the pipeline before assembly.

3. **Gamma ≈ 2.0 Approximation**:
   Correctly scaling linear output space to an sRGB ~2.2 monitor normally requires `std::pow(color, 1.0f / 2.2f)`, costing 3 heavy transcendental evaluations per pixel. This renderer safely degrades this standard to exactly a **2.0** exponent correction, mapping seamlessly to the standard `std::sqrt()` function which invokes a hyper-optimized intrinsic hardware route, rescuing millions of cycles per frame.
