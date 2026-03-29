#pragma once
#include "vec4.h"
#include <cmath>
#include <cstring>

struct Mat4
{
    float m[4][4];

    Mat4() { std::memset(m, 0, sizeof(m)); }

    static Mat4 identity()
    {
        Mat4 r;
        r.m[0][0] = r.m[1][1] = r.m[2][2] = r.m[3][3] = 1.f;
        return r;
    }

    static Mat4 translation(float tx, float ty, float tz)
    {
        Mat4 r = identity();
        r.m[0][3] = tx;
        r.m[1][3] = ty;
        r.m[2][3] = tz;
        return r;
    }

    static Mat4 scale(float sx, float sy, float sz)
    {
        Mat4 r;
        r.m[0][0] = sx;
        r.m[1][1] = sy;
        r.m[2][2] = sz;
        r.m[3][3] = 1.f;
        return r;
    }

    static Mat4 rotationX(float a)
    {
        Mat4 r = identity();
        r.m[1][1] = std::cos(a);
        r.m[1][2] = -std::sin(a);
        r.m[2][1] = std::sin(a);
        r.m[2][2] = std::cos(a);
        return r;
    }

    static Mat4 rotationY(float a)
    {
        Mat4 r = identity();
        r.m[0][0] = std::cos(a);
        r.m[0][2] = std::sin(a);
        r.m[2][0] = -std::sin(a);
        r.m[2][2] = std::cos(a);
        return r;
    }

    static Mat4 rotationZ(float a)
    {
        Mat4 r = identity();
        r.m[0][0] = std::cos(a);
        r.m[0][1] = -std::sin(a);
        r.m[1][0] = std::sin(a);
        r.m[1][1] = std::cos(a);
        return r;
    }

    static Mat4 perspective(float fovY, float aspect, float zNear, float zFar)
    {
        Mat4 p;
        float f = 1.f / std::tan(fovY * 0.5f);
        p.m[0][0] = f / aspect;
        p.m[1][1] = f;
        p.m[2][2] = (zFar + zNear) / (zNear - zFar);
        p.m[2][3] = (2.f * zFar * zNear) / (zNear - zFar);
        p.m[3][2] = -1.f;
        return p;
    }

    static Mat4 lookAt(const Vec3 &eye, const Vec3 &center, const Vec3 &up)
    {
        Vec3 f = (center - eye).normalized();
        Vec3 r = f.cross(up).normalized();
        Vec3 u = r.cross(f);
        Mat4 m = identity();
        m.m[0][0] = r.x;
        m.m[0][1] = r.y;
        m.m[0][2] = r.z;
        m.m[0][3] = -r.dot(eye);
        m.m[1][0] = u.x;
        m.m[1][1] = u.y;
        m.m[1][2] = u.z;
        m.m[1][3] = -u.dot(eye);
        m.m[2][0] = -f.x;
        m.m[2][1] = -f.y;
        m.m[2][2] = -f.z;
        m.m[2][3] = f.dot(eye);
        return m;
    }

    static Mat4 orthographic(float left, float right,
                             float bottom, float top,
                             float zNear, float zFar)
    {
        Mat4 r;
        r.m[0][0] = 2.f / (right - left);
        r.m[1][1] = 2.f / (top - bottom);
        r.m[2][2] = -2.f / (zFar - zNear);
        r.m[0][3] = -(right + left) / (right - left);
        r.m[1][3] = -(top + bottom) / (top - bottom);
        r.m[2][3] = -(zFar + zNear) / (zFar - zNear);
        r.m[3][3] = 1.f;
        return r;
    }

