#pragma once
#include "vertex.h"
#include <cstdint>

// A triangle ready for rasterization — three ScreenVertices + flat color.
// Color will eventually be replaced by a material/shader reference.
struct Triangle
{
    ScreenVertex v[3];
    uint32_t     color;   // ARGB8888, flat-shaded until lighting is implemented
};
