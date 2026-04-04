#pragma once
#include "../math/mat4.h"
#include "../math/vec3.h"
#include <algorithm>
#include <cmath>
#include <vector>


// Owns a depth buffer rendered from the light's point of view.
// Used by PhongShader to determine whether a fragment is in shadow.
//
// Coordinate convention:
//   lightView      = lookAt(lightPos, scene_center, up)
//   lightProj      = orthographic(...) for directional lights
//   lightSpaceMVP  = lightProj * lightView
//
// Shadow map depth is in NDC z ∈ [-1,+1].
// At sample time we remap to [0,1] for storage clarity.

struct ShadowMap {
  int width = 1024;
  int height = 1024;
  float bias = 0.010f; // prevents shadow acne

  Mat4 lightView;
  Mat4 lightProj;

  // Call once to set up the light matrices.
  // dir      : world-space direction the light shines (towards scene)
  // center   : world-space point the light looks at
  // extent   : half-size of the ortho frustum (tune to scene size)
  // zNear/Far: depth range
  void setup(const Vec3 &dir, const Vec3 &center, float extent = 5.f,
             float zNear = 0.1f, float zFar = 50.f) {
    // Place light far back along its direction
    Vec3 lightPos = center - dir * (zFar * 0.5f);

    // Choose an up vector not parallel to dir
    Vec3 up = {0.f, 1.f, 0.f};
    Vec3 d = dir;
    float dot = std::abs(d.x * up.x + d.y * up.y + d.z * up.z);
    if (dot > 0.99f)
      up = {1.f, 0.f, 0.f};

    lightView = Mat4::lookAt(lightPos, center, up);
    lightProj =
        Mat4::orthographic(-extent, extent, -extent, extent, zNear, zFar);

    m_depth.assign(width * height, 1.f);
  }

  // Clear depth to far plane before shadow pass
  void clear() { std::fill(m_depth.begin(), m_depth.end(), 1.f); }

  // Write depth — called during shadow pass rasterization
  // Returns true if depth passes (closer than stored)
  bool testAndSet(int x, int y, float z) {
    if (x < 0 || y < 0 || x >= width || y >= height)
      return false;
    int idx = y * width + x;
    if (z < m_depth[idx]) {
      m_depth[idx] = z;
      return true;
    }
    return false;
  }

  // Sample shadow map at (u,v) ∈ [0,1].
  // Returns stored depth at that texel.
  float sample(float u, float v) const {
    int x = static_cast<int>(u * width);
    int y = static_cast<int>(v * height);
    x = std::max(0, std::min(width - 1, x));
    y = std::max(0, std::min(height - 1, y));
    return m_depth[y * width + x];
  }

  // Returns true if world-space point is in shadow.
  // lightSpaceMVP = lightProj * lightView * model  (pass identity for world
  // pos)
  float shadowFactor(const Vec3 &worldPos) const {
    Vec4 lc = (lightProj * lightView) * Vec4(worldPos, 1.f);
    if (lc.w <= 0.f)
      return 1.0f;

    float ndcX = lc.x / lc.w;
    float ndcY = lc.y / lc.w;
    float ndcZ = lc.z / lc.w;

    // Outside light frustum — completely lit
    if (ndcX < -1.f || ndcX > 1.f || ndcY < -1.f || ndcY > 1.f || ndcZ < -1.f ||
        ndcZ > 1.f)
      return 1.0f;

    float u = ndcX * 0.5f + 0.5f;
    float v = 1.f - (ndcY * 0.5f + 0.5f); // flip Y
    float fragDepth = ndcZ;

    float shadow = 0.0f;
    float texelSizeX = 1.0f / width;
    float texelSizeY = 1.0f / height;

    // 3x3 PCF Grid
    for (int x = -1; x <= 1; ++x) {
      for (int y = -1; y <= 1; ++y) {
        float pcfDepth = sample(u + x * texelSizeX, v + y * texelSizeY);
        // If the fragment is CLOSER than the shadow map, it is lit (1.0).
        // If it is further, it is in shadow (0.0).
        shadow += (fragDepth > pcfDepth + bias) ? 0.0f : 1.0f;
      }
    }

    // Average the 9 samples
    return shadow / 9.0f;
  }

  Mat4 lightSpaceMatrix() const { return lightProj * lightView; }

  const std::vector<float> &depthData() const { return m_depth; }

private:
  std::vector<float> m_depth;
};