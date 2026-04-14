#pragma once
#include "../math/vec3.h"
#include <algorithm>
#include <vector>

// HDR linear framebuffer — stores raw Vec3 radiance values.
// Tonemapping + packing to uint32_t happens only once in Renderer::applyBloom.
// Layout: row-major, top-left origin.
class Framebuffer
{
    int m_width, m_height;
    std::vector<Vec3> &m_buffer; // non-owning reference to Renderer's HDR buffer

  public:
    Framebuffer(int w, int h, std::vector<Vec3> &buffer) : m_width(w), m_height(h), m_buffer(buffer) {}

    void clear(const Vec3 &clearColor) { std::fill(m_buffer.begin(), m_buffer.end(), clearColor); }

    inline void setPixel(int x, int y, const Vec3 &color) { m_buffer[y * m_width + x] = color; }
    inline Vec3 getPixel(int x, int y) const { return m_buffer[y * m_width + x]; }
};
