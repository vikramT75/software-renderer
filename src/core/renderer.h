#pragma once
#include "../math/mat4.h"
#include "../pipeline/clipping.h"
#include "../pipeline/projection.h"
#include "../pipeline/triangle.h"
#include "../pipeline/vertex.h"
#include "../rasterizer/rasterizer.h"
#include "../scene/scene.h"
#include "../shading/shader.h"
#include "depthbuffer.h"
#include "framebuffer.h"
#include "shadow_map.h"
#include <algorithm>
#include <cstdint>
#include <vector>

class Renderer
{
  public:
    CullMode cullMode = CullMode::Back;
    const Shader *activeShader = nullptr;

    Renderer(int w, int h) : m_width(w), m_height(h), m_pixels(w * h, 0u), m_fb(w, h, m_pixels.data()), m_db(w, h)
    {
    }

    void beginFrame(const Scene &scene)
    {
        if (scene.environmentMap && scene.environmentMap->loaded)
        {
            Mat4 view = scene.camera.view();
            view.m[0][3] = view.m[1][3] = view.m[2][3] = 0;
            Mat4 invVP = (scene.camera.projection() * view).inverse();

            auto getRay = [&](float x, float y)
            {
                Vec4 ray = invVP * Vec4(x, y, 1.0f, 1.0f);
                float wInv = 1.0f / ray.w;
                return Vec3(ray.x * wInv, ray.y * wInv, ray.z * wInv).normalized();
            };

            Vec3 rayTL = getRay(-1.f, 1.f), rayTR = getRay(1.f, 1.f), rayBL = getRay(-1.f, -1.f),
                 rayBR = getRay(1.f, -1.f);

            for (int y = 0; y < m_height; ++y)
            {
                float tV = (float)y / (float)(m_height - 1);
                Vec3 left = rayTL * (1.0f - tV) + rayBL * tV, right = rayTR * (1.0f - tV) + rayBR * tV;
                for (int x = 0; x < m_width; ++x)
                {
                    float tH = (float)x / (float)(m_width - 1);
                    Vec3 dir = left * (1.0f - tH) + right * tH;
                    Vec3 color = scene.environmentMap->sampleSpherical(dir);
                    m_pixels[y * m_width + x] = ShaderUtils::packColor(color);
                }
            }
        }
        else
            m_fb.clear(scene.clearColor);
        m_db.clear();
    }

    void applyBloom()
    {
        std::vector<Vec3> bright(m_width * m_height, {0.f, 0.f, 0.f});
        for (int i = 0; i < m_width * m_height; ++i)
        {
            uint32_t p = m_pixels[i];
            Vec3 c = {((p >> 16) & 0xFF) / 255.f, ((p >> 8) & 0xFF) / 255.f, (p & 0xFF) / 255.f};
            float lum = c.x * 0.2126f + c.y * 0.7152f + c.z * 0.0722f;
            if (lum > 0.92f)
                bright[i] = c * 0.25f;
        }

        // Symmetric Ping-Pong Blur
        for (int p = 0; p < 4; ++p)
        {
            std::vector<Vec3> temp = bright;
            for (int y = 1; y < m_height - 1; ++y)
            {
                for (int x = 1; x < m_width - 1; ++x)
                {
                    int i = y * m_width + x;
                    bright[i] = (temp[i - 1] + temp[i + 1] + temp[i - m_width] + temp[i + m_width]) * 0.25f;
                }
            }
        }

        for (int i = 0; i < m_width * m_height; ++i)
        {
            uint32_t p = m_pixels[i];
            Vec3 orig = {((p >> 16) & 0xFF) / 255.f, ((p >> 8) & 0xFF) / 255.f, (p & 0xFF) / 255.f};
            Vec3 combined = orig + bright[i];
            uint8_t ir = (uint8_t)(std::min(combined.x, 1.0f) * 255), ig = (uint8_t)(std::min(combined.y, 1.0f) * 255),
                    ib = (uint8_t)(std::min(combined.z, 1.0f) * 255);
            m_pixels[i] = 0xFF000000 | (ir << 16) | (ig << 8) | ib;
        }
    }

