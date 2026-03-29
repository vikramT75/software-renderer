#pragma once
#include "../math/mat4.h"
#include "../math/math_utils.h"
#include <cmath>

struct Camera
{
    Vec3 position = {0.f, 0.f, 3.f};
    Vec3 target = {0.f, 0.f, 0.f};
    Vec3 up = {0.f, 1.f, 0.f};
    float fovY = MathUtils::toRadians(60.f);
    float aspect = 4.f / 3.f;
    float zNear = 0.1f;
    float zFar = 1000.f;

    // Euler angles (radians) — maintained internally for free-look
    float yaw = -MathUtils::HALF_PI; // -90° faces down -Z initially
    float pitch = 0.f;

    // Sensitivity / speed
    float moveSpeed = 5.f;    // units per second
    float lookSpeed = 0.002f; // radians per pixel

    Mat4 view() const { return Mat4::lookAt(position, position + forward(), up); }
    Mat4 projection() const { return Mat4::perspective(fovY, aspect, zNear, zFar); }

    // Forward vector derived from yaw/pitch
    Vec3 forward() const
    {
        return Vec3{
            std::cos(pitch) * std::cos(yaw),
            std::sin(pitch),
            std::cos(pitch) * std::sin(yaw)}
            .normalized();
    }

    Vec3 right() const
    {
        return forward().cross({0.f, 1.f, 0.f}).normalized();
    }

    // Call once per frame with delta time (seconds) and input state.
    // InputState is a plain struct — camera.h doesn't include sdl_window.h.
    void update(float dt, bool moveF, bool moveB, bool moveL, bool moveR,
                bool moveUp, bool moveDown, bool fast,
                float mouseDX, float mouseDY)
    {
        float speed = moveSpeed * (fast ? 4.f : 1.f) * dt;

        Vec3 f = forward();
        Vec3 r = right();

        if (moveF)
            position = position + f * speed;
        if (moveB)
            position = position - f * speed;
        if (moveR)
            position = position + r * speed;
        if (moveL)
            position = position - r * speed;
        if (moveUp)
            position.y += speed;
        if (moveDown)
            position.y -= speed;

        yaw += mouseDX * lookSpeed;
        pitch -= mouseDY * lookSpeed; // subtract: SDL y increases downward

        // Clamp pitch to avoid gimbal flip
        pitch = MathUtils::clamp(pitch,
                                 -MathUtils::HALF_PI + 0.01f,
                                 MathUtils::HALF_PI - 0.01f);
    }
};