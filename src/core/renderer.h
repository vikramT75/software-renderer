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

// DEADLOCK FREE Thread Pool
struct ThreadPool
{
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    std::condition_variable finishedCondition;
    int activeTasks = 0; // No longer needs to be atomic, protected by mutex
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

                        // BUG FIX: Lock mutex before decrementing and notifying
                        {
                            std::unique_lock<std::mutex> lock(this->queueMutex);
                            activeTasks--;
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
            activeTasks++;
        }
        condition.notify_one();
    }

    void wait()
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        finishedCondition.wait(lock, [this]() { return activeTasks == 0; });
    }

    ~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread &worker : workers)
            worker.join();
    }
};

struct QueuedTriangle
{
    Triangle tri;
    const Shader *shader;
    CullMode cull;
};

class Renderer
{
  public:
    CullMode cullMode = CullMode::Back;
    const Shader *activeShader = nullptr;

    Renderer(int w, int h)
        : m_width(w), m_height(h), m_numThreads(std::max(1u, std::thread::hardware_concurrency())),
          m_pool(m_numThreads), m_pixels(w * h, 0u), m_fb(w, h, m_pixels.data()), m_db(w, h)
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

            parallelFor(0, m_height,
                        [&](int yMin, int yMax)
                        {
                            for (int y = yMin; y < yMax; ++y)
                            {
                                float tV = (float)y / (float)(m_height - 1);
                                Vec3 left = rayTL * (1.0f - tV) + rayBL * tV, right = rayTR * (1.0f - tV) + rayBR * tV;
                                for (int x = 0; x < m_width; ++x)
                                {
                                    float tH = (float)x / (float)(m_width - 1);
                                    Vec3 dir = left * (1.0f - tH) + right * tH;
                                    m_pixels[y * m_width + x] =
                                        ShaderUtils::packColor(scene.environmentMap->sampleSpherical(dir));
                                }
                            }
                        });
        }
        else
            m_fb.clear(scene.clearColor);
        m_db.clear();
    }

    void applyBloom()
    {
        std::vector<Vec3> bright(m_width * m_height, {0.f, 0.f, 0.f});
        std::vector<Vec3> temp(m_width * m_height, {0.f, 0.f, 0.f});

        parallelFor(0, m_width * m_height,
                    [&](int iMin, int iMax)
                    {
                        for (int i = iMin; i < iMax; ++i)
                        {
                            uint32_t p = m_pixels[i];
                            Vec3 c = {((p >> 16) & 0xFF) / 255.f, ((p >> 8) & 0xFF) / 255.f, (p & 0xFF) / 255.f};
                            if ((c.x * 0.2126f + c.y * 0.7152f + c.z * 0.0722f) > 0.92f)
                                bright[i] = c * 0.25f;
                        }
                    });

        for (int p = 0; p < 4; ++p)
        {
            temp = bright;
            parallelFor(1, m_height - 1,
                        [&](int yMin, int yMax)
                        {
                            for (int y = yMin; y < yMax; ++y)
                            {
                                for (int x = 1; x < m_width - 1; ++x)
                                {
                                    int i = y * m_width + x;
                                    bright[i] =
                                        (temp[i - 1] + temp[i + 1] + temp[i - m_width] + temp[i + m_width]) * 0.25f;
                                }
                            }
                        });
        }

        parallelFor(0, m_width * m_height,
                    [&](int iMin, int iMax)
                    {
                        for (int i = iMin; i < iMax; ++i)
                        {
                            uint32_t p = m_pixels[i];
                            Vec3 orig = {((p >> 16) & 0xFF) / 255.f, ((p >> 8) & 0xFF) / 255.f, (p & 0xFF) / 255.f};
                            Vec3 combined = orig + bright[i];
                            uint8_t ir = (uint8_t)(std::min(combined.x, 1.0f) * 255),
                                    ig = (uint8_t)(std::min(combined.y, 1.0f) * 255),
                                    ib = (uint8_t)(std::min(combined.z, 1.0f) * 255);
                            m_pixels[i] = 0xFF000000 | (ir << 16) | (ig << 8) | ib;
                        }
                    });
    }

    void render(Scene &scene)
    {
        scene.updateHierarchy();
        Mat4 v = scene.camera.view(), p = scene.camera.projection();

        bool shadowPassFound = false;
        for (auto &e : scene.entities)
        {
            if (!e->mesh || !e->material || !e->material->castsShadow)
                continue;
            setShadowModel(e->worldMatrix, scene.shadowMap);
            if (!shadowPassFound)
            {
                scene.shadowMap.clear();
                m_shadowMap = &scene.shadowMap;
                m_inShadowPass = true;
                shadowPassFound = true;
            }
            drawTriangles(e->mesh->vertices, e->mesh->indices, 0);
        }
        m_inShadowPass = false;
        m_shadowMap = nullptr;

        m_triangleBin.clear();
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
                    Vec4 wp = m_model * Vec4(vert.position, 1.f);
                    cv[j].worldPos = {wp.x, wp.y, wp.z};
                    Vec4 wn = m_normalMat * Vec4(vert.normal, 0.f);
                    cv[j].normal = {wn.x, wn.y, wn.z};
                    Vec4 wt = m_normalMat * Vec4(vert.tangent, 0.f);
                    cv[j].tangent = {wt.x, wt.y, wt.z};
                    cv[j].uv = vert.uv;
                }

                ClipPolygon poly = clipTriangle(cv);
                for (int j = 1; j + 1 < poly.count; ++j)
                {
                    QueuedTriangle qt;
                    qt.tri.v[0] = toScreen(poly.verts[0], m_width, m_height);
                    qt.tri.v[1] = toScreen(poly.verts[j], m_width, m_height);
                    qt.tri.v[2] = toScreen(poly.verts[j + 1], m_width, m_height);
                    qt.shader = e->material->shader;
                    qt.cull = e->material->cullMode;
                    m_triangleBin.push_back(qt);
                }
            }
        }

        beginFrame(scene);

        const int tileSize = 32;
        int tilesX = (m_width + tileSize - 1) / tileSize;
        int tilesY = (m_height + tileSize - 1) / tileSize;

        for (int ty = 0; ty < tilesY; ++ty)
        {
            for (int tx = 0; tx < tilesX; ++tx)
            {
                m_pool.enqueue(
                    [this, tx, ty, tileSize]()
                    {
                        int xMin = tx * tileSize, xMax = std::min(xMin + tileSize, m_width) - 1;
                        int yMin = ty * tileSize, yMax = std::min(yMin + tileSize, m_height) - 1;
                        for (const auto &qt : m_triangleBin)
                        {
                            float triMinX = std::min({qt.tri.v[0].sx, qt.tri.v[1].sx, qt.tri.v[2].sx});
                            float triMaxX = std::max({qt.tri.v[0].sx, qt.tri.v[1].sx, qt.tri.v[2].sx});
                            float triMinY = std::min({qt.tri.v[0].sy, qt.tri.v[1].sy, qt.tri.v[2].sy});
                            float triMaxY = std::max({qt.tri.v[0].sy, qt.tri.v[1].sy, qt.tri.v[2].sy});
                            if (triMaxX < xMin || triMinX > xMax || triMaxY < yMin || triMinY > yMax)
                                continue;
                            rasterizeTriangle(qt.tri, m_fb, m_db, qt.cull, qt.shader, xMin, xMax, yMin, yMax);
                        }
                    });
            }
        }
        m_pool.wait();
    }

    const uint32_t *pixelData() const
    {
        return m_pixels.data();
    }

  private:
    int m_width, m_height;
    unsigned int m_numThreads;
    ThreadPool m_pool;
    std::vector<uint32_t> m_pixels;
    Framebuffer m_fb;
    Depthbuffer m_db;
    Mat4 m_mvp, m_model, m_normalMat, m_shadowMVP;
    ShadowMap *m_shadowMap = nullptr;
    bool m_inShadowPass = false;
    std::vector<QueuedTriangle> m_triangleBin;

    void parallelFor(int start, int end, std::function<void(int, int)> func)
    {
        int total = end - start;
        if (total <= 0)
            return;

        // The Agent's Fix: Ceiling division to prevent overloading the last thread
        int chunk = (total + m_numThreads - 1) / m_numThreads;

        for (unsigned int i = 0; i < m_numThreads; ++i)
        {
            int s = start + i * chunk;
            if (s >= end)
                break; // If we rounded up enough to finish early, stop spawning tasks

            // Clamp the end index so the last thread stops exactly where it should
            int e = std::min(s + chunk, end);

            m_pool.enqueue([func, s, e]() { func(s, e); });
        }
        m_pool.wait();
    }

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

    void submitClipped(const ClipVertex cv[3], uint32_t /*color*/)
    {
        ClipPolygon poly = clipTriangle(cv);
        if (poly.count < 3)
            return;
        for (int i = 1; i + 1 < poly.count; ++i)
        {
            if (m_inShadowPass)
                rasterizeShadow(poly.verts[0], poly.verts[i], poly.verts[i + 1], *m_shadowMap);
        }
    }

    void rasterizeShadow(const ClipVertex &a, const ClipVertex &b, const ClipVertex &c, ShadowMap &sm)
    {
        ScreenVertex v0 = toScreen(a, sm.width, sm.height);
        ScreenVertex v1 = toScreen(b, sm.width, sm.height);
        ScreenVertex v2 = toScreen(c, sm.width, sm.height);

        auto edgeFunc = [](const ScreenVertex &va, const ScreenVertex &vb, float cx, float cy)
        { return (vb.sx - va.sx) * (cy - va.sy) - (vb.sy - va.sy) * (cx - va.sx); };

        float originalArea = edgeFunc(v0, v1, v2.sx, v2.sy);
        if (std::abs(originalArea) < 0.00001f)
            return;

        float area2 = originalArea;
        if (area2 < 0.f)
        {
            std::swap(v1, v2);
            area2 = -area2;
        }

        int minX = std::max(0, (int)std::floor(std::min({v0.sx, v1.sx, v2.sx})));
        int maxX = std::min(sm.width - 1, (int)std::ceil(std::max({v0.sx, v1.sx, v2.sx})));
        int minY = std::max(0, (int)std::floor(std::min({v0.sy, v1.sy, v2.sy})));
        int maxY = std::min(sm.height - 1, (int)std::ceil(std::max({v0.sy, v1.sy, v2.sy})));

        float stepX0 = v1.sy - v2.sy, stepY0 = v2.sx - v1.sx;
        float stepX1 = v2.sy - v0.sy, stepY1 = v0.sx - v2.sx;
        float stepX2 = v0.sy - v1.sy, stepY2 = v1.sx - v0.sx;

        float px = (float)minX + 0.5f, py = (float)minY + 0.5f;

        float w0_row = edgeFunc(v1, v2, px, py);
        float w1_row = edgeFunc(v2, v0, px, py);
        float w2_row = edgeFunc(v0, v1, px, py);

        float invArea = 1.0f / area2;

        for (int y = minY; y <= maxY; ++y)
        {
            float w0 = w0_row, w1 = w1_row, w2 = w2_row;
            for (int x = minX; x <= maxX; ++x)
            {
                if (w0 >= 0 && w1 >= 0 && w2 >= 0)
                {
                    float b0 = w0 * invArea, b1 = w1 * invArea, b2 = w2 * invArea;
                    float z = b0 * v0.z + b1 * v1.z + b2 * v2.z;
                    sm.testAndSet(x, y, z);
                }
                w0 += stepX0;
                w1 += stepX1;
                w2 += stepX2;
            }
            w0_row += stepY0;
            w1_row += stepY1;
            w2_row += stepY2;
        }
    }
};