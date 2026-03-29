#pragma once
#include <cstdint>
#include <algorithm>

// Owns the pixel buffer written each frame.
// Layout: row-major, top-left origin.  Format: ARGB8888 (matches SDL).
struct Framebuffer
{
    int      width;
    int      height;
    uint32_t* pixels;   // owned externally — passed in from Renderer

    Framebuffer(int w, int h, uint32_t* data)
        : width(w), height(h), pixels(data) {}

    void clear(uint32_t color)
    {
        std::fill(pixels, pixels + width * height, color);
    }

    inline void setPixel(int x, int y, uint32_t color)
    {
        if (x < 0 || y < 0 || x >= width || y >= height) return;
        pixels[y * width + x] = color;
    }

    inline uint32_t getPixel(int x, int y) const
    {
        return pixels[y * width + x];
    }
};
