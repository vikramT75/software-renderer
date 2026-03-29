#pragma once
#include "framebuffer.h"
#include "depthbuffer.h"
#include "../pipeline/vertex.h"
#include "../pipeline/triangle.h"
#include "../pipeline/projection.h"
#include "../pipeline/clipping.h"
#include "../rasterizer/rasterizer.h"
#include "../shading/shader.h"
#include "../math/mat4.h"
#include <vector>
#include <cstdint>

class Renderer
{
public:
    CullMode cullMode = CullMode::Back;
    const Shader *activeShader = nullptr;

    Renderer(int width, int height)
        : m_width(width), m_height(height),
          m_pixels(width * height, 0u),
          m_fb(width, height, m_pixels.data()),
          m_db(width, height)
    {
    }

    void beginFrame(uint32_t clearColor = 0xFF000000)
    {
        m_fb.clear(clearColor);
        m_db.clear();
    }

    void setMVP(const Mat4 &model, const Mat4 &view, const Mat4 &projection)
    {
        m_model = model;
        m_mvp = projection * view * model;
        m_normalMat = model.normalMatrix();
    }

    void drawTriangles(const std::vector<Vertex> &vertices,
                       const std::vector<uint32_t> &indices,
                       uint32_t color)
    {
        for (size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            ClipVertex cv[3];
            for (int j = 0; j < 3; ++j)
            {
                const Vertex &v = vertices[indices[i + j]];
                cv[j].clip = m_mvp * Vec4(v.position, 1.f);
                Vec4 wp = m_model * Vec4(v.position, 1.f);
                cv[j].worldPos = {wp.x, wp.y, wp.z};
                Vec4 wn = m_normalMat * Vec4(v.normal, 0.f);
                cv[j].normal = Vec3{wn.x, wn.y, wn.z};
                cv[j].uv = v.uv;
            }
            submitClipped(cv, color);
        }
    }

    void drawRawTriangle(const Vertex raw[3], uint32_t color)
    {
        ClipVertex cv[3];
        for (int i = 0; i < 3; ++i)
        {
            cv[i].clip = m_mvp * Vec4(raw[i].position, 1.f);
            Vec4 wp = m_model * Vec4(raw[i].position, 1.f);
            cv[i].worldPos = {wp.x, wp.y, wp.z};
            Vec4 wn = m_normalMat * Vec4(raw[i].normal, 0.f);
            cv[i].normal = Vec3{wn.x, wn.y, wn.z};
            cv[i].uv = raw[i].uv;
        }
        submitClipped(cv, color);
    }

    void endFrame() {}

    const uint32_t *pixelData() const { return m_pixels.data(); }
    int width() const { return m_width; }
    int height() const { return m_height; }

private:
    int m_width, m_height;
    std::vector<uint32_t> m_pixels;
    Framebuffer m_fb;
    Depthbuffer m_db;
    Mat4 m_mvp;
    Mat4 m_model;
    Mat4 m_normalMat;

    void submitClipped(const ClipVertex cv[3], uint32_t color)
    {
        std::vector<ClipVertex> poly = clipTriangle(cv);
        if (poly.size() < 3)
            return;

        for (size_t i = 1; i + 1 < poly.size(); ++i)
        {
            Triangle tri;
            tri.v[0] = toScreen(poly[0], m_width, m_height);
            tri.v[1] = toScreen(poly[i], m_width, m_height);
            tri.v[2] = toScreen(poly[i + 1], m_width, m_height);
            tri.color = color;
            rasterizeTriangle(tri, m_fb, m_db, cullMode, activeShader);
        }
    }
};