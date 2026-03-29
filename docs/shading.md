# Shading

## Architecture

All shaders implement `Shader` from `shading/shader.h`:

```cpp
struct Shader {
    virtual uint32_t shade(const FragmentInput& frag) const = 0;
};
```

`FragmentInput` carries: world-space position and normal, UV, depth.

The renderer will call `shader.shade(frag)` per surviving fragment once
shading is wired up.

---

## Lambert (diffuse)

Planned file: `shading/lambert.cpp`

```
L_diffuse = albedo * max(0, dot(N, L)) * lightColor * lightIntensity
```

Where N is the surface normal and L is the direction *towards* the light.
Requires interpolated world-space normals and at least one `Light` from
`scene/light.h`.

---

## Phong (specular)

Planned file: `shading/phong.cpp`

```
R = reflect(-L, N)
L_specular = specularColor * pow(max(0, dot(R, V)), shininess)
L_total = ambient + L_diffuse + L_specular
```

Where V is the direction towards the camera (eye vector).

---

## PBR (Cook-Torrance)

Planned file: `shading/pbr.cpp`

Microfacet BRDF with:
- GGX normal distribution function (NDF)
- Smith-Schlick geometric attenuation (G)
- Fresnel-Schlick approximation (F)

```
f_r = (D * G * F) / (4 * dot(N,L) * dot(N,V))
```

Parameters: albedo, metallic, roughness, AO.

---

## Colour packing

All shaders return `uint32_t` in **ARGB8888** layout:

```
Bit 31-24: Alpha
Bit 23-16: Red
Bit 15-8:  Green
Bit  7-0:  Blue
```

Helper (no SDL dependency required):

```cpp
inline uint32_t packRGBA(float r, float g, float b, float a = 1.f)
{
    auto c = [](float f) -> uint8_t {
        return static_cast<uint8_t>(std::clamp(f, 0.f, 1.f) * 255.f);
    };
    return (uint32_t(c(a)) << 24) | (uint32_t(c(r)) << 16)
         | (uint32_t(c(g)) <<  8) |  uint32_t(c(b));
}
```
