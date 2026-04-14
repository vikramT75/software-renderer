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

    // Generates a physically correct irradiance map via hemispherical Monte Carlo integration.
    // For each texel (representing a normal direction N), integrates L(ωi)·cos(θi)·dω
    // over the upper hemisphere, which is the diffuse irradiance term in the rendering equation.
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
        irr->isHDR = true; // Store as full-precision float — no 8-bit clamping
        irr->hdrData.resize(irr->width * irr->height);

        std::cout << "Generating Irradiance Map..." << std::endl;

        for (int y = 0; y < irr->height; ++y)
        {
            for (int x = 0; x < irr->width; ++x)
            {
                float u = (float)x / irr->width;
                float v = (float)y / irr->height;

                float phi_N = u * 2.0f * MathUtils::PI;
                float theta_N = (v - 0.5f) * MathUtils::PI;

                // World-space normal for this texel
                Vec3 N = {std::cos(theta_N) * std::cos(phi_N), std::sin(theta_N), std::cos(theta_N) * std::sin(phi_N)};

                // Store full HDR irradiance — no clamping
                irr->hdrData[y * irr->width + x] = calculateIrradiance(sky, N);
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

    // Integrates L(ωi)·cos(θ)·sin(θ)·dφ·dθ over the hemisphere whose pole is N.
    // sin(θ) is the spherical coordinate Jacobian; cos(θ) is the Lambertian weight.
    // The result is normalised by π / numSamples (Monte Carlo estimator).
    Vec3 calculateIrradiance(const Texture *sky, const Vec3 &N)
    {
        // Build an orthonormal TBN frame around N
        Vec3 up = std::abs(N.y) < 0.999f ? Vec3{0.f, 1.f, 0.f} : Vec3{0.f, 0.f, 1.f};
        Vec3 right = up.cross(N).normalized();
        Vec3 newUp = N.cross(right).normalized();

        Vec3 irradiance = {0.f, 0.f, 0.f};
        int samples = 0;
        float delta = 0.05f; // ~2.9° step — gives 126 azimuth × 32 elevation = ~4 000 samples/texel

        for (float phi = 0.0f; phi < 2.0f * MathUtils::PI; phi += delta)
        {
            for (float theta = 0.0f; theta < 0.5f * MathUtils::PI; theta += delta)
            {
                // Spherical → tangent space Cartesian
                Vec3 tangentSample = {std::sin(theta) * std::cos(phi), std::sin(theta) * std::sin(phi),
                                      std::cos(theta)};

                // Tangent space → world space
                Vec3 sampleVec = right * tangentSample.x + newUp * tangentSample.y + N * tangentSample.z;

                // cos(θ) = Lambertian weight; sin(θ) = spherical Jacobian
                irradiance += sky->sampleSpherical(sampleVec) * std::cos(theta) * std::sin(theta);
                ++samples;
            }
        }

        // π comes from integrating the hemisphere weighting analytically
        return irradiance * (MathUtils::PI / float(samples));
    }
};