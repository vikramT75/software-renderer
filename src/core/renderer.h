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

    void beginFrame(const Scene &scene)
    {
        if (scene.environmentMap && scene.environmentMap->loaded)
        {
            Mat4 view = scene.camera.view();
            // Skybox should not move with camera translation
            view.m[0][3] = 0;
            view.m[1][3] = 0;
            view.m[2][3] = 0;

            Mat4 invVP = (scene.camera.projection() * view).inverse();

            // Calculate 4 corner rays once per frame
            auto getRay = [&](float x, float y)
            {
                Vec4 ray = invVP * Vec4(x, y, 1.0f, 1.0f);
                // Perspective division and normalization
                float wInv = 1.0f / ray.w;
                return Vec3(ray.x * wInv, ray.y * wInv, ray.z * wInv).normalized();
            };

            Vec3 rayTL = getRay(-1.f, 1.f);
            Vec3 rayTR = getRay(1.f, 1.f);
            Vec3 rayBL = getRay(-1.f, -1.f);
            Vec3 rayBR = getRay(1.f, -1.f);

            for (int y = 0; y < m_height; ++y)
            {
                float tV = (float)y / (float)(m_height - 1);
                Vec3 left = rayTL * (1.0f - tV) + rayBL * tV;
                Vec3 right = rayTR * (1.0f - tV) + rayBR * tV;

                for (int x = 0; x < m_width; ++x)
                {
                    float tH = (float)x / (float)(m_width - 1);
                    // Bilinear interpolation across the screen
                    Vec3 dir = left * (1.0f - tH) + right * tH;

                    Vec3 color = scene.environmentMap->sampleSpherical(dir);

                    // Simple tone mapping & Gamma correction
                    uint8_t r = (uint8_t)(std::pow(color.x / (color.x + 1.f), 1.f / 2.2f) * 255);
                    uint8_t g = (uint8_t)(std::pow(color.y / (color.y + 1.f), 1.f / 2.2f) * 255);
                    uint8_t b = (uint8_t)(std::pow(color.z / (color.z + 1.f), 1.f / 2.2f) * 255);
                    m_pixels[y * m_width + x] = 0xFF000000 | (r << 16) | (g << 8) | b;
                }
            }
        }
        else
        {
            m_fb.clear(scene.clearColor);
        }
        m_db.clear();
    }

    void setMVP(const Mat4 &model, const Mat4 &view, const Mat4 &projection)
    {
        m_model = model;
        m_mvp = projection * view * model;
        m_normalMat = model.normalMatrix();
    }

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

    void render(Scene &scene)
    {
        scene.updateHierarchy();
        Mat4 view = scene.camera.view();
        Mat4 proj = scene.camera.projection();

        bool shadowStarted = false;
        for (const auto &entityPtr : scene.entities)
        {
            Entity *entity = entityPtr.get();
            if (!entity->mesh || !entity->material || !entity->material->castsShadow)
                continue;
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

        beginFrame(scene);

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
            setMVP(entity->worldMatrix, view, proj);
            drawTriangles(entity->mesh->vertices, entity->mesh->indices, 0);
        }
    }

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
                    Vec4 wt = m_normalMat * Vec4(v.tangent, 0.f);
                    cv[j].tangent = {wt.x, wt.y, wt.z};
                    cv[j].uv = v.uv;
                }
            }
            submitClipped(cv, color);
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

    void submitClipped(const ClipVertex cv[3], uint32_t color)
    {
        std::vector<ClipVertex> poly = clipTriangle(cv);
        if (poly.size() < 3)
            return;
        for (size_t i = 1; i + 1 < poly.size(); ++i)
        {
            if (m_inShadowPass)
                rasterizeShadow(poly[0], poly[i], poly[i + 1], *m_shadowMap);
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
        ScreenVertex v0 = toScreen(a, sm.width, sm.height);
        ScreenVertex v1 = toScreen(b, sm.width, sm.height);
        ScreenVertex v2 = toScreen(c, sm.width, sm.height);
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