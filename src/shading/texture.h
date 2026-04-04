#pragma once
#include "../math/math_utils.h"
#include "../math/vec2.h"
#include "../math/vec3.h"
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>


#ifndef STBI_INCLUDE_LINE
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_THREAD_LOCALS
#endif
#include "../third_party/stb_image.h"

class Texture
{
  public:
    int width = 0, height = 0, channels = 0;
    std::vector<uint32_t> data;
    bool loaded = false;

    ~Texture() = default;

    void load(const std::string &path)
    {
        stbi_set_flip_vertically_on_load(true);
        unsigned char *img = stbi_load(path.c_str(), &width, &height, &channels, 4);

        if (!img)
        {
            throw std::runtime_error("Failed to load texture: " + path);
        }

        data.resize(width * height);
        for (int i = 0; i < width * height; ++i)
        {
            uint8_t r = img[i * 4 + 0];
            uint8_t g = img[i * 4 + 1];
            uint8_t b = img[i * 4 + 2];
            uint8_t a = img[i * 4 + 3];
            data[i] = (a << 24) | (r << 16) | (g << 8) | b;
        }

        stbi_image_free(img);
        loaded = true;
    }

    uint32_t sampleBilinear(float u, float v) const
    {
        if (!loaded || data.empty())
            return 0xFFFFFFFF;

        u = u - std::floor(u);
        v = v - std::floor(v);

        float x = u * (width - 1);
        float y = v * (height - 1);

        int x0 = (int)std::floor(x);
        int y0 = (int)std::floor(y);
        int x1 = std::min(x0 + 1, width - 1);
        int y1 = std::min(y0 + 1, height - 1);

        float dx = x - (float)x0;
        float dy = y - (float)y0;

        auto getPixel = [&](int px, int py)
        {
            uint32_t p = data[py * width + px];
            return Vec3{(float)((p >> 16) & 0xFF), (float)((p >> 8) & 0xFF), (float)(p & 0xFF)};
        };

        Vec3 p00 = getPixel(x0, y0);
        Vec3 p10 = getPixel(x1, y0);
        Vec3 p01 = getPixel(x0, y1);
        Vec3 p11 = getPixel(x1, y1);

        Vec3 color = p00 * (1.0f - dx) * (1.0f - dy) + p10 * dx * (1.0f - dy) + p01 * (1.0f - dx) * dy + p11 * dx * dy;

        return 0xFF000000 | ((uint8_t)color.x << 16) | ((uint8_t)color.y << 8) | (uint8_t)color.z;
    }

    Vec3 sampleSpherical(const Vec3 &direction) const
    {
        if (!loaded || data.empty())
            return {0.f, 0.f, 0.f};

        Vec3 d = direction.normalized();

        // Revised spherical mapping to prevent inversion
        // atan2(z, x) is the longitude, asin(y) is the latitude
        float u = 0.5f - (std::atan2(d.z, d.x) / (2.0f * MathUtils::PI));
        float v = 0.5f + (std::asin(d.y) / MathUtils::PI);

        uint32_t c = sampleBilinear(u, v);
        return {((c >> 16) & 0xFF) / 255.f, ((c >> 8) & 0xFF) / 255.f, (c & 0xFF) / 255.f};
    }
};