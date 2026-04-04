#pragma once
#include "../core/shadow_map.h"
#include "../shading/texture.h"
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
    uint32_t clearColor = 0xFF111122;

    // Environment map for IBL and Skybox
    Texture *environmentMap = nullptr;

    std::vector<std::unique_ptr<Entity>> entities;

    Entity *createEntity(const std::string &name)
    {
        auto e = std::make_unique<Entity>();
        e->name = name;
        Entity *ptr = e.get();
        entities.push_back(std::move(e));
        return ptr;
    }

    void updateHierarchy()
    {
        Mat4 identity;
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                identity.m[i][j] = (i == j) ? 1.0f : 0.0f;
            }
        }
        for (auto &e : entities)
        {
            if (e->parent == nullptr)
            {
                e->updateWorldTransform(identity);
            }
        }
    }
};