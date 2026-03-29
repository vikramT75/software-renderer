#pragma once
#include "../math/vec2.h"
#include "../math/math_utils.h"
#include <cstdint>
#include <string>
#include <stdexcept>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb_image.h"

struct Texture
{
    int width = 0;
    int height = 0;
    bool loaded = false;

    // Loads BMP, PNG, JPG, TGA, GIF — anything stb_image supports.
    void load(const std::string &path)
    {
        if (m_pixels)
            stbi_image_free(m_pixels);

        int channels;
        // Force 4 channels (RGBA) regardless of source format
        m_pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);

        if (!m_pixels)
            throw std::runtime_error("Texture: cannot load " + path + " — " + stbi_failure_reason());

        std::cout << "Texture loaded: " << path << " "
                  << width << "x" << height
                  << " channels=" << channels << "\n";

        // Debug: first 4 pixels
        for (int i = 0; i < 4 && i < width * height; ++i)
        {
            uint8_t *p = m_pixels + i * 4;
            std::cout << "  Texel " << i << ": "
                      << "R=" << (int)p[0] << " "
                      << "G=" << (int)p[1] << " "
                      << "B=" << (int)p[2] << " "
                      << "A=" << (int)p[3] << "\n";
        }

        loaded = true;
    }

    // Keep loadBMP as an alias so existing code doesn't break
    void loadBMP(const std::string &path) { load(path); }

    ~Texture()
    {
        if (m_pixels)
            stbi_image_free(m_pixels);
    }

    uint32_t sampleNearest(float u, float v) const
    {
        u = u - std::floor(u);
        v = v - std::floor(v);
        int x = MathUtils::clamp((int)(u * width), 0, width - 1);
        int y = MathUtils::clamp((int)(v * height), 0, height - 1);
        return texelAt(x, y);
    }

    uint32_t sampleBilinear(float u, float v) const
    {
        u = u - std::floor(u);
        v = v - std::floor(v);

        float fx = u * (width - 1);
        float fy = v * (height - 1);
        int x0 = (int)fx, y0 = (int)fy;
        int x1 = std::min(x0 + 1, width - 1);
        int y1 = std::min(y0 + 1, height - 1);
        float tx = fx - x0, ty = fy - y0;

        uint32_t c00 = texelAt(x0, y0);
        uint32_t c10 = texelAt(x1, y0);
        uint32_t c01 = texelAt(x0, y1);
        uint32_t c11 = texelAt(x1, y1);

        auto bilerp = [&](int shift) -> uint8_t
        {
            auto unpack = [shift](uint32_t c)
            { return ((c >> shift) & 0xFF) / 255.f; };
            float top = unpack(c00) + (unpack(c10) - unpack(c00)) * tx;
            float bot = unpack(c01) + (unpack(c11) - unpack(c01)) * tx;
            return (uint8_t)((top + (bot - top) * ty) * 255.f);
        };

        return 0xFF000000 | (uint32_t(bilerp(16)) << 16) | (uint32_t(bilerp(8)) << 8) | uint32_t(bilerp(0));
    }

    Texture() = default;
    Texture(const Texture &) = delete;
    Texture &operator=(const Texture &) = delete;

private:
    uint8_t *m_pixels = nullptr; // RGBA8, owned by stb_image

    // Read a texel and pack to ARGB8888
    inline uint32_t texelAt(int x, int y) const
    {
        uint8_t *p = m_pixels + (y * width + x) * 4;
        return (0xFF000000) | (uint32_t(p[0]) << 16) // R
               | (uint32_t(p[1]) << 8)               // G
               | (uint32_t(p[2]));                   // B
        // p[3] is alpha — ignored for now, wire in when transparency is needed
    }
};