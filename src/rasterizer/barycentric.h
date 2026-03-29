#pragma once

// Edge function: positive when P is to the left of edge AB
// (standard counter-clockwise winding convention).
// Returns twice the signed area of triangle ABP.
inline float edgeFunction(float ax, float ay,
                           float bx, float by,
                           float px, float py)
{
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

struct Barycentric
{
    float w0, w1, w2;  // normalised barycentric coords  (sum = 1)
    bool  inside;

    // Compute barycentric coords of point (px,py) inside triangle v0,v1,v2.
    // area2 = edgeFunction(v0,v1,v2) — pass it in to avoid recomputation.
    static Barycentric compute(float v0x, float v0y,
                               float v1x, float v1y,
                               float v2x, float v2y,
                               float px,  float py,
                               float area2)
    {
        Barycentric b;
        float e0 = edgeFunction(v1x, v1y, v2x, v2y, px, py);
        float e1 = edgeFunction(v2x, v2y, v0x, v0y, px, py);
        float e2 = edgeFunction(v0x, v0y, v1x, v1y, px, py);

        // Allow both windings (area2 can be negative for CW triangles)
        bool posTest = (e0 >= 0.f && e1 >= 0.f && e2 >= 0.f);
        bool negTest = (e0 <= 0.f && e1 <= 0.f && e2 <= 0.f);
        b.inside = posTest || negTest;

        b.w0 = e0 / area2;
        b.w1 = e1 / area2;
        b.w2 = e2 / area2;
        return b;
    }
};
