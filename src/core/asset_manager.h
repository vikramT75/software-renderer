#pragma once
#include "../loaders/obj_loader.h"
#include "../shading/texture.h"
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

class AssetManager
{
  public:
    // Retrieves a mesh. Loads it if it hasn't been loaded yet.
    Mesh *getMesh(const std::string &path)
    {
        auto it = m_meshes.find(path);
        if (it != m_meshes.end())
        {
            return it->second.get(); // Return cached mesh
        }

        try
        {
            auto mesh = std::make_unique<Mesh>(loadOBJ(path));
            Mesh *ptr = mesh.get();
            m_meshes[path] = std::move(mesh);
            std::cout << "Loaded Mesh: " << path << " (Verts: " << ptr->vertices.size() << ")\n";
            return ptr;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to load mesh " << path << ": " << e.what() << "\n";
            return nullptr;
        }
    }

    // Retrieves a texture. Loads it if it hasn't been loaded yet.
    Texture *getTexture(const std::string &path)
    {
        auto it = m_textures.find(path);
        if (it != m_textures.end())
        {
            return it->second.get(); // Return cached texture
        }

        // Not found, load it
        try
        {
            auto tex = std::make_unique<Texture>();
            tex->load(path);
            Texture *ptr = tex.get();
            m_textures[path] = std::move(tex);
            std::cout << "Loaded Texture: " << path << "\n";
            return ptr;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to load texture " << path << ": " << e.what() << "\n";
            return nullptr;
        }
    }

    void clear()
    {
        m_meshes.clear();
        m_textures.clear();
    }

  private:
    std::unordered_map<std::string, std::unique_ptr<Mesh>> m_meshes;
    std::unordered_map<std::string, std::unique_ptr<Texture>> m_textures;
};