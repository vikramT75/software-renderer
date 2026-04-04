#pragma once
#include "vertex.h"
#include <cstdint>

struct Triangle
{
    ScreenVertex v[3];
    uint32_t color;
};