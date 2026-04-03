#pragma once
#include "camera.h"
#include "light.h"
#include "entity.h"
#include "../core/shadow_map.h"
#include <vector>
#include <cstdint>

// Scene = pure data container.  Zero rendering logic.
//
// Owns the camera, lights, shadow map, and the entity list.
// The Renderer consumes a const Scene& to produce a frame.

struct Scene
{
    Camera camera;
    LightList lights;
    ShadowMap shadowMap;
    std::vector<Entity> entities;
    uint32_t clearColor = 0xFF1a1a2e;
};
