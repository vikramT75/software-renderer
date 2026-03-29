#pragma once
#include "../math/vec2.h"
#include "../math/math_utils.h"
#include <cstdint>
#include <string>
#include <stdexcept>
#include <fstream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iostream>

struct Texture
{
    int width = 0;
    int height = 0;
    bool loaded = false;

    void loadBMP(const std::string &path)
    {
        std::ifstream f(path, std::ios::binary);
        if (!f)
            throw std::runtime_error("Texture: cannot open " + path);

        auto read32s = [&]() -> int32_t
        {
            uint8_t b[4];
            f.read(reinterpret_cast<char *>(b), 4);
            return static_cast<int32_t>(b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24));
        };
        auto read32u = [&]() -> uint32_t
        {
            uint8_t b[4];
            f.read(reinterpret_cast<char *>(b), 4);
            return b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24);
        };
        auto read16u = [&]() -> uint16_t
        {
            uint8_t b[2];
            f.read(reinterpret_cast<char *>(b), 2);
            return b[0] | (b[1] << 8);
        };

        // File header
        uint8_t sig[2];
        f.read(reinterpret_cast<char *>(sig), 2);
        if (sig[0] != 'B' || sig[1] != 'M')
            throw std::runtime_error("Texture: not a BMP: " + path);

        read32u(); // file size
        read16u();
        read16u();                        // reserved
        uint32_t pixelOffset = read32u(); // offset to pixels

        // DIB header
        read32u(); // dib size
        int32_t w = read32s();
        int32_t h = read32s(); // negative = top-down
        read16u();             // planes
        uint16_t bpp = read16u();
        uint32_t compress = read32u();

        if (compress != 0)
            throw std::runtime_error("Texture: compressed BMP not supported");
        if (bpp != 24 && bpp != 32)
            throw std::runtime_error("Texture: only 24/32-bit BMP supported");

        bool topDown = (h < 0);
        width = std::abs(w);
        height = std::abs(h);

        int bytesPerPixel = bpp / 8;
        int rowSize = (width * bytesPerPixel + 3) & ~3;

        m_pixels.resize(width * height);
        f.seekg(pixelOffset, std::ios::beg);

        std::vector<uint8_t> row(rowSize);
        for (int fileRow = 0; fileRow < height; ++fileRow)
        {
            f.read(reinterpret_cast<char *>(row.data()), rowSize);
            // top-down BMP: fileRow 0 = image top (y=0)
            // bottom-up BMP: fileRow 0 = image bottom (y=height-1)
            int imgY = topDown ? fileRow : (height - 1 - fileRow);

            for (int x = 0; x < width; ++x)
            {
                uint8_t *px = row.data() + x * bytesPerPixel;
                uint8_t b = px[0], g = px[1], r = px[2];
                uint8_t a = (bpp == 32) ? px[3] : 255;
                m_pixels[imgY * width + x] =
                    (uint32_t(a) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
            }
        }

        // Debug
        std::cout << "BMP loaded: " << width << "x" << height
                  << " bpp=" << bpp
                  << (topDown ? " top-down" : " bottom-up") << "\n";
        for (int i = 0; i < 4; ++i)
        {
            uint32_t p = m_pixels[i];
            std::cout << "  Texel " << i << ": "
                      << "R=" << ((p >> 16) & 0xFF) << " "
                      << "G=" << ((p >> 8) & 0xFF) << " "
                      << "B=" << ((p) & 0xFF) << "\n";
        }

        loaded = true;
    }

    uint32_t sampleNearest(float u, float v) const
    {
        u = u - std::floor(u);
        v = v - std::floor(v);
        int x = MathUtils::clamp((int)(u * width), 0, width - 1);
        int y = MathUtils::clamp((int)(v * height), 0, height - 1);
        return m_pixels[y * width + x];
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

        uint32_t c00 = m_pixels[y0 * width + x0];
        uint32_t c10 = m_pixels[y0 * width + x1];
        uint32_t c01 = m_pixels[y1 * width + x0];
        uint32_t c11 = m_pixels[y1 * width + x1];

        auto bilerp = [&](int shift) -> uint8_t
        {
            auto u = [shift](uint32_t c)
            { return ((c >> shift) & 0xFF) / 255.f; };
            float top = u(c00) + (u(c10) - u(c00)) * tx;
            float bot = u(c01) + (u(c11) - u(c01)) * tx;
            return (uint8_t)((top + (bot - top) * ty) * 255.f);
        };

        return 0xFF000000 | (uint32_t(bilerp(16)) << 16) | (uint32_t(bilerp(8)) << 8) | uint32_t(bilerp(0));
    }

    Texture() = default;
    Texture(const Texture &) = delete;
    Texture &operator=(const Texture &) = delete;

private:
    std::vector<uint32_t> m_pixels;
};