    void render(Scene &scene)
    {
        scene.updateHierarchy();
        Mat4 v = scene.camera.view(), p = scene.camera.projection();
        bool shadowPass = false;
        for (auto &e : scene.entities)
        {
            if (!e->mesh || !e->material || !e->material->castsShadow)
                continue;
            setShadowModel(e->worldMatrix, scene.shadowMap);
            if (!shadowPass)
            {
                scene.shadowMap.clear();
                m_shadowMap = &scene.shadowMap;
                m_inShadowPass = true;
                shadowPass = true;
            }
            drawTriangles(e->mesh->vertices, e->mesh->indices, 0);
        }
        m_inShadowPass = false;
        m_shadowMap = nullptr;
        beginFrame(scene);
        for (auto &e : scene.entities)
        {
            if (!e->mesh || !e->material)
                continue;
            if (e->material->shader)
                e->material->shader->setFrameState(scene.camera.position, &scene.lights, &scene.shadowMap);
            cullMode = e->material->cullMode;
            activeShader = e->material->shader;
            setMainModel(e->worldMatrix, v, p);
            drawTriangles(e->mesh->vertices, e->mesh->indices, 0);
        }
    }

    const uint32_t *pixelData() const
    {
        return m_pixels.data();
    }

  private:
    int m_width, m_height;
    std::vector<uint32_t> m_pixels;
    Framebuffer m_fb;
    Depthbuffer m_db;
    Mat4 m_mvp, m_model, m_normalMat, m_shadowMVP;
    ShadowMap *m_shadowMap = nullptr;
    bool m_inShadowPass = false;

    void setShadowModel(const Mat4 &m, ShadowMap &sm)
    {
        m_model = m;
        m_shadowMVP = sm.lightSpaceMatrix() * m;
    }
    void setMainModel(const Mat4 &m, const Mat4 &v, const Mat4 &p)
    {
        m_model = m;
        m_mvp = p * v * m;
        m_normalMat = m.normalMatrix();
    }

    void drawTriangles(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices, uint32_t color)
    {
        for (size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            ClipVertex cv[3];
            for (int j = 0; j < 3; ++j)
            {
                const Vertex &v = vertices[indices[i + j]];
                cv[j].clip = (m_inShadowPass ? m_shadowMVP : m_mvp) * Vec4(v.position, 1.f);
                Vec4 wp = m_model * Vec4(v.position, 1.f);
                cv[j].worldPos = {wp.x, wp.y, wp.z};

                // Tangent Bug Fix: Correctly handle transformation
                if (!m_inShadowPass)
                {
                    Vec4 wn = m_normalMat * Vec4(v.normal, 0.f);
                    cv[j].normal = {wn.x, wn.y, wn.z};
                    Vec4 wt = m_normalMat * Vec4(v.tangent, 0.f);
                    cv[j].tangent = {wt.x, wt.y, wt.z};
                }
                else
                {
                    cv[j].normal = v.normal;
                    cv[j].tangent = v.tangent;
                }
                cv[j].uv = v.uv;
            }
            submitClipped(cv, color);
        }
    }

    void submitClipped(const ClipVertex cv[3], uint32_t color)
    {
        ClipPolygon poly = clipTriangle(cv);
        if (poly.count < 3)
            return;
        for (int i = 1; i + 1 < poly.count; ++i)
        {
            if (m_inShadowPass)
                rasterizeShadow(poly.verts[0], poly.verts[i], poly.verts[i + 1], *m_shadowMap);
            else
            {
                Triangle tri;
                tri.v[0] = toScreen(poly.verts[0], m_width, m_height);
                tri.v[1] = toScreen(poly.verts[i], m_width, m_height);
                tri.v[2] = toScreen(poly.verts[i + 1], m_width, m_height);
                tri.color = color;
                rasterizeTriangle(tri, m_fb, m_db, cullMode, activeShader);
            }
        }
    }

    void rasterizeShadow(const ClipVertex &a, const ClipVertex &b, const ClipVertex &c, ShadowMap &sm)
    {
        ScreenVertex v0 = toScreen(a, sm.width, sm.height), v1 = toScreen(b, sm.width, sm.height),
                     v2 = toScreen(c, sm.width, sm.height);
        float area2 = edgeFunction(v0.sx, v0.sy, v1.sx, v1.sy, v2.sx, v2.sy);
        if (area2 == 0.f)
            return;
        int minX = std::max(0, (int)std::floor(std::min({v0.sx, v1.sx, v2.sx}))),
            maxX = std::min(sm.width - 1, (int)std::ceil(std::max({v0.sx, v1.sx, v2.sx})));
        int minY = std::max(0, (int)std::floor(std::min({v0.sy, v1.sy, v2.sy}))),
            maxY = std::min(sm.height - 1, (int)std::ceil(std::max({v0.sy, v1.sy, v2.sy})));
        for (int y = minY; y <= maxY; ++y)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                auto bar = Barycentric::compute(v0.sx, v0.sy, v1.sx, v1.sy, v2.sx, v2.sy, x + 0.5f, y + 0.5f, area2);
                if (bar.inside)
                    sm.testAndSet(x, y, bar.w0 * v0.z + bar.w1 * v1.z + bar.w2 * v2.z);
            }
        }
    }
};