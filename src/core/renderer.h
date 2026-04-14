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
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// DEADLOCK-FREE Thread Pool
// ---------------------------------------------------------------------------
struct ThreadPool
{
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    std::condition_variable finishedCondition;
    int activeTasks = 0; // protected by queueMutex
    bool stop = false;

    ThreadPool(size_t threads)
    {
        for (size_t i = 0; i < threads; ++i)
        {
            workers.emplace_back(
                [this]
                {
                    for (;;)
                    {
                        std::function<void()> task;
                        {
                            std::unique_lock<std::mutex> lock(this->queueMutex);
                            this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                            if (this->stop && this->tasks.empty())
                                return;
                            task = std::move(this->tasks.front());
                            this->tasks.pop();
                        }
                        task();
                        {
                            std::unique_lock<std::mutex> lock(this->queueMutex);
                            --activeTasks;
                            if (activeTasks == 0)
                                finishedCondition.notify_all();
                        }
                    }
                });
        }
    }

    void enqueue(std::function<void()> task)
    {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            tasks.emplace(std::move(task));
            ++activeTasks;
        }
        condition.notify_one();
    }

    void wait()
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        finishedCondition.wait(lock, [this] { return activeTasks == 0; });
    }

    ~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread &w : workers)
            w.join();
    }
};

// ---------------------------------------------------------------------------

struct QueuedTriangle
{
    Triangle tri;
    const Shader *shader;
    CullMode cull;
    float depthSq; // For Painter's Algorithm sorting
};

class Renderer
{
  public:
    Renderer(int w, int h)
        : m_width(w), m_height(h),
          m_numThreads(std::max(1u, std::thread::hardware_concurrency())),
          m_pool(m_numThreads),
          m_hdrBuffer(w * h, {0.f, 0.f, 0.f}), // Primary HDR render target
          m_pixels(w * h, 0u),                  // Final LDR display buffer (SDL)
          m_fb(w, h, m_hdrBuffer),              // Framebuffer writes to HDR
          m_db(w, h)
    {
    }

    // -----------------------------------------------------------------------
    // beginFrame — fill the background into the HDR buffer
    // -----------------------------------------------------------------------
    void beginFrame(const Scene &scene)
    {
        if (scene.environmentMap && scene.environmentMap->loaded)
        {
            // Strip translation from view to get a pure rotation skybox matrix
            Mat4 view = scene.camera.view();
            view.m[0][3] = view.m[1][3] = view.m[2][3] = 0;
            Mat4 invVP = (scene.camera.projection() * view).inverse();

            auto getRay = [&](float x, float y)
            {
                Vec4 ray = invVP * Vec4(x, y, 1.0f, 1.0f);
                float wInv = 1.0f / ray.w;
                return Vec3(ray.x * wInv, ray.y * wInv, ray.z * wInv).normalized();
            };

            Vec3 rayTL = getRay(-1.f,  1.f), rayTR = getRay(1.f,  1.f);
            Vec3 rayBL = getRay(-1.f, -1.f), rayBR = getRay(1.f, -1.f);

            parallelFor(0, m_height,
                        [&](int yMin, int yMax)
                        {
                            for (int y = yMin; y < yMax; ++y)
                            {
                                float tV = (float)y / (float)(m_height - 1);
                                Vec3 left  = rayTL * (1.0f - tV) + rayBL * tV;
                                Vec3 right = rayTR * (1.0f - tV) + rayBR * tV;
                                for (int x = 0; x < m_width; ++x)
                                {
                                    float tH = (float)x / (float)(m_width - 1);
                                    Vec3 dir = left * (1.0f - tH) + right * tH;
                                    // Write raw HDR radiance — no tonemapping here
                                    m_hdrBuffer[y * m_width + x] =
                                        scene.environmentMap->sampleSpherical(dir);
                                }
                            }
                        });
        }
        else
        {
            // Unpack uint32_t clear colour → linear Vec3
            uint32_t cc = scene.clearColor;
            Vec3 clearVec = {((cc >> 16) & 0xFF) / 255.f,
                             ((cc >>  8) & 0xFF) / 255.f,
                             ( cc        & 0xFF) / 255.f};
            m_fb.clear(clearVec);
        }
        m_db.clear();
    }

