#include "platform/sdl_window.h"
#include "core/renderer.h"
#include "scene/camera.h"
#include "scene/transform.h"
#include "scene/light.h"
#include "math/math_utils.h"
#include "loaders/obj_loader.h"
#include "shading/phong.h"
#include "shading/texture.h"
#include <iostream>

int main(int argc, char *argv[])
{
    const int WIDTH = 800;
    const int HEIGHT = 600;

    SDLWindow window("swr — software renderer", WIDTH, HEIGHT);
    Renderer renderer(WIDTH, HEIGHT);

    Camera camera;
    camera.position = {0.f, 0.f, 3.f};
    camera.fovY = MathUtils::toRadians(60.f);
    camera.aspect = static_cast<float>(WIDTH) / HEIGHT;
    camera.zNear = 0.1f;
    camera.zFar = 100.f;
    camera.moveSpeed = 3.f;
    camera.lookSpeed = 0.003f;

    // -----------------------------------------------------------------------
    // Lights
    // -----------------------------------------------------------------------
    LightList lightList;

    // Key light — warm directional from top-right
    Light keyLight;
    keyLight.type = LightType::Directional;
    keyLight.direction = Vec3{-1.f, -1.f, -1.f}.normalized();
    keyLight.color = {1.0f, 0.95f, 0.8f}; // warm white
    keyLight.intensity = 0.7f;
    lightList.add(keyLight);

    // Fill light — cool directional from the left, dimmer
    Light fillLight;
    fillLight.type = LightType::Directional;
    fillLight.direction = Vec3{1.f, -0.5f, 0.f}.normalized();
    fillLight.color = {1.0f, 1.0f, 1.0f}; // white
    fillLight.intensity = 0.3f;
    lightList.add(fillLight);

    // Point light — red, orbits the cube
    Light pointLight;
    pointLight.type = LightType::Point;
    pointLight.color = {1.0f, 1.0f, 1.0f}; // white
    pointLight.intensity = 1.2f;
    pointLight.attConstant = 1.f;
    pointLight.attLinear = 0.35f;
    pointLight.attQuadratic = 0.44f;
    lightList.add(pointLight); // position updated each frame below

    // -----------------------------------------------------------------------
    // Shader
    // -----------------------------------------------------------------------
    Texture diffuse;
    try
    {
        diffuse.load("assets/textures/cube.bmp");
    }
    catch (const std::exception &e)
    {
        std::cerr << "Texture: " << e.what() << "\n";
    }

    PhongShader phong;
    phong.lights = &lightList;
    phong.albedo = {0.8f, 0.8f, 0.8f};
    phong.specular = {1.f, 1.f, 1.f};
    phong.shininess = 64.f;
    phong.ambient = 0.04f;
    phong.diffuseMap = diffuse.loaded ? &diffuse : nullptr;
    renderer.activeShader = &phong;

    // -----------------------------------------------------------------------
    // Mesh
    // -----------------------------------------------------------------------
    Mesh mesh;
    bool hasMesh = false;
    try
    {
        mesh = loadOBJ("assets/models/cube.obj");
        hasMesh = true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "OBJ: " << e.what() << "\n";
    }

    static const Vertex triNear[3] = {
        {Vec3{-0.9f, 0.9f, -4.0f}, Vec3{0, 0, 1}, Vec2{0, 0}},
        {Vec3{0.9f, 0.9f, -4.0f}, Vec3{0, 0, 1}, Vec2{1, 0}},
        {Vec3{0.0f, -0.9f, -4.0f}, Vec3{0, 0, 1}, Vec2{0.5f, 1}},
    };
    static const Vertex triFar[3] = {
        {Vec3{-1.8f, 0.9f, -6.0f}, Vec3{0, 0, 1}, Vec2{0, 0}},
        {Vec3{0.0f, 0.9f, -6.0f}, Vec3{0, 0, 1}, Vec2{1, 0}},
        {Vec3{-0.9f, -0.9f, -6.0f}, Vec3{0, 0, 1}, Vec2{0.5f, 1}},
    };
    static const Vertex triBack[3] = {
        {Vec3{-0.5f, 1.2f, -5.0f}, Vec3{0, 0, 1}, Vec2{0, 0}},
        {Vec3{1.2f, 1.2f, -5.0f}, Vec3{0, 0, 1}, Vec2{1, 0}},
        {Vec3{0.35f, -1.2f, -5.0f}, Vec3{0, 0, 1}, Vec2{0.5f, 1}},
    };

    Transform modelTransform;
    float angle = 0.f;
    uint32_t last = SDL_GetTicks();

    while (window.pollEvents())
    {
        uint32_t now = SDL_GetTicks();
        float dt = (now - last) / 1000.f;
        last = now;

        angle += 1.5f * dt;
        modelTransform.rotation.y = angle;
        modelTransform.rotation.x = angle * 0.4f;

        // Orbit point light around the cube
        float orbitR = 2.f;
        lightList.lights[2].position = {
            std::cos(angle * 1.3f) * orbitR,
            std::sin(angle * 0.7f) * orbitR,
            std::sin(angle * 1.3f) * orbitR};

        const InputState &in = window.input();
        camera.update(dt,
                      in.w, in.s, in.a, in.d,
                      in.e, in.q,
                      in.shift,
                      in.mouseDX, in.mouseDY);

        phong.cameraPos = camera.position;

        renderer.beginFrame(0xFF111111);
        renderer.setMVP(modelTransform.matrix(), camera.view(), camera.projection());

        if (hasMesh)
            renderer.drawTriangles(mesh.vertices, mesh.indices, 0);
        else
        {
            renderer.drawRawTriangle(triBack, 0);
            renderer.drawRawTriangle(triFar, 0);
            renderer.drawRawTriangle(triNear, 0);
        }

        renderer.endFrame();
        window.present(renderer.pixelData());
    }

    return 0;
}