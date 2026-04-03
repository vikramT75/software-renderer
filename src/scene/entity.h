#pragma once
#include "transform.h"
#include "material.h"
#include "../loaders/obj_loader.h" // Mesh
#include <string>

// Entity = the binding unit that connects geometry, appearance, and placement.
//
// Multiple entities can share the same Mesh* and Material* — e.g. 100 trees
// all pointing to the same mesh data and material, saving RAM.
//
// Ownership: Entity does NOT own the Mesh or Material.  The caller owns them
// and entities hold non-owning pointers.

struct Entity
{
    std::string name;
    Transform transform;
    const Mesh *mesh = nullptr;      // NOT owned — shared across entities
    Material *material = nullptr;    // NOT owned — can be shared too
};