    // -----------------------------------------------------------------------
    // applyBloom — extract HDR highlights, blur, composite, then tonemap+pack
    // This is the SINGLE point where HDR Vec3 → LDR uint32_t conversion occurs.
    // -----------------------------------------------------------------------
    void applyBloom()
    {
        std::vector<Vec3> bright(m_width * m_height, {0.f, 0.f, 0.f});
        std::vector<Vec3> temp  (m_width * m_height, {0.f, 0.f, 0.f});

        // 1. Extract bright pixels from the HDR buffer.
        //    Threshold > 1.0 ensures only true HDR highlights bloom (not just
        //    post-tonemap bright pixels like in the old LDR approach).
        parallelFor(0, m_width * m_height,
                    [&](int iMin, int iMax)
                    {
                        for (int i = iMin; i < iMax; ++i)
                        {
                            const Vec3 &c = m_hdrBuffer[i];
                            float lum = c.x * 0.2126f + c.y * 0.7152f + c.z * 0.0722f;
                            if (lum > 1.0f)
                                bright[i] = c * 0.25f;
                        }
                    });

        // 2. 4-pass box blur
        for (int pass = 0; pass < 4; ++pass)
        {
            temp = bright;
            parallelFor(1, m_height - 1,
                        [&](int yMin, int yMax)
                        {
                            for (int y = yMin; y < yMax; ++y)
                                for (int x = 1; x < m_width - 1; ++x)
                                {
                                    int i = y * m_width + x;
                                    bright[i] = (temp[i - 1] + temp[i + 1] +
                                                 temp[i - m_width] + temp[i + m_width]) * 0.25f;
                                }
                        });
        }

        // 3. Composite bloom onto HDR, then tonemap+pack to uint32_t
        parallelFor(0, m_width * m_height,
                    [&](int iMin, int iMax)
                    {
                        for (int i = iMin; i < iMax; ++i)
                        {
                            Vec3 combined = m_hdrBuffer[i] + bright[i];
                            // Tonemap + pack happens exactly once per pixel
                            m_pixels[i] = ShaderUtils::packColor(combined);
                        }
                    });
    }

