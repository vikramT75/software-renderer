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

    Renderer(int width, int height)
        : m_width(width), m_height(height), m_pixels(width * height, 0u), m_fb(width, height, m_pixels.data()),
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

    // -----------------------------------------------------------------------
    // Shadow pass
    // -----------------------------------------------------------------------
    void beginShadowPass(ShadowMap &sm)
    {
        sm.clear();
        m_shadowMap = &sm;
        m_inShadowPass = true;
    }

    void endShadowPass()
    {
        m_inShadowPass = false;
        m_shadowMap = nullptr;
    }

    void setModel(const Mat4 &model)
    {
        m_model = model;
        m_normalMat = model.normalMatrix();
        if (m_inShadowPass && m_shadowMap)
            m_shadowMVP = m_shadowMap->lightSpaceMatrix() * model;
    }

    // -----------------------------------------------------------------------
    // Scene-based rendering
    // -----------------------------------------------------------------------
    void render(Scene &scene)
    {
        // 1. Update all world matrices based on the entity hierarchy
        scene.updateHierarchy();

        Mat4 view = scene.camera.view();
        Mat4 proj = scene.camera.projection();

        // ── Shadow pass ─────────────────────────────────────────────────
        bool shadowStarted = false;
        for (const auto &entityPtr : scene.entities)
        {
            Entity *entity = entityPtr.get();
            if (!entity->mesh || !entity->material || !entity->material->castsShadow)
                continue;

            // Use the cached world matrix calculated by the Scene Graph!
            setModel(entity->worldMatrix);

            if (!shadowStarted)
            {
                beginShadowPass(scene.shadowMap);
                shadowStarted = true;
            }

            drawTriangles(entity->mesh->vertices, entity->mesh->indices, 0);
        }
        if (shadowStarted)
            endShadowPass();

        // ── Main pass ───────────────────────────────────────────────────
        beginFrame(scene.clearColor);

        for (auto &entityPtr : scene.entities)
        {
            Entity *entity = entityPtr.get();
            if (!entity->mesh || !entity->material)
                continue;

            Shader *shader = entity->material->shader;

            if (shader)
                shader->setFrameState(scene.camera.position, &scene.lights, &scene.shadowMap);

            cullMode = entity->material->cullMode;
            activeShader = shader;

            // Use the cached world matrix calculated by the Scene Graph!
            setMVP(entity->worldMatrix, view, proj);
            drawTriangles(entity->mesh->vertices, entity->mesh->indices, 0);
        }

        endFrame();
    }

    // -----------------------------------------------------------------------
    // Main draw calls
    // -----------------------------------------------------------------------
    void drawTriangles(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices, uint32_t color)
    {
        for (size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            ClipVertex cv[3];
            for (int j = 0; j < 3; ++j)
            {
                const Vertex &v = vertices[indices[i + j]];
                if (m_inShadowPass)
                {
                    cv[j].clip = m_shadowMVP * Vec4(v.position, 1.f);

                    // FIXED: Apply world matrix during shadow pass for correct depth calculation
                    Vec4 wp = m_model * Vec4(v.position, 1.f);
                    cv[j].worldPos = {wp.x, wp.y, wp.z};

                    cv[j].normal = v.normal;
                    cv[j].uv = v.uv;
                    cv[j].tangent = v.tangent;
                }
                else
                {
                    cv[j].clip = m_mvp * Vec4(v.position, 1.f);
                    Vec4 wp = m_model * Vec4(v.position, 1.f);
                    cv[j].worldPos = {wp.x, wp.y, wp.z};

                    Vec4 wn = m_normalMat * Vec4(v.normal, 0.f);
                    cv[j].normal = {wn.x, wn.y, wn.z};

                    // Transform Tangent into World Space for Normal Mapping
                    Vec4 wt = m_normalMat * Vec4(v.tangent, 0.f);
                    cv[j].tangent = {wt.x, wt.y, wt.z};

                    cv[j].uv = v.uv;
                }
            }
            submitClipped(cv, color);
        }
    }

    void drawRawTriangle(const Vertex raw[3], uint32_t color)
    {
        ClipVertex cv[3];
        for (int i = 0; i < 3; ++i)
        {
            if (m_inShadowPass)
            {
                cv[i].clip = m_shadowMVP * Vec4(raw[i].position, 1.f);
                Vec4 wp = m_model * Vec4(raw[i].position, 1.f);
                cv[i].worldPos = {wp.x, wp.y, wp.z};
                cv[i].normal = raw[i].normal;
                cv[i].uv = raw[i].uv;
                cv[i].tangent = raw[i].tangent;
            }
            else
            {
                cv[i].clip = m_mvp * Vec4(raw[i].position, 1.f);
                Vec4 wp = m_model * Vec4(raw[i].position, 1.f);
                cv[i].worldPos = {wp.x, wp.y, wp.z};

                Vec4 wn = m_normalMat * Vec4(raw[i].normal, 0.f);
                cv[i].normal = {wn.x, wn.y, wn.z};

                Vec4 wt = m_normalMat * Vec4(raw[i].tangent, 0.f);
                cv[i].tangent = {wt.x, wt.y, wt.z};

                cv[i].uv = raw[i].uv;
            }
        }
        submitClipped(cv, color);
    }

    void endFrame()
    {
    }

    const uint32_t *pixelData() const
    {
        return m_pixels.data();
    }
    int width() const
    {
        return m_width;
    }
    int height() const
    {
        return m_height;
    }

  private:
    int m_width, m_height;
    std::vector<uint32_t> m_pixels;
    Framebuffer m_fb;
    Depthbuffer m_db;
    Mat4 m_mvp;
    Mat4 m_model;
    Mat4 m_normalMat;
    Mat4 m_shadowMVP;
    ShadowMap *m_shadowMap = nullptr;
    bool m_inShadowPass = false;

    void submitClipped(const ClipVertex cv[3], uint32_t color)
    {
        std::vector<ClipVertex> poly = clipTriangle(cv);
        if (poly.size() < 3)
            return;

        for (size_t i = 1; i + 1 < poly.size(); ++i)
        {
            if (m_inShadowPass)
            {
                rasterizeShadow(poly[0], poly[i], poly[i + 1], *m_shadowMap);
            }
            else
            {
                Triangle tri;
                tri.v[0] = toScreen(poly[0], m_width, m_height);
                tri.v[1] = toScreen(poly[i], m_width, m_height);
                tri.v[2] = toScreen(poly[i + 1], m_width, m_height);
                tri.color = color;
                rasterizeTriangle(tri, m_fb, m_db, cullMode, activeShader);
            }
        }
    }

    void rasterizeShadow(const ClipVertex &a, const ClipVertex &b, const ClipVertex &c, ShadowMap &sm)
    {
        auto toShadowScreen = [&](const ClipVertex &cv) -> ScreenVertex { return toScreen(cv, sm.width, sm.height); };

        ScreenVertex v0 = toShadowScreen(a);
        ScreenVertex v1 = toShadowScreen(b);
        ScreenVertex v2 = toShadowScreen(c);

        int minX = std::max(0, (int)std::floor(std::min({v0.sx, v1.sx, v2.sx})));
        int maxX = std::min(sm.width - 1, (int)std::ceil(std::max({v0.sx, v1.sx, v2.sx})));
        int minY = std::max(0, (int)std::floor(std::min({v0.sy, v1.sy, v2.sy})));
        int maxY = std::min(sm.height - 1, (int)std::ceil(std::max({v0.sy, v1.sy, v2.sy})));

        float area2 = edgeFunction(v0.sx, v0.sy, v1.sx, v1.sy, v2.sx, v2.sy);
        if (area2 == 0.f)
            return;

        for (int y = minY; y <= maxY; ++y)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                auto bar = Barycentric::compute(v0.sx, v0.sy, v1.sx, v1.sy, v2.sx, v2.sy, x + 0.5f, y + 0.5f, area2);

                if (!bar.inside)
                    continue;

                float z = bar.w0 * v0.z + bar.w1 * v1.z + bar.w2 * v2.z;
                sm.testAndSet(x, y, z);
            }
        }
    }
};