#pragma once
#include "../core/shadow_map.h"
#include "camera.h"
#include "entity.h"
#include "light.h"
#include <memory>
#include <string>
#include <vector>

struct Scene
{
    Camera camera;
    LightList lights;
    ShadowMap shadowMap;
    uint32_t clearColor = 0xFF111122; // Dark blue/gray background

    // Ownership of all entities in the scene
    std::vector<std::unique_ptr<Entity>> entities;

    // Helper to create and track a new entity
    Entity *createEntity(const std::string &name)
    {
        auto e = std::make_unique<Entity>();
        e->name = name;
        Entity *ptr = e.get();
        entities.push_back(std::move(e));
        return ptr;
    }

    // Call this once per frame before rendering to calculate all world matrices
    void updateHierarchy()
    {
        // 1. Explicitly create an Identity Matrix (Fix for the invisible scene bug)
        Mat4 identity;
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                identity.m[i][j] = (i == j) ? 1.0f : 0.0f;
            }
        }

        // 2. Propagate transforms starting from root entities (entities with no parent)
        for (auto &e : entities)
        {
            if (e->parent == nullptr)
            {
                e->updateWorldTransform(identity);
            }
        }
    }
};