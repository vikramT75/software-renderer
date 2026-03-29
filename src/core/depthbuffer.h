#pragma once
#include <vector>
#include <algorithm>
#include <limits>

// Stores per-pixel depth (NDC z, range [-1, +1]).
// Lower z = closer to the camera (right-handed, OpenGL convention).
struct Depthbuffer
{
    int width;
    int height;
    std::vector<float> depth;

    Depthbuffer(int w, int h) : width(w), height(h), depth(w * h, 1.f) {}

    void clear()
    {
        std::fill(depth.begin(), depth.end(), 1.f);
    }

    // Returns true and writes depth if z passes the test (closer than stored).
    inline bool test(int x, int y, float z)
    {
        int idx = y * width + x;
        if (z < depth[idx])
        {
            depth[idx] = z;
            return true;
        }
        return false;
    }

    inline float get(int x, int y) const
    {
        return depth[y * width + x];
    }
};
