#pragma once
#include "../math/mat4.h"
#include "../math/math_utils.h"

struct Transform
{
    Vec3  position  = {0.f, 0.f, 0.f};
    Vec3  rotation  = {0.f, 0.f, 0.f};   // Euler XYZ in radians
    Vec3  scaleVec  = {1.f, 1.f, 1.f};

    // Returns model matrix: T * Ry * Rx * Rz * S  (applied right-to-left)
    Mat4 matrix() const
    {
        Mat4 S = Mat4::scale(scaleVec.x, scaleVec.y, scaleVec.z);
        Mat4 Rx = Mat4::rotationX(rotation.x);
        Mat4 Ry = Mat4::rotationY(rotation.y);
        Mat4 Rz = Mat4::rotationZ(rotation.z);
        Mat4 T  = Mat4::translation(position.x, position.y, position.z);
        return T * Ry * Rx * Rz * S;
    }
};
