#pragma once
#include "../loaders/obj_loader.h"
#include "../math/mat4.h"
#include "material.h"
#include "transform.h"
#include <memory>
#include <string>
#include <vector>

struct Entity
{
    std::string name;
    Transform transform;
    Mesh *mesh = nullptr;
    Material *material = nullptr;

    // Hierarchy pointers
    Entity *parent = nullptr;
    std::vector<Entity *> children;

    // Cached world-space transform
    Mat4 worldMatrix;

    // Recursively updates this entity and all its children
    void updateWorldTransform(const Mat4 &parentWorldMatrix)
    {
        worldMatrix = parentWorldMatrix * transform.matrix();
        for (auto *child : children)
        {
            child->updateWorldTransform(worldMatrix);
        }
    }

    // Helper to attach a child
    void addChild(Entity *child)
    {
        if (child)
        {
            child->parent = this;
            children.push_back(child);
        }
    }
};