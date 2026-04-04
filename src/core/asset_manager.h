#pragma once
#include "../loaders/obj_loader.h"
#include "../math/math_utils.h"
#include "../shading/texture.h"
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>


class AssetManager
{
  public:
    Mesh *getMesh(const std::string &path)
    {
        auto it = m_meshes.find(path);
        if (it != m_meshes.end())
            return it->second.get();

        try
        {
            auto mesh = std::make_unique<Mesh>(loadOBJ(path));
            Mesh *ptr = mesh.get();
            m_meshes[path] = std::move(mesh);
            std::cout << "Loaded Mesh: " << path << "\n";
            return ptr;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Mesh load error: " << e.what() << "\n";
            return nullptr;
        }
    }

    Texture *getTexture(const std::string &path)
    {
        auto it = m_textures.find(path);
        if (it != m_textures.end())
            return it->second.get();

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
            std::cerr << "Texture load error: " << e.what() << "\n";
            return nullptr;
        }
    }

    // Generates a low-res blurred Irradiance map from a skybox
    Texture *getIrradianceMap(const std::string &skyPath)
    {
        std::string key = skyPath + "_irradiance";
        if (m_textures.count(key))
            return m_textures[key].get();

        Texture *sky = getTexture(skyPath);
        if (!sky)
            return nullptr;

        auto irr = std::make_unique<Texture>();
        irr->width = 32;
        irr->height = 16;
        irr->data.resize(irr->width * irr->height);

        std::cout << "Generating Irradiance Map (be patient)..." << std::endl;

        for (int y = 0; y < irr->height; ++y)
        {
            for (int x = 0; x < irr->width; ++x)
            {
                float u = (float)x / irr->width;
                float v = (float)y / irr->height;

                float phi = u * 2.0f * MathUtils::PI;
                float theta = (v - 0.5f) * MathUtils::PI;

                // Normal direction for this pixel
                Vec3 N = {std::cos(theta) * std::cos(phi), std::sin(theta), std::cos(theta) * std::sin(phi)};

                // Sample a small area to simulate blurring
                Vec3 sum = {0.f, 0.f, 0.f};
                int samples = 0;
                for (float i = -0.1f; i <= 0.1f; i += 0.05f)
                {
                    for (float j = -0.1f; j <= 0.1f; j += 0.05f)
                    {
                        sum += sky->sampleSpherical(N + Vec3{i, j, i});
                        samples++;
                    }
                }
                Vec3 avg = sum * (1.0f / (float)samples);

                uint8_t r = (uint8_t)(MathUtils::clamp(avg.x, 0.f, 1.f) * 255);
                uint8_t g = (uint8_t)(MathUtils::clamp(avg.y, 0.f, 1.f) * 255);
                uint8_t b = (uint8_t)(MathUtils::clamp(avg.z, 0.f, 1.f) * 255);
                irr->data[y * irr->width + x] = 0xFF000000 | (r << 16) | (g << 8) | b;
            }
        }

        irr->loaded = true;
        Texture *ptr = irr.get();
        m_textures[key] = std::move(irr);
        return ptr;
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