    // -----------------------------------------------------------------------
    // render — shadow pass → bin triangles → tiled rasterization
    // -----------------------------------------------------------------------
    void render(Scene &scene)
    {
        scene.updateHierarchy();
        Mat4 v = scene.camera.view(), p = scene.camera.projection();

        // --- 1. Shadow Pass (serial — ShadowMap::testAndSet is not thread-safe) ---
        scene.shadowMap.clear();
        for (auto &e : scene.entities)
        {
            if (!e->mesh || !e->material || !e->material->castsShadow)
                continue;
            setShadowModel(e->worldMatrix, scene.shadowMap);

            for (size_t i = 0; i + 2 < e->mesh->indices.size(); i += 3)
            {
                ClipVertex cv[3];
                for (int j = 0; j < 3; ++j)
                {
                    const Vertex &vert = e->mesh->vertices[e->mesh->indices[i + j]];
                    cv[j].clip = m_shadowMVP * Vec4(vert.position, 1.f);
                    Vec4 wp    = m_model     * Vec4(vert.position, 1.f);
                    cv[j].worldPos = {wp.x, wp.y, wp.z};
                    // Normal/tangent/uv not needed for depth-only pass
                }
                ClipPolygon poly = clipTriangle(cv);
                for (int j = 1; j + 1 < poly.count; ++j)
                {
                    Triangle tri;
                    tri.v[0] = toScreen(poly.verts[0],     scene.shadowMap.width, scene.shadowMap.height);
                    tri.v[1] = toScreen(poly.verts[j],     scene.shadowMap.width, scene.shadowMap.height);
                    tri.v[2] = toScreen(poly.verts[j + 1], scene.shadowMap.width, scene.shadowMap.height);
                    // IsShadowPass=true → depth-only, no framebuffer, no shader
                    rasterizeTriangle<true>(tri, nullptr, nullptr, &scene.shadowMap,
                                           e->material->cullMode, nullptr,
                                           0, scene.shadowMap.width - 1,
                                           0, scene.shadowMap.height - 1);
                }
            }
        }

        // --- 2. Main Pass — build the triangle bin ---
        m_opaqueBin.clear();
        m_transparentBin.clear();

        for (auto &e : scene.entities)
        {
            if (!e->mesh || !e->material)
                continue;
            if (e->material->shader)
                e->material->shader->setFrameState(scene.camera.position, &scene.lights, &scene.shadowMap);
            setMainModel(e->worldMatrix, v, p);

            for (size_t i = 0; i + 2 < e->mesh->indices.size(); i += 3)
            {
                ClipVertex cv[3];
                for (int j = 0; j < 3; ++j)
                {
                    const Vertex &vert = e->mesh->vertices[e->mesh->indices[i + j]];
                    cv[j].clip = m_mvp * Vec4(vert.position, 1.f);
                    Vec4 wp    = m_model     * Vec4(vert.position, 1.f);
                    cv[j].worldPos = {wp.x, wp.y, wp.z};
                    Vec4 wn    = m_normalMat * Vec4(vert.normal,  0.f); cv[j].normal  = {wn.x, wn.y, wn.z};
                    Vec4 wt    = m_normalMat * Vec4(vert.tangent, 0.f); cv[j].tangent = {wt.x, wt.y, wt.z};
                    cv[j].uv = vert.uv;
                }

                ClipPolygon poly = clipTriangle(cv);
                for (int j = 1; j + 1 < poly.count; ++j)
                {
                    QueuedTriangle qt;
                    qt.tri.v[0] = toScreen(poly.verts[0],     m_width, m_height);
                    qt.tri.v[1] = toScreen(poly.verts[j],     m_width, m_height);
                    qt.tri.v[2] = toScreen(poly.verts[j + 1], m_width, m_height);
                    qt.shader = e->material->shader;
                    qt.cull   = e->material->cullMode;

                    if (e->material->isTransparent)
                    {
                        // Calculate centroid depth for Painter's Algorithm
                        Vec3 centroid = (poly.verts[0].worldPos + poly.verts[j].worldPos + poly.verts[j + 1].worldPos) * (1.0f / 3.0f);
                        qt.depthSq = (centroid - scene.camera.position).lengthSq();
                        m_transparentBin.push_back(qt);
                    }
                    else
                    {
                        qt.depthSq = 0.f; // Not needed for opaque
                        m_opaqueBin.push_back(qt);
                    }
                }
            }
        }

        // Fill background before rasterizing
        beginFrame(scene);

        // --- 3. Tiled Parallel Rasterization ---
        const int tileSize = 32;
        int tilesX = (m_width  + tileSize - 1) / tileSize;
        int tilesY = (m_height + tileSize - 1) / tileSize;

        // --- 3a. Rasterize Opaque Geometry ---
        for (int ty = 0; ty < tilesY; ++ty)
        {
            for (int tx = 0; tx < tilesX; ++tx)
            {
                m_pool.enqueue(
                    [this, tx, ty, tileSize]()
                    {
                        int xMin = tx * tileSize, xMax = std::min(xMin + tileSize, m_width)  - 1;
                        int yMin = ty * tileSize, yMax = std::min(yMin + tileSize, m_height) - 1;

                        for (const auto &qt : m_opaqueBin)
                        {
                            float triMinX = std::min({qt.tri.v[0].sx, qt.tri.v[1].sx, qt.tri.v[2].sx});
                            float triMaxX = std::max({qt.tri.v[0].sx, qt.tri.v[1].sx, qt.tri.v[2].sx});
                            float triMinY = std::min({qt.tri.v[0].sy, qt.tri.v[1].sy, qt.tri.v[2].sy});
                            float triMaxY = std::max({qt.tri.v[0].sy, qt.tri.v[1].sy, qt.tri.v[2].sy});

                            if (triMaxX < xMin || triMinX > xMax ||
                                triMaxY < yMin || triMinY > yMax)
                                continue;

                            // IsShadowPass=false, WriteDepth=true
                            rasterizeTriangle<false, true>(qt.tri, &m_fb, &m_db, nullptr,
                                                           qt.cull, qt.shader,
                                                           xMin, xMax, yMin, yMax);
                        }
                    });
            }
        }
        m_pool.wait(); // MUST finish opaque before drawing transparent!

        // --- 3b. Sort & Rasterize Transparent Geometry (Painter's Algorithm) ---
        if (!m_transparentBin.empty())
        {
            std::sort(m_transparentBin.begin(), m_transparentBin.end(), [](const QueuedTriangle &a, const QueuedTriangle &b)
                      {
                          return a.depthSq > b.depthSq; // Sort furthest to closest
                      });

            for (int ty = 0; ty < tilesY; ++ty)
            {
                for (int tx = 0; tx < tilesX; ++tx)
                {
                    m_pool.enqueue(
                        [this, tx, ty, tileSize]()
                        {
                            int xMin = tx * tileSize, xMax = std::min(xMin + tileSize, m_width)  - 1;
                            int yMin = ty * tileSize, yMax = std::min(yMin + tileSize, m_height) - 1;

                            for (const auto &qt : m_transparentBin)
                            {
                                float triMinX = std::min({qt.tri.v[0].sx, qt.tri.v[1].sx, qt.tri.v[2].sx});
                                float triMaxX = std::max({qt.tri.v[0].sx, qt.tri.v[1].sx, qt.tri.v[2].sx});
                                float triMinY = std::min({qt.tri.v[0].sy, qt.tri.v[1].sy, qt.tri.v[2].sy});
                                float triMaxY = std::max({qt.tri.v[0].sy, qt.tri.v[1].sy, qt.tri.v[2].sy});

                                if (triMaxX < xMin || triMinX > xMax ||
                                    triMaxY < yMin || triMinY > yMax)
                                    continue;

                                // IsShadowPass=false, WriteDepth=false
                                rasterizeTriangle<false, false>(qt.tri, &m_fb, &m_db, nullptr,
                                                                qt.cull, qt.shader,
                                                                xMin, xMax, yMin, yMax);
                            }
                        });
                }
            }
            m_pool.wait();
        }
    }