    Mat4 operator*(const Mat4 &b) const
    {
        Mat4 r;
        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 4; ++col)
                for (int k = 0; k < 4; ++k)
                    r.m[row][col] += m[row][k] * b.m[k][col];
        return r;
    }

    Vec4 operator*(const Vec4 &v) const
    {
        return {
            m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z + m[0][3] * v.w,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z + m[1][3] * v.w,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z + m[2][3] * v.w,
            m[3][0] * v.x + m[3][1] * v.y + m[3][2] * v.z + m[3][3] * v.w};
    }

    Mat4 transposed() const
    {
        Mat4 r;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                r.m[i][j] = m[j][i];
        return r;
    }

    // Analytic 4x4 inverse via cofactor expansion.
    // Valid for any invertible matrix. Returns identity if not invertible.
    Mat4 inverse() const
    {
        const float *s = &m[0][0];
        float inv[16];

        inv[0] = s[5] * s[10] * s[15] - s[5] * s[11] * s[14] - s[9] * s[6] * s[15] + s[9] * s[7] * s[14] + s[13] * s[6] * s[11] - s[13] * s[7] * s[10];
        inv[4] = -s[4] * s[10] * s[15] + s[4] * s[11] * s[14] + s[8] * s[6] * s[15] - s[8] * s[7] * s[14] - s[12] * s[6] * s[11] + s[12] * s[7] * s[10];
        inv[8] = s[4] * s[9] * s[15] - s[4] * s[11] * s[13] - s[8] * s[5] * s[15] + s[8] * s[7] * s[13] + s[12] * s[5] * s[11] - s[12] * s[7] * s[9];
        inv[12] = -s[4] * s[9] * s[14] + s[4] * s[10] * s[13] + s[8] * s[5] * s[14] - s[8] * s[6] * s[13] - s[12] * s[5] * s[10] + s[12] * s[6] * s[9];
        inv[1] = -s[1] * s[10] * s[15] + s[1] * s[11] * s[14] + s[9] * s[2] * s[15] - s[9] * s[3] * s[14] - s[13] * s[2] * s[11] + s[13] * s[3] * s[10];
        inv[5] = s[0] * s[10] * s[15] - s[0] * s[11] * s[14] - s[8] * s[2] * s[15] + s[8] * s[3] * s[14] + s[12] * s[2] * s[11] - s[12] * s[3] * s[10];
        inv[9] = -s[0] * s[9] * s[15] + s[0] * s[11] * s[13] + s[8] * s[1] * s[15] - s[8] * s[3] * s[13] - s[12] * s[1] * s[11] + s[12] * s[3] * s[9];
        inv[13] = s[0] * s[9] * s[14] - s[0] * s[10] * s[13] - s[8] * s[1] * s[14] + s[8] * s[2] * s[13] + s[12] * s[1] * s[10] - s[12] * s[2] * s[9];
        inv[2] = s[1] * s[6] * s[15] - s[1] * s[7] * s[14] - s[5] * s[2] * s[15] + s[5] * s[3] * s[14] + s[13] * s[2] * s[7] - s[13] * s[3] * s[6];
        inv[6] = -s[0] * s[6] * s[15] + s[0] * s[7] * s[14] + s[4] * s[2] * s[15] - s[4] * s[3] * s[14] - s[12] * s[2] * s[7] + s[12] * s[3] * s[6];
        inv[10] = s[0] * s[5] * s[15] - s[0] * s[7] * s[13] - s[4] * s[1] * s[15] + s[4] * s[3] * s[13] + s[12] * s[1] * s[7] - s[12] * s[3] * s[5];
        inv[14] = -s[0] * s[5] * s[14] + s[0] * s[6] * s[13] + s[4] * s[1] * s[14] - s[4] * s[2] * s[13] - s[12] * s[1] * s[6] + s[12] * s[2] * s[5];
        inv[3] = -s[1] * s[6] * s[11] + s[1] * s[7] * s[10] + s[5] * s[2] * s[11] - s[5] * s[3] * s[10] - s[9] * s[2] * s[7] + s[9] * s[3] * s[6];
        inv[7] = s[0] * s[6] * s[11] - s[0] * s[7] * s[10] - s[4] * s[2] * s[11] + s[4] * s[3] * s[10] + s[8] * s[2] * s[7] - s[8] * s[3] * s[6];
        inv[11] = -s[0] * s[5] * s[11] + s[0] * s[7] * s[9] + s[4] * s[1] * s[11] - s[4] * s[3] * s[9] - s[8] * s[1] * s[7] + s[8] * s[3] * s[5];
        inv[15] = s[0] * s[5] * s[10] - s[0] * s[6] * s[9] - s[4] * s[1] * s[10] + s[4] * s[2] * s[9] + s[8] * s[1] * s[6] - s[8] * s[2] * s[5];

        float det = s[0] * inv[0] + s[1] * inv[4] + s[2] * inv[8] + s[3] * inv[12];
        if (std::abs(det) < 1e-8f)
            return identity();

        float invDet = 1.f / det;
        Mat4 result;
        for (int i = 0; i < 16; ++i)
            (&result.m[0][0])[i] = inv[i] * invDet;
        return result;
    }

    // Normal matrix: transpose of inverse of the model matrix.
    // Use this to transform normals — keeps them perpendicular to surfaces
    // under non-uniform scaling.
    Mat4 normalMatrix() const
    {
        return inverse().transposed();
    }
};