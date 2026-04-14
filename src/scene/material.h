#pragma once
#include "../rasterizer/rasterizer.h" // CullMode
#include "../shading/shader.h"

// Material = binding between a shader instance and per-entity render state.
//
// The Shader* already holds material properties (albedo, roughness, metallic,
// texture maps, etc.).  Material wraps it with render-time settings that the
// renderer needs but that aren't part of the shading equation itself.
//
// Ownership: Material does NOT own the Shader.  The caller (main.cpp or a
// resource manager) owns shader instances and materials reference them.

struct Material
{
    Shader *shader = nullptr;           // NOT owned — PBRShader, PhongShader, etc.
    CullMode cullMode = CullMode::Back; // per-material face culling
    bool castsShadow = true;            // should this material appear in shadow pass?
    bool isTransparent = false;         // Tell the renderer to depth-sort this
};