    const uint32_t *pixelData() const { return m_pixels.data(); }

  private:
    int m_width, m_height;
    unsigned int m_numThreads;
    ThreadPool m_pool;
    std::vector<Vec3>     m_hdrBuffer; // Linear HDR render target (Vec3 per pixel)
    std::vector<uint32_t> m_pixels;    // LDR display buffer uploaded to SDL each frame
    Framebuffer m_fb;
    Depthbuffer m_db;
    Mat4 m_mvp, m_model, m_normalMat, m_shadowMVP;
    std::vector<QueuedTriangle> m_opaqueBin;
    std::vector<QueuedTriangle> m_transparentBin;

    void parallelFor(int start, int end, std::function<void(int, int)> func)
    {
        int total = end - start;
        if (total <= 0) return;
        // Ceiling division so no thread gets skipped
        int chunk = (total + m_numThreads - 1) / m_numThreads;
        for (unsigned int i = 0; i < m_numThreads; ++i)
        {
            int s = start + i * chunk;
            if (s >= end) break;
            int e = std::min(s + chunk, end);
            m_pool.enqueue([func, s, e]() { func(s, e); });
        }
        m_pool.wait();
    }

    void setShadowModel(const Mat4 &m, ShadowMap &sm)
    {
        m_model     = m;
        m_shadowMVP = sm.lightSpaceMatrix() * m;
    }

    void setMainModel(const Mat4 &m, const Mat4 &vMat, const Mat4 &pMat)
    {
        m_model     = m;
        m_mvp       = pMat * vMat * m;
        m_normalMat = m.normalMatrix();
    }
};