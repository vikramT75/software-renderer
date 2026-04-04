#pragma once
#include "../math/vec2.h"
#include "../math/vec3.h"
#include "../math/vec4.h"

struct Vertex {
  Vec3 position;
  Vec3 normal;
  Vec2 uv;
  Vec3 tangent;
};

struct ClipVertex {
  Vec4 clip;
  Vec3 worldPos; // world-space position — for lighting
  Vec3 normal;   // world-space normal
  Vec2 uv;
  Vec3 tangent;

  Vec3 ndc() const { return clip.perspectiveDivide(); }
};

struct ScreenVertex {
  float sx, sy;
  float z;
  float invW;
  Vec3 worldPos; // premultiplied by invW
  Vec3 normal;   // premultiplied by invW
  Vec2 uv;       // premultiplied by invW
  Vec3 tangent;  // premultiplied by invW